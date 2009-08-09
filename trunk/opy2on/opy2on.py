#!/usr/bin/python
# -*- coding: utf-8

from binascii import hexlify
import socket
import sys
import os.path
import os
import re
import traceback
import time
import threading

sys.path.append("lib")

import o2on_profile
import o2on_node
import o2on_util
import o2on_config
import o2on_server
import o2on_dat
import o2on_const
import o2on_key
import o2on_job
import o2on_im

def showstat(args):
    glob.logger.begin()
    glob.logger.log("GLOBAL", "nodes %d" % len(glob.nodedb))
    glob.logger.log("GLOBAL", "datquery %d" % len(glob.datquery))
    glob.logger.log("GLOBAL", "dat %d" % len(glob.datdb))
    glob.logger.log("GLOBAL", "key %d" % len(glob.keydb))
    glob.logger.end()

def showmynode(args):
    if glob.prof.mynode.ip:
        glob.logger.log("GLOBAL",
                        "my node is %s%s%s" % (hexlify(glob.prof.mynode.id), 
                                               o2on_node.ip2e(glob.prof.mynode.ip),
                                               o2on_node.port2e(glob.prof.mynode.port)))
    else:
        glob.logger.log("GLOBAL", "Didn't get global IP")

def read_2channel_brd():
    res = []
    regBoard = re.compile(r'^\s+[^.]+\.'+o2on_const.regHosts+r'\s+([a-z0-9]+)\s')
    if os.path.isfile(o2on_config.Path2channel_brd):
        f = open(o2on_config.Path2channel_brd,'r')
        while True:
            line = f.readline()
            if line == '':break
            m = regBoard.match(line)
            if m:
                res.append(m.group(1)+":"+m.group(2))
        f.close()
    else: glob.logger.popup("GLOBAL", "No 2channel.brd file")
    return res

def show_myid(x):
    glob.logger.log("GLOBAL", "my ID is %s" % hexlify(glob.prof.mynode.id))

def show_mypubkey(x):
    glob.logger.log("GLOBAL", "my pubkey is %s" % hexlify(glob.prof.mynode.pubkey))

def exportnode(x):
    glob.nodedb.exportnode()

def showdatquery(x):
    glob.logger.begin()
    glob.logger.log("GLOBAL", "-"*80)
    for x in str(glob.datquery).split("\n"):
        glob.logger.log("GLOBAL", x)
    glob.logger.log("GLOBAL", "-"*80)
    glob.logger.end()

def showhelp(x):
    glob.logger.begin()
    glob.logger.log("GLOBAL", "datq: show searching dat")
    glob.logger.log("GLOBAL", "exit: finish program")
    glob.logger.log("GLOBAL", "exportnode: export nodes to 'exportnodes.xml'")
    glob.logger.log("GLOBAL", "help: show this help")
    glob.logger.log("GLOBAL", "keys: show keys")
    glob.logger.log("GLOBAL", "myid: show my ID")
    glob.logger.log("GLOBAL", "mynode: show my node info")
    glob.logger.log("GLOBAL", "mypubkey: show my RSA public key")
    glob.logger.log("GLOBAL", "nodes: show nodes")
    glob.logger.log("GLOBAL", 
                    "stat: show the numbers of nodes, searching dats, owning dats, keys")
    glob.logger.end()

def readcommand(glob):
    try:
        regexit = re.compile(r'^exit\s*$')
        while True: 
            foo = sys.stdin.readline()
            if regexit.match(foo): break
            if foo == '': break
            args = re.compile("\s+").split(foo)
            procd = False
            for c in commands.keys():
                if c == args[0]: 
                    commands[c](args[1:])
                    procd = True
                    break
            if not procd: glob.logger.log("GLOBAL", "No such command: %s" % args[0])
    except Exception,inst:
        if o2on_config.OutputErrorFile:
            f = open('error-'+str(int(time.time()))+'.txt', 'w')
            traceback.print_exc(file=f)
            f.close()
        glob.logger.popup("ERROR", str(inst))
    glob.shutdown.set()

class dummy: pass

#socket.setdefaulttimeout(o2on_config.SocketTimeout)

if not os.path.exists(o2on_const.DBDir):
    os.makedirs(o2on_const.DBDir)

glob = dummy()
glob.logger = o2on_util.Logger()
glob.prof = o2on_profile.Profile(glob.logger)

glob.nodedb = o2on_node.NodeDB(glob)
glob.datdb = o2on_dat.DatDB(glob)
glob.keydb = o2on_key.KeyDB(glob)
glob.imdb = o2on_im.IMDB(glob)

glob.datquery = o2on_util.DatQuery()
glob.keyquery = o2on_util.KeyQuery()
glob.allboards = read_2channel_brd()

glob.shutdown = threading.Event()

o2on_node.build_common_header(glob.prof)
o2on_server.build_common_header(glob.prof)

show_myid(None)
show_mypubkey(None)

jobs = (
    o2on_job.GetIPThread(glob),
    o2on_job.ProxyServerThread(glob),
    o2on_job.AdminServerThread(glob),
    o2on_job.NodeCollectorThread(glob),
    o2on_job.DatCollectorThread(glob),
    o2on_job.AskNodeCollectionThread(glob),
    o2on_job.PublishOrigThread(glob),
    o2on_job.PublishKeyThread(glob),
    o2on_job.SearchThread(glob),
    o2on_job.DatQueryThread(glob),
    o2on_job.P2PServerThread(glob),
    )

for j in jobs: j.start()

commands = {
    "datq": showdatquery,
    'exportnode': exportnode,
    "help": showhelp,
    "keys": (lambda x: glob.keydb.show()),
    "myid": show_myid,
    "mynode" : showmynode,
    "mypubkey": show_mypubkey,
    "nodes": (lambda x: glob.nodedb.show()),
    "stat": showstat,
}

th = threading.Thread(target=readcommand, args=(glob,))
th.setDaemon(True)
th.start()

try:
    glob.shutdown.wait()
except KeyboardInterrupt:
    pass
except Exception,inst:
    if o2on_config.OutputErrorFile:
        f = open('error-'+str(int(time.time()))+'.txt', 'w')
        traceback.print_exc(file=f)
        f.close()
    glob.logger.popup("ERROR", str(inst))

glob.logger.log("GLOBAL", "Finish Jobs")
for j in jobs: j.stop()
glob.logger.popup("GLOBAL", "Waiting for Jobs to stop")
n = len(jobs)
c = 0
for j in jobs:
    j.join(1)
    while j.isAlive():
        glob.logger.popup("GLOBAL", "Waiting for %s" % j.name)
        j.join(7)
    c += 1
    glob.logger.log("GLOBAL", "Finished %d/%d" % (c, n))
glob.imdb.save()
glob.keydb.save()
glob.datdb.save()
glob.nodedb.save()
glob.datquery.save()
glob.keyquery.save()
glob.logger.popup("GLOBAL", "Finished Completely")
