#!/usr/bin/python

from __future__ import with_statement

import threading
import cPickle
import os.path
import re
import gzip
import random
import time

from o2on_const import regHosts, DatQueryFile, DatDBFile
import o2on_config
import o2on_util

class Dat:
    def __init__(self, path=None):
        self.reset()
        self.published = 0
        if path: self.setpath(path)
    def __cmp__(self,x):
        return cmp(self.hash(), x.hash())
    def reset(self):
        self.domain = None
        self.board =  None
        self.datnum = None
    def setpath(self,path):
        self.reset()
        regDatPath = re.compile(r'^'+regHosts+'/([^/]+)/(\d+)$')
        regDatUrl  = re.compile(r'^http://[^.]+\.'+regHosts+'/test/read\.cgi/([^/]+)/(\d+)/$')
        m = regDatPath.match(path)
        if not m: m = regDatUrl.match(path)
        if not m: return False
        d = m.group(1)
        if d == 'machi.to': d = 'machibbs.com'
        self.domain = d
        self.board =  m.group(2)
        self.datnum = m.group(3)
        return True
    def path(self):
        if self.valid():
            return "/".join((self.domain, self.board, self.datnum))
    def fullboard(self): 
        if self.domain and self.board:
            return self.domain+":"+self.board
        raise Exception
    def valid(self):
        return ((self.domain and self.board and self.datnum) != None)
    def datpath(self):
        if self.valid():
            return os.path.join(o2on_config.DatDir, self.domain, self.board, 
                                self.datnum[:4],self.datnum+".dat")
        raise Exception
    def title(self):
        dp = self.datpath()
        if os.path.exists(dp):         f=open(dp)
        elif os.path.exists(dp+".gz"): f=gzip.GzipFile(dp+".gz")
        else: f=None
        if f:
            first=f.readline()
            f.close()
            m = re.compile(r'^.*<>.*<>.*<>.*<>(.*)$').match(first)
            if not m: return None
            try:
                first = first.decode('cp932').encode('utf-8')
            except UnicodeDecodeError, inst:
                try:
                    first = first.decode('euc_jp').encode('utf-8')
                except UnicodeDecodeError, inst: 
                    first = first.decode('cp932','opy2on_replace').encode('utf-8')
            m = re.compile(r'^.*<>.*<>.*<>.*<>(.*)$').match(first)
            if not m: return None
            return m.group(1)
        return None
    def data(self):
        dp = self.datpath()
        if os.path.exists(dp):         f=open(dp)
        elif os.path.exists(dp+".gz"): f=gzip.GzipFile(dp+".gz")
        else: f=None
        if f:
            res=f.read()
            f.close()
            return res
        return None
    def hash(self):
        return o2on_util.datkeyhash(self.path())
    def save(self,data, start=0):
        if not self.valid(): return
        bl = o2on_config.DatCollectionBoardList
        if bl != None: 
            if not self.fullboard() in bl: return False
        if(len(data)<2): return False
        if(data[-1] != "\n"): return False
        if(data[-2] == "\r"): return False
        if start == 0:
            m = re.compile(r'^([^\n]*)\n').match(data)
            first = m.group(1)
            if  not re.compile(r'^.*<>.*<>.*<>.*<>.*$').match(first): return False
        output = self.datpath()
        if os.path.exists(output):
            if o2on_config.DatSaveAsGzip:
                f = open(output,'rb')
                g = gzip.GzipFile(output+".gz",'w')
                g.write(f.read())
                g.close()
                f.close()
                os.remove(output)
            else:
                if(os.path.getsize(output)<start): return True
                if(os.path.getsize(output)==start):
                    f=open(output,'ab')
                    f.write(data)
                    f.close()
                    return True
                f=open(output,'rb')
                saved = f.read()
                f.close()
                if saved[start:] == data[:(len(saved)-start)]:
                    l = start + len(data) - len(saved)
                    if l >0:
                        f=open(output,'ab')
                        f.write(data[(len(saved)-start):])
                        f.close()
                        return True
                return True
        if os.path.exists(output+".gz"):
            f=gzip.GzipFile(output+".gz",'rb')
            saved = f.read()
            f.close()
            if(len(saved)<start): return True
            if(len(saved)==start):
                f=gzip.GzipFile(output+".gz",'ab')
                f.write(data)
                f.close()
                return True
            if saved[start:] == data[:(len(saved)-start)]:
                l = start + len(data) - len(saved)
                if l >0:
                    f=gzip.GzipFile(output+".gz",'ab')
                    f.write(data[(len(saved)-start):])
                    f.close()
                    return True
            return True
        else:
            if start>0: return True
            if not os.path.exists(os.path.dirname(output)): 
                os.makedirs(os.path.dirname(output))
            if o2on_config.DatSaveAsGzip:
                f = gzip.GzipFile(output+".gz",'w')
            else:
                f = open(output, 'wb')
            f.write(data)
            f.close()
            return True

