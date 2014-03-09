#!/usr/bin/python

from __future__ import with_statement

import threading
import os.path
import cPickle
import datetime

from o2on_const import IMDBFile
from o2on_node import e2ip, ip2e
from binascii import hexlify, unhexlify

class IMessage:
    def __init__(self):
        self.ip = None
        self.port = None
        self.id = None
        self.pubkey = None
        self.name = ""
        self.date = 0
        self.msg = None
        self.key = None
        self.mine = False
        self.paths = [] 
        self.broadcast = False
    def __eq__(self, x):
        if self.broadcast != x.broadcast: return False
        if self.broadcast:
            return self.ip == x.ip and self.port == x.port and self.msg == x.msg
        else:
            return self.key == x.key
    def from_node(self, n):
        self.ip = n.ip
        self.port = n.port
        self.id = n.id
        self.pubkey = n.pubkey
        self.name = n.name
    def from_xml_node(self, xml_node):
        res = {}
        for c in ('ip','port','id','pubkey','name','msg',):
            try: res[c] = xml_node.getElementsByTagName(c)[0].childNodes[0].data
            except IndexError: continue
        if res.get('ip'): self.ip = e2ip(res.get('ip'))
        if res.get('port'): self.port = int(res.get('port'))
        if res.get('id'): self.id = unhexlify(res.get('id'))
        if res.get('pubkey'): self.pubkey = unhexlify(res.get('pubkey'))
        if res.get('name'): self.name = res.get('name').encode('utf-8')
        if res.get('msg'): self.msg = res.get('msg').encode('utf-8')

class IMDB:
    def __init__(self, g):
        self.lock = threading.Lock()
        with self.lock:
            self.ims = []
        self.glob = g
        self.load()
    def im_list(self):
        res = []
        with self.lock:
            for i in self.ims:
                x = (i.mine, str(datetime.datetime.fromtimestamp(int(i.date))),
                     hexlify(i.id), ip2e(i.ip), i.port, i.name.decode('utf-8'), 
                     i.msg.decode('utf-8'))
                res.append(x)
        return res
    def add(self,im):
        with self.lock:
            self.ims.append(im)
    def save(self):
        self.expire()
        f = open(IMDBFile,'wb')
        with self.lock:
            cPickle.dump(self.ims, f, -1)
        f.close()
    def load(self):
        if os.path.isfile(IMDBFile):
            f = open(IMDBFile,'rb')
            with self.lock:
                self.ims = cPickle.load(f)
            f.close()
    def expire(self):
        pass
