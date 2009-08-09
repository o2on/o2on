#!/usr/bin/python
# -*- coding: utf-8

from __future__ import with_statement

import os
from Crypto.Cipher import AES # pycrypto
import xml.dom.minidom
import re
import urllib2
import cPickle
from struct import unpack, pack
from binascii import unhexlify, hexlify
import httplib
import socket
import threading
import random
import time
from errno import EHOSTUNREACH, ECONNREFUSED, ETIMEDOUT, ECONNRESET

import o2on_config
from o2on_util import hash_xor_bitlength
import o2on_const
import o2on_util
import o2on_dat
import o2on_key

class NodeRemovable:
    pass
class NodeRefused:
    pass

AESKEY = "0000000000000000"

def aescounter():
    return "\x00" * 16

def e2port(s):
    aesobj = AES.new(AESKEY, AES.MODE_CTR, counter=aescounter)
    return unpack("<H",aesobj.decrypt(unhexlify(s)+"\x00"*14)[0:2])[0]

def e2ip(s):
    aesobj = AES.new(AESKEY, AES.MODE_CTR, counter=aescounter)
    x = aesobj.decrypt(unhexlify(s)+"\x00"*12)
    return ("%d.%d.%d.%d" % (int(unpack("B",x[0])[0]),
                             int(unpack("B",x[1])[0]),
                             int(unpack("B",x[2])[0]),
                             int(unpack("B",x[3])[0])))

def port2e(p):
    aesobj = AES.new(AESKEY, AES.MODE_CTR, counter=aescounter)
    return hexlify(aesobj.encrypt(pack("<H",p)+"\x00"*14)[:2])

def ip2e(ip):
    aesobj = AES.new(AESKEY, AES.MODE_CTR, counter=aescounter)
    regIP = re.compile(r'(\d+)\.(\d+)\.(\d+)\.(\d+)')
    m = regIP.match(ip)
    if not m: raise Exception
    return hexlify(aesobj.encrypt(pack("B", int(m.group(1)))+
                                  pack("B", int(m.group(2)))+
                                  pack("B", int(m.group(3)))+
                                  pack("B", int(m.group(4)))+"\x00"*12)[:4])

def hash_xor(a,b):
    res = ""
    for i in range(0,len(a)):
        res += chr(ord(a[i]) ^ ord(b[i]))
    return res

def hash_bittest(x, pos):
    return (((ord(x[pos/8]) >> (pos%8)) & 0x01 != 0))

common_header = {}
def build_common_header(prof):
    global common_header
    if not prof.mynode.id: raise Exception("My ID is NULL")
    if prof.mynode.port==None: raise Exception("My Port is NULL")
    if not prof.mynode.name: prof.mynode.name=""
    if not prof.mynode.pubkey: raise Exception("My pubkey is NULL")
    common_header = {'Connection': "close",
                     'X-O2-Node-ID': hexlify(prof.mynode.id),
                     'X-O2-Port': str(prof.mynode.port),
                     'X-O2-Node-Name': prof.mynode.name,
                     'X-O2-Node-Flags':'--D',
                     'User-Agent': prof.mynode.ua,
                     'X-O2-RSA-Public-Key': hexlify(prof.mynode.pubkey)}
    
