#!/usr/bin/python

import random
from struct import pack 
import threading
import hashlib
import re
import os.path
import cPickle

import o2on_config
from o2on_const import DatQueryFile, KeyQueryFile, AppName

if o2on_config.UseDBus: import dbus

def randomid():
    return "".join(map(lambda x: pack("B",random.randint(0,0xff)),range(0,20)))

def datkeyhash(key):
    return hashlib.sha1(key.encode('utf_16_le')).digest()
def datfullboard(key):
    m=re.compile(r'^([^/]+)/([^/]+)').match(key)
    return m.group(1)+":"+m.group(2)

class Logger:
    def __init__(self):
        self.lock = threading.RLock()
        self.bus = None
    def begin(self): self.lock.acquire()
    def end(self): self.lock.release()
    def log(self, categ, s):
        if o2on_config.NoLog: return
        categ = categ[:10]
        categ += " " * (10-len(categ))
        if isinstance(s, str): pass #s=s.encode('utf-8','replace')
        elif isinstance(s, unicode): pass
        else: s = str(s)
        with self.lock:
            for l in map(lambda x: "[%s] %s" % (categ, s), s.split("\n")):
                    print l
    def popup(self, categ, s):
        if o2on_config.UseDBus:
            if not self.bus: self.bus = dbus.SessionBus()
            obj = self.bus.get_object("org.freedesktop.Notifications",
                                      "/org/freedesktop/Notifications")
            obj.Notify(AppName, 0, '', AppName+" "+categ,s, [], {}, -1,
                       dbus_interface="org.freedesktop.Notifications")
        else:
            self.log(categ, s)

def hash_xor_bitlength(a,b):
    if len(a) != 20 or len(b) != 20: raise Exception
    for i in range(len(a)-1,-1,-1):
        xored = ord(a[i]) ^ ord(b[i])
        for j in range(7,-1,-1):
            if (xored & (1<<j)):
                return i*8+j+1
    return 0

def xml_collecting_boards(glob):
    boards = o2on_config.DatCollectionBoardList or glob.allboards
    board_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
    board_xml += "<boards>\r\n"
    for b in boards: board_xml += "<board>%s</board>\r\n" % b
    board_xml += "</boards>\r\n"
    return board_xml

class Query:
    def __init__(self, filename):
        self.lock = threading.Lock()
        self.file = filename
        with self.lock:
            self.list = []
        self.load()
        with self.lock:
            self.semap = threading.Semaphore(len(self.list))
    def __len__(self):
        with self.lock: return len(self.list)
    def __str__(self):
        with self.lock:
            return "\n".join(map(str, self.list))
    def save(self):
        pkl_file = open(self.file,"wb")
        with self.lock:
            cPickle.dump(self.list, pkl_file,-1)
        pkl_file.close()
    def load(self):
        if(os.path.isfile(self.file)):
            pkl_file = open(self.file,"rb")
            with self.lock:
                self.list = cPickle.load(pkl_file)
            pkl_file.close()
    def add(self,x):
        with self.lock:
            if not x in self.list:
                self.list.append(x)
                self.semap.release()
    def pop(self):
        self.semap.acquire()
        with self.lock:
            if len(self.list):
                return self.list.pop(0)

class DatQuery(Query):
    def __init__(self):
        Query.__init__(self, DatQueryFile)
class KeyQuery(Query):
    def __init__(self):
        Query.__init__(self, KeyQueryFile)
