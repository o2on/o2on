/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server_HTTP_Admin.h
 * description	: o2on server class
 *
 */

#pragma once
#include "O2Server_HTTP.h"
#include "O2Protocol_Dat.h"
#include "O2Protocol_Kademlia.h"
#include "O2Profile.h"
#include "O2DatIO.h"
#include "O2NodeDB.h"
#include "O2FriendDB.h"
#include "O2KeyDB.h"
#include "O2IMDB.h"
#include "O2IPFilter.h"
#include "O2Client.h"
#include "O2ReportMaker.h"
#include "O2Boards.h"
#include "thread.h"
#include <sys/types.h>
#include <sys/stat.h>

#if defined(MODULE)
#undef MODULE
#endif
#define MODULE	L"AdminServer"




//
// O2Server_HTTP_Admin
//
class O2Server_HTTP_Admin
	: public O2Server_HTTP
	, public O2Protocol
{
private:
	typedef std::vector<O2Job*> JobsT;

	O2Profile		*Profile;
	O2DatDB			*DatDB;
	O2DatIO			*DatIO;
	O2NodeDB		*NodeDB;
	O2FriendDB		*FriendDB;
	O2KeyDB			*KeyDB;
	O2KeyDB			*SakuKeyDB;
	O2KeyDB			*QueryDB;
	O2KeyDB			*SakuDB;
	O2IMDB			*IMDB;
	O2IMDB			*BroadcastDB;
	O2IPFilter		*IPFilter_P2P;
	O2IPFilter		*IPFilter_Proxy;
	O2IPFilter		*IPFilter_Admin;
	O2Job_Broadcast	*Job_Broadcast;
	O2Client		*Client;
	O2ReportMaker	*ReportMaker;
	O2Boards		*Boards;
	JobsT			Jobs;
	HWND			hwndBaloonCallback;
	UINT			msgBaloonCallback;
	uint64			ThreadNum;
	Mutex			ThreadNumLock;
	SQLResultList	sqlresult;
	wstring			sql;

	typedef std::map<string,uint64> TrafficMapT;
	TrafficMapT RecvByteMap;
	TrafficMapT SendByteMap;

public:
	O2Server_HTTP_Admin(O2Logger		*lgr
					  , O2IPFilter		*ipf
					  , O2Profile		*prof
					  , O2DatDB			*datdb
					  , O2DatIO			*datio
					  , O2NodeDB		*ndb
					  , O2FriendDB		*fdb
					  , O2KeyDB			*kdb
					  , O2KeyDB			*skdb
					  , O2KeyDB			*qdb
					  , O2KeyDB			*sakudb
					  , O2IMDB			*imdb
					  , O2IMDB			*imbc
					  , O2IPFilter		*ipf_p2p
					  , O2IPFilter		*ipf_proxy
					  , O2IPFilter		*ipf_admin
					  , O2Job_Broadcast *bc
					  , O2Client		*client
					  , O2Boards		*brd)
		: O2Server_HTTP(MODULE, lgr, ipf_admin)
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
		, BroadcastDB(imbc)
		, IPFilter_P2P(ipf_p2p)
		, IPFilter_Proxy(ipf_proxy)
		, IPFilter_Admin(ipf_admin)
		, Job_Broadcast(bc)
		, Client(client)
		, Boards(brd)
		, ReportMaker(NULL)
		, hwndBaloonCallback(NULL)
		, msgBaloonCallback(0)
		, ThreadNum(0)
	{
	}
	~O2Server_HTTP_Admin()
	{
	}
	uint64 GetThreadNum(void)
	{
		return (ThreadNum);
	}
	void SetReportMaker(O2ReportMaker *rm)
	{
		ReportMaker = rm;
	}
	void SetBaloonCallbackMsg(HWND hwnd, UINT msg)
	{
		hwndBaloonCallback = hwnd;
		msgBaloonCallback = msg;
	}

protected:
	// -----------------------------------------------------------------------
	//	O2Server_HTTP methods (override)
	// -----------------------------------------------------------------------
	virtual void ParseRequest(O2SocketSession *ss)
	{
		ThreadParam *param = new ThreadParam;
		param->me = this;
		param->ss = ss;

		HANDLE handle =
			(HANDLE)_beginthreadex(NULL, 0, StaticParseThread, (void*)param, 0, NULL);
		CloseHandle(handle);
	}

private:
	// -----------------------------------------------------------------------
	//	Own methods
	// -----------------------------------------------------------------------
	struct ThreadParam {
		O2Server_HTTP_Admin *me;
		O2SocketSession *ss;
	};

	static uint WINAPI StaticParseThread(void *data)
	{
		ThreadParam *param = (ThreadParam*)data;
		O2Server_HTTP_Admin *me = param->me;
		O2SocketSession *ss = param->ss;
		delete param;

		me->ThreadNumLock.Lock();
		me->ThreadNum++;
		me->ThreadNumLock.Unlock();

		CoInitialize(NULL);
		me->ParseThread(ss);
		CoUninitialize();

		me->ThreadNumLock.Lock();
		me->ThreadNum--;
		me->ThreadNumLock.Unlock();

		//_endthreadex(0);
		return (0);
	}

	void ParseThread(O2SocketSession *ss)
	{
		HTTPHeader *hdr = (HTTPHeader*)ss->data;
		string url = hdr->url;
		strmap::iterator it;

		if (hdr->paths.size() == 2 && hdr->paths[0] == "xml")
			ReturnXML(ss, hdr);
		else
			ReturnFile(ss, hdr);

		if (ss->sbuff.empty() && ss->sbuffoffset == 0) {
			// 404
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
		}

		ss->SetCanDelete(true);
	}

	// -----------------------------------------------------------------------
	//	ReturnXML
	// -----------------------------------------------------------------------
	void ReturnXML(O2SocketSession *ss, HTTPHeader *hdr)
	{
		bool post = hdr->method == "POST" ? true : false;
		string path = hdr->paths[1];

		/*
			/xml/node			: GET	:
			/xml/ininode		: GET	:
			/xml/ininode		: POST	:
			/xml/key			: GET	:
			/xml/sakukey		: GET	:
			/xml/query			: GET	:
			/xml/query			: POST	:
			/xml/saku			: GET	:
			/xml/saku			: POST	:
			/xml/ipf			: GET	:
			/xml/ipf			: POST	:
			/xml/getboards		: GET	:
			/xml/bbsmenu		: GET	:
			/xml/bbsmenu		: POST	:
			/xml/thread			: GET	:
			/xml/thread			: POST	:
			/xml/dat			: GET	:
			/xml/im				: GET	:
			/xml/im				: POST	:
			/xml/sendim			: POST	:
			/xml/imbroadcast	: GET	:
			/xml/imbroadcast	: POST	:
			/xml/friend			: GET	:
			/xml/friend			: POST	:
			/xml/log			: GET	:
			/xml/report			: GET	:
			/xml/profile		: GET	:
			/xml/rprofile		: GET	:
			/xml/config			: GET	:
			/xml/config			: POST	:
			/xml/sql			: GET	:
			/xml/sql			: POST	:
			/xml/notification	: GET	:
			/xml/shutdown		: GET	:
		*/

		if (path == "node")
			GET_xml_node(ss);
		else if (path == "ininode")
			post ? POST_xml_ininode(ss,hdr) : GET_xml_ininode(ss);
		else if (path == "key")
			GET_xml_key(ss);
		else if (path == "sakukey")
			GET_xml_sakukey(ss);
		else if (path == "query")
			post ? POST_xml_query(ss,hdr) : GET_xml_query(ss);
		else if (path == "saku")
			post ? POST_xml_saku(ss,hdr) : GET_xml_saku(ss);
		else if (path == "ipf")
			post ? POST_xml_ipf(ss,hdr) : GET_xml_ipf(ss,hdr);
		else if (path == "getboards")
			GET_xml_getboards(ss);
		else if (path == "bbsmenu")
			post ? POST_xml_bbsmenu(ss,hdr) : GET_xml_bbsmenu(ss);
		else if (path == "thread")
			post ? POST_xml_thread(ss,hdr) : GET_xml_thread(ss,hdr);
		else if (path == "dat")
			GET_xml_dat(ss,hdr);
		else if (path == "im")
			post ? POST_xml_im(ss,hdr) : GET_xml_im(ss);
		else if (path == "sendim")
			POST_xml_sendim(ss,hdr);
		else if (path == "imbroadcast")
			post ? POST_xml_imbroadcast(ss,hdr) : GET_xml_imbroadcast(ss);
		else if (path == "friend")
			post ? POST_xml_friend(ss,hdr) : GET_xml_friend(ss);
		else if (path == "log")
			GET_xml_log(ss,hdr);
		else if (path == "report")
			GET_xml_report(ss);
		else if (path == "profile")
			GET_xml_profile(ss);
		else if (path == "rprofile")
			GET_xml_rprofile(ss,hdr);
		else if (path == "config")
			post ? POST_xml_config(ss,hdr) : GET_xml_config(ss);
		else if (path == "sql")
			post ? POST_xml_sql(ss,hdr) : GET_xml_sql(ss);
		else if (path == "notification")
			GET_xml_notification(ss);
		else if (path == "shutdown")
			GET_xml_shutdown(ss);
		else {
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
		}
	}

	// -----------------------------------------------------------------------
	//	ReturnFile
	// -----------------------------------------------------------------------
	void ReturnFile(O2SocketSession *ss, HTTPHeader *hdr)
	{
		if (strstr(hdr->pathname.c_str(), "..")) {
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
			return;
		}

		// convert url -> filepath
		string filename;
		filename += Profile->GetAdminRootA();
		filename += (hdr->pathname == "/" ? "/index.html" : hdr->pathname);
		std::replace(filename.begin(), filename.end(), '/', '\\');

		// check file
		struct _stat st;
		if (_stat(filename.c_str(), &st) == -1) {
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
			return;
		}

		// xsl ?
		bool is_xsl = strstr(hdr->pathname.c_str(), ".xsl") ? true : false;

		// check modified
		if (!is_xsl) {
			time_t ifms = hdr->GetFieldDate("If-Modified-Since");
			if (ifms != 0 && st.st_mtime <= ifms) {
				ss->Lock();
				MakeResponse_304(Profile, ss->sbuff);
				ss->Unlock();
				return;
			}
		}

		// load file
		string out;
		out.resize(st.st_size);
		FILE *fp;
		if (fopen_s(&fp, filename.c_str(), "rb") == 0) {
			fread(&out[0], st.st_size, 1, fp);
				fclose(fp);
		}

		// replace XSL constant value
		if (is_xsl) {
			const char *qval;
			qval = hdr->GetQueryString("LIMIT");
			if (qval) {
				size_t pos = out.find("LIMIT");
				if (FOUND(pos)) {
					out.replace(pos, 5, qval);
				}
			}
			qval = hdr->GetQueryString("SORTKEY");
			if (qval) {
				size_t pos = out.find("SORTKEY");
				if (FOUND(pos)) {
					out.replace(pos, 7, qval);
				}
			}
			qval = hdr->GetQueryString("SORTORDER");
			if (qval) {
				size_t pos = out.find("SORTORDER");
				if (FOUND(pos)) {
					out.replace(pos, 9, qval);
				}
			}
			qval = hdr->GetQueryString("SORTTYPE");
			if (qval) {
				size_t pos = out.find("SORTTYPE");
				if (FOUND(pos)) {
					out.replace(pos, 8, qval);
				}
			}
			st.st_mtime = time(NULL);
		}

		ss->Lock();
		{
			HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
			header.status = 200;
			AddResponseHeaderFields(header, Profile);
			AddContentFields(header, out.size(), header.filename2contenttype(filename.c_str()), NULL);
			header.AddFieldDate("Last-Modified", st.st_mtime);
			header.Make(ss->sbuff);
			ss->sbuff += out;
		}
		ss->Unlock();
	}

public:
	// -----------------------------------------------------------------------
	//	GET_xml_node
	// -----------------------------------------------------------------------
	void GET_xml_node(O2SocketSession *ss)
	{
		O2NodeSelectCondition cond(NODE_XMLELM_ALL | NODE_XMLELM_INFO);
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";
		cond.include_port0 = true;

		string xml;
		NodeDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_ininode
	// -----------------------------------------------------------------------
	void GET_xml_ininode(O2SocketSession *ss)
	{
		string xml;
		xml = "<?xml version=\"1.0\"?>";

		string nodestr;
		if (Profile->GetEncryptedProfile(nodestr)) {
			xml += "<e>" + nodestr + "</e>";
		}
		else if (!Profile->GetP2PPort()) {
			wstring w = L"<e>ポートを開放していないので取得できません</e>";
			string s;
			FromUnicode(_T(DEFAULT_XML_CHARSET), w, s);
			xml += s;
		}
		else {
			wstring w = L"<e>グローバルIPが確定していないため取得できません</e>";
			string s;
			FromUnicode(_T(DEFAULT_XML_CHARSET), w, s);
			xml += s;
		}

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_ininode
	// -----------------------------------------------------------------------
	void POST_xml_ininode(O2SocketSession *ss, HTTPHeader *hdr)
	{
		size_t addnum = 0;

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("initext");
			if (it != hdr->queries.end()) {
				addnum = NodeDB->AddEncodedNode(it->second.c_str(), it->second.size());
			}
			else if ((it = hdr->queries.find("url")) != hdr->queries.end()) {
				string url = it->second;
				if ((it = hdr->queries.find("regist")) != hdr->queries.end()) {
					string nodestr;
					if (Profile->GetEncryptedProfile(nodestr))
						url += "?" + nodestr;
				}

				HTTPHeader h(HTTPHEADERTYPE_REQUEST);
				h.ver = 10;

				HTTPSocket hsock(SELECT_TIMEOUT_MS, "");
				string out;
				HTTPHeader oh(HTTPHEADERTYPE_RESPONSE);
				if (hsock.request(url.c_str(), h, NULL, 0, true)) {
					while (hsock.response(out, oh) >= 0)
					{
						if (oh.contentlength) {
							if (out.size() - oh.length >= oh.contentlength)
								break;
						}
					}
					addnum += NodeDB->ImportFromXML(NULL, &out[oh.length], out.size()-oh.length, NULL);
				}
				hsock.close();
			}
		}

		if (addnum) {
			wchar_t message[64];
			swprintf_s(message, 64, L"%u件追加しました", addnum);
			NodeDB->SetXMLMessage(message, L"succeeded");
		}
		else {
			NodeDB->SetXMLMessage(L"追加失敗", L"error");
		}

		//
		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_key
	// -----------------------------------------------------------------------
	void GET_xml_key(O2SocketSession *ss)
	{
		O2KeySelectCondition cond(KEY_XMLELM_ALL | KEY_XMLELM_INFO);
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";

		string xml;
		KeyDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_sakukey
	// -----------------------------------------------------------------------
	void GET_xml_sakukey(O2SocketSession *ss)
	{
		O2KeySelectCondition cond(KEY_XMLELM_ALL | KEY_XMLELM_INFO);
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";

		string xml;
		SakuKeyDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_query
	// -----------------------------------------------------------------------
	void GET_xml_query(O2SocketSession *ss)
	{
		O2KeySelectCondition cond(KEY_XMLELM_ALL | KEY_XMLELM_INFO);
		cond.orderbydate = true;
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";

		string xml;
		QueryDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_query
	// -----------------------------------------------------------------------
	void POST_xml_query(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring msg;
		wstring msgtype = L"succeeded";

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("act");
			if (it != hdr->queries.end()) {
				string &act = it->second;
				if (act == "add") {
					// register query
					it = hdr->queries.find("url");
					if (it != hdr->queries.end()) {
						O2DatPath datpath;
						if (datpath.set(it->second.c_str())) {

							O2Key key;
							datpath.gethash(key.hash);
							key.ip = 0;
							key.port = 0;
							key.size = DatIO->GetSize(datpath);
							datpath.geturl(key.url);
							if (key.size) {
								DatIO->GetTitle(datpath);
								datpath.gettitle(key.title);
							}
							it = hdr->queries.find("note");
							if (it != hdr->queries.end()) {
								ToUnicode(L"utf-8", it->second, key.note);
							}
							key.date = 0;
							key.enable = true;

							QueryDB->AddKey(key);
							QueryDB->Save(Profile->GetQueryFilePath());

							if (DatIO->KakoHantei(datpath)) {
								msg = L"登録しました（完全ぽいキャッシュを所有）";
							}
							else if (key.size) {
								msg = L"登録しました（キャッシュ所有）";
							}
							else {
								msg = L"登録しました";
							}
						}
						else {
							msg = L"URLがおかしいです";
							msgtype = L"error";
						}
					}
				}
				else if (act == "delete") {
					// remove query
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						strarray hasharray;
						// comma separate
						if (split(it->second.c_str(), ",", hasharray ) > 0) {
							for (uint hashidx = 0; hashidx < hasharray.size(); hashidx++) {
								if (hasharray[ hashidx ].size() >= HASHSIZE*2) {
									hashT hash;
									hash.assign( hasharray[ hashidx ].c_str(), hasharray[ hashidx ].size() );
									if (QueryDB->DeleteKey(hash)) {
										msg = L"削除しました";
										QueryDB->Save(Profile->GetQueryFilePath());
									}
								}
							}
						}
					}
				}
				else if (act == "activate") {
					// activate query
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						strarray hasharray;
						// comma separate
						if (split(it->second.c_str(), ",", hasharray ) > 0) {
							for (uint hashidx = 0; hashidx < hasharray.size(); hashidx++) {
								if (hasharray[ hashidx ].size() >= HASHSIZE * 2) {
									hashT hash;
									hash.assign( hasharray[ hashidx ].c_str(), hasharray[ hashidx ].size() );
									if (QueryDB->SetEnable(hash,true)) {
										msg = L"有効にしました";
										QueryDB->Save(Profile->GetQueryFilePath());
									}
								}
							}
						}
					}
				}
				else if (act == "deactivate") {
					// deactivate query
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						strarray hasharray;
						// comma separate
						if (split(it->second.c_str(), ",", hasharray ) > 0) {
							for (uint hashidx = 0; hashidx < hasharray.size(); hashidx++) {
								if (hasharray[ hashidx ].size() >= HASHSIZE * 2) {
									hashT hash;
									hash.assign( hasharray[ hashidx ].c_str(), hasharray[ hashidx ].size() );
									if (QueryDB->SetEnable(hash,false)) {
										msg = L"無効にしました";
										QueryDB->Save(Profile->GetQueryFilePath());
									}
								}
							}
						}
					}
				}
			}
		}

		if (msg.empty())
			QueryDB->SetXMLMessage(L"処理失敗", L"error");
		else
			QueryDB->SetXMLMessage(msg.c_str(), msgtype.c_str());

		//
		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_saku
	// -----------------------------------------------------------------------
	void GET_xml_saku(O2SocketSession *ss)
	{
		O2KeySelectCondition cond(KEY_XMLELM_ALL | KEY_XMLELM_INFO);
		cond.orderbydate = true;
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";

		string xml;
		SakuDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_saku
	// -----------------------------------------------------------------------
	void POST_xml_saku(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring msg;
		wstring msgtype = L"succeeded";

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("act");
			if (it != hdr->queries.end()) {
				string &act = it->second;
				if (act == "add") {
					// register saku
					it = hdr->queries.find("url");
					if (it != hdr->queries.end()) {
						O2DatPath datpath;
						if (datpath.set(it->second.c_str())) {

							O2Key key;
							datpath.gethash(key.hash);
							key.ip = 0;
							key.port = 0;
							key.size = 0;
							datpath.geturl(key.url);
							if (key.size) {
								DatIO->GetTitle(datpath);
								datpath.gettitle(key.title);
							}
							it = hdr->queries.find("title");
							if (it != hdr->queries.end()) {
								ToUnicode(L"utf-8", it->second, key.title);
							}
							it = hdr->queries.find("note");
							if (it != hdr->queries.end()) {
								ToUnicode(L"utf-8", it->second, key.note);
							}
							key.date = 0;
							key.enable = true;

							if (!key.note.empty()) {
								SakuDB->AddKey(key);
								SakuDB->Save(Profile->GetSakuFilePath());
								msg = L"登録しました";
							}
							else {
								msg = L"理由を入力してください";
								msgtype = L"error";
							}
						}
						else {
							msg = L"URLがおかしいです";
							msgtype = L"error";
						}
					}
				}
				else if (act == "delete") {
					// remove saku
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						if (it->second.size() >= HASHSIZE*2) {
							hashT hash;
							hash.assign(it->second.c_str(), it->second.size());
							if (SakuDB->DeleteKey(hash)) {
								msg = L"削除しました";
								SakuDB->Save(Profile->GetSakuFilePath());
							}
						}
					}
				}
				else if (act == "activate") {
					// activate saku
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						if (it->second.size() >= HASHSIZE*2) {
							hashT hash;
							hash.assign(it->second.c_str(), it->second.size());
							if (SakuDB->SetEnable(hash,true)) {
								msg = L"有効にしました";
								SakuDB->Save(Profile->GetSakuFilePath());
							}
						}
					}
				}
				else if (act == "deactivate") {
					// activate saku
					it = hdr->queries.find("hash");
					if (it != hdr->queries.end()) {
						if (it->second.size() >= HASHSIZE*2) {
							hashT hash;
							hash.assign(it->second.c_str(), it->second.size());
							if (SakuDB->SetEnable(hash,false)) {
								msg = L"配信を停止しました";
								SakuDB->Save(Profile->GetSakuFilePath());
							}
						}
					}
				}
			}
		}

		if (msg.empty())
			SakuDB->SetXMLMessage(L"処理失敗", L"error");
		else
			SakuDB->SetXMLMessage(msg.c_str(), msgtype.c_str());

		//
		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_ipf
	// -----------------------------------------------------------------------
	void GET_xml_ipf(O2SocketSession *ss, HTTPHeader *hdr)
	{
		string type;
		strmap::iterator it = hdr->queries.find("type");
		if (it != hdr->queries.end())
			type = it->second;

		O2IPFilter *ipf;
		if (type == "P2P")
			ipf = IPFilter_P2P;
		else if (type == "Proxy")
			ipf = IPFilter_Proxy;
		else if (type == "Admin")
			ipf = IPFilter_Admin;
		else
			ipf = IPFilter_P2P;

		string xml;
		ipf->ExportToXML(xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_ipf
	// -----------------------------------------------------------------------
	void POST_xml_ipf(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring msg;
		wstring msgtype = L"succeeded";

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			// type
			string type;
			strmap::iterator it = hdr->queries.find("type");
			if (it != hdr->queries.end())
				type = it->second;

			O2IPFilter *ipf;
			wstring filename;
			if (type == "P2P") {
				ipf = IPFilter_P2P;
				filename = Profile->GetIPF_P2PFilePath();
			}
			else if (type == "Proxy") {
				ipf = IPFilter_Proxy;
				filename = Profile->GetIPF_ProxyFilePath();
			}
			else if (type == "Admin") {
				ipf = IPFilter_Admin;
				filename = Profile->GetIPF_AdminFilePath();
			}
			else {
				ipf = IPFilter_P2P;
				filename = Profile->GetIPF_P2PFilePath();
			}

			//default
			uint defaultflag = ipf->getdefault();
			it = hdr->queries.find("default");
			if (it != hdr->queries.end()) {
				defaultflag = it->second == "allow" ? O2_ALLOW : O2_DENY;
			}

			//record
			O2IPFilterRecords recs;
			bool error = false;
			uint i = 1;
			while (1) {
				char e[10];
				char f[10];
				char c[10];
				sprintf_s(e, 10, "e%d", i);
				sprintf_s(f, 10, "f%d", i);
				sprintf_s(c, 10, "c%d", i);

				O2IPFilterRecord rec;

				it = hdr->queries.find(e);
				if (it != hdr->queries.end())
					rec.enable = true;
				else
					rec.enable = false;

				it = hdr->queries.find(f);
				if (it == hdr->queries.end()) break;
				rec.flag = it->second == "allow" ? O2_ALLOW : O2_DENY;

				it = hdr->queries.find(c);
				if (it == hdr->queries.end()) break;
				if (it->second.empty()) break;
				ascii2unicode(it->second, rec.cond);

				if (!ipf->check(rec.enable, rec.flag, rec.cond.c_str())) {
					msgtype = L"error";
					msg = L"エラー（";
					msg += rec.cond;
					msg += L"）";
					error = true;
					break;
				}
				recs.push_back(rec);
				i++;
			}
			if (!error) {
				ipf->setdefault(defaultflag);
				ipf->clear();
				for (size_t i = 0; i < recs.size(); i++)
					ipf->add(recs[i].enable, recs[i].flag, recs[i].cond.c_str());
				msg = L"変更しました";
				ipf->Save(filename.c_str());
			}
		}

		if (msg.empty())
			QueryDB->SetXMLMessage(L"処理失敗", L"error");
		else
			QueryDB->SetXMLMessage(msg.c_str(), msgtype.c_str());

		//
		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_getboards
	// -----------------------------------------------------------------------
	void GET_xml_getboards(O2SocketSession *ss)
	{
		Boards->Get();
		Boards->Save();

		ss->Lock();
		MakeResponse_302(Profile, "/xml/bbsmenu", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_bbsmenu
	// -----------------------------------------------------------------------
	void GET_xml_bbsmenu(O2SocketSession *ss)
	{
		wstring domain;
		wstring bbsname;
		strmap::iterator it;

		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";
		xml += "<?xml-stylesheet type=\"text/xsl\" href=\"/bbsmenu.xsl\"?>";

		string tmp;
		Boards->MakeBBSMenuXML(tmp, DatDB);
		xml += "<bbsmenu>";
		xml += tmp;
		xml += "</bbsmenu>";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_bbsmenu
	// -----------------------------------------------------------------------
	void POST_xml_bbsmenu(O2SocketSession *ss, HTTPHeader *hdr)
	{
		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it;
			it = hdr->queries.find("bbsurl");
			if (it != hdr->queries.end() && !it->second.empty()) {
				Boards->AddEx(it->second.c_str());
			}

			wstring boardname;
			wstrarray enableboards;
			for (it = hdr->queries.begin(); it != hdr->queries.end(); it++) {
				if (strchr(it->first.c_str(), ':')) {
					ascii2unicode(it->first, boardname);
					enableboards.push_back(boardname);
				}
			}
			Boards->EnableEx(enableboards);
		}
		else
			Boards->ClearEx();

		Boards->SaveEx();
		Profile->SetDatStorageFlag(Boards->SizeEx() ? true : false);

		ss->Lock();
		MakeResponse_302(Profile, "/xml/bbsmenu", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_thread
	// -----------------------------------------------------------------------
	void GET_xml_thread(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring domain;
		wstring bbsname;
		strmap::iterator it;

		it = hdr->queries.find("domain");
		if (it != hdr->queries.end())
			ascii2unicode(it->second.c_str(), domain);
		it = hdr->queries.find("bbsname");
		if (it != hdr->queries.end())
			ascii2unicode(it->second.c_str(), bbsname);

		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";

		if (!domain.empty() && !bbsname.empty()) {
			string tmp;
			DatIO->ExportToXML(domain.c_str(), bbsname.c_str(), tmp);
			xml += "<threads>";
			xml += tmp;
			xml += "</threads>";
		}
		else {
			xml += "<threads><message></message></threads>";
		}

//		string gzxml;
//		ZLibCompressor *gzc = new ZLibCompressor(gzxml, true);
//		gzc->init(Z_DEFAULT_COMPRESSION, xml.c_str(), xml.size());
//		gzc->onetime();
//		delete gzc;


		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);
//		header.AddFieldString("Content-Encoding", "gzip");

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
//		ss->sbuff.append(gzxml.c_str(), gzxml.size());
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_thread
	// -----------------------------------------------------------------------
	void POST_xml_thread(O2SocketSession *ss, HTTPHeader *hdr)
	{
		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			hashT hash;
			hashListT hashlist;
			strmap::iterator it;
			for (it = hdr->queries.begin(); it != hdr->queries.end(); it++) {
				if (it->first.size() == HASHSIZE*2) {
					hash.assign(it->first.c_str(), it->first.size());
					hashlist.push_back(hash);
				}
			}
			if (!hashlist.empty())
				DatIO->Delete(hashlist);
		}

		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_dat
	// -----------------------------------------------------------------------
	void GET_xml_dat(O2SocketSession *ss, HTTPHeader *hdr)
	{
		hashT hash;
		string html;
		string encoding;


		strmap::iterator it = hdr->queries.find("hash");
		if (it != hdr->queries.end()) {
			hash.assign(it->second.c_str(), it->second.size());
			DatIO->Dat2HTML(hash, html, encoding);
		}

		if (html.empty())
			html = "取得失敗";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, html.size(), "text/html", encoding.c_str());

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += html;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_im
	// -----------------------------------------------------------------------
	void GET_xml_im(O2SocketSession *ss)
	{
		O2IMSelectCondition cond(IM_XMLELM_ALL | IM_XMLELM_INFO);
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";
		cond.desc = true;

		string xml;
		IMDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_im
	// -----------------------------------------------------------------------
	void POST_xml_im(O2SocketSession *ss, HTTPHeader *hdr)
	{
		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			hashT hash;
			hashListT hashlist;
			strmap::iterator it;
			for (it = hdr->queries.begin(); it != hdr->queries.end(); it++) {
				if (it->first.size() == HASHSIZE*2) {
					hash.assign(it->first.c_str(), it->first.size());
					hashlist.push_back(hash);
				}
			}
			if (!hashlist.empty())
				IMDB->DeleteMessage(hashlist);
		}

		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_sendim
	// -----------------------------------------------------------------------
	void POST_xml_sendim(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring msg;
		wstring msgtype = L"succeeded";

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			string e_ip;
			string port_str;
			string id_str;
			string name;
			string pub_str;
			wstring msgW;
			const char *qval;

			//
			qval = hdr->GetQueryString("e_ip");
			e_ip = qval ? qval : "";
			qval = hdr->GetQueryString("port");
			port_str = qval ? qval : "";
			qval = hdr->GetQueryString("id");
			id_str = qval ? qval : "";
			qval = hdr->GetQueryString("name");
			name = qval ? qval : "";
			qval = hdr->GetQueryString("msg");
			if (qval) ToUnicode(_T(DEFAULT_XML_CHARSET), qval, strlen(qval), msgW);
			qval = hdr->GetQueryString("pubkey");
			pub_str = qval ? qval : "";

			if (!e_ip.empty() && !port_str.empty() && !id_str.empty() && !msgW.empty()) {
				O2Node tmpnode;
				tmpnode.id.assign(id_str.c_str(), id_str.size());
				tmpnode.pubkey.assign(pub_str.c_str(), pub_str.size());
				ToUnicode(_T(DEFAULT_XML_CHARSET), name, tmpnode.name);

				string imxml;
				if (IMDB->MakeSendXML(Profile, _T(DEFAULT_XML_CHARSET), msgW.c_str(), imxml)) {
					O2SocketSession imss;
					imss.ip   = e2ip(e_ip.c_str(), e_ip.size());
					imss.port = (ushort)strtoul(port_str.c_str(), NULL, 10);

					string url;
					MakeURL(imss.ip, imss.port, O2PROTOPATH_IM, url);

					HTTPHeader imhdr(HTTPHEADERTYPE_REQUEST);
					imhdr.method = "POST";
					imhdr.SetURL(url.c_str());
					AddRequestHeaderFields(imhdr, Profile);
					AddContentFields(imhdr, imxml.size(), "text/xml", DEFAULT_XML_CHARSET);
					imhdr.Make(imss.sbuff);
					imss.sbuff += imxml;
					Client->AddRequest(&imss, true);
					imss.Wait();

					if (!imss.error && !imss.rbuff.empty()) {
						msg = L"送信しました";
						O2IMessage im;
						im.ip = imss.ip;
						im.port = imss.port;
						im.id = tmpnode.id;
						im.pubkey = tmpnode.pubkey;
						im.name = tmpnode.name;
						im.msg = msgW;
						im.mine = true;
						IMDB->AddMessage(im);
					}
					else {
						msg = L"送信失敗";
						msgtype = L"error";
					}

					HTTPHeader *rhdr = (HTTPHeader*)imss.data;
					if (rhdr) {
						O2Node node;
						if (GetNodeInfoFromHeader(*rhdr, imss.ip, imss.port, node)) {
							node.status = O2_NODESTATUS_PASTLINKEDTO;
							node.connection_me2n = 1;
							node.recvbyte_me2n = imss.rbuff.size();
							node.sendbyte_me2n = imss.sbuff.size();
							NodeDB->touch(node);

							if (tmpnode.id != node.id) {
								NodeDB->remove(tmpnode);
							}
						}
						delete rhdr;
					}
				}
				if (msg.empty() || msgtype == L"error") {
					NodeDB->remove(tmpnode);
				}
			}
		}

		if (msg.empty())
			QueryDB->SetXMLMessage(L"処理失敗", L"error");
		else
			QueryDB->SetXMLMessage(msg.c_str(), msgtype.c_str());

		//
		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_imbroadcast
	// -----------------------------------------------------------------------
	void GET_xml_imbroadcast(O2SocketSession *ss)
	{
		O2IMSelectCondition cond(IM_XMLELM_ALL | IM_XMLELM_INFO);
		cond.xsl = L"/imbroadcast.xsl";
		cond.timeformat = L"%H:%M:%S";
		cond.desc = true;

		string xml;
		BroadcastDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_imbroadcast
	// -----------------------------------------------------------------------
	void POST_xml_imbroadcast(O2SocketSession *ss, HTTPHeader *hdr)
	{
		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("msg");
			if (it != hdr->queries.end()) {
				O2IMessage im;
				im.ip = Profile->GetIP();
				im.port = Profile->GetP2PPort();
				Profile->GetID(im.id);
				string pubstr;
				Profile->GetPubkeyA(pubstr);
				im.pubkey.assign(pubstr.c_str(), pubstr.size());
				im.name = Profile->GetNodeNameW();
				ToUnicode(_T("utf-8"), it->second, im.msg);
				im.date = time(NULL);
				im.key.random();
				im.mine = true;
				im.broadcast = true;
				if (Job_Broadcast->Add(im))
					BroadcastDB->AddMessage(im);
			}
		}

		GET_xml_imbroadcast(ss);
	}
	// -----------------------------------------------------------------------
	//	GET_xml_friend
	// -----------------------------------------------------------------------
	void GET_xml_friend(O2SocketSession *ss)
	{
		O2NodeSelectCondition cond(NODE_XMLELM_ALL | NODE_XMLELM_INFO);
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";
		cond.include_port0 = true;

		string xml;
		FriendDB->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_friend
	// -----------------------------------------------------------------------
	void POST_xml_friend(O2SocketSession *ss, HTTPHeader *hdr)
	{
		wstring msg;
		wstring msgtype = L"succeeded";

		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("act");
			if (it != hdr->queries.end()) {
				string &act = it->second;
				if (act == "add") {
					string id_str;
					string e_ip;
					string port_str;
					string name;
					string pub_str;

					const char *qval;
					qval = hdr->GetQueryString("id");
					id_str = qval ? qval : "";
					qval = hdr->GetQueryString("e_ip");
					e_ip = qval ? qval : "";
					qval = hdr->GetQueryString("port");
					port_str = qval ? qval : "";
					qval = hdr->GetQueryString("name");
					name = qval ? qval : "";
					qval = hdr->GetQueryString("pubkey");
					pub_str = qval ? qval : "";

					if (!id_str.empty() && !e_ip.empty() && !port_str.empty() && !name.empty() && !pub_str.empty()) {
						O2Node node;
						node.id.assign(id_str.c_str(), id_str.size());
						node.ip = e2ip(e_ip.c_str(), e_ip.size());
						node.port = (ushort)strtoul(port_str.c_str(), NULL, 10);
						ToUnicode(_T(DEFAULT_XML_CHARSET), name, node.name);
						node.pubkey.assign(pub_str.c_str(), pub_str.size());
						if (FriendDB->Add(node)) {
							NodeDB->SetXMLMessage(L"追加しました", L"succeeded");
						}
						else {
							NodeDB->SetXMLMessage(L"既に追加されています", L"succeeded");
						}
						FriendDB->Save(Profile->GetFriendFilePath());
					}
				}
				else if (act == "delete") {
					hashT hash;
					strmap::iterator it;
					for (it = hdr->queries.begin(); it != hdr->queries.end(); it++) {
						if (it->first.size() == HASHSIZE*2) {
							hash.assign(it->first.c_str(), it->first.size());
							FriendDB->Delete(hash);
						}
					}
					FriendDB->Save(Profile->GetFriendFilePath());
				}
			}
		}

		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_log
	// -----------------------------------------------------------------------
	void GET_xml_log(O2SocketSession *ss, HTTPHeader *hdr)
	{
		string type = "log";
		strmap::iterator it = hdr->queries.find("type");
		if (it != hdr->queries.end())
			type = it->second;

		O2LogSelectCondition cond(LOG_XMLELM_ALL | IM_XMLELM_INFO);
		if (type == "log")
			cond.type = LOGGER_LOG;
		else if (type == "net")
			cond.type = LOGGER_NETLOG;
		else if (type == "hokan")
			cond.type = LOGGER_HOKANLOG;
		else if (type == "ipf")
			cond.type = LOGGER_IPFLOG;
		else
			cond.type = LOGGER_LOG;
		cond.timeformat = L"%Y/%m/%d %H:%M:%S";

		string xml;
		Logger->ExportToXML(cond, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_report
	// -----------------------------------------------------------------------
	void GET_xml_report(O2SocketSession *ss)
	{
		string out;
		ReportMaker->GetReport(out, false);

		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, out.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += out;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_profile
	// -----------------------------------------------------------------------
	void GET_xml_profile(O2SocketSession *ss)
	{
		string out;
		ReportMaker->GetReport(out, true);

		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, out.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += out;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_rprofile
	// -----------------------------------------------------------------------
	void GET_xml_rprofile(O2SocketSession *ss, HTTPHeader *hdr)
	{
		string out;
		string e_ip;
		string port_str;
		string id_str;
		const char *qval;

		//
		qval = hdr->GetQueryString("e_ip");
		e_ip = qval ? qval : "";
		qval = hdr->GetQueryString("port");
		port_str = qval ? qval : "";
		qval = hdr->GetQueryString("id");
		id_str = qval ? qval : "";

		if (!e_ip.empty() && !port_str.empty() && !id_str.empty()) {
			O2Node tmpnode;
			tmpnode.id.assign(id_str.c_str(), id_str.size());

			O2SocketSession pross;
			pross.ip   = e2ip(e_ip.c_str(), e_ip.size());
			pross.port = (ushort)strtoul(port_str.c_str(), NULL, 10);

			string url;
			MakeURL(pross.ip, pross.port, O2PROTOPATH_PROFILE, url);

			HTTPHeader prohdr(HTTPHEADERTYPE_REQUEST);
			prohdr.method = "GET";
			prohdr.SetURL(url.c_str());
			AddRequestHeaderFields(prohdr, Profile);
			prohdr.Make(pross.sbuff);
			Client->AddRequest(&pross, true);
			pross.Wait();

			HTTPHeader *rhdr = (HTTPHeader*)pross.data;
			if (!pross.error && !pross.rbuff.empty() && rhdr) {
				if (pross.rbuff.size() - rhdr->length >= rhdr->contentlength)
					out = &pross.rbuff[rhdr->length];

				O2Node node;
				if (GetNodeInfoFromHeader(*rhdr, pross.ip, pross.port, node)) {
					node.status = O2_NODESTATUS_PASTLINKEDTO;
					node.connection_me2n = 1;
					node.recvbyte_me2n = pross.rbuff.size();
					node.sendbyte_me2n = pross.sbuff.size();
					NodeDB->touch(node);

					if (tmpnode.id != node.id) {
						NodeDB->remove(tmpnode);
					}
				}
				delete rhdr;
			}

			if (out.empty()) {
				NodeDB->remove(tmpnode);
			}
		}

		if (out.empty()) {
			wstring xml;
			xml += L"<?xml version=\"1.0\" encoding=\"";
			xml += _T(DEFAULT_XML_CHARSET);
			xml += L"\"?>";
			xml += L"<?xml-stylesheet type=\"text/xsl\" href=\"/profile.xsl\"?>";
			xml += L"<report><message>取得に失敗しました</message></report>";
			FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
		}

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, out.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += out;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_config
	// -----------------------------------------------------------------------
	void GET_xml_config(O2SocketSession *ss)
	{
		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";
		xml += "<config></config>";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_config
	// -----------------------------------------------------------------------
	void POST_xml_config(O2SocketSession *ss, HTTPHeader *hdr)
	{
		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";
		xml += "<config></config>";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_sql
	// -----------------------------------------------------------------------
	void GET_xml_sql(O2SocketSession *ss)
	{
		wstring str;
		wstring tmpstr;
		str += L"<?str version=\"1.0\" encoding=\"";
		str += _T(DEFAULT_XML_CHARSET);
		str += L"\"?>";
		str += L"<result>";
		makeCDATA(sql, tmpstr);
		str += L"<sql>";
		str += tmpstr;
		str += L"</sql>";
		if (!sqlresult.empty()) {
			wchar_t tmp[16];
			swprintf_s(tmp, 16, L"%u", sqlresult.size()-1);
			str += L"<count>";
			str += tmp;
			str += L"</count>";
		}
		for (size_t i = 0; i < sqlresult.size(); i++) {
			str += L"<row>";
			for (size_t j = 0; j < sqlresult[i].size(); j++) {
				makeCDATA(sqlresult[i][j], tmpstr);
				if (i == 0) {
					str += L"<name>";
					str += tmpstr;
					str += L"</name>";
				}
				else {
					str += L"<col>";
					str += tmpstr;
					str += L"</col>";
				}
			}
			str += L"</row>";
		}
		str += L"</result>";
		sqlresult.clear();

		string xml;
		FromUnicode(_T(DEFAULT_XML_CHARSET), str, xml);

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	POST_xml_sql
	// -----------------------------------------------------------------------
	void POST_xml_sql(O2SocketSession *ss, HTTPHeader *hdr)
	{
		sqlresult.clear();
		if (hdr->ParseQueryString(&ss->rbuff[hdr->length])) {
			strmap::iterator it = hdr->queries.find("sql");
			if (it != hdr->queries.end()) {
				ToUnicode(_T(DEFAULT_XML_CHARSET), it->second, sql);
				DatDB->select(sql.c_str(), sqlresult);
			}
		}

		ss->Lock();
		MakeResponse_302(Profile, "/main.html", ss->sbuff);
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_notification
	// -----------------------------------------------------------------------
	void GET_xml_notification(O2SocketSession *ss)
	{
		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";
		xml += "<?xml-stylesheet type=\"text/xsl\" href=\"/notification.xsl\"?>";
		xml += "<notification>";

		char tmp[32];
		sprintf_s(tmp, 32, "%I64u", time(NULL));
		xml += "<date>";
		xml += tmp;
		xml += "</date>";

		if (IMDB->HaveNewMessage())
			xml += "<newim/>";
		if (NodeDB->IsDetectNewVer())
			xml += "<newver/>";
		xml += "</notification>";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	GET_xml_shutdown
	// -----------------------------------------------------------------------
	void GET_xml_shutdown(O2SocketSession *ss)
	{
		string xml;
		xml += "<?xml version=\"1.0\" encoding=\"";
		xml += DEFAULT_XML_CHARSET;
		xml += "\"?>";
		xml += "<shutdown/>";

		//
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		AddResponseHeaderFields(header, Profile);
		AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		ss->Lock();
		header.Make(ss->sbuff);
		ss->sbuff += xml;
		ss->Unlock();

		PostMessage(hwndBaloonCallback, WM_CLOSE, 0, 0);

	}
};
