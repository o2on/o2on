/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_AskCollection.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2NodeDB.h"
#include "O2Boards.h"
#include "O2Client.h"
#include "O2Protocol_Dat.h"
#include "dataconv.h"




class O2Job_AskCollection
	: public O2Job
	, public O2Protocol_Dat
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2KeyDB		*KeyDB;
	O2Boards	*Boards;
	O2Client	*Client;

public:
	O2Job_AskCollection(const wchar_t	*name
				    , time_t			interval
					, bool				startup
					, O2Logger			*lgr
					, O2Profile			*prof
					, O2NodeDB			*ndb
					, O2KeyDB			*kdb
					, O2Boards			*boards
					, O2Client			*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, NodeDB(ndb)
		, KeyDB(kdb)
		, Boards(boards)
		, Client(client)
	{
	}

	~O2Job_AskCollection()
	{
	}

	void JobThreadFunc(void)
	{
		hashT tmpID;
		tmpID.random();

		O2NodeDB::NodeListT neighbors;
		if (NodeDB->neighbors(tmpID, neighbors, false) == 0)
			return;

		O2NodeDB::NodeListT::iterator it;
		for (it = neighbors.begin(); it != neighbors.end() && IsActive(); it++) {
			it->lastlink = 0;
			it->reset();

			// リクエスト発行
			O2SocketSession ss;
			ss.ip = it->ip;
			ss.port = it->port;
			MakeRequest_Collection(&ss, Profile, Boards, ss.sbuff);

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, *it)) {
				// 取り込み
				const char *xml = &ss.rbuff[header->length];
				size_t len = ss.rbuff.size() - header->length;
				Boards->ImportNodeFromXML(*it, xml, len);
			}

			if (header) delete header;

			Sleep(IsActive() ? ASK_COLLECTION_INTERVAL_MS : 0);
		}

		for (it = neighbors.begin(); it != neighbors.end() && IsActive(); it++) {
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
	}
};
