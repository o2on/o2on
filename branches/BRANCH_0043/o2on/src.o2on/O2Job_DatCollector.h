/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_DatCollector.h
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
#include "O2Boards.h"
#include "O2Client.h"
#include "O2Protocol_Dat.h"
#include "dataconv.h"
#include "../cryptopp/osrng.h"




class O2Job_DatCollector
	: public O2Job
	, public O2Protocol_Dat
{
protected:
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;
	O2KeyDB		*KeyDB;
	O2KeyDB		*QueryDB;
	O2DatIO		*DatIO;
	O2Boards	*Boards;
	O2Client	*Client;
	HWND		hwndBaloonCallback;
	UINT		msgBaloonCallback;

public:
	O2Job_DatCollector(const wchar_t	*name
					 , time_t			interval
					 , bool				startup
					 , O2Logger			*lgr
					 , O2Profile		*prof
					 , O2NodeDB			*ndb
					 , O2KeyDB			*kdb
					 , O2KeyDB			*qdb
					 , O2DatIO			*dio
					 , O2Boards			*boards
					 , O2Client			*client)
		: O2Job(name, interval, startup)
		, Logger(lgr)
		, Profile(prof)
		, NodeDB(ndb)
		, KeyDB(kdb)
		, QueryDB(qdb)
		, DatIO(dio)
		, Boards(boards)
		, Client(client)
		, hwndBaloonCallback(NULL)
		, msgBaloonCallback(0)
	{
	}

	~O2Job_DatCollector()
	{
	}

	void SetBaloonCallbackMsg(HWND hwnd, UINT msg)
	{
		hwndBaloonCallback = hwnd;
		msgBaloonCallback = msg;
	}

	void JobThreadFunc(void)
	{
		// 取得する板を決める
		wstrarray boards;
		if (Boards->GetExList(boards) == 0)
			return;
		CryptoPP::AutoSeededRandomPool rng;
		wstring &board = boards[rng.GenerateWord32(0, boards.size()-1)];

		// ノード一覧を取得
		O2NodeDB::NodeListT nodes;
		if (Boards->GetExNodeList(board.c_str(), nodes) == 0)
			return;

		bool imported = false;

		O2NodeDB::NodeListT::iterator it;
		for (it = nodes.begin(); it != nodes.end() && !imported && IsActive(); it++) {
			it->lastlink = 0;
			it->reset();

			// リクエスト発行
			O2SocketSession ss;
			ss.ip = it->ip;
			ss.port = it->port;
			MakeRequest_Dat(NULL, board.c_str(), &ss, Profile, DatIO, ss.sbuff);

			Client->AddRequest(&ss);
			ss.Wait();

			HTTPHeader *header = (HTTPHeader*)ss.data;
			if (CheckResponse(&ss, header, NodeDB, *it)) {
				// 本文のdat取り込み
				const char *rawdata = &ss.rbuff[header->length];
				uint64 datsize = ss.rbuff.size() - header->length;
				imported = ImportDat(DatIO, Profile, Boards, *header, rawdata, datsize,
					Logger, ss.ip, ss.port, QueryDB, hwndBaloonCallback, msgBaloonCallback);
				//1個成功したら終わり
			}
			else if (header && header->status != 200) {
				Boards->RemoveExNode(board.c_str(), *it);
			}

			if (header) delete header;

			Sleep(IsActive() ? DAT_COLLECTOR_INTERVAL_MS : 0);
		}

		O2NodeDB::NodeListT::iterator endit = it;
		for (it = nodes.begin(); it != endit && IsActive(); it++) {
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
