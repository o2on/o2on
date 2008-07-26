/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2ReportMaker.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include <vector>

class O2Logger;
class O2Profile;
class O2DatIO;
class O2DatDB;
class O2NodeDB;
class O2FriendDB;
class O2KeyDB;
class O2IMDB;
class O2IPFilter;
class O2PerformanceCounter;
class O2Server_HTTP_P2P;
class O2Server_HTTP_Proxy;
class O2Server_HTTP_Admin;
class O2Client;
class O2Job_QueryDat;
class O2Job_Broadcast;
class O2Job;

				
				
				
//
//	O2ReportMaker
//
class O2ReportMaker
{
private:
	typedef std::vector<O2Job*> JobsT;

	O2Logger				*Logger;
	O2Profile				*Profile;
	O2DatDB					*DatDB;
	O2DatIO					*DatIO;
	O2NodeDB				*NodeDB;
	O2FriendDB				*FriendDB;
	O2KeyDB					*KeyDB;
	O2KeyDB					*SakuKeyDB;
	O2KeyDB					*QueryDB;
	O2KeyDB					*SakuDB;
	O2IMDB					*IMDB;
	O2IMDB					*BroadcastDB;
	O2IPFilter				*IPFilter_P2P;
	O2IPFilter				*IPFilter_Proxy;
	O2IPFilter				*IPFilter_Admin;
	O2PerformanceCounter	*PerformanceCounter;
	O2Server_HTTP_P2P		*Server_P2P;
	O2Server_HTTP_Proxy		*Server_Proxy;
	O2Server_HTTP_Admin		*Server_Admin;
	O2Client				*Client;
	O2Job_QueryDat			*Job_QueryDat;
	O2Job_Broadcast			*Job_Broadcast;
	JobsT					Jobs;

public:
	O2ReportMaker(O2Logger				*lgr
				, O2Profile				*prof
				, O2DatDB				*datdb
				, O2DatIO				*datio
				, O2NodeDB				*ndb
				, O2FriendDB			*fdb
				, O2KeyDB				*kdb
				, O2KeyDB				*skdb
				, O2KeyDB				*qdb
				, O2KeyDB				*sakudb
				, O2IMDB				*imdb
				, O2IMDB				*bc
				, O2IPFilter			*ipf_p2p
				, O2IPFilter			*ipf_proxy
				, O2IPFilter			*ipf_admin
				, O2PerformanceCounter	*pmc
				, O2Server_HTTP_P2P		*sv_p2p
				, O2Server_HTTP_Proxy	*sv_proxy
				, O2Server_HTTP_Admin	*sv_admin
				, O2Client				*client
				, O2Job_QueryDat		*job_querydat
				, O2Job_Broadcast		*job_bc);
	~O2ReportMaker();

	void PushJob(O2Job *job);
	void GetReport(string &out, bool pub);
};
