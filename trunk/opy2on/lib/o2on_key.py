#!/usr/bin/python
# -*- coding: utf-8

from __future__ import with_statement

import os.path
import re
import cPickle
import os.path
from binascii import unhexlify, hexlify
import xml.dom.minidom
import threading
import subprocess
import os
import datetime
import hashlib
import time

from o2on_const import KeyDBFile, regHosts
import o2on_config
from o2on_util import hash_xor_bitlength
import o2on_dat
import o2on_node

class Key:
    def __init__(self):
        self.hash = None
        self.nodeid = None
        self.ip = None
        self.port = None
        self.size = None
        self.url = ""
        self.title = ""
        self.note = ""
        self.published = 0
        self.ikhash = None
    def __cmp__(self,x):
        return cmp(self.idkeyhash(), x.idkeyhash())
    def idkeyhash(self):
        if not self.ikhash: 
            if self.nodeid: self.ikhash = hashlib.sha1(self.hash + self.nodeid).digest()
            else: self.ikhash = self.hash
        return self.ikhash
    def from_key(self, key):
        if self.idkeyhash() == key.idkeyhash():
            self.ip = key.ip
            self.port = key.port
            if self.size < key.size: self.size = key.size
            if key.url!="": self.url = key.url
            if key.title!="":self.title = key.title
            if key.note!="":self.note = key.note
    def from_dat(self, dat):
        self.hash = dat.hash()
        data = dat.data()
        self.size = len(data)
        first = data.split("\n",1)[0]
        try:
            first = first.decode('cp932').encode('utf-8')
        except UnicodeDecodeError, inst:
            try:
                first = first.decode('euc_jp').encode('utf-8')
            except UnicodeDecodeError, inst: raise inst
        m = re.compile(r'^.*<>.*<>.*<>.*<>(.*)$').match(first)
        if m: self.title = m.group(1)
        #reg = re.compile(r'^(?:2ch\.net|machibbs\.com)/(?:cyugoku|hokkaidou|k(?:an(?:a|to)|inki|(?:ousinet|yusy)u)|o(?:(?:kinaw|sak)a)|sikoku|t(?:a(?:(?:m|war)a)|o(?:kyo|u(?:hoku|kai))))')
        #if re.compile("^\s*$").match(self.title) and \
        #        not reg.match(dat.path()): print dat.datpath()
        self.url = "http://xxx.%s/test/read.cgi/%s/%s/" % (dat.domain,
                                                           dat.board,
                                                           dat.datnum)
    def from_node(self, node):
        self.nodeid = node.id
        self.ip = node.ip
        self.port = node.port
    def xml(self):
        if not self.valid(): return ""
        board_xml =  "<key>\r\n"
        board_xml += "<hash>%s</hash>\r\n" % hexlify(self.hash)
        board_xml += "<nodeid>%s</nodeid>\r\n" % hexlify(self.nodeid)
        board_xml += "<ip>%s</ip>\r\n" % o2on_node.ip2e(self.ip)
        board_xml += "<port>%d</port>\r\n" % (self.port or 0)
        board_xml += "<size>%d</size>\r\n" % self.size
        board_xml += "<url>%s</url>\r\n" % self.url
        title = self.title.decode('utf-8')
        if len(title)>32: title = title[:31]+"…".decode('utf-8')
        board_xml += "<title><![CDATA[%s]]></title>\r\n" % title
        note = self.note.decode('utf-8')
        if len(note)>32: note = note[:31]+"…".decode('utf-8')
        board_xml += "<note><![CDATA[%s]]></note>\r\n" % note
        board_xml += "</key>\r\n"
        return board_xml
    def valid(self):
        return self.hash and self.nodeid and self.ip and self.port
    def from_xml_node(self, xml_node):
        res = {}
        for c in ('hash', 'nodeid', 'ip', 'port', 'size', 'url', 'title', 'note'):
            try: res[c] = xml_node.getElementsByTagName(c)[0].childNodes[0].data
            except IndexError: continue
        if res.get('hash'): self.hash = unhexlify(res.get('hash'))
        if res.get('nodeid'): self.nodeid = unhexlify(res.get('nodeid'))
        if res.get('ip'): self.ip = o2on_node.e2ip(res.get('ip'))
        self.port = res.get('port', self.port)
        if self.port: self.port = int(self.port)
        self.size = res.get('size', self.size)
        if self.size: self.size = int(self.size)
        self.url = res.get('url', self.url)
        if res.get('title'): self.title = res.get('title').encode('utf-8')
        if res.get('note'): self.note = res.get('note').encode('utf-8')

