#!/usr/bin/python

from __future__ import with_statement

import threading
import cPickle
import os.path
import re
import gzip
import random
import time
import sqlite3
from binascii import unhexlify, hexlify

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
            self.need_rebuild = False
        if len(self)==0:
            self.need_rebuild = True
    def __len__(self):
        if(not os.path.isfile(DatDBFile)): return 0
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.execute('SELECT COUNT(*) FROM dattbl')
            return c.fetchone()[0]
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
    def makeDat(self, col):
        if col:
            dat = Dat(col[0])
            dat.published = col[1]
            return dat
        return None
    def getRandomInBoard(self,board):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.execute('SELECT datpath, published FROM dattbl WHERE board = ? ORDER BY RANDOM() LIMIT 1', (board,))
            return self.makeDat(c.fetchone())
    def choice(self):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.execute('SELECT datpath,published FROM dattbl ORDER BY RANDOM() LIMIT 1')
            return self.makeDat(c.fetchone())
    def get(self,x):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.execute('SELECT datpath, published FROM dattbl WHERE hash = ?', (hexlify(x),))
            return self.makeDat(c.fetchone())
    def has_keyhash(self,key):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.execute('SELECT COUNT(*) FROM dattbl WHERE hash = ?', (hexlify(key),))
            return c.fetchone()[0]==1
    def add_dat(self, dat):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.cursor()
            c.execute('SELECT datpath, published FROM dattbl WHERE hash = ?', 
                      (hexlify(dat.hash()),))
            befdat = self.makeDat(c.fetchone())
            if not befdat: dat.published = int(time.time())
            else: dat.published = befdat.published
            c.execute('REPLACE INTO dattbl  VALUES(?, ?, ?, ?)',
                      (dat.path(), hexlify(dat.hash()), dat.fullboard(), dat.published))
            try: c.execute('COMMIT')
            except sqlite3.OperationalError: pass
    def add(self, path, data, start=0):        
        dat = Dat(path)
        if dat.save(data, start): self.add_dat(dat)
    def published(self, datid, publish_time):
        if len(datid) != 20: raise Exception
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.cursor()
            c.execute('SELECT datpath, published FROM dattbl WHERE hash = ?', (hexlify(datid),))
            dat = self.makeDat(c.fetchone())
            if not dat: raise Exception
            dat.published = publish_time
            c.execute('UPDATE dattbl SET published = ? WHERE hash = ?', (publish_time,hexlify(datid),))
            try: c.execute('COMMIT')
            except sqlite3.OperationalError: pass
    def dat_to_publish(self, last_published_before, limit):
        res = []
        if limit == 0: return res
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c=sqlite_conn.execute('SELECT datpath, published FROM dattbl '+
                                  'WHERE published < ? ORDER BY published DESC LIMIT ?', 
                                  (last_published_before,limit))
            while True:
                dat = self.makeDat(c.fetchone())
                if not dat: return res
                res.append(dat)
            return res
    def generate(self):
        reghost = re.compile(regHosts+'$')
        regnumdir = re.compile('^\d{4}$')
        regdat = re.compile('^(\d+)\.dat(?:\.gz)?$')
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            c = sqlite_conn.cursor()
            try: c.execute('DROP TABLE dattbl')
            except sqlite3.OperationalError: pass
            c.execute('CREATE TABLE dattbl(datpath, hash PRIMARY KEY, board, published)')
        for h in os.listdir(o2on_config.DatDir):
            if not reghost.match(h): continue
            for b in os.listdir(os.path.join(o2on_config.DatDir, h)):
                with self.lock:
                    for d in os.listdir(os.path.join(o2on_config.DatDir, h, b)):
                        if not regnumdir.match(d): continue
                        for f in os.listdir(os.path.join(o2on_config.DatDir, h, b, d)):
                            m = regdat.match(f)
                            if not m: continue
                            path = h+"/"+b+"/"+m.group(1)
                            dat = Dat(path)
                            try:
                                c.execute('INSERT OR IGNORE INTO dattbl VALUES(?, ?, ?, ?)', 
                                          (path, hexlify(dat.hash()), dat.fullboard(), 0))
                            except sqlite3.IntegrityError:
                                raise Exception("dup hash %s %s" % (hexlify(dat.hash()), path))
                            self.glob.logger.log("DATDB", "added %s" % path)
                    try: c.execute('COMMIT')
                    except sqlite3.OperationalError: pass
    def load(self):
        pass
    def save(self):
        with self.lock:
            sqlite_conn = sqlite3.connect(DatDBFile)
            try: sqlite_conn.execute('COMMIT')
            except sqlite3.OperationalError: pass
                

