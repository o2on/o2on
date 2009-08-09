#!/usr/bin/python
# -*- coding: utf-8

import BaseHTTPServer
import re
import socket
from urlparse import urlparse
import httplib
import os.path
import gzip
import base64
import zlib
import StringIO
from binascii import hexlify, unhexlify
import xml.dom.minidom
import urllib
import cgi
import time
import traceback
import sys
from xml.parsers.expat import ExpatError
import threading
import select
from errno import ECONNRESET, EPIPE, ETIMEDOUT

import o2on_config
from o2on_const import regHosts, ProtocolVer, AppName
import o2on_node
import o2on_dat
from o2on_node import ip2e, port2e, e2ip
import o2on_key
import o2on_im
import o2on_util

class O2ONServer(BaseHTTPServer.HTTPServer):
    def __init__(self, handler, port, g):
        BaseHTTPServer.HTTPServer.__init__(self,
                                           ('', port), 
                                           handler)
        self.glob = g
        self.requests = []
        self.__is_shut_down = threading.Event()
        self.__serving = False
    def serve_forever(self, poll_interval=0.5):
        #hasattr(BaseHTTPServer.HTTPServer, '_handle_request_noblock'):
        if sys.hexversion >= 0x020600f0:
            BaseHTTPServer.HTTPServer.serve_forever(self, poll_interval) # 2.6
        else:
            self._serve_forever(poll_interval) # 2.5
    def _serve_forever(self, poll_interval=0.5):
        """Handle one request at a time until shutdown.

        Polls for shutdown every poll_interval seconds. Ignores
        self.timeout. If you need to do periodic tasks, do them in
        another thread.
        """
        self.__serving = True
        self.__is_shut_down.clear()
        while self.__serving:
            # XXX: Consider using another file descriptor or
            # connecting to the socket to wake this up instead of
            # polling. Polling reduces our responsiveness to a
            # shutdown request and wastes cpu at all other times.
            r, w, e = select.select([self], [], [], poll_interval)
            if r:
                self.handle_request()
        self.__is_shut_down.set()
    def shutdown(self):
        for r in self.requests: 
            try:
                r.shutdown(socket.SHUT_RDWR)
                r.close()
            except Exception:
                pass
        if hasattr(BaseHTTPServer.HTTPServer, 'shutdown'):
            BaseHTTPServer.HTTPServer.shutdown(self)
        else:
            self.__serving = False
            self.__is_shut_down.wait()
    def finish_request(self, request, client_address):
        self.requests.append(request)
        try:
            BaseHTTPServer.HTTPServer.finish_request(self, request, client_address)
        except socket.timeout:
            pass
        except Exception,inst:
            errno = None
            if isinstance(inst, socket.error):
                if hasattr(inst, 'errno'): errno = inst.errno  # 2.6
                else: errno =  inst[0]  # 2.5
            if  errno in (ECONNRESET, EPIPE, ETIMEDOUT):
                pass
            else:
                if o2on_config.OutputErrorFile:
                    f = open('error-'+str(int(time.time()))+'.txt', 'w')
                    f.write(str(inst)+"\n")
                    traceback.print_exc(file=f)
                    f.close()
                self.glob.logger.popup("ERROR", str(inst))
                self.glob.shutdown.set()
        self.requests.remove(request)

class ProxyServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    URLTYPE_NORMAL   = 0
    URLTYPE_DAT      = 1
    URLTYPE_KAKO_DAT = 2
    URLTYPE_KAKO_GZ  = 3
    URLTYPE_OFFLAW   = 4
    URLTYPE_MACHI    = 5
    URLTYPE_UNKNOWN  = 6
    regs = (re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/test/read.cgi/[^/]+/\d+/$'),
            re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/([^/]+)/dat/(\d+)\.dat$'),
            re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/([^/]+)/kako/\d+/\d+/(\d+)\.dat$'),
            re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/([^/]+)/kako/\d+/\d+/(\d+)\.dat\.gz$'),
            re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/test/offlaw.cgi/[^/]+/\d+/\?raw='),
            re.compile(
            r'^http://[^.]+\.'+regHosts+r'(?::\d+)?/[^/]+/read.pl\?BBS=[^&]+&KEY=\d+'),)
    def __init__(self,request, client_address, server):
        BaseHTTPServer.BaseHTTPRequestHandler.__init__(self,request, client_address, server)
    def urltype(self):
        x = 0
        for r in self.regs:
            if r.match(self.path): return x
            x += 1
        return x
    def get_requested_header(self):
        h = self.headers.headers
        hr = {}
        for x in h:
            f = x.split(": ",1)
            hr[f[0]] = f[1][:-2]
        return hr
    def get_connection(self, remove=[]):
        h = self.headers.headers
        hr = {}
        for x in h:
            f = x.split(": ",1)
            if f[0] != "Proxy-Connection":
                hr[f[0]] = f[1][:-2]
        hr["Connection"] = "close"
        p = urlparse(self.path)
        if "@" in p.netloc:
            x = p.netloc.split("@",1)
            loc = x[1]
            hr["Authorization"] = "Basic "+base64.b64encode(x[0])
        else:
            loc = p.netloc
        for r in remove:
            if r in hr: del hr[r]
        conn = httplib.HTTPConnection(loc)
        conn.request("GET",p.path, None, hr)
        return conn
    def msg(self,r):
        res = ''
        for x in r.getheaders():
            if x[0] in ("transfer-encoding",):
                pass
            else: res+=x[0]+': '+x[1]+'\r\n'
        return res
    def normal_proxy(self):
        try:
            conn = self.get_connection()
            r= conn.getresponse()
            conn.close()
        except socket.timeout:
            return
        self.wfile.write("HTTP/%d.%d %d %s\r\n" % 
                         (r.version/10,r.version%10,r.status,r.reason))
        self.wfile.write(self.msg(r))
        self.wfile.write("\r\n")
        self.wfile.write(r.read())
        self.wfile.close()
    def datpath(self):
        m = self.regs[self.URLTYPE_DAT].match(self.path)
        if not m: m = self.regs[self.URLTYPE_KAKO_DAT].match(self.path)
        if not m: m = self.regs[self.URLTYPE_KAKO_GZ].match(self.path)
        if not m: return None
        return os.path.join(o2on_config.DatDir, m.group(1), m.group(2), 
                            m.group(3)[:4],m.group(3)+".dat")
    def datkey(self):
        m = self.regs[self.URLTYPE_DAT].match(self.path)
        if not m: m = self.regs[self.URLTYPE_KAKO_DAT].match(self.path)
        if not m: m = self.regs[self.URLTYPE_KAKO_GZ].match(self.path)
        if not m: return None
        return "/".join((m.group(1), m.group(2), m.group(3)))
    def readcgi_url(self):
        
        return None
    def dattitle(self,data):
        first = data.split("\n",1)[0]
        m = re.compile(r'^.*<>.*<>.*<>.*<>(.*)$').match(first)
        if not m: return ""
        try:
            first = first.replace("\x86\xa6", "\x81E").decode('cp932').encode('utf-8')
        except UnicodeDecodeError, inst:
            try:
                first = first.decode('euc_jp').encode('utf-8')
            except UnicodeDecodeError, inst: raise inst
        m = re.compile(r'^.*<>.*<>.*<>.*<>(.*)$').match(first)
        if not m: return ""
        return m.group(1)
    def do_GET(self):
        logger = self.server.glob.logger
        logger.log("PROXY", "proxy requested %s" % self.path)
        ut = self.urltype()
        if ut in (self.URLTYPE_UNKNOWN, self.URLTYPE_NORMAL, self.URLTYPE_MACHI):
            self.normal_proxy()
            return
        try:
            conn = self.get_connection()
            r= conn.getresponse()
            conn.close()
        except socket.timeout:
            r = None
        data = None
        header = None
        if r:
            logger.log("PROXY", "\tresponse %s" % r.status)
            if ut != self.URLTYPE_OFFLAW and r.status in (200,206,304):
                logger.log("PROXY", "\tgot response from server")
                data = r.read()
                if r.getheader("content-encoding") == "gzip":
                    sf = StringIO.StringIO(data)
                    dec = gzip.GzipFile(fileobj=sf)
                    datdata = dec.read()
                else:
                    datdata = data
                self.wfile.write("HTTP/%d.%d %d %s\r\n" % 
                                 (r.version/10,r.version%10,r.status,r.reason))
                self.wfile.write(self.msg(r))
                self.wfile.write("\r\n")
                self.wfile.write(data)
                self.wfile.close()
                dk = self.datkey()
                dkh = o2on_util.datkeyhash(dk)
                dp = self.datpath()
                if r.status == 200:
                    if not self.server.glob.datdb.has_keyhash(dkh):
                        # 持ってない dat が取得された
                        logger.log("PROXY", "\tsave responsed dat for myself")
                        self.server.glob.datdb.add(dk, datdata)
                else:
                    if self.server.glob.datdb.has_keyhash(dkh):
                        if r.status == 206:
                            # 持ってる dat の差分
                            rg = r.getheader('Content-Range')
                            start = 0
                            if rg:
                                m=re.compile(r'bytes (\d+)-').search(rg)
                                start = int(m.group(1))
                                logger.log("PROXY", "\tsave diff dat for myself (%d-)" % start)
                                self.server.glob.datdb.add(dk, datdata, start)
                    elif o2on_config.RequestNonExistDat:
                        # 持ってない dat がリクエストされた -> こっそり取得
                        logger.log("PROXY", "\trequest whole dat for myself :-)")
                        try:
                            conn = self.get_connection(['If-Modified-Since', 'Range'])
                            r2= conn.getresponse()
                            conn.close()
                        except socket.timeout:
                            r2 = None
                        if r2 and r2.status == 200:
                            data = r2.read()
                            if r.getheader("content-encoding") == "gzip":
                                sf = StringIO.StringIO(data)
                                dec = gzip.GzipFile(fileobj=sf)
                                data = dec.read()
                            self.server.glob.datdb.add(dk, data)
            elif ut in (self.URLTYPE_DAT, self.URLTYPE_KAKO_DAT, self.URLTYPE_KAKO_GZ):
                logger.log("PROXY",  "\ttry to read dat from cache")
                dp = self.datpath()
                wdata = None
                if ut == self.URLTYPE_KAKO_GZ:
                    if os.path.exists(dp):
                        f=open(dp)
                        wdata=zlib.compress(f.read())
                    elif os.path.exists(dp+".gz"):  
                        f=open(dp+".gz",'r')
                        wdata=f.read()
                    else: f= None
                    if f: f.close()
                else:
                    if os.path.exists(dp):
                        f=open(dp)
                        wdata=f.read()
                    elif os.path.exists(dp+".gz"):
                        f=gzip.GzipFile(dp+".gz",'r')
                        wdata=f.read()
                    else: f= None
                    if f: f.close()
                if wdata: # FIXME range, gzip
                    logger.log("PROXY", "\tfound cached dat")
                    #reqheader = self.get_requested_header()
                    self.wfile.write("HTTP/1.0 200 OK\r\n")
                    self.wfile.write("Content-Type: text/plain\r\n")
                    self.wfile.write("\r\n")
                    self.wfile.write(wdata)
                    self.wfile.close()
                    f.close()
                    # gzip で書き直す
                    if os.path.exists(dp) and o2on_config.DatSaveAsGzip:
                        f = open(dp)
                        g = gzip.GzipFile(dp+".gz",'w')
                        g.write(f.read())
                        g.close()
                        f.close()
                        os.remove(dp)
                else:
                    logger.popup("PROXY", "no cached dat. query for the dat.\n%s" % \
                                     self.datkey())
                    data = r.read()
                    self.wfile.write("HTTP/%d.%d %d %s\r\n" % 
                                     (r.version/10,r.version%10,r.status,r.reason))
                    self.wfile.write(self.msg(r))
                    self.wfile.write("\r\n")
                    self.wfile.write(data)
                    self.wfile.close()
                    try:
                        conn = self.get_connection(['If-Modified-Since', 'Range', 
                                                    'User-Agent'])
                        r2= conn.getresponse()
                        conn.close()
                    except socket.timeout:
                        r2 = None
                    if r2 and r2.status == 203:
                        data = r2.read()
                        if r2.getheader("content-encoding") == "gzip":
                            sf = StringIO.StringIO(data)
                            dec = gzip.GzipFile(fileobj=sf)
                            data = dec.read()
                        title = self.dattitle(data)
                    else: title = ""
                    self.server.glob.datquery.add_bydatkey(self.datkey(),
                                                           None, title, True)
                    self.server.glob.datquery.save()

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
                     'Server': prof.mynode.ua,
                     'X-O2-RSA-Public-Key': hexlify(prof.mynode.pubkey)}

