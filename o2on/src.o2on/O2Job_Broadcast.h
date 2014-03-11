/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_Broadcast.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2NodeDB.h"
#include "O2IMDB.h"
#include "O2Client.h"
#include "dataconv.h"




class O2Job_Broadcast
	: public O2Job
	, public Mutex
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2KeyDB		*KeyDB;
	O2IMDB		*BroadcastDB;
	O2Client	*Client;
	O2IMessages	Queue;

public:
	O2Job_Broadcast(const wchar_t	*name
			     , time_t			interval
				 , bool				startup
			     , O2Logger			*lgr
			     , O2Profile		*prof
				 , O2NodeDB			*ndb
				 , O2KeyDB			*kdb
			     , O2IMDB			*bc
			     , O2Client			*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, NodeDB(ndb)
		, KeyDB(kdb)
		, BroadcastDB(bc)
		, Client(client)
	{
	}

	~O2Job_Broadcast()
	{
	}

	bool Add(const O2IMessage &im)
	{
		if (BroadcastDB->Exist(im))
			return false;
		Lock();
		Queue.push_back(im);
		Unlock();
		return true;
	}

	size_t GetQueueCount(void)
	{
		return (Queue.size());
	}

	void JobThreadFunc(void)
	{
		Lock();
		O2IMessages queue(Queue);
		Queue.clear();
		Unlock();

		if (queue.size() == 0)
			return;

		hashT myid;
		Profile->GetID(myid);

		O2IMessagesIt it;
		for (it = queue.begin(); it != queue.end() && IsActive(); it++) {
			O2IMessage &im = *it;

			TRACEA("[BroadcastJob]");
			TRACEW(it->msg.c_str());
			TRACEA("\n");

			O2NodeDB::NodeListT neighbors;
			if (NodeDB->neighbors(myid, neighbors, false, O2_BROADCAST_PATH_LIMIT+2) == 0)
				return;

			O2NodeDB::NodeListT::iterator nit = neighbors.begin();
			while (nit != neighbors.end()) {
				if (nit->id == im.id) {
					nit = neighbors.erase(nit);
					continue;
				}
				hashListT::iterator hit = std::find(im.paths.begin(), im.paths.end(), nit->id);
				if (hit != im.paths.end()) {
					nit = neighbors.erase(nit);
					continue;
				}
				nit++;
			}

			if (neighbors.empty())
				continue;

			hashT myid;
			Profile->GetID(myid);
			im.paths.push_back(myid);
			while (im.paths.size() > O2_BROADCAST_PATH_LIMIT)
				im.paths.pop_front();

			for (nit = neighbors.begin(); nit != neighbors.end() && IsActive(); nit++) {
				nit->lastlink = 0;
				nit->reset();

				// Broadcast”­s
				O2SocketSession ss;
				ss.ip = nit->ip;
				ss.port = nit->port;

				string xml;
				BroadcastDB->MakeSendXML(im, xml);

				string url;
				MakeURL(ss.ip, ss.port, O2PROTOPATH_BROADCAST, url);

				HTTPHeader hdr(HTTPHEADERTYPE_REQUEST);
				hdr.method = "POST";
				hdr.SetURL(url.c_str());
				AddRequestHeaderFields(hdr, Profile);
				AddContentFields(hdr, xml.size(), "text/xml", DEFAULT_XML_CHARSET);
				hdr.Make(ss.sbuff);
				ss.sbuff += xml;

				Client->AddRequest(&ss);
				ss.Wait();

				HTTPHeader *header = (HTTPHeader*)ss.data;
				if (CheckResponse(&ss, header, NodeDB, *nit)) {
					;
				}

				if (header) delete header;
				Sleep(1000);
			}

			for (nit = neighbors.begin(); nit != neighbors.end() && IsActive(); nit++) {
				if (nit->lastlink) {
					// ¬Œ÷‚µ‚½ƒm[ƒh‚ðtouch
					NodeDB->touch(*nit);
				}
				else {
					// Ž¸”s‚µ‚½ƒm[ƒh‚ðremove
					NodeDB->remove(*nit);
					KeyDB->DeleteKeyByNodeID(nit->id);
				}
			}

			Sleep(IsActive() ? 1000 : 0);
		}
	}
};
