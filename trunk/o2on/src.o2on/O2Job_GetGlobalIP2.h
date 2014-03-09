/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.s69.xrea.com/
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
					, O2Logger		*lgr
					, O2Profile		*prof
					, O2NodeDB		*ndb
					, O2Client		*client)
		: O2Job(name, interval)
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

		size_t i;
		for (i = 0; i < neighbors.size() && IsActive(); i++) {
			neighbors[i].lastlink = 0;
			neighbors[i].reset();

			// PING発行
			O2SocketSession ss(1, 5);
			ss.ip = neighbors[i].ip;
			ss.port = neighbors[i].port;
			O2Protocol_Kademlia pk;
			MakeRequest_Kademlia_PING(&ss, Profile, ss.sbuff);

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, neighbors[i])) {
				// グローバルIP確定
				if (Profile->GetIP() == 0) {
					ulong globalIP = e2ip(&ss.rbuff[header->length], 8);
					Profile->SetIP(globalIP);
					NodeDB->SetSelfNodeIP(globalIP);

					wstring ipstr;
					ulong2ipstr(globalIP, ipstr);
					Logger->AddLog(O2LT_INFO, L"Job", 0, 0,
						L"グローバルIP確定 (%s)", ipstr.c_str());

					NodeDB->touch(neighbors[i]);
				}
				else
					NodeDB->remove(neighbors[i]);
			}

			if (header) delete header;
		}

		// 成功したらやめ
		if (Profile->GetIP() != 0)
			SetActive(false);
	}
};