class P2PServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    regPath = re.compile(r'^(?:http://[^/]+)?/([^/]+)')
    def do_dat(self, node):
        logger = self.server.glob.logger
        if self.command == 'POST' and self.headers.getheader('Content-Length'):
            l = int(self.headers.getheader('Content-Length'))
            logger.log("P2PSERVER", 
                       "Client gave me omiyage dat %s" % hexlify(node.id))
            data = self.rfile.read(l)
            daturl = self.headers.get('X-O2-Original-DAT-URL')
            dat = o2on_dat.Dat()
            if daturl:
                if ".." in daturl: return self.response_400("DAT URL include ..")
                if not dat.setpath(daturl): return self.response_400("invalid dat url")
            else:
                datpath = self.headers.get('X-O2-DAT-Path')
                if datpath:
                    if ".." in datpath: return self.response_400("datpath include ..")
                    if not dat.setpath(datpath): return self.response_400("invalid datpath")
            if not dat.save(data): 
                logger.log("P2PSERVER",
                           "I don't like this omiyage dat %s" % self.client_address[0])
                return self.response_400("invalid omiyage")
            else:
                self.server.glob.datdb.add_dat(dat)
        # give dat
        targetkey = self.headers.get('X-O2-Target-Key')
        targetboard = self.headers.get('X-O2-Target-Board')
        if targetkey:
            dat = self.server.glob.datdb.get(targetkey)
        elif targetboard:
            dat = self.server.glob.datdb.getRandomInBoard(targetboard)
        else: dat = self.server.glob.datdb.choice(targetboard)
        if not dat: return self.response_404()
        headers = common_header.copy()
        data = dat.data()
        headers['X-O2-DAT-Path'] = dat.path()
        headers['Content-Type'] = 'text/plain; charset=shift_jis'
        headers['Content-Length'] = str(len(data))
        self.wfile.write("HTTP/1.0 200 OK\r\n")
        for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
        self.wfile.write("\r\n")
        self.wfile.write(data)
        self.wfile.close()
    def do_collection(self, node):
        boards = o2on_config.DatCollectionBoardList
        if boards == None:
            boards = self.server.glob.allboards
        data = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        data += "<boards>\r\n"
        for b in boards:
            data += "<board>%s</board>\r\n" % b
        data += "</boards>\r\n"
        headers = common_header.copy()
        headers['Content-Type'] = 'text/xml; charset=utf-8'
        headers['Content-Length'] = str(len(data))
        self.wfile.write("HTTP/1.0 200 OK\r\n")
        for h in headers: self.wfile.write("%s: %s\r\n" % (h, headers[h]))
        self.wfile.write("\r\n")
        self.wfile.write(data)
        self.wfile.close()
        self.server.glob.logger.log("P2PSERVER", "gave collection %s" % hexlify(node.id))
        if self.command == 'POST':
            l = int(self.headers.getheader('Content-Length'))
            data = self.rfile.read(l)
            # o2on の bug 対策
            if data.rfind("</boards>") == -1:
                index = data.rfind("<boards>")
                data = data[:index] + "</boards>" + data[index+len("<boards>"):]
            if len(data):
                dom = xml.dom.minidom.parseString(data)
                nn = dom.getElementsByTagName("boards")
                result = []
                if len(nn):
                    for b in nn[0].getElementsByTagName("board"):
                        result.append(b.childNodes[0].data)
                dom.unlink()
                self.server.glob.logger.log("P2PSERVER", "got collection %s" % hexlify(node.id))
                self.server.glob.nodedb.reset_collection_for_node(node)
                for b in result: self.server.glob.nodedb.add_collection(b,node)
    def do_ping(self, node):
        headers = common_header.copy()
        headers['Content-Type'] = 'text/plain'
        headers['Content-Length'] = "8"
        self.wfile.write("HTTP/1.0 200 OK\r\n")
        for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
        self.wfile.write("\r\n")
        self.wfile.write(ip2e(node.ip))
        self.wfile.close()
        self.server.glob.logger.log("P2PSERVER", "respond to ping %s" % hexlify(node.id))
    def do_findnode(self, node):
        target = self.headers.get('X-O2-Target-Key')
        if not target: return self.response_400("no target key to findnode")
        target = unhexlify(target)
        neighbors = self.server.glob.nodedb.neighbors_nodes(target, True)
        if neighbors:
            data = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            data += "<nodes>\r\n"
            for node in neighbors: data += node.xml()
            data += "</nodes>\r\n"
            data = data.encode('utf-8')
            headers = common_header.copy()
            headers['Content-Type'] = 'text/xml; charset=utf-8'
            headers['Content-Length'] = str(len(data))
            self.wfile.write("HTTP/1.0 200 OK\r\n")
            for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
            self.wfile.write("\r\n")
            self.wfile.write(data)
            self.wfile.close()
        else: return self.response_404()
    def do_store(self, node):
        headers = common_header.copy()
        self.wfile.write("HTTP/1.0 200 OK\r\n")
        for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
        self.wfile.write("\r\n")
        self.wfile.close()
        if self.headers.getheader('Content-Length'):
            l = int(self.headers.getheader('Content-Length'))
            category = self.headers.get('X-O2-Key-Category')
            if not category: category = 'dat'
            if category == 'dat':
                data = self.rfile.read(l)
                if data:
                    try:
                        dom = xml.dom.minidom.parseString(data)
                    except ExpatError:
                        return
                    top = dom.getElementsByTagName("keys")
                    if len(top):
                        for k in top[0].getElementsByTagName("key"):
                            key = o2on_key.Key()
                            key.from_xml_node(k)
                            self.server.glob.keydb.add(key)
                    dom.unlink()
            else: self.server.glob.logger.log("P2PSERVER","Unknown Category %s" % category)
    def do_im(self, node):
        headers = common_header.copy()
        self.wfile.write("HTTP/1.0 200 OK\r\n")
        for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
        self.wfile.write("\r\n")
        self.wfile.close()
        if self.headers.getheader('Content-Length'):
            l = int(self.headers.getheader('Content-Length'))
            data = self.rfile.read(l)
            dom = xml.dom.minidom.parseString(data)
            top = dom.getElementsByTagName("messages")
            if len(top):
                for n in top[0].getElementsByTagName("message"):
                    im = o2on_im.IMessage()
                    im.from_xml_node(n)
                    im.date = int(time.time())
                    self.server.glob.imdb.add(im)
                self.server.glob.logger.popup("IM", "Received Message!")
                self.server.glob.imdb.save()
            dom.unlink()
    def do_findvalue(self,node):
        target = self.headers.get('X-O2-Target-Key')
        if not target: return self.response_400("no target key to findvalue")
        self.server.glob.logger.log("P2PSERVER",
                                    "\tfindvalue from %s for %s" % (hexlify(node.id), target))
        target = unhexlify(target)
        xml_data = None
        keys = self.server.glob.keydb.get_bydatkey(target)
        if keys:
            xml_data = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
            xml_data += "<keys>\r\n"
            for key in keys: xml_data += key.xml()
            xml_data += "</keys>\r\n"
        else:
            neighbors = self.server.glob.nodedb.neighbors_nodes(target, True)
            if len(neighbors)>0:
                xml_data = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
                xml_data += "<nodes>\r\n"
                for node in neighbors:
                    if node.ip and node.port:
                        xml_data += "<node>\r\n"
                        xml_data += "<id>%s</id>\r\n" % hexlify(node.id)
                        xml_data += "<ip>%s</ip>\r\n" % ip2e(node.ip)
                        xml_data += "<port>%s</port>\r\n" % node.port
                        if node.name:
                            xml_data += "<name><![CDATA[%s]]></name>\r\n" % \
                                node.name.decode('utf-8')
                        if node.pubkey:
                            xml_data += "<pubkey>%s</pubkey>\r\n" % hexlify(node.pubkey)
                        xml_data += "</node>\r\n"
                xml_data += "</nodes>\r\n"
        if xml_data:
            xml_data = xml_data.encode('utf-8')
            headers = common_header.copy()
            headers['Content-Type'] = 'text/xml; charset=utf-8'
            headers['Content-Length'] = str(len(xml_data))
            self.wfile.write("HTTP/1.0 200 OK\r\n")
            for h in headers: self.wfile.write("%s: %s\r\n" % (h,headers[h]))
            self.wfile.write("\r\n")
            self.wfile.write(xml_data)
            self.wfile.close()
        else: return self.response_404()
    job = {'dat':do_dat,
           'collection': do_collection,
           'ping': do_ping,
           'findnode':do_findnode,
           'store':do_store,
           'findvalue':do_findvalue,
           'im':do_im,}
    def response_400(self, reason=""):
        logger = self.server.glob.logger
        logger.log("P2PSERVER",
                   "response 400 %s (%s)" % (self.client_address[0], reason))
        logger.log("P2PSERVER", "\tpath was %s" % self.path)
        logger.log("P2PSERVER", "\theader was %s" % self.headers)
        header = common_header.copy()
        self.wfile.write("HTTP/1.0 400 Bad Request\r\n")
        for h in header: self.wfile.write("%s: %s\r\n" % (h,header[h]))
        self.wfile.write("\r\n")
        self.wfile.close()
    def response_404(self):
        #print "p2p server response 404 %s" % self.client_address[0]
        header = common_header.copy()
        self.wfile.write("HTTP/1.0 404 Not Found\r\n")
        for h in header: self.wfile.write("%s: %s\r\n" % (h,header[h]))
        self.wfile.write("\r\n")
        self.wfile.close()
    def get_requested_header(self):
        h = self.headers.headers
        hr = {}
        for x in h:
            f = x.split(": ",1)
            hr[f[0]] = f[1][:-2]
        return hr
    def do_POST(self):
        self.do_GET()
    def do_GET(self):
        self.server.glob.logger.log("P2PSERVER", "connection came %s" % (self.path))

        nid = self.headers.getheader('X-O2-Node-ID')
        if not nid: return self.response_400("No NodeID")
        port = self.headers.getheader('X-O2-Port')
        if not port: return self.response_400("No Port")
        port = int(port)
        node = o2on_node.Node(unhexlify(nid), self.client_address[0], port)

        if not self.headers.getheader('X-O2-RSA-Public-Key'):
            return self.response_400("No public key")
        node.pubkey = unhexlify(self.headers.getheader('X-O2-RSA-Public-Key'))

        name = self.headers.getheader('X-O2-Node-Name')
        if name: node.name = name.decode('utf-8').encode('utf-8')
        flag = self.headers.getheader('X-O2-Node-Flags')
        if flag: node.setflag(flag)

        ua = self.headers.getheader('User-Agent')
        if not ua: return self.response_400("No UA")
        if len(ua)<6: return self.response_403()
        m = re.compile(r'O2/(\d+(?:\.\d+)?)').match(ua)
        if not m: return self.response_403()
        if float(m.group(1)) < ProtocolVer: return self.response_403()
        node.ua = ua

        self.server.glob.nodedb.add_node(node)
        m = self.regPath.match(self.path)
        if m:
            func = self.job.get(m.group(1))
            if func: return func(self, node)
        return self.response_404()


class AdminServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    default = "status"
    regPath = re.compile(r'^(?:http://[^/]+)?/([^/]+)(?:/?(.*?)/?)?$')
    html_header = """\
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" 
 "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="ja" lang="ja" dir="ltr">
  <head>
   <title>%s - Admin %s</title>
   <style type="text/css">
<!--
#navlist
{
padding: 3px 0;
margin-left: 0;
border-bottom: 1px solid #778;
font: bold 12px Verdana, sans-serif;
}

#navlist li
{
list-style: none;
margin: 0;
display: inline;
}

#navlist li a
{
padding: 3px 0.5em;
margin-left: 3px;
border: 1px solid #778;
border-bottom: none;
background: #DDE;
text-decoration: none;
}

#navlist li a:link { color: #448; }
#navlist li a:visited { color: #667; }

#navlist li a:hover
{
color: #000;
background: #AAE;
border-color: #227;
}

#navlist li a#current
{
background: white;
border-bottom: 1px solid white;
}

#mine
{
background: blue;
}
-->
   </style>
  </head>
<body>
"""
    html_footer = """\
</body>
</html>
"""
    pages = (("status", "状態"),
             ("nodes", "ノード"),
             ("keys", "datキー"),
             ("dats", "所有dat"),
             ("datq", "dat検索"),
             ("im", "IM"),
             ("shutdown", "シャットダウン"),)
    def send_nav(self, cur):
        self.wfile.write("<ul id=\"navlist\">\n")
        for x in self.pages:
            if x[0] == cur:
                self.wfile.write(
                    "<li id=\"active\"><a href=\"/%s\" id=\"current\">%s</a></li>\n" % x)
            else:
                self.wfile.write("<li><a href=\"/%s\">%s</a></li>\n" % x)
        self.wfile.write("</ul>\n")
    def send_common(self, cur, curname):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Connection', 'close')        
        self.end_headers()
        self.wfile.write(self.html_header % (AppName, curname))
        self.send_nav(cur)
    def datq(self, args):
        datq = self.server.glob.datquery
        self.send_common("datq", "Searching Dats")
        self.wfile.write("""\
<div class='section'>
 <h2 class='section_title'>検索中dat</h2>
 <div class='section_body'>
  <p>検索中dat数 %d</p>
  <table>
   <tr><th>URL</th><th>Title</th><th>最終検索日時</th></tr>
""" % (len(datq)))
        for x in datq.datq_list():
            self.wfile.write(
                ("<tr><td><a href='%s'>%s</a></td><td>%s</td><td>%s</td></tr>\n" \
                     % (x[0],x[0],x[1],x[2])).encode('utf-8'))
        self.wfile.write("""\
  </table>
 </div>
</div>
""")
        self.wfile.write(self.html_footer)
    def im_send(self,args):
        if not re.compile(r'^[0-9a-f]{40}$').match(args[1]) or \
                not re.compile(r'^[0-9a-f]{8}$').match(args[2]) or \
                not re.compile(r'^\d+$').match(args[3]) or \
                not self.server.glob.prof.mynode.ip: 
            self.send_common("im", "Instant Messenger Send")
            self.wfile.write("""\
<div class='section'>
 <h2 class='section_title'>IM送信エラー</h2>
 <div class='section_body'>
  <p>グローバルIPが確定していないか、送信先がおかしいです。</p>
 </div>
</div>
""")
            self.wfile.write(self.html_footer)
            return
        nodedb = self.server.glob.nodedb
        nid = unhexlify(args[1])
        ip = e2ip(args[2])
        port = int(args[3])
        node = nodedb[nid] or o2on_node.Node(nid,ip,port)
        if node.name: name = "%s (ID: %s)" % (node.name.decode('utf-8'), args[1])
        else: name = "ID: %s" % args[1]
        l = self.headers.get('Content-Length')

        if self.command == "GET" or not l:
            self.send_common("im", "Instant Messenger Send")
            self.wfile.write(("""\
<div class='section'>
 <h2 class='section_title'>IM送信</h2>
 <div class='section_body'>
  <p>%sにIMを送信。</p>
  <form action='/im/send/%s/%s/%s' method='POST'>
   <input type='text' name='immsg' size='50' maxlength='256'/><br />
   <input type='submit' id='imsend' value='送信' />
  </form>
 </div>
</div>
""".decode('utf-8') % (name, args[1], args[2], args[3])).encode('utf-8'))
            self.wfile.write(self.html_footer)
        else:
            l = int(l)
            data = self.rfile.read(l)
            m=re.compile(r'^immsg=(.*)$').match(data)
            data = urllib.unquote_plus(m.group(1)).decode('utf-8')

            result = "失敗"
            try:
                node.im(self.server.glob.prof.mynode, data)
            except o2on_node.NodeRemovable:
                nodedb.remove(node)
                nodedb.save()
                self.server.glob.keydb.remove_bynodeid(node.id)
                self.server.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            else:
                result = "成功"
                nodedb.add_node(node)
                nodedb.save()
                im = o2on_im.IMessage()
                im.from_node(self.server.glob.prof.mynode)
                im.msg = data.encode('utf-8')
                im.date = int(time.time())
                im.mine = True
                self.server.glob.imdb.add(im)
                self.server.glob.imdb.save()
            self.send_common("im", "Instant Messenger Sent")
            self.wfile.write(("""\
<div class='section'>
 <h2 class='section_title'>IMを送信しました。</h2>
 <div class='section_body'>
  <p>%sに以下のIMを送信し、%sしました。</p>
  <p>%s</p>
  <p><a href='/im'>もどる</a></p>
 </div>
</div>
""".decode('utf-8') % (name,result.decode('utf-8'),data)).encode('utf-8'))
            self.wfile.write(self.html_footer)
    def im(self,args):
        if len(args)==4 and args[0] == "send": return self.im_send(args)
        imdb = self.server.glob.imdb
        self.send_common("im", "Instant Messenger")
        self.wfile.write("""\
<div class='section'>
 <h2 class='section_title'>IM</h2>
 <div class='section_body'>
  <table>
   <tr><th>日時</th><th>名前</th><th>メッセージ</th></tr>
""")
        for x in imdb.im_list():
            if x[0]:
                self.wfile.write(("<tr class=\"mine\"><td>%s</td><td>"\
                                      "<a href='/im/send/%s/%s/%d'>%s</a></td>\n"\
                                      "<td>%s</td></tr>" % x[1:]).encode('utf-8'))
            else:
                self.wfile.write(("<tr class=\"other\"><td>%s</td><td>"\
                                      "<a href='/im/send/%s/%s/%d'>%s</a></td>\n"\
                                      "<td>%s</td></tr>" % x[1:]).encode('utf-8'))
        self.wfile.write("""\
  </table>
 </div>
</div>
""")
        self.wfile.write(self.html_footer)
    def keys(self,args):
        keydb = self.server.glob.keydb
        self.send_common("keys", "Key")
        self.wfile.write("""\
<div class='section'>
 <h2 class='section_title'>キー情報</h2>
 <div class='section_body'>
  <p>キー数 %d</p>
  <table>
   <tr>
    <th>d</th><th>IP</th><th>Port</th><th>URL</th><th>title</th><th>note</th><th>date</th>
    <th>size</th><th>hash</th>
   </tr>
""" % (len(keydb)))
        for x in keydb.key_list():
            self.wfile.write(("""\
<tr>
 <td>%d</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>
 <td>%d</td><td>%s</td>""" % x).encode('utf-8'))
        self.wfile.write("""\
  </table>
 </div>
</div>
""")
        self.wfile.write(self.html_footer)
    def nodes(self,args):
        nodedb = self.server.glob.nodedb
        self.send_common("nodes", "Nodes")
        self.wfile.write("""\
<div class='section'>
 <h2 class='section_title'>ノード情報</h2>
 <div class='section_body'>
  <p>ノード数 %d</p>
  <table>
   <tr><th>d</th><th>Name</th><th>flg</th><th>IP</th><th>Port</th><th>UA</th><th>ID</th></tr>
""" % (len(nodedb)))
        for x in nodedb.node_list():
            self.wfile.write(
                ("<tr><td>%d</td><td><a href='/im/send/%s/%s/%d'>%s</a></td><td>%s</td>"\
                     "<td>%s</td><td>%d</td><td>%s</td><td>%s</td></tr>\n" % x).encode('utf-8'))
        self.wfile.write("""\
  </table>
 </div>
</div>
""")
        self.wfile.write(self.html_footer)
    def status(self,args):
        self.send_common("status", "Status Summary")
        glob = self.server.glob
        prof = glob.prof
        if prof.mynode.ip:
            ip = "%s (%s)" % (prof.mynode.ip, ip2e(prof.mynode.ip))
        else:
            ip =  "未取得"
        name = prof.mynode.name or "なし"
        if prof.mynode.ip:
            nodehash = hexlify(glob.prof.mynode.id)+ip2e(glob.prof.mynode.ip)+\
                port2e(glob.prof.mynode.port)
        else: nodehash = "IP未取得"
        self.wfile.write("""\
<p class='section'>
 <h2 class='section_title'>自ノード情報</h2>
 <div class='section_body'>
  <table>
   <tr><th>ID</th><th>IP</th><th>ポート</th><th>ノード名</th><th>UA</th><th>ハッシュ</th></tr>
   <tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>
  </table>
 </div>
</p>
""" % (hexlify(prof.mynode.id), ip, prof.mynode.port, name, prof.mynode.ua, nodehash))
        self.wfile.write("""\
<p class='section'>
 <h2 class='section_title'>概要</h2>
 <div class='section_body'>
  <table>
   <tr>
    <th><a href='/nodes'>ノード数</a></th>
    <th><a href='/dats'>dat数</a></th>
    <th><a href='/keys'>datキー数</a></th>
    <th><a href='/datq'>検索中dat数</a></th></tr>
   <tr><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>
  </table>
 </div>
</p>
""" % (len(glob.nodedb), len(glob.datdb), len(glob.keydb), len(glob.datquery)))
        self.wfile.write(self.html_footer)
    def shutdown(self, args):
        if len(args) == 1 and args[0] == "really":
            self.send_common("shutdown", "Shutdown")
            self.wfile.write("""\
<p class='section'>
 <h2 class='section_title'>シャットダウン</h2>
 <div class='section_body'>
  <p>opy2onにシャットダウンコマンドを送信しました。</p>
 </div>
</p>
""")
            self.wfile.write(self.html_footer)
            self.server.glob.shutdown.set()
        else:
            self.send_common("shutdown", "Shutdown")
            self.wfile.write("""\
<p class='section'>
 <h2 class='section_title'>シャットダウン</h2>
 <div class='section_body'>
  <p>opy2onをシャットダウンしますか?</p>
  <p><a href="/shutdown/really">はい</a> / <a href="/">いいえ</a></p>
 </div>
</p>
""")
            self.wfile.write(self.html_footer)
    def do_POST(self):
        self.do_GET()
    def do_GET(self):
        m = self.regPath.match(self.path)
        if m: 
            path = m.group(1)
            if m.group(2) != "": args = m.group(2).split("/")
            else: args = []
        else: 
            path = self.default
            args = []
        if not hasattr(self, path): 
            self.send_error(404)
            return
        method = getattr(self, path)
        method(args)
    # BaseHTTPServer の log を抑制
    def log_message(self, format, *args):
        pass