class DatDB:
    def __init__(self,g):
        self.glob = g
        self.lock = threading.Lock()
        with self.lock:
            self.hashmap = {}
            self.boardmap = {}
            self.publishmap = {}
            self.need_rebuild = False
        self.load()
        if len(self.hashmap) == 0:
            self.need_rebuild = True
    def __len__(self):
        with self.lock:
            return len(self.hashmap)
    def checkrebuild(self):
        with self.lock:
            tmp = self.need_rebuild
        if tmp:
            self.glob.logger.log("DATDB", "Generating DatDB")
            self.generate()
            self.save()
            self.glob.logger.log("DATDB","Generated DatDB")
            with self.lock:
                self.need_rebuild = False
    def getRandomInBoard(self,board):
        if board in self.boardmap:
            h = random.choice(self.boardmap[board])
            return self.hashmap[h]
        return None
    def choice(self):
        return self.hashmap[random.choice(self.hashmap.keys())]
    def get(self,x):
        with self.lock:
            return self.hashmap.get(x)
    def has_keyhash(self,key):
        with self.lock:
            return key in self.hashmap
    def add_dat(self, dat):
        with self.lock:
            befdat = self.hashmap.get(dat.hash())
            self.hashmap[dat.hash()] = dat
            if not dat.fullboard() in self.boardmap:
                self.boardmap[dat.fullboard()] = []
            self.boardmap[dat.fullboard()].append(dat.hash())
            if not befdat:
                dat.published = int(time.time())
                if dat.published not in self.publishmap: 
                    self.publishmap[dat.published]=[]
                self.publishmap[dat.published].append(dat.hash())
            else:
                dat.published = befdat.published
    def add(self, path, data, start=0):
        dat = Dat(path)
        if dat.save(data, start): self.add_dat(dat)
    def published(self, datid, publish_time):
        if len(datid) != 20: raise Exception
        with self.lock:
            if datid not in self.hashmap: raise Exception
            dat = self.hashmap[datid]
            self.publishmap[dat.published].remove(datid)
            dat.published = publish_time
            if publish_time not in self.publishmap: self.publishmap[publish_time]=[]
            self.publishmap[publish_time].append(datid)
    def dat_to_publish(self, last_published_before, limit):
        res = []
        if limit == 0: return res
        for x in sorted(self.publishmap.keys()):
            for y in self.publishmap[x]:
                res.append(self.hashmap[y])
                limit -= 1
                if limit == 0: return res
        return res
    def generate(self):
        regdat = re.compile('^(\d+)\.dat(?:\.gz)?$')
        sep = re.escape(os.sep)
        regdatdir = re.compile(regHosts+sep+'(.+)'+sep+'\d{4}$')
        with self.lock:
            self.hashmap = {}
            self.boardmap = {}
            self.publishmap = {0:[]}
        for root, dirs, files in os.walk(o2on_config.DatDir):
            for f in files:
                m1 = regdat.match(f)
                if not m1: continue
                m2 = regdatdir.search(root)
                if not m2: continue
                path = m2.group(1)+"/"+m2.group(2)+"/"+m1.group(1)
                d = Dat(path)
                with self.lock:
                    self.hashmap[d.hash()] = d
                    if not d.fullboard() in self.boardmap:
                        self.boardmap[d.fullboard()] = []
                    self.boardmap[d.fullboard()].append(d.hash())
                    self.publishmap[0].append(d.hash())
                self.glob.logger.log("DATDB", "added %s" % path)
    def load(self):
        if(os.path.isfile(DatDBFile)):
            pkl_file = open(DatDBFile,"rb")
            with self.lock:
                self.hashmap = cPickle.load(pkl_file)
                self.boardmap =  cPickle.load(pkl_file)
                self.publishmap =  cPickle.load(pkl_file)
            pkl_file.close()
    def save(self):
        pkl_file = open(DatDBFile,"wb")
        with self.lock:
            cPickle.dump(self.hashmap, pkl_file,-1)
            cPickle.dump(self.boardmap, pkl_file,-1)
            cPickle.dump(self.publishmap, pkl_file,-1)
        pkl_file.close()

