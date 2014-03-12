/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2ReportMaker.cpp
 * description	: 
 *
 */

#include "O2ReportMaker.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "O2DatDB.h"
#include "O2DatIO.h"
#include "O2NodeDB.h"
#include "O2FriendDB.h"
#include "O2KeyDB.h"
#include "O2IMDB.h"
#include "O2IPFilter.h"
#include "O2PerformanceCounter.h"
#include "O2Server_HTTP_P2P.h"
#include "O2Server_HTTP_Proxy.h"
#include "O2Server_HTTP_Admin.h"
#include "O2Client.h"
#include "O2Job_QueryDat.h"
#include "O2Job_Broadcast.h"
#include "O2Job.h"
#include <sys/types.h>
#include <sys/stat.h>

#define TRACE_CONNECTIONS	1




O2ReportMaker::
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
			, O2Job_Broadcast		*job_bc)
	: Logger(lgr)
	, Profile(prof)
	, DatDB(datdb)
	, DatIO(datio)
	, NodeDB(ndb)
	, FriendDB(fdb)
	, KeyDB(kdb)
	, SakuKeyDB(skdb)
	, QueryDB(qdb)
	, SakuDB(sakudb)
	, IMDB(imdb)
	, BroadcastDB(bc)
	, IPFilter_P2P(ipf_p2p)
	, IPFilter_Proxy(ipf_proxy)
	, IPFilter_Admin(ipf_admin)
	, PerformanceCounter(pmc)
	, Server_P2P(sv_p2p)
	, Server_Proxy(sv_proxy)
	, Server_Admin(sv_admin)
	, Client(client)
	, Job_QueryDat(job_querydat)
	, Job_Broadcast(job_bc)
{
}

O2ReportMaker::
~O2ReportMaker()
{
}

void
O2ReportMaker::
PushJob(O2Job *job)
{
	Jobs.push_back(job);
}