class Node:
    def __init__(self, node_id=None, i=None, p=None):
        self.id = node_id
        self.ip = i
        self.port = p
        self.name = None
        self.pubkey = None
        self.ua = ""
        self.lock = threading.Lock()
        self.lastping = None
        self.removable = False
        self.flag = "???"
        self.flag_running = False
        self.flag_history = False
        self.flag_dat = False
    def __getstate__(self):
        return (self.id, self.ip,self.port, self.name, self.pubkey,self.ua,self.flag)
    def __setstate__(self,x):
        self.id, self.ip, self.port, self.name, self.pubkey,self.ua,self.flag = x[:7]
        self.setflag(self.flag)
        self.lock = threading.Lock()
        self.lastping = None
        self.removable = False
    def __cmp__(self,x):
        return cmp(self.id, x.id)
    def from_node(self, n):
        if self.id == n.id:
            self.ip = n.ip
            self.port = n.port
            self.pubkey = n.pubkey
            # TODO pubkey の変更には気をつけたほうがいい
            if n.ua !="": self.ua = n.ua
            if n.name !="": self.name = n.name
            if n.flag !="": self.setflag(n.flag)
    def from_xml(self, xmlnode):
        try:
            self.ip = e2ip(xmlnode.getElementsByTagName("ip")[0].childNodes[0].data)
            self.port = int(xmlnode.getElementsByTagName("port")[0].childNodes[0].data)
            self.id = unhexlify(xmlnode.getElementsByTagName("id")[0].childNodes[0].data)
        except KeyError: return
        names = xmlnode.getElementsByTagName("name")
        if len(names)>0 and len(names[0].childNodes)>0:
            self.name = names[0].childNodes[0].data.encode('utf-8')
        pubkeys = xmlnode.getElementsByTagName("pubkey")
        if len(pubkeys)>0 and len(pubkeys[0].childNodes)>0:
            self.pubkey = unhexlify(pubkeys[0].childNodes[0].data)
    def xml(self):
        data = "<node>\r\n"
        data += "<id>%s</id>\r\n" % hexlify(self.id)
        data += "<ip>%s</ip>\r\n" % ip2e(self.ip)
        data += "<port>%s</port>\r\n" % self.port
        if self.name:
            data += "<name><![CDATA[%s]]></name>\r\n" % self.name.decode('utf-8')
        if self.pubkey:
            data += "<pubkey>%s</pubkey>\r\n" % hexlify(self.pubkey)
        data += "</node>\r\n"
        return data
    def setflag(self,flag):
        self.flag = flag
        self.flag_running = "r" in flag
        self.flag_history = "t" in flag
        self.flag_dat = "D" in flag
    def request(self, method, path, body, addheaders):
        if self.removable: raise NodeRemovable
        headers = common_header.copy()
        if method == 'POST':
            headers['X-O2-Node-Name'] = \
                headers['X-O2-Node-Name'].decode('utf-8').encode('ascii','replace')
        for x in addheaders: headers[x] = addheaders[x]
        with self.lock:
            conn = httplib.HTTPConnection(self.ip, self.port)
            try:
                socket.setdefaulttimeout(15)
                conn.connect()
                socket.setdefaulttimeout(None)
                conn.sock.settimeout(o2on_config.SocketTimeout)
                conn.request(method,path,body, headers)                
                r = conn.getresponse()
                conn.close()
            except socket.timeout:
                socket.setdefaulttimeout(None)
                self.removable = True
                raise NodeRemovable
            except socket.error, inst:
                socket.setdefaulttimeout(None)
                errno = None
                if hasattr(inst, 'errno'): errno = inst.errno
                else: errno =  inst[0]
                if errno in (EHOSTUNREACH, ETIMEDOUT): raise NodeRemovable
                if errno in (ECONNREFUSED, ECONNRESET): raise NodeRefused
                else: raise inst
            except httplib.BadStatusLine: 
                socket.setdefaulttimeout(None)
                raise NodeRefused
            else: 
                name = r.getheader('X-O2-Node-Name')
                pubkey = r.getheader('X-O2-RSA-Public-Key')
                ua = r.getheader('Server')
                flag = r.getheader('X-O2-Node-Flags')
                if not name or not pubkey or not ua: raise NodeRemovable
                self.name = name
                self.pubkey = unhexlify(pubkey)
                self.ua = ua
                if flag: self.setflag(flag)
                return r
    def im(self, mynode, msg):
        headers = {}
        im_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        im_xml += "<messages>\r\n"
        im_xml += "<message>\r\n"
        mynode_xml = mynode.xml()
        mynode_xml = mynode_xml[len("<node>\r\n"):]
        mynode_xml = mynode_xml[:-len("</node>\r\n")]
        im_xml += mynode_xml
        im_xml += "<msg><![CDATA[%s]]></msg>\r\n" % msg
        im_xml += "</message>\r\n"
        im_xml += "</messages>\r\n"
        im_xml = im_xml.encode('utf-8')
        headers['Content-Type'] = 'text/xml; charset=utf-8'
        headers['Content-Length'] = str(len(im_xml))
        r = self.request("POST", "/im", im_xml, headers)
        if r.status != 200: raise Exception("im status %d" % r.status)
        return True
    def store(self,category, keys):
        headers={"X-O2-Key-Category": category}
        board_xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        board_xml += "<keys>\r\n"
        for key in keys: board_xml += key.xml()
        board_xml += "</keys>\r\n"
        board_xml = board_xml.encode('utf-8')
        headers['Content-Type'] = 'text/xml; charset=utf-8'
        headers['Content-Length'] = str(len(board_xml))
        r = self.request("POST", "/store", board_xml, headers)
        if r.status != 200: raise Exception("store status %d" % r.status)
        return
    def findnode(self, targetid):
        headers = {"X-O2-Target-Key" : hexlify(targetid)}
        r = self.request("GET","/findnode",None,headers)
        data = None
        result = []
        try:
            if r.status == 200:
                l = r.getheader('Content-Length')
                if l: l = int(l)
                else: return []
                data = r.read(l)
            elif r.status == 404: pass
            else: raise Exception("status error %d" % r.status)
        except socket.error:
            pass
        else:
            if data:
                dom = xml.dom.minidom.parseString(data)
                nn = dom.getElementsByTagName("nodes")
                if len(nn):
                    for n in nn[0].getElementsByTagName("node"):
                        node = Node()
                        node.from_xml(n)
                        if node.ip and node.port and node.id:
                            result.append(node)
                dom.unlink()
        return result
    def dat(self, dathash, board, datdb=None):
        headers = {}
        omiyage = None
        if datdb and board:
            omiyage = datdb.getRandomInBoard(board)
        if(dathash):
            headers['X-O2-Target-Key'] = hexlify(dathash)
        elif(board):
            headers['X-O2-Target-Board'] = board
        if omiyage:
            data = omiyage.data()
            headers['X-O2-DAT-Path'] = omiyage.path()
            headers['Content-Type'] = 'text/plain; charset=shift_jis'
            headers['Content-Length'] = str(len(data))
        if omiyage:
            r = self.request("POST","/dat", data,headers)
        else:
            r = self.request("GET","/dat",None,headers)
        stat = r.status
        data = r.read()
        if stat == 200:
            path = None
            path = r.getheader("X-O2-Original-DAT-URL")
            if not path: path = r.getheader("X-O2-DAT-Path")
            if not path: return None
            if ".." in path: return None
            dat = o2on_dat.Dat(path)
            if dat.save(data): return dat
        elif stat == 404: pass
        elif stat == 400: pass
        else: raise Exception("dat status %d" % stat)
        return None
    def ping(self, force=False):
        if not force and self.lastping and \
                (time.time() - self.lastping < o2on_config.RePingSec):
            return True
        r = self.request("GET","/ping",None,{})
        if r.status == 200:
            self.lastping = int(time.time())
            l = r.getheader('Content-Length')
            if l: l = int(l)
            else: return False
            return r.read(l)
        else: raise Exception("ping status %d" % r.status)
        return False
    def collection(self, glob):
        board_xml = o2on_util.xml_collecting_boards(glob)
        headers = {'Content-Type':'text/xml; charset=utf-8',
                   'Content-Length':len(board_xml)}
        r = self.request("POST","/collection",board_xml,headers)
        result = []
        if r.status == 200:
            data = r.read()
            # 本家o2on の bug 対策
            if data.rfind("</boards>") == -1:
                index = data.rfind("<boards>")
                data = data[:index] + "</boards>" + data[index+len("<boards>"):]
            if len(data):
                dom = xml.dom.minidom.parseString(data)
                nn = dom.getElementsByTagName("boards")
                if len(nn):
                    for b in nn[0].getElementsByTagName("board"):
                        result.append(b.childNodes[0].data)
                dom.unlink()
        else: raise Exception("collection status %d" % r.status)
        return result
    def findvalue(self, kid):
        headers = {"X-O2-Target-Key":hexlify(kid)}
        r = self.request("GET","/findvalue",None,headers)
        if r.status == 200:
            res = []
            dom = xml.dom.minidom.parseString(r.read())
            nodes = dom.getElementsByTagName("nodes")
            keys = dom.getElementsByTagName("keys")
            if len(nodes):
                for n in nodes[0].getElementsByTagName("node"):
                    node = Node()
                    node.from_xml(n)
                    if node.ip and node.port and node.id:
                        res.append(node)
            elif len(keys):
                for k in keys[0].getElementsByTagName("key"):
                    key = o2on_key.Key()
                    key.from_xml_node(k)
                    res.append(key)
            return res
        elif r.status == 404: pass
        else: raise Exception("findvalue status %d" % r.status)
