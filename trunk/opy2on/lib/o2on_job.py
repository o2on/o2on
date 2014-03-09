#!/usr/bin/python
# -*- coding: utf-8

from binascii import hexlify, unhexlify
import threading
import time
import socket
import BaseHTTPServer
import gzip
import random
import re
import time
import socket
import os
import httplib
import traceback
import sys
import errno

import o2on_server
import o2on_config
import o2on_util
import o2on_node
import o2on_key

if o2on_config.RecordProfile:
    import cProfile

class JobThread(threading.Thread):
    def __init__(self, name, s, g):
        threading.Thread.__init__(self)
        self.finish = False
        self.awake  = False
        self.name = name
        self.glob = g
        self.sec = s
        self.node = None
    def shutdown(self):
        if self.node: self.node.shutdown()
        self.node = None
    def stop(self):
        self.finish = True
    def wakeup(self):
        self.awake = True
    def run(self):
        try:
            if o2on_config.RecordProfile:
                if not os.path.exists(o2on_config.ProfileDir): 
                    try:
                        os.makedirs(o2on_config.ProfileDir)
                    except OSError, inst:
                        if inst.errno != errno.EEXIST: raise inst
                profname = os.path.join(o2on_config.ProfileDir,
                                        "o2on_"+"_".join(self.name.split(" "))+".prof")
                cProfile.runctx('self.dummy()', None, {'self':self,}, profname)
            else: self.dummy()
        except Exception,inst:
            if o2on_config.OutputErrorFile:
                f = open('error-'+str(int(time.time()))+'.txt', 'w')
                traceback.print_exc(file=f)
                f.close()
            self.glob.logger.popup("ERROR", str(inst))
            self.glob.shutdown.set()
    def dummy(self):
        self.glob.logger.log("JOBMANAGER", "job %s started" % self.name)
        while not self.finish:
            #t = time.time()
            self.node = None
            self.dojob(self.glob.nodedb, self.glob.logger, self.glob.prof, self.glob.datdb,
                       self.glob.datquery)
            self.node = None
            if self.finish: break
            diff = int(self.sec) #int(self.sec - (time.time()-t))
            if 0<diff: 
                #self.glob.logger.log("job %s sleep for %d secs" % (self.name,diff))
                self.awake = False
                for x in range(0,diff):
                    if self.finish or self.awake: break
                    time.sleep(1)
        self.glob.logger.log("JOBMANAGER", "job %s finished" % self.name)
    def dojob(self, nodes, logger, prof, datdb):
        pass

class NodeCollectorThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"findnode",180,g)
    def dojob(self, nodes, logger, prof, datdb, datq):
        if len(nodes) == 0:
            if len(o2on_config.Node_List_URLs)>0: 
                logger.popup("NODECOLLECTOR","load node from web")
                nodes.node_from_web()
        target = o2on_util.randomid()
        for x in nodes.neighbors_nodes(target, False):
            if self.finish: break
            logger.log("NODECOLLECTOR", "findnode to %s" % (hexlify(x.id)))
            try:
                self.node = x
                newnodes = x.findnode(target)
                self.node = None
            except o2on_node.NodeRemovable:
                nodes.remove(x)
                nodes.save()
                self.glob.keydb.remove_bynodeid(x.id)                
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                self.glob.logger.log("NODECOLLECTOR", inst)
            else:
                for n in newnodes: 
                    logger.log("NODECOLLECTOR","\tadd node %s" % (hexlify(n.id)))
                    nodes.add_node(n)
                nodes.add_node(x)
                nodes.save()
            time.sleep(1)
        if len(nodes)<30: self.sec = 30
        else: self.sec = 180

class DatCollectorThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"dat collector",60,g)
    def dojob(self, nodes, logger, prof, datdb, datq):
        board = nodes.get_random_board()
        logger.log("DATCOLLECTOR","Try to get dat in %s" % board)
        if not board: return
        for n in nodes.get_nodes_for_board(board):
            if self.finish: break
            logger.log("DATCOLLECTOR","dat (%s) to %s" % (board,hexlify(n.id)))
            try:
                self.node = n
                dat = n.dat(None, board, datdb)
                self.node = None
            except o2on_node.NodeRemovable:
                nodes.remove(n)
                nodes.save()
                self.glob.keydb.remove_bynodeid(n.id)
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                self.glob.logger.log("DATCOLLECTOR", inst)
            else:
                if dat: 
                    logger.log("DATCOLLECTOR","\tGot dat %s" % dat.path())
                    nodes.add_node(n)
                    datdb.add_dat(dat)
                    nodes.save()
                    datdb.save()
                    break
            time.sleep(1)

class GetIPThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"get global IP",60,g)
    def dojob(self, nodes, logger, prof, datdb, datq):
        regLocalIP = re.compile(r'^(?:10\.|192\.168\.|172\.(?:1[6-9]|2[0-9]|3[01])\.)')
        for n in nodes.neighbors_nodes(prof.mynode.id, False, 100):
            if self.finish: break
            logger.log("GETIP","getIP to %s" % hexlify(n.id))
            try:
                self.node = n
                r = n.ping(True)
                self.node = None
            except o2on_node.NodeRemovable:
                nodes.remove(n)
                nodes.save()
                self.glob.keydb.remove_bynodeid(n.id)                
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                self.glob.logger.log("GETIP", inst)
            else:
                if r:
                    ip = o2on_node.e2ip(r[:8])
                    if not regLocalIP.match(ip):
                        if o2on_config.ReCheckIP == None:
                            self.finish = True
                        else:
                            self.sec = o2on_config.ReCheckIP * 60
                        if prof.mynode.ip != ip:
                            logger.popup("GETIP","Got Global IP %s" % ip)
                            prof.mynode.ip = ip
                    nodes.add_node(n)
                    break

class AskNodeCollectionThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"ask node collection",60,g)
    def dojob(self, nodedb, logger, prof, datdb, datq):
        for n in nodedb.neighbors_nodes(o2on_util.randomid(), False):
            if self.finish: break
            logger.log("ASKNODECOLLECTION", "node collection to %s" % (hexlify(n.id)))
            try:
                self.node = n
                colboards = n.collection(self.glob)
                self.node = None
            except o2on_node.NodeRemovable:
                nodedb.remove(n)
                nodedb.save()
                self.glob.keydb.remove_bynodeid(n.id)                
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                logger.log("ASKNODECOLLECTION", inst)
            else:
                logger.log("ASKNODECOLLECTION",
                           "\tadd collection for %s" % (hexlify(n.id)))
                nodedb.reset_collection_for_node(n)
                for b in colboards: nodedb.add_collection(b,n)
                nodedb.add_node(n)
                nodedb.save()

class PublishOrigThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"publish original",60,g)
    def dojob(self, nodedb, logger, prof, datdb, datq):
        mynode = prof.mynode
        if not mynode.ip or mynode.port == 0: return
        if len(nodedb)<10: return
        dats = datdb.dat_to_publish(time.time()-(3*60*60), 500)
        keys = []
        for d in dats:
            if self.finish: return
            k = o2on_key.Key()
            k.from_dat(d)
            k.from_node(prof.mynode)
            keys.append(k)
        publish_nodes = {}
        for k in keys:
            if self.finish: return
            for n in nodedb.neighbors_nodes(k.hash, False):
                if self.finish: return
                if n.id not in publish_nodes: publish_nodes[n.id] = []
                publish_nodes[n.id].append(k)
        for n in publish_nodes:
            if self.finish: return
            node = nodedb[n]
            if not node: continue
            logger.log("PUBLISHORIGINAL","publish original to %s" % (hexlify(n)))
            try:
                self.node = node
                node.store("dat", publish_nodes[n])
                self.node = None
            except o2on_node.NodeRemovable:
                nodedb.remove(node)
                nodedb.save()
                self.glob.keydb.remove_bynodeid(node.id)                
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                logger.log("PUBLISHORIGINAL", inst)
            else:
                nodedb.add_node(node)
                pubtime = int(time.time())
                for k in publish_nodes[n]: 
                    datdb.published(k.hash,pubtime)
                self.glob.keydb.save()
                datdb.save()
                logger.log("PUBLISHORIGINAL","\tpublished original")
                nodedb.save()
            time.sleep(1)

class PublishKeyThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"publish key",120,g)
    def dojob(self, nodedb, logger, prof, datdb, datq):
        mynode = prof.mynode
        if not mynode.ip or mynode.port == 0:
            return
        publish_nodes = {}
        #t = time.time()
        keys = self.glob.keydb.keys_to_publish(int(time.time()-30*60))
        #logger.log("get keys to publush %d" % (time.time()-t))
        for k in keys:
            if self.finish: return
            for n in nodedb.neighbors_nodes(k.hash, False):
                if self.finish: return
                if n.id not in publish_nodes: publish_nodes[n.id] = []
                publish_nodes[n.id].append(k)
        for n in publish_nodes:
            if self.finish: return
            node = nodedb[n]
            if not node: continue
            logger.log("PUBLISHKEY","publish key to %s" % (hexlify(n)))
            try:
                self.node = node
                node.store("dat", publish_nodes[n])
                self.node = None
            except o2on_node.NodeRemovable:
                nodedb.remove(node)
                nodedb.save()
                self.glob.keydb.remove_bynodeid(node.id)                
                self.glob.keydb.save()
            except o2on_node.NodeRefused:
                pass
            except socket.error, inst:
                logger.log("PUBLISHKEY", inst)
            else:
                nodedb.add_node(node)
                pubtime = int(time.time())
                for k in publish_nodes[n]: 
                    self.glob.keydb.published(k.hash,pubtime)
                self.glob.keydb.save()
                logger.log("PUBLISHKEY","\tpublished key")
                nodedb.save()
            time.sleep(1)

class SearchThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"search",60,g)
    def stop(self):
        JobThread.stop(self)
        self.glob.datquery.semap.release()
    def dojob(self, nodedb, logger, prof, datdb, datq):
        d = datq.pop()
        if not d: return
        if datdb.has_keyhash(d.hash):
            datq.save()
            return
        d.published = int(time.time())
        datq.add(d)
        datq.save()
        if self.finish: return
        reckey = []
        next = nodedb.neighbors_nodes(d.hash,False,5)
        sent = []
        keys = self.glob.keydb.get_bydatkey(d.hash)
        if keys: reckey.extend(keys)
        while True:
            if self.finish: return
            neighbors = next
            if len(neighbors)==0: break
            next = []
            for node in neighbors:
                if self.finish: return
                logger.log("SEARCH","findvalue to %s for %s" % (hexlify(node.id),d.url))
                sent.append(node.id)
                try:
                    self.node = node
                    res = node.findvalue(d.hash)
                    self.node = None
                except o2on_node.NodeRemovable:
                    nodedb.remove(node)
                    nodedb.save()
                    self.glob.keydb.remove_bynodeid(node.id)
                    self.glob.keydb.save()
                except o2on_node.NodeRefused:
                    pass
                except socket.error, inst:
                    logger.log("SEARCH", inst)
                else:
                    nodedb.add_node(node)
                    if not res: res = []
                    for x in res:
                        if isinstance(x, o2on_node.Node):
                            nodedb.add_node(x)
                            if x.id != prof.mynode.id and x not in next and x.id not in sent:
                                logger.log("SEARCH","\tadd new neighbors %s" % hexlify(x.id))
                                next.append(x)
                        elif isinstance(x, o2on_key.Key): 
                            logger.log("SEARCH","\tadd new key")
                            if x not in reckey: reckey.append(x)
                    nodedb.save()
        if len(reckey) == 0: logger.log("SEARCH","\tfailed to get key for %s" % d.url)
        for key in reckey: 
            self.glob.keydb.add(key)
            self.glob.keyquery.add(key)
        self.glob.keydb.save()

class DatQueryThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"dat query",60,g)
    def stop(self):
        JobThread.stop(self)
        self.glob.keyquery.semap.release()
    def dojob(self, nodedb, logger, prof, datdb, datq):
        k = self.glob.keyquery.pop()
        if not k: return
        node = nodedb[k.nodeid]
        if not node: node = o2on_node.Node(k.nodeid, k.ip, k.port)
        logger.log("DATQUERY","dat query %s to %s" % (hexlify(k.hash),hexlify(node.id)))
        try:
            self.node = node
            dat = node.dat(k.hash, None, self.glob)
            self.node = None
        except o2on_node.NodeRemovable:
            nodedb.remove(node)
            nodedb.save()
            self.glob.keydb.remove_bynodeid(node.id)
            self.glob.keydb.save()
        except o2on_node.NodeRefused:
            pass
        except socket.error, inst:
            logger.log("DATQUERY", inst)
        else:
            if dat:
                logger.popup("DATQUERY", "Got queried dat %s" % dat.path())
                datdb.add_dat(dat)
                datdb.save()
            nodedb.add_node(node)
            nodedb.save()
        self.glob.keyquery.save()

class RebuildDatDBThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"rebuild dat DB",60,g)
    def dojob(self, nodes, logger, prof, datdb, datq):
        datdb.checkrebuild()
        self.finish = True

# Server thread

class ProxyServerThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"proxy server",0,g)
        self.serv = o2on_server.O2ONServer(o2on_server.ProxyServerHandler, 
                                           o2on_config.ProxyPort, g)
    def stop(self):
        JobThread.stop(self)
        self.serv.shutdown()
    def dojob(self, nodes, logger, prof, datdb, datq):
        self.serv.serve_forever()

class P2PServerThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"p2p server",0,g)
        if g.prof.mynode.port == 0:
            self.serv = None
        else:
            self.serv = o2on_server.O2ONServer(o2on_server.P2PServerHandler, 
                                               g.prof.mynode.port, g)
    def stop(self):
        JobThread.stop(self)
        if self.serv: self.serv.shutdown()
    def dojob(self, nodes, logger, prof, datdb, datq):
        if not self.serv: 
            self.finish = True
            return
        self.serv.serve_forever()

class AdminServerThread(JobThread):
    def __init__(self, g):
        JobThread.__init__(self,"admin server",0,g)
        self.serv = o2on_server.O2ONServer(o2on_server.AdminServerHandler, 
                                           o2on_config.AdminPort, g)
    def stop(self):
        JobThread.stop(self)
        self.serv.shutdown()
    def dojob(self, nodes, logger, prof, datdb, datq):
        self.serv.serve_forever()