void
O2ReportMaker::
GetReport(string &out, bool pub)
{
	double ptime;
	double ptimeavg;
	int handle_c;
	int thread_c;
	PerformanceCounter->GetValue(ptime, ptimeavg, handle_c, thread_c);

	long tzoffset;
	_get_timezone(&tzoffset);

	// start time
	time_t starttime = PerformanceCounter->GetStartTime();
	wchar_t starttime_str[32];
	if (starttime != 0) {
		time_t t = starttime - tzoffset;
		struct tm tm;
		gmtime_s(&tm, &t);
		wcsftime(starttime_str, 32, L"%Y/%m/%d %H:%M:%S", &tm);
	}
	else
		wcscpy_s(starttime_str, 32, L"-");

	// uptime
	uint64 uptime = PerformanceCounter->GetUptime();
	wchar_t uptime_str[32];
	if (uptime != 0) {
		swprintf_s(uptime_str, 32, L"%I64dd %02I64d:%02I64d:%02I64d",
			uptime/86400, (uptime%86400)/3600, (uptime%3600)/60, uptime%60);
	}
	else
		wcscpy_s(uptime_str, 32, L"-");

	// uptime (累計)
	uint64 total_uptime = PerformanceCounter->GetTotalUptime() + uptime;
	wchar_t total_uptime_str[32];
	swprintf_s(total_uptime_str, 32, L"%I64dd %02I64d:%02I64d:%02I64d",
		total_uptime/86400, (total_uptime%86400)/3600, (total_uptime%3600)/60, total_uptime%60);

	wchar_t ptime_str[32];
	swprintf_s(ptime_str, 32, L"%.1f%% (%.1f%%)", ptime, ptimeavg);

	// 送受信バイト数 (現在のセッション)
	uint64 total_u = Server_P2P->GetSendByte()
		+ Client->GetSendByte();
	uint64 total_d = Server_P2P->GetRecvByte()
		+ Client->GetRecvByte();
	wchar_t traffic_u[32];
	swprintf_s(traffic_u, 32, L"%.1f(%.1f)",
		(uptime == 0 ? 0 : (double)total_u/1024/uptime),
		(uptime == 0 ? 0 : (double)total_u/uptime*8));
	wchar_t traffic_d[32];
	swprintf_s(traffic_d, 32, L"%.1f(%.1f)",
		(uptime == 0 ? 0 : (double)total_d/1024/uptime),
		(uptime == 0 ? 0 : (double)total_d/uptime*8));
	wchar_t traffic_ud[32];
	swprintf_s(traffic_ud, 32, L"%.1f(%.1f)",
		(uptime == 0 ? 0 : (double)(total_u+total_d)/1024/uptime),
		(uptime == 0 ? 0 : (double)(total_u+total_d)/uptime*8));

	// 送受信バイト数 (累計)
	uint64 accum_total_u =
		PerformanceCounter->GetTotalSend() + total_u;
	uint64 accum_total_d =
		PerformanceCounter->GetTotalRecv() + total_d;
	wchar_t a_traffic_u[32];
	swprintf_s(a_traffic_u, 32, L"%.1f(%.1f)",
		(total_uptime == 0 ? 0 : (double)accum_total_u/1024/total_uptime),
		(total_uptime == 0 ? 0 : (double)accum_total_u/total_uptime*8));
	wchar_t a_traffic_d[32];
	swprintf_s(a_traffic_d, 32, L"%.1f(%.1f)",
		(total_uptime == 0 ? 0 : (double)accum_total_d/1024/total_uptime),
		(total_uptime == 0 ? 0 : (double)accum_total_d/total_uptime*8));
	wchar_t a_traffic_ud[32];
	swprintf_s(a_traffic_ud, 32, L"%.1f(%.1f)",
		(total_uptime == 0 ? 0 : (double)(accum_total_u+accum_total_d)/1024/total_uptime),
		(total_uptime == 0 ? 0 : (double)(accum_total_u+accum_total_d)/total_uptime*8));

	//dat数、サイズ
	uint64 datnum = DatDB->select_datcount();
	uint64 datsize = DatDB->select_totaldisksize();
	uint64 pubnum = DatDB->select_publishcount(PUBLISH_ORIGINAL_TT);

	//publish率
	wchar_t publishper[32];
	if (datnum == 0)
		wcscpy_s(publishper, 32, L"0.0%");
	else {
		swprintf(publishper, 32, L"%.1f%% (%I64u)", (double)pubnum/datnum*100.0, pubnum);
	}

	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += _T(DEFAULT_XML_CHARSET);
	xml += L"\"?>"EOL;
	if (pub) xml += L"<?xml-stylesheet type=\"text/xsl\" href=\"/profile.xsl\"?>";
	xml += L"<report>"EOL;

	//
	//	名前、コメント、閲覧履歴
	//
	if (pub)
	{
		xml += L"<name><![CDATA[";
		xml += Profile->GetNodeNameW();
		xml += L"]]></name>"EOL;
		xml += L"<category>"EOL;
		xml += L"<table>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"ID");
		hashT hash;
		Profile->GetID(hash);
		wstring hashstr;
		hash.to_string(hashstr);
		xml_AddElement(xml, L"td", L"class=\"L\"", hashstr.c_str());
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"コメント");
		xml += L" <pre><![CDATA[";
		xml += Profile->GetComment();
		xml += L"]]></pre>"EOL;
		xml += L"</tr>"EOL;
		//
		if (Profile->IsPublicRecentDat()) {
			xml += L"<tr>"EOL;
			xml_AddElement(xml, L"td", L"type=\"h\"", L"閲覧履歴");
			xml += L"<pre><![CDATA[";
			O2KeyList recent;
			Server_Proxy->GetRecentDatList(recent);
			for (O2KeyListIt it = recent.begin(); it != recent.end(); it++) {
				wchar_t timestr[TIMESTR_BUFF_SIZE];
				struct tm tm;
				localtime_s(&tm, &it->date);
				wcsftime(timestr, TIMESTR_BUFF_SIZE, L"%Y/%m/%d %H:%M:%S", &tm);
				xml += timestr;
				xml += L" [ ";
				xml += it->url;
				xml += L" ] ";
				xml += it->title;
				xml += L"\r\n";
			}
			xml += L"]]></pre>"EOL;
			xml += L"</tr>"EOL;
		}
		xml += L"</table>"EOL;
		xml += L"</category>"EOL;
	}

	//
	//	概要
	//
	if (!pub || Profile->IsPublicReport())
	{
		xml += L"<category>"EOL;
		xml_AddElement(xml, L"caption", NULL, L"概要");
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"状態");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"CPU(平均)");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"ハンドル");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"スレッド");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"総dat数");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"総datサイズ");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"publish率");
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		if (PerformanceCounter->IsActive())
			xml_AddElement(xml, L"td", L"class=\"active\"", L"稼動中");
		else
			xml_AddElement(xml, L"td", L"class=\"deactive\"", L"停止中");
		xml_AddElement(xml, L"td", L"class=\"C\"", ptime_str);
		xml_AddElement(xml, L"td", L"class=\"N\"", handle_c);
		xml_AddElement(xml, L"td", L"class=\"N\"", thread_c);
		xml_AddElement(xml, L"td", L"class=\"N\"", datnum);
		xml_AddElement(xml, L"td", L"class=\"N\"", datsize);
		xml_AddElement(xml, L"td", L"class=\"R\"", publishper);
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		//
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"起動日時");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"グローバルIP");
		xml_AddElement(xml, L"td", (Server_P2P->IsActive()   ? L"type=\"h\" class=\"active\"" : L"type=\"h\" class=\"deactive\""), L"P2P");
		xml_AddElement(xml, L"td", (Server_Proxy->IsActive() ? L"type=\"h\" class=\"active\"" : L"type=\"h\" class=\"deactive\""), L"Proxy");
		xml_AddElement(xml, L"td", (Server_Admin->IsActive() ? L"type=\"h\" class=\"active\"" : L"type=\"h\" class=\"deactive\""), L"Admin");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Ver");
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		wstring ipstr;
		ulong2ipstr(Profile->GetIP(), ipstr);
		xml_AddElement(xml, L"td", L"class=\"C\"", starttime_str);
		xml_AddElement(xml, L"td", L"class=\"C\"", ipstr.c_str());
		xml_AddElement(xml, L"td", L"class=\"C\"", Profile->GetP2PPort());
		xml_AddElement(xml, L"td", L"class=\"C\"", Profile->GetProxyPort());
		xml_AddElement(xml, L"td", L"class=\"C\"", Profile->GetAdminPort());
		xml_AddElement(xml, L"td", L"class=\"C\"", Profile->GetUserAgentW());
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		//
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"稼働時間");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"上り");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"下り");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"合計");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"上りKB/s(bps)");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"下りKB/s(bps)");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"合計KB/s(bps)");
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"今回");
		xml_AddElement(xml, L"td", L"class=\"C\"", uptime_str);
		xml_AddElement(xml, L"td", L"class=\"N\"", total_u);
		xml_AddElement(xml, L"td", L"class=\"N\"", total_d);
		xml_AddElement(xml, L"td", L"class=\"N\"", total_u+total_d);
		xml_AddElement(xml, L"td", L"", L"");
		xml_AddElement(xml, L"td", L"class=\"R\"", traffic_u);
		xml_AddElement(xml, L"td", L"class=\"R\"", traffic_d);
		xml_AddElement(xml, L"td", L"class=\"R\"", traffic_ud);
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"累計");
		xml_AddElement(xml, L"td", L"class=\"C\"", total_uptime_str);
		xml_AddElement(xml, L"td", L"class=\"N\"", accum_total_u);
		xml_AddElement(xml, L"td", L"class=\"N\"", accum_total_d);
		xml_AddElement(xml, L"td", L"class=\"N\"", accum_total_u+accum_total_d);
		xml_AddElement(xml, L"td", L"", L"");
		xml_AddElement(xml, L"td", L"class=\"R\"", a_traffic_u);
		xml_AddElement(xml, L"td", L"class=\"R\"", a_traffic_d);
		xml_AddElement(xml, L"td", L"class=\"R\"", a_traffic_ud);
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"</category>"EOL;
		//
		//	DB
		//
		xml += L"<category>"EOL;
		xml_AddElement(xml, L"caption", NULL, L"DB");
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Node");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Friend");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Key");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"SakuKey");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Query");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Saku");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"IM");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Broadcast");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"BCQueue");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Logger");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"IPFilter");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"DatRequest");
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"保持数");
		xml_AddElement(xml, L"td", L"class=\"N\"", NodeDB->count());
		xml_AddElement(xml, L"td", L"class=\"N\"", FriendDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", KeyDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", SakuKeyDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", QueryDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", SakuDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", IMDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", BroadcastDB->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", Job_Broadcast->GetQueueCount());
		xml_AddElement(xml, L"td", L"class=\"N\"", Logger->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", IPFilter_P2P->Count()+IPFilter_Proxy->Count()+IPFilter_Admin->Count());
		xml_AddElement(xml, L"td", L"class=\"N\"", Job_QueryDat->GetRequestQueueCount());
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"</category>"EOL;
		//
		//	Server
		//
		xml += L"<category>"EOL;
		xml_AddElement(xml, L"caption", NULL, L"Server");
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"dat");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"collection");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"im");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"broadcast");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"profile");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"ping");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"store");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"findnode");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"findvalue");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"ERROR");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"?");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"計");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Proxy");
		xml_AddElement(xml, L"td", L"type=\"h\"", L"Admin");
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"接続回数");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("dat"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("collection"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("im"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("broadcast"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("profile"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("ping"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("store"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("findnode"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("findvalue"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("ERROR"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath("?"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionCountByPath(NULL));
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Proxy->GetTotalSessionCount());
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Admin->GetTotalSessionCount());
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"up");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("dat"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("collection"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("im"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("broadcast"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("profile"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("ping"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("store"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("findnode"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("findvalue"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("ERROR"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("?"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath(NULL));
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Proxy->GetSendByte());
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Admin->GetSendByte());
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"down");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("dat"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("collection"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("im"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("broadcast"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("profile"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("ping"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("store"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("findnode"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("findvalue"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("ERROR"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath("?"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetRecvByteByPath(NULL));
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Proxy->GetRecvByte());
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Admin->GetRecvByte());
		xml += L"</tr>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"up + down");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("dat")			+ Server_P2P->GetRecvByteByPath("dat"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("collection")	+ Server_P2P->GetRecvByteByPath("collection"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("im")			+ Server_P2P->GetRecvByteByPath("im"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("broadcast")	+ Server_P2P->GetRecvByteByPath("broadcast"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("profile")		+ Server_P2P->GetRecvByteByPath("profile"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("ping")		+ Server_P2P->GetRecvByteByPath("ping"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("store")		+ Server_P2P->GetRecvByteByPath("store"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("findnode")	+ Server_P2P->GetRecvByteByPath("findnode"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("findvalue")	+ Server_P2P->GetRecvByteByPath("findvalue"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("ERROR")		+ Server_P2P->GetRecvByteByPath("ERROR"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath("UNKNOWN")		+ Server_P2P->GetRecvByteByPath("UNKNOWN"));
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSendByteByPath(NULL)			+ Server_P2P->GetRecvByteByPath(NULL));
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Proxy->GetSendByte() + Server_Proxy->GetRecvByte());
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_Admin->GetRecvByte() + Server_Admin->GetRecvByte());
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"同時接続数ピーク");
		xml_AddElement(xml, L"td", L"class=\"N\"", Server_P2P->GetSessionPeak());
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"</category>"EOL;
		//
		//	Agent
		//
		xml += L"<category>"EOL;
		xml_AddElement(xml, L"caption", NULL, L"Agent");
		xml += L"<table>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"");
		size_t i;
		for (i = 0; i < Jobs.size(); i++) {
			if (Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"type=\"h\" class=\"active\"", Jobs[i]->GetName());
			else
				xml_AddElement(xml, L"td", L"type=\"h\" class=\"deactive\"", Jobs[i]->GetName());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"実行");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"C\"", L"-");
			else if (Jobs[i]->IsWorking())
				xml_AddElement(xml, L"td", L"class=\"active\"", L"実行中");
			else
				xml_AddElement(xml, L"td", L"class=\"wait\"", L"待機");
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"間隔(s)");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"C\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"C\"", Jobs[i]->GetInterval());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"起動まで");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive() || Jobs[i]->IsWorking())
				xml_AddElement(xml, L"td", L"class=\"C\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"C\"", Jobs[i]->GetRemain());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"接続回数");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"R\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"N\"", Jobs[i]->GetTotalSessionCount());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"up");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"R\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"N\"", Jobs[i]->GetSendByte());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"down");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"R\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"N\"", Jobs[i]->GetRecvByte());
		}
		xml += L"</tr>"EOL;
		//
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"up+down");
		for (i = 0; i < Jobs.size(); i++) {
			if (!Jobs[i]->IsActive())
				xml_AddElement(xml, L"td", L"class=\"R\"", L"-");
			else
				xml_AddElement(xml, L"td", L"class=\"N\"", Jobs[i]->GetSendByte()+Jobs[i]->GetRecvByte());
		}
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"<table>"EOL;
		xml += L"<tr>"EOL;
		xml_AddElement(xml, L"td", L"type=\"h\"", L"同時接続数ピーク");
		xml_AddElement(xml, L"td", L"class=\"N\"", Client->GetSessionPeak());
		xml += L"</tr>"EOL;
		xml += L"</table>"EOL;
		xml += L"</category>"EOL;

		if (O2DEBUG && TRACE_CONNECTIONS && !pub) {
			//
			//	Connections
			//
			xml += L"<category>"EOL;
			xml_AddElement(xml, L"caption", NULL, L"Connections");
			xml += L"<table>"EOL;
			xml += L"<tr>"EOL;
			xml_AddElement(xml, L"td", L"type=\"h\"", L"");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"IP");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"Port");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"接続時間(s)");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"recv");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"send");
			xml_AddElement(xml, L"td", L"type=\"h\"", L"rbuff");
			xml += L"</tr>"EOL;
			O2SocketSessionPList lst;
			time_t now = time(NULL);
			Server_P2P->GetSessionList(lst);
			for (O2SocketSessionPListIt it = lst.begin(); it != lst.end(); it++) {
				O2SocketSession *ss = *it;
				wstring e_ip;
				ip2e(ss->ip, e_ip);
				xml += L"<tr>"EOL;
				xml_AddElement(xml, L"td", L"class=\"C\"", L"Server");
				xml_AddElement(xml, L"td", L"class=\"C\"", e_ip.c_str());
				xml_AddElement(xml, L"td", L"class=\"C\"", ss->port);
				xml_AddElement(xml, L"td", L"class=\"R\"", now - ss->connect_t);
				xml_AddElement(xml, L"td", L"class=\"N\"", ss->rbuffoffset);
				xml_AddElement(xml, L"td", L"class=\"N\"", ss->sbuffoffset);
				wstring rbuff;
				ascii2unicode(ss->rbuff, rbuff);
				xml_AddElement(xml, L"td", L"class=\"L\"", rbuff.c_str(), true);
				xml += L"</tr>"EOL;
				delete *it;
			}
			lst.clear();
			Client->GetSessionList(lst);
			for (O2SocketSessionPListIt it = lst.begin(); it != lst.end(); it++) {
				O2SocketSession *ss = *it;
				wstring e_ip;
				ip2e(ss->ip, e_ip);
				xml += L"<tr>"EOL;
				xml_AddElement(xml, L"td", L"class=\"C\"", L"Agent");
				xml_AddElement(xml, L"td", L"class=\"C\"", e_ip.c_str());
				xml_AddElement(xml, L"td", L"class=\"C\"", ss->port);
				xml_AddElement(xml, L"td", L"class=\"R\"", now - ss->connect_t);
				xml_AddElement(xml, L"td", L"class=\"N\"", ss->rbuffoffset);
				xml_AddElement(xml, L"td", L"class=\"N\"", ss->sbuffoffset);
				wstring rbuff;
				ascii2unicode(ss->rbuff, rbuff);
				xml_AddElement(xml, L"td", L"class=\"L\"", rbuff.c_str(), true);
				xml += L"</tr>"EOL;
				delete *it;
			}
			xml += L"</table>"EOL;
			xml += L"</category>"EOL;
		}
	}
	xml += L"</report>"EOL;

	FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
}