class NodeDB:
    def __init__(self, glob):
        self.glob = glob
        self.KBuckets = []
        self.port0nodes = []
        self.lock = threading.Lock()
        for x in range(0,160): self.KBuckets.append([])
        self.nodes = dict()
        self.boardmap = dict()
        self.load()
        if len(self.nodes) == 0: self.node_from_web()
    def __len__(self):
        with self.lock:
            return len(self.nodes)
    def __getitem__(self,x):
        if x == self.glob.prof.mynode.id:
            return self.glob.prof.mynode
        with self.lock:
            return self.nodes.get(x)
    def show(self):
        self.glob.logger.begin()
        with self.lock:
            if len(self.nodes) == 0:
                self.glob.logger.log("NODEDB", "No nodes")
                self.glob.logger.end()
                return
            for l in range(0,160):
                for x in self.KBuckets[l]:
                    node = self.nodes[x]
                    if node.name: name = node.name.decode('utf-8')
                    else: name = "名無しさん".decode('utf-8')
                    s = "%3d %-8s\t%s %s:%d\t%24s\t%s" % (l+1, name,
                                                          node.flag,
                                                          ip2e(node.ip), node.port,
                                                          node.ua, hexlify(node.id))
                    s = s.encode('utf-8')
                    self.glob.logger.log("NODEDB", s)
        self.glob.logger.end()
    def node_list(self):
        with self.lock:
            res = []
            if len(self.nodes) == 0: return res
            for l in range(0,160):
                for x in self.KBuckets[l]:
                    node = self.nodes[x]
                    if node.name: name = node.name.decode('utf-8')
                    else: name = "名無しさん".decode('utf-8')
                    x =  (l+1, hexlify(node.id), ip2e(node.ip), node.port,name,
                          node.flag, ip2e(node.ip), node.port,
                          node.ua, hexlify(node.id))
                    res.append(x)
            return res
    def exportnode(self):
        with self.lock:
            f = open('exportnodes.xml','w')
            f.write("<nodes>\n")
            for x in self.nodes:
                n = self.nodes[x]
                f.write("<str>%s%s%s</str>\n" % (hexlify(n.id), ip2e(n.ip), port2e(n.port)))
            f.write("</nodes>\n")
            f.close()
    def get_nodes_for_board(self, board):
        with self.lock:
            if not board in self.boardmap: return []
            if len(self.boardmap[board])==0: 
                del self.boardmap[board]
                return []
            return map(lambda x: self.nodes[x], self.boardmap[board])
    def get_random_board(self):
        with self.lock:
            if len(self.boardmap) == 0: return None
            return random.choice(self.boardmap.keys())
    def reset_collection_for_node(self,n):
        with self.lock:
            zeroboard = []
            for x in self.boardmap:
                if n.id in self.boardmap[x]:
                    self.boardmap[x].remove(n.id)
                    if len(self.boardmap[x]) == 0: 
                        zeroboard.append(x)
            for z in zeroboard: del self.boardmap[z]
    def add_collection(self, board, n):
        if board not in self.glob.allboards: return
        r = None
        with self.lock:
            if not board in self.boardmap:
                self.boardmap[board] = [n.id]
            elif len(self.boardmap[board])<10:
                self.boardmap[board].append(n.id)
            else:
                nt = self.nodes.get(self.boardmap[board][0])
                if not nt:
                    raise Exception
                    del self.boardmap[board][0]
                    self.boardmap[board].append(n.id)
                else:
                    try:
                        r = nt.ping()
                    except (NodeRemovable, NodeRefused):
                        r = False
                    if r:
                        self.boardmap[board].append(self.boardmap[board][0])
                        del self.boardmap[board][0]
                    else:
                        del self.boardmap[board][0]
                        self.boardmap[board].append(n.id)
        if n.id not in self.nodes: self.add_node(n)
        if r: self.add_node(nt)
    def choice(self):
        with self.lock:
            return self.nodes[random.choice(self.nodes.keys())]
    def remove(self, x):
        if x.port == 0:
            with self.lock:
                if x in self.port0nodes:
                    self.port0nodes.remove(x)
            return
        with self.lock:
            if x.id in self.nodes:
                del self.nodes[x.id]
                self.KBuckets[hash_xor_bitlength(
                        self.glob.prof.mynode.id, x.id)-1].remove(x.id)
                zeroboard = []
                for b in self.boardmap.keys():
                    if x.id in self.boardmap[b]:
                        self.boardmap[b].remove(x.id)
                        if len(self.boardmap[b]) == 0:
                            zeroboard.append(b)
                for b in zeroboard: self.boardmap[b]
    def save(self):
        with self.lock:
            pkl_file = open(o2on_const.NodeDBFile, 'wb')
            cPickle.dump(self.nodes, pkl_file,-1)
            cPickle.dump(self.boardmap, pkl_file,-1)
            cPickle.dump(self.port0nodes, pkl_file,-1)
            pkl_file.close()
    def load(self):
        if(os.path.isfile(o2on_const.NodeDBFile)):
            pkl_file = open(o2on_const.NodeDBFile, 'rb')
            try:
                tmp = cPickle.load(pkl_file)
            except EOFError:
                tmp = dict()
            with self.lock:
                try: self.boardmap = cPickle.load(pkl_file)
                except EOFError: tmp2 = dict()
            with self.lock:
                try: self.port0nodes = cPickle.load(pkl_file)
                except: self.port0nodes = []
            pkl_file.close()
            for x in tmp: self.add_node(tmp[x])
    def add_node(self,node):
        if node.port == 0:
            self.glob.logger.log("NODEDB", "Added port0 node %s" % hexlify(node.id))
            with self.lock:
                if node in self.port0nodes:
                    self.port0nodes.remove(node)
                self.port0nodes.append(node)
            return
        bitlen = hash_xor_bitlength(self.glob.prof.mynode.id, node.id)-1
        if(bitlen<0): return
        with self.lock:
            if node.id in self.KBuckets[bitlen]:
                n = self.nodes[node.id]
                n.from_node(node)
                self.KBuckets[bitlen].remove(node.id)
                self.KBuckets[bitlen].append(node.id)
            elif len(self.KBuckets[bitlen]) < max(20, bitlen/2):
                self.KBuckets[bitlen].append(node.id)
                self.nodes[node.id] = node
            else:
                n = self.nodes[self.KBuckets[bitlen][0]]        
                try:
                    r = n.ping()
                except (NodeRemovable, NodeRefused):
                    r = False
                if r:
                    del self.KBuckets[bitlen][0]
                    self.KBuckets[bitlen].append(n.id)
                else:
                    del self.nodes[n.id]
                    del self.KBuckets[bitlen][0]
                    zeroboard = []
                    for b in self.boardmap.keys():
                        if n.id in self.boardmap[b]:
                            self.boardmap[b].remove(n.id)
                            if len(self.boardmap[b]) == 0:
                                zeroboard.append(b)
                    for b in zeroboard: del self.boardmap[b]
                    self.KBuckets[bitlen].append(node.id)
                    self.nodes[node.id] = node
    def neighbors_nodes(self,target, includeself, cnt=3):
        myid = self.glob.prof.mynode.id
        d = hash_xor(target, myid)
        bitlen = hash_xor_bitlength(target, myid) - 1
        result = []
        if(bitlen>=0):
            with self.lock:
                if(len(self.KBuckets[bitlen])):
                    for x in sorted(map(lambda x: hash_xor(x,target),
                                        self.KBuckets[bitlen])):
                        result.append(self.nodes[hash_xor(x,target)])
                        if(len(result)>=cnt): return result
                for l in range(bitlen-1,-1,-1):
                    if(hash_bittest(d,l) and len(self.KBuckets[l])):
                        for x in sorted(map(lambda x: hash_xor(x,target),
                                            self.KBuckets[l])):
                            result.append(self.nodes[hash_xor(x,target)])
                            if(len(result)>=cnt): return result
        if(includeself and self.glob.prof.mynode.ip and self.glob.prof.mynode.port != 0):
            result.append(self.glob.prof.mynode)
            if(len(result)>=cnt): return result
        if(bitlen>=0):
            for l in range(0,bitlen):
                with self.lock:
                    if(not hash_bittest(d,l) and len(self.KBuckets[l])):
                        for x in sorted(map(lambda x: hash_xor(x,target),
                                            self.KBuckets[l])):
                            result.append(self.nodes[hash_xor(x,target)])
                            if(len(result)>=cnt): return result
        for l in range(bitlen+1,160):
            with self.lock:
                if(len(self.KBuckets[l])):
                    for x in sorted(map(lambda x: hash_xor(x,target),
                                        self.KBuckets[l])):
                        result.append(self.nodes[hash_xor(x,target)])
                        if(len(result)>=cnt): return result
        return result
    def node_from_web(self):
        nodeurls = o2on_config.Node_List_URLs
        regNode = re.compile("([0-9a-f]{40})([0-9a-f]{8})([0-9a-f]{4})")
        for url in nodeurls:
            xmldata = urllib2.urlopen(url)
            dom = xml.dom.minidom.parseString(xmldata.read())
            n = dom.getElementsByTagName("nodes")
            if len(n):
                for y in n[0].getElementsByTagName("str"):
                    if len(y.childNodes) == 1:
                        m = regNode.search(y.childNodes[0].data)
                        self.add_node(Node(unhexlify(m.group(1)),
                                           e2ip(m.group(2)), 
                                           e2port(m.group(3))))
            dom.unlink()
