/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_QueryDat.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2NodeDB.h"
#include "O2KeyDB.h"
#include "O2DatIO.h"
#include "O2Client.h"
#include "O2Protocol_Dat.h"
#include "dataconv.h"




class O2Job_QueryDat
	: public O2Job
	, public O2Protocol_Dat
	, public Mutex
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2KeyDB		*KeyDB;
	O2KeyDB		*QueryDB;
	O2DatIO		*DatIO;
	O2Client	*Client;
	O2KeyList	RequestQueue;
	HWND		hwndBaloonCallback;
	UINT		msgBaloonCallback;

public:
	O2Job_QueryDat(const wchar_t	*name
			     , time_t			interval
				 , bool				startup
			     , O2Logger			*lgr
			     , O2Profile		*prof
			     , O2NodeDB			*ndb
			     , O2KeyDB			*kdb
			     , O2KeyDB			*qdb
			     , O2DatIO			*dio
			     , O2Client			*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, KeyDB(kdb)
		, NodeDB(ndb)
		, QueryDB(qdb)
		, DatIO(dio)
		, Client(client)
		, hwndBaloonCallback(NULL)
		, msgBaloonCallback(0)
	{
	}

	~O2Job_QueryDat()
	{
	}

	void SetBaloonCallbackMsg(HWND hwnd, UINT msg)
	{
		hwndBaloonCallback = hwnd;
		msgBaloonCallback = msg;
	}

	void AddRequest(const O2Key &key)
	{
		Lock();
		O2KeyListIt it = std::find(
			RequestQueue.begin(), RequestQueue.end(), key);
		if (it == RequestQueue.end())
			RequestQueue.push_back(key);
		Unlock();
	}

	size_t GetRequestQueueCount(void)
	{
		return (RequestQueue.size());
	}

	void JobThreadFunc(void)
	{
		Lock();
		O2KeyList keylist(RequestQueue);
		RequestQueue.clear();
		Unlock();

		if (keylist.size() == 0)
			return;

		O2NodeDB::NodeListT Nodes;

		// キーでループ
		O2KeyListIt it;
		for (it = keylist.begin(); it != keylist.end() && IsActive(); it++) {
			O2Key &key = *it;

			TRACEA("[GETDAT]");
			TRACEW(it->url.c_str());
			TRACEA("\n");

			O2Node node;
			node.id = it->nodeid;
			node.ip = it->ip;
			node.port = it->port;

			// リクエスト発行
			O2SocketSession ss;
			ss.ip = key.ip;
			ss.port = key.port;
			MakeRequest_Dat(&key.hash, NULL, &ss, Profile, DatIO, ss.sbuff);

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, node)) {
				// 本文のdat取り込み
				const char *rawdata = &ss.rbuff[header->length];
				uint64 datsize = ss.rbuff.size() - header->length;

				ImportDat(DatIO, Profile, NULL, *header, rawdata, datsize,
					Logger, ss.ip, ss.port, QueryDB, hwndBaloonCallback, msgBaloonCallback);
			}

			if (header) delete header;
			Nodes.push_back(node);

			Sleep(IsActive() ? QUERY_DAT_INTERVAL_MS : 0);
		}

		O2NodeDB::NodeListT::iterator nit;
		for (nit = Nodes.begin(); nit != Nodes.end() && IsActive(); nit++) {
			if (nit->lastlink) {
				// 成功したノードをtouch
				NodeDB->touch(*nit);
			}
			else {
				// 失敗したノードをremove
				NodeDB->remove(*nit);
				KeyDB->DeleteKeyByNodeID(nit->id);
			}
		}
	}
};