class KeyDB:
    def __init__(self, g):
        self.lock = threading.Lock()
        with self.lock:
            self.keys = {}
            self.lenmap = {}
            self.publishmap = {}
            self.datkeymap = {}
        self.glob = g
        self.load()
    def __len__(self):
        with self.lock:
            return len(self.keys)
    def show(self):
        pager = os.environ.get('PAGER')
        if not pager: 
            self.glob.logger.log("KEYDB", "PAGER env must be set")
            return
        proc = subprocess.Popen(pager, shell=True, stdin=subprocess.PIPE)
        pipe = proc.stdin
        try: 
            with self.lock:
                for l in sorted(self.lenmap.keys()):
                    for h in self.lenmap[l]:
                        key = self.keys[h]
                        s = "%3d %s:%d\t%s\t%s\t%s\t%s\t%dB\t%s\n" % \
                            (l, o2on_node.ip2e(key.ip),
                             key.port, key.url,
                             key.title.decode('utf-8'),
                             key.note.decode('utf-8'),
                             str(datetime.datetime.fromtimestamp(int(key.published))),
                             key.size,
                             hexlify(key.hash))
                        pipe.write(s.encode('utf-8'))
                pipe.close()
                proc.wait()
        except IOError, inst:
            if inst.errno == 32:pass # Broken pipe
            else: raise inst
        self.glob.logger.log("KEYDB", "Finished to show keys")
    def key_list(self):
        res = []
        with self.lock:
            for l in sorted(self.lenmap.keys()):
                for h in self.lenmap[l]:
                    key = self.keys[h]
                    x = (l, o2on_node.ip2e(key.ip),
                         key.port, key.url,
                         key.title.decode('utf-8'),
                         key.note.decode('utf-8'),
                         str(datetime.datetime.fromtimestamp(int(key.published))),
                         key.size,
                         hexlify(key.hash))
                    res.append(x)
        return res
    def keys_to_publish(self, last_published_before):
        res = []
        for x in sorted(self.publishmap.keys()):
            for y in self.publishmap[x]:
                res.append(self.keys[y])
        return res
    def published(self, idkeyhash, publish_time):
        if len(idkeyhash) != 20: raise Exception
        with self.lock:
            if idkeyhash not in self.keys: return
            key = self.keys[idkeyhash]
            self.publishmap[key.published].remove(idkeyhash)
            key.published = publish_time
            if publish_time not in self.publishmap: self.publishmap[publish_time]=[]
            self.publishmap[publish_time].append(idkeyhash)
    def get_bydatkey(self, target):
        with self.lock:
            return map(lambda x: self.keys.get(x), self.datkeymap.get(target,[]))
    def remove_bynodeid(self, nid):
        if len(nid) != 20:raise Exception
        removes = []
        with self.lock:
            for k in self.keys.values():
                if k.nodeid == nid: removes.append(k)
        for k in removes:
            self.remove(k)
    def remove(self,k):
        with self.lock:
            del self.keys[k.idkeyhash()]
            l = hash_xor_bitlength(self.glob.prof.mynode.id, k.hash)
            self.lenmap[l].remove(k.idkeyhash())
            self.publishmap[k.published].remove(k.idkeyhash())
            self.datkeymap[k.hash].remove(k.idkeyhash())
            if len(self.lenmap[l]) == 0: del self.lenmap[l]
            if len(self.publishmap[k.published])==0: del self.publishmap[k.published]
            if len(self.datkeymap[k.hash])==0: del self.datkeymap[k.hash]
    def add(self, k):
        if not k.valid(): return
        k.published = int(time.time())
        if k.idkeyhash() in self.keys:
            self.keys[k.idkeyhash()].from_key(k)
            return
        bl = hash_xor_bitlength(self.glob.prof.mynode.id, k.hash)
        with self.lock:
            if len(self.keys) < 3000:
                pass
            else:
                maxlen = max(self.lenmap.keys())
                if bl <= maxlen:
                    maxkey = self.keys[self.lenmap[maxlen][0]]
                    del self.keys[self.lenmap[maxlen][0]]
                    del self.lenmap[maxlen][0]
                    if len(self.lenmap[maxlen]) == 0:
                        del self.lenmap[maxlen]
                    self.publishmap[maxkey.published].remove(maxkey.idkeyhash())
                    if len(self.publishmap[maxkey.published]) == 0:
                        del self.publishmap[maxkey.published]
                    self.datkeymap[maxkey.hash].remove(maxkey.idkeyhash())
                    if len(self.datkeymap[maxkey.hash]) == 0:
                        del self.datkeymap[maxkey.hash]
                else: return
            self.keys[k.idkeyhash()] = k
            if bl not in self.lenmap: self.lenmap[bl] = []
            self.lenmap[bl].append(k.idkeyhash())
            if k.published not in self.publishmap: 
                self.publishmap[k.published] = []
            self.publishmap[k.published].append(k.idkeyhash())
            if k.hash not in self.datkeymap:
                self.datkeymap[k.hash] = []
            self.datkeymap[k.hash].append(k.idkeyhash())
    def save(self):
        f = open(KeyDBFile,'wb')
        with self.lock:
            cPickle.dump(self.keys, f, -1)
            cPickle.dump(self.lenmap, f, -1)
            cPickle.dump(self.publishmap,f,-1)
            cPickle.dump(self.datkeymap,f,-1)
        f.close()
    def load(self):
        if os.path.isfile(KeyDBFile):
            f = open(KeyDBFile,'rb')
            with self.lock:
                self.keys = cPickle.load(f)
                self.lenmap = cPickle.load(f)
                self.publishmap = cPickle.load(f)
                try:
                    self.datkeymap = cPickle.load(f)
                except EOFError:
                    rebuild = True
                else: rebuild = False
            f.close()
            if rebuild:
                keys = self.keys.values()
                self.keys.clear()
                self.lenmap.clear()
                self.publishmap.clear()
                for k in keys: self.add(k)
