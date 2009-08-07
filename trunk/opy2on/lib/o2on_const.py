#!/usr/bin/python
import os.path

regHosts = r'(2ch\.net|bbspink\.com|machibbs\.com|machi\.to)'

DBDir = "db"
DatQueryFile = os.path.join(DBDir, "datq.pkl")
KeyQueryFile = os.path.join(DBDir, "keyq.pkl")
DatDBFile    = os.path.join(DBDir, "datdb.pkl")
KeyDBFile    = os.path.join(DBDir, "keydb.pkl")
NodeDBFile   = os.path.join(DBDir, "nodedb.pkl")
ProfileFile  = os.path.join(DBDir, "profile.pkl")
IMDBFile     = os.path.join(DBDir, "imdb.pkl")

ProtocolName = "O2"
ProtocolVer = 0.2
AppName = "opy2on"
AppMajorVer = 0
AppMinorVer = 0
AppBuildNo =  1

