/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server_HTTP_P2P.h
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
#include "O2KeyDB.h"
#include "O2IMDB.h"
#include "O2Job_Broadcast.h"
#include "O2ReportMaker.h"
#include "O2Version.h"
#include "thread.h"

#if defined(MODULE)
#undef MODULE
#endif
#define MODULE	L"P2PServer"




//
// O2Server_HTTP_P2P
//
class O2Server_HTTP_P2P
	: public O2Server_HTTP
	, public O2Protocol
	, public O2Protocol_Dat
	, public O2Protocol_Kademlia
{
private:
	O2Profile		*Profile;
	O2DatIO			*DatIO;
	O2Boards		*Boards;
	O2NodeDB		*NodeDB;
	O2KeyDB			*KeyDB;
	O2KeyDB			*SakuKeyDB;
	O2KeyDB			*QueryDB;
	O2IMDB			*IMDB;
	O2IMDB			*BroadcastDB;
	O2Job_Broadcast	*Job_Broadcast;
	O2ReportMaker	*ReportMaker;
	HWND			hwndBaloonCallback;
	UINT			msgBaloonCallback;
	uint64			ThreadNum;
	Mutex			ThreadNumLock;

	typedef std::map<string,uint64> TrafficMapT;
	TrafficMapT SessionCountMap;
	TrafficMapT RecvByteMap;
	TrafficMapT SendByteMap;

public:
	O2Server_HTTP_P2P(O2Logger				*lgr
					, O2IPFilter			*ipf
					, O2Profile				*prof
					, O2DatIO				*datio
					, O2Boards				*boards
					, O2NodeDB				*ndb
					, O2KeyDB				*kdb
					, O2KeyDB				*skdb
					, O2KeyDB				*qdb
					, O2IMDB				*imdb
					, O2IMDB				*bc
					, O2Job_Broadcast		*jbc)
		: O2Server_HTTP(MODULE, lgr, ipf)
		, Profile(prof)
		, DatIO(datio)
		, Boards(boards)
		, NodeDB(ndb)
		, KeyDB(kdb)
		, SakuKeyDB(skdb)
		, QueryDB(qdb)
		, IMDB(imdb)
		, BroadcastDB(bc)
		, Job_Broadcast(jbc)
		, ReportMaker(NULL)
		, hwndBaloonCallback(NULL)
		, msgBaloonCallback(0)
		, ThreadNum(0)
	{
	}
	~O2Server_HTTP_P2P()
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

	void AddSessionCountByPath(const char *path, uint64 n)
	{
		TrafficMapT::iterator it = SessionCountMap.find(path);
		if (it != SessionCountMap.end())
			it->second += n;
		else
			SessionCountMap.insert(TrafficMapT::value_type(path, n));
	}
	void AddRecvByteByPath(const char *path, uint64 n)
	{
		TrafficMapT::iterator it = RecvByteMap.find(path);
		if (it != RecvByteMap.end())
			it->second += n;
		else
			RecvByteMap.insert(TrafficMapT::value_type(path, n));
	}
	void AddSendByteByPath(const char *path, uint64 n)
	{
		TrafficMapT::iterator it = SendByteMap.find(path);
		if (it != SendByteMap.end())
			it->second += n;
		else
			SendByteMap.insert(TrafficMapT::value_type(path, n));
	}

	uint64 GetSessionCountByPath(const char *path)
	{
		TrafficMapT::iterator it;
		if (path) {
			it = SessionCountMap.find(path);
			if (it == SessionCountMap.end())
				return (0);
			return (it->second);
		}
		else {
			uint64 n = 0;
			for (it = SessionCountMap.begin(); it != SessionCountMap.end(); it++)
				n += it->second;
			return (n);
		}
	}
	uint64 GetRecvByteByPath(const char *path)
	{
		TrafficMapT::iterator it;
		if (path) {
			it = RecvByteMap.find(path);
			if (it == RecvByteMap.end())
				return (0);
			return (it->second);
		}
		else {
			uint64 n = 0;
			for (it = RecvByteMap.begin(); it != RecvByteMap.end(); it++)
				n += it->second;
			return (n);
		}
	}
	uint64 GetSendByteByPath(const char *path)
	{
		TrafficMapT::iterator it;
		if (path) {
			it = SendByteMap.find(path);
			if (it == SendByteMap.end())
				return (0);
			return (it->second);
		}
		else {
			uint64 n = 0;
			for (it = SendByteMap.begin(); it != SendByteMap.end(); it++)
				n += it->second;
			return (n);
		}
	}

protected:
	// -----------------------------------------------------------------------
	//	O2Server_HTTP methods (override)
	// -----------------------------------------------------------------------
	virtual void OnSessionLimit(O2SocketSession *ss)
	{
		ss->sbuff = "HTTP/1.1 503 Service Temporary Unavailable\r\n\r\n";
		ss->can_send = true;
		ss->can_recv = false;
		ss->active = true;
		SessionMapLock.Lock();
		sss.insert(O2SocketSessionPMap::value_type(ss->ip, ss));
		SessionMapLock.Unlock();
		TRACEA("------------------------------------------\n");
		TRACEA("HTTP/1.1 503 Service Temporary Unavailable\n");
		TRACEA("------------------------------------------\n");
	}
	virtual void OnServerStop(void)
	{
		SessionCountMap.clear();
		RecvByteMap.clear();
		SendByteMap.clear();
	}
	virtual void ParseRequest(O2SocketSession *ss)
	{
		ThreadParam *param = new ThreadParam;
		param->me = this;
		param->ss = ss;

		HANDLE handle =
			(HANDLE)_beginthreadex(NULL, 0, StaticParseThread, (void*)param, 0, NULL);
		CloseHandle(handle);
	}
	virtual void OnClose(O2SocketSession *ss)
	{
		if (ss->error || ss->sbuff.empty()) {
			AddSessionCountByPath("?", 1);
			AddRecvByteByPath("?", ss->rbuff.size());
			AddSendByteByPath("?", ss->sbuff.size());
		}
		O2Server_HTTP::OnClose(ss);
	}

private:
	// -----------------------------------------------------------------------
	//	Own methods
	// -----------------------------------------------------------------------
	struct ThreadParam {
		O2Server_HTTP_P2P *me;
		O2SocketSession *ss;
	};

	static uint WINAPI StaticParseThread(void *data)
	{
		ThreadParam *param = (ThreadParam*)data;
		O2Server_HTTP_P2P *me = param->me;
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
		string path;

		O2Node node;
		if (!GetNodeInfoFromHeader(*hdr, ss->ip, 0, node)) {
			ss->Lock();
			MakeResponse_400(Profile, ss->sbuff);
			ss->Unlock();
			path = "ERROR";
		}
		else if (node.o2ver() < PROTOCOL_VER) {
			ss->Lock();
			MakeResponse_403(Profile, ss->sbuff);
			ss->Unlock();
			path = "ERROR";
		}
		else {
			if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_DAT) == 0)
				Parse_Dat(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_COLLECTION) == 0)
				Parse_Collection(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_IM) == 0)
				Parse_IM(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_BROADCAST) == 0)
				Parse_Broadcast(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_PROFILE) == 0)
				Parse_Profile(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_KADEMLIA_PING) == 0)
				Parse_Kademlia_PING(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_KADEMLIA_STORE) == 0)
				Parse_Kademlia_STORE(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_KADEMLIA_FINDNODE) == 0)
				Parse_Kademlia_FINDNODE(ss, hdr, node);
			else if (strcmp(hdr->paths[0].c_str(), O2PROTOPATH_KADEMLIA_FINDVALUE) == 0)
				Parse_Kademlia_FINDVALUE(ss, hdr, node);
		}

		if (ss->sbuff.empty() && ss->sbuffoffset == 0) {
			// 404
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
			path = "?";
		}
		else {
			path = hdr->paths[0];
		}

		AddSessionCountByPath(path.c_str(), 1);
		uint64 recvbyte = ss->rbuff.size();
		uint64 sendbyte = ss->sbuff.size();
		AddRecvByteByPath(path.c_str(), recvbyte);
		AddSendByteByPath(path.c_str(), sendbyte);

		ss->SetCanDelete(true);

		// touch
		node.status = O2_NODESTATUS_PASTLINKEDFROM;
		node.connection_n2me = 1;
		node.recvbyte_n2me = recvbyte;
		node.sendbyte_n2me = sendbyte;
		NodeDB->touch(node);
	}

	// -----------------------------------------------------------------------
	//	Parse_Dat
	// -----------------------------------------------------------------------
	void Parse_Dat(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		O2DatPath datpath;
		size_t hokan_byte = 0;
		const char *dat = NULL;
		size_t datsize = 0;

		if (hdr->contentlength) {
			dat = &ss->rbuff[hdr->length];
			datsize = ss->rbuff.size() - hdr->length;
			bool imported = ImportDat(DatIO, Boards, *hdr, dat, datsize,
				Logger, node.ip, node.port, QueryDB, hwndBaloonCallback, msgBaloonCallback);
			if (!imported) {
				ss->Lock();
				MakeResponse_400(Profile, ss->sbuff);
				ss->Unlock();
				return;
			}
		}

		hashT target;
		bool has_target = GetTargetKeyFromHeader(*hdr, target);

		wstring board;
		bool has_board = GetTargetBoardFromHeader(*hdr, board);

		if (has_target) {
			ss->Lock();
			MakeResponse_Dat(&target, NULL, Profile, DatIO, ss->sbuff);
			ss->Unlock();
		}
		else if (has_board) {
			ss->Lock();
			MakeResponse_Dat(NULL, board.c_str(), Profile, DatIO, ss->sbuff);
			ss->Unlock();
		}
		else {
			ss->Lock();
			MakeResponse_Dat(NULL, NULL, Profile, DatIO, ss->sbuff);
			ss->Unlock();
		}
	}
	// -----------------------------------------------------------------------
	//	Parse_Collection
	// -----------------------------------------------------------------------
	void Parse_Collection(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		const char *xml = NULL;
		size_t len = 0;

		ss->Lock();
		MakeResponse_Collection(Profile, Boards, ss->sbuff);
		ss->Unlock();

		if (hdr->contentlength) {
			xml = &ss->rbuff[hdr->length];
			len = ss->rbuff.size() - hdr->length;
			Boards->ImportNodeFromXML(node, xml, len);
		}
	}
	// -----------------------------------------------------------------------
	//	Parse_IM
	// -----------------------------------------------------------------------
	void Parse_IM(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		ss->Lock();
		MakeResponse_200(Profile, ss->sbuff);
		ss->Unlock();

		IMDB->ImportFromXML(NULL, &ss->rbuff[hdr->length], hdr->contentlength);
		IMDB->Save(Profile->GetIMFilePath(), false);

		if (Logger) {
			Logger->AddLog(O2LT_IM, MODULE, node.ip, node.port,
				L"メッセージ受信 (%d byte)", hdr->contentlength);
		}
		if (hwndBaloonCallback && Profile->IsBaloon_IM()) {
			SendMessage(hwndBaloonCallback, msgBaloonCallback,
				(WPARAM)L"o2on", (LPARAM)L"メッセージが届きました");
		}
	}
	// -----------------------------------------------------------------------
	//	Parse_Broadcast
	// -----------------------------------------------------------------------
	void Parse_Broadcast(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		ss->Lock();
		MakeResponse_200(Profile, ss->sbuff);
		ss->Unlock();

		O2IMDB tmpdb(Logger);
		tmpdb.ImportFromXML(NULL, &ss->rbuff[hdr->length], hdr->contentlength);
		O2IMessages msg;
		tmpdb.GetMessages(msg);

		for (O2IMessagesIt it = msg.begin(); it != msg.end(); it++) {
			if (Job_Broadcast->Add(*it)) {
				it->paths.clear();
				BroadcastDB->AddMessage(*it);
			}
		}
	}
	// -----------------------------------------------------------------------
	//	Parse_Profile
	// -----------------------------------------------------------------------
	void Parse_Profile(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
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
	//	Parse_Kademlia_PING
	// -----------------------------------------------------------------------
	void Parse_Kademlia_PING(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		ss->Lock();
		{
			MakeResponse_Kademlia_PING(Profile, ss->sbuff);
			string ip_e;
			ip2e(ss->ip, ip_e);
			ss->sbuff += ip_e;
		}
		ss->Unlock();
	}
	// -----------------------------------------------------------------------
	//	Parse_Kademlia_STORE
	// -----------------------------------------------------------------------
	void Parse_Kademlia_STORE(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		ss->Lock();
		MakeResponse_Kademlia_STORE(Profile, ss->sbuff);
		ss->Unlock();

		strmap::iterator it = hdr->fields.find(X_O2_KEY_CATEGORY);
		if (it == hdr->fields.end())
			KeyDB->ImportFromXML(NULL, &ss->rbuff[hdr->length], hdr->contentlength);
		else if (it->second == "dat")
			KeyDB->ImportFromXML(NULL, &ss->rbuff[hdr->length], hdr->contentlength);
		else if (it->second == "saku")
			SakuKeyDB->ImportFromXML(NULL, &ss->rbuff[hdr->length], hdr->contentlength);
	}
	// -----------------------------------------------------------------------
	//	Parse_Kademlia_FINDNODE
	// -----------------------------------------------------------------------
	void Parse_Kademlia_FINDNODE(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		hashT target;
		if (!GetTargetKeyFromHeader(*hdr, target)) {
			ss->Lock();
			MakeResponse_400(Profile, ss->sbuff);
			ss->Unlock();
			return;
		}

		O2NodeDB::NodeListT neighbors;
		if (NodeDB->neighbors(target, neighbors, true) > 0) {
			string xml;
			O2NodeSelectCondition cond;
			NodeDB->ExportToXML(neighbors, cond, xml);
	
			ss->Lock();
			MakeResponse_Kademlia_FINDNODE(Profile, xml.size(), ss->sbuff);
			ss->sbuff += xml;
			ss->Unlock();
		}
		else {
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
		}
	}
	// -----------------------------------------------------------------------
	//	Parse_Kademlia_FINDVALUE
	// -----------------------------------------------------------------------
	void Parse_Kademlia_FINDVALUE(O2SocketSession *ss, HTTPHeader *hdr, const O2Node &node)
	{
		hashT target;
		if (!GetTargetKeyFromHeader(*hdr, target)) {
			ss->Lock();
			MakeResponse_400(Profile, ss->sbuff);
			ss->Unlock();
			return;
		}

		O2KeyList keylist;
		O2NodeDB::NodeListT neighbors;

		if (KeyDB->GetKeyList(target, keylist) > 0) {
			string xml;
			KeyDB->ExportToXML(keylist, xml);
			ss->Lock();
			MakeResponse_Kademlia_FINDVALUE(Profile, xml.size(), ss->sbuff);
			ss->sbuff += xml;
			ss->Unlock();
		}
		else if (NodeDB->neighbors(target, neighbors, true) > 0) {
			string xml;
			O2NodeSelectCondition cond;
			NodeDB->ExportToXML(neighbors, cond, xml);
			ss->Lock();
			MakeResponse_Kademlia_FINDVALUE(Profile, xml.size(), ss->sbuff);
			ss->sbuff += xml;
			ss->Unlock();
		}
		else {
			ss->Lock();
			MakeResponse_404(Profile, ss->sbuff);
			ss->Unlock();
		}
	}
};
