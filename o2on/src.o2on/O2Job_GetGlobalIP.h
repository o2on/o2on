/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_GetGlobalIP.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2NodeDB.h"
#include "O2Client.h"
#include "O2Protocol_Kademlia.h"
#include "dataconv.h"




class O2Job_GetGlobalIP
	: public O2Job
	, public O2Protocol_Kademlia
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2Client	*Client;

public:
	O2Job_GetGlobalIP(const wchar_t	*name
				    , time_t		interval
					, bool			startup
					, O2Logger		*lgr
					, O2Profile		*prof
					, O2NodeDB		*ndb
					, O2Client		*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, NodeDB(ndb)
		, Client(client)
	{
	}

	~O2Job_GetGlobalIP()
	{
	}

	void JobThreadFunc(void)
	{
		hashT tmpID;
		Profile->GetID(tmpID);

		O2NodeDB::NodeListT neighbors;
		if (NodeDB->neighbors(tmpID, neighbors, false, 100) == 0)
			return;

		O2NodeDB::NodeListT::iterator it;
		for (it = neighbors.begin(); it != neighbors.end() && IsActive(); it++) {
			it->lastlink = 0;
			it->reset();

			// PING発行
			O2SocketSession ss;
			ss.ip = it->ip;
			ss.port = it->port;
			O2Protocol_Kademlia pk;
			MakeRequest_Kademlia_PING(&ss, Profile, ss.sbuff);

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, *it)) {
				// グローバルIP確定
				if (Profile->GetIP() == 0) {
					ulong globalIP = e2ip(&ss.rbuff[header->length], 8);
					Profile->SetIP(globalIP);
					NodeDB->SetSelfNodeIP(globalIP);

					wstring ipstr;
					ulong2ipstr(globalIP, ipstr);
					Logger->AddLog(O2LT_INFO, L"Job", 0, 0,
						L"グローバルIP確定 (%s)", ipstr.c_str());

					NodeDB->touch(*it);
				}
				else
					NodeDB->remove(*it);
			}

			if (header) delete header;
			Sleep(1000);
		}

		// 成功したらやめ
		if (Profile->GetIP() != 0)
			SetActive(false);
	}
};
