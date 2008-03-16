/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_PublishKeys.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2NodeDB.h"
#include "O2KeyDB.h"
#include "O2Client.h"
#include "O2Protocol_Kademlia.h"
#include "dataconv.h"




class O2Job_PublishKeys
	: public O2Job
	, public O2Protocol_Kademlia
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2KeyDB		*KeyDB;
	O2KeyDB		*SakuKeyDB;
	O2Client	*Client;

	typedef std::map<O2Node,O2KeyList> O2NodeKeyMap;
	typedef O2NodeKeyMap::iterator O2NodeKeyMapIt;

public:
	O2Job_PublishKeys(const wchar_t	*name
				    , time_t		interval
					, bool			startup
					, O2Logger		*lgr
					, O2Profile		*prof
					, O2NodeDB		*ndb
					, O2KeyDB		*kdb
					, O2KeyDB		*skdb
					, O2Client		*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, NodeDB(ndb)
		, KeyDB(kdb)
		, SakuKeyDB(skdb)
		, Client(client)
	{
	}

	~O2Job_PublishKeys()
	{
	}

	void JobThreadFunc(void)
	{
		if (Profile->GetP2PPort() == 0) {
			Active = false;
			return;
		}

//stopwatch sw("[[ PublishKeys ]]");
		Publish(KeyDB, "dat");
		Publish(SakuKeyDB, "saku");
	}

	void Publish(O2KeyDB *db, const char *category)
	{
		O2KeyList keylist;
		if (db->GetKeyList(keylist, time(NULL)-PUBLISH_TT) == 0)
			return;

		O2NodeKeyMap nkmap;

//stopwatch *sw = new stopwatch("PublishKeys: MakeMap");
		O2KeyListIt kit;
		for (kit = keylist.begin(); kit != keylist.end() && IsActive(); kit++) {
			O2NodeDB::NodeListT neighbors;
			if (NodeDB->neighbors(kit->hash, neighbors, false) == 0)
				break;

			O2NodeKeyMapIt nkit;
			std::pair<O2NodeKeyMapIt,bool> ret;

			O2NodeDB::NodeListT::iterator it;
			for (it = neighbors.begin(); it != neighbors.end() && IsActive(); it++) {
				nkit = nkmap.find(*it);
				if (nkit == nkmap.end()) {
					ret = nkmap.insert(O2NodeKeyMap::value_type(*it, O2KeyList()));
					nkit = ret.first;
				}
				nkit->second.push_back(*kit);
				Sleep(1);
			}
		}
		keylist.clear();
//delete sw;

		if (nkmap.empty())
			return;

		O2NodeDB::NodeListT proceededNodes;

		O2NodeKeyMapIt nkit;
		for (nkit = nkmap.begin(); nkit != nkmap.end() && IsActive(); nkit++) {
			string xml;
			if (KeyDB->ExportToXML(nkit->second, xml) == 0)
				continue;

			O2Node node(nkit->first);
			node.lastlink = 0;
			node.reset();

			// STORE発行
			O2SocketSession ss;
			ss.ip = node.ip;
			ss.port = node.port;
			O2Protocol_Kademlia pk;
			MakeRequest_Kademlia_STORE(&ss, Profile, category, xml.size(), ss.sbuff);
			ss.sbuff += xml;

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, node)) {
				O2KeyListIt kit;
				for (kit = nkit->second.begin(); kit != nkit->second.end() && IsActive(); kit++) {
					keylist.push_back(*kit);
				}
			}

			proceededNodes.push_back(node);

			if (header) delete header;
			Sleep(IsActive() ? PUBLISH_INTERVAL_MS : 0);
		}

		O2NodeDB::NodeListT::iterator it;
		for (it = proceededNodes.begin(); it != proceededNodes.end() && IsActive(); it++) {
			if (it->lastlink) {
				// 成功したノードをtouch
				NodeDB->touch(*it);
			}
			else {
				// 失敗したノードをremove
				NodeDB->remove(*it);
				KeyDB->DeleteKeyByNodeID(it->id);
			}
		}
#if 0
		// キーでループ
		O2KeyListIt it;
		for (it = keylist.begin(); it != keylist.end() && IsActive(); it++) {
			string xml;
			if (db->ExportToXML(*it, xml) == 0)
				continue;
/*
			TRACEA("+++++++++++++++++++++++++++++++++++++++++++++++++\n");
			TRACEA(" Publish Keys\n");
			TRACEA("+++++++++++++++++++++++++++++++++++++++++++++++++\n");
			TRACEA(xml.c_str());
			TRACEA("\n");
*/
			O2NodeDB::NodeListT neighbors;
			if (NodeDB->neighbors(it->hash, neighbors, false) == 0)
				break;

			// キーの近隣ノードでループ
			for (size_t i = 0; i < neighbors.size() && IsActive(); i++) {
				neighbors[i].lastlink = 0;
				neighbors[i].reset();

				// STORE発行
				O2SocketSession ss;
				ss.ip = neighbors[i].ip;
				ss.port = neighbors[i].port;
				O2Protocol_Kademlia pk;
				MakeRequest_Kademlia_STORE(&ss, Profile, category, xml.size(), ss.sbuff);
				ss.sbuff += xml;

				Client->AddRequest(&ss);
				ss.Wait();

				HTTPHeader *header = (HTTPHeader*)ss.data;
				if (CheckResponse(&ss, header, NodeDB, neighbors[i])) {
					db->SetDate(it->hash, time(NULL));
				}

				if (header) delete header;
				Sleep(IsActive() ? PUBLISH_INTERVAL_MS : 0);
			}

			for (size_t i = 0; i < neighbors.size() && IsActive(); i++) {
				if (neighbors[i].lastlink) {
					// 成功したノードをtouch
					NodeDB->touch(neighbors[i]);
				}
				else {
					// 失敗したノードをremove
					NodeDB->remove(neighbors[i]);
					KeyDB->DeleteKeyByNodeID(neighbors[i].id);
				}
			}
		}
#endif
	}
};
