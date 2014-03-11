#!/usr/bin/python 

from Crypto.PublicKey import RSA  # pycrypto
import os
import cPickle
from binascii import unhexlify

import o2on_util
import o2on_config
import o2on_node
from o2on_const import *

class MyRSA:
    def __init__(self, rsa):
        self.rsa = rsa
    def hex(self):
        s = hex(self.rsa.n)[2:-1]
        return "0" * (320-len(s)) + s
    def pubkey(self):
        return unhexlify(self.hex())

def uname(logger):
    if os.name == 'posix':
        try:
            return os.uname()[0]+" "+os.uname()[4]
        except AttributeError:
            logger.popup("Unknown POSIX %s.\nPlease report it." % os.name)
            return "Unknown POSIX"
    elif os.name == 'nt':
        return "Win"
    try:
        uname = os.uname()[0]+" "+os.uname()[4]
    except AttributeError:
        uname = ""
    logger.popup("GLOBAL","Unknown OS %s (%s).\nPlease report it." % (os.name, uname))
    return "Unknown"

class Profile:
    def __init__(self, l):
        self.logger = l
        self.mynode = o2on_node.Node()
        self.mynode.id = o2on_util.randomid()
        self.mynode.ip = None
        self.mynode.port = o2on_config.P2PPort
        self.mynode.name = o2on_config.NodeName[:8].encode('utf-8')
        self.mynode.pubkey = None
        self.rsa = None
        self.mynode.ua = "%s/%.1f (%s/%1d.%02d.%04d; %s)" % \
            (ProtocolName, ProtocolVer, AppName, AppMajorVer, AppMinorVer, AppBuildNo, 
             uname(l))
        self.load()
        if not self.rsa:
            self.logger.log("PROFILE","generating RSA key")
            self.rsa = MyRSA(RSA.generate(160*8,os.urandom))
            self.save()
        self.mynode.pubkey = self.rsa.pubkey()
    def save(self):
        pkl_file = open(ProfileFile,"wb")
        cPickle.dump(self.rsa, pkl_file,-1)
        cPickle.dump(self.mynode.id, pkl_file,-1)
        pkl_file.close()
    def load(self):
        if(os.path.isfile(ProfileFile)):
            pkl_file = open(ProfileFile,"rb")
            self.rsa = cPickle.load(pkl_file)
            self.mynode.id =  cPickle.load(pkl_file)
            pkl_file.close()
