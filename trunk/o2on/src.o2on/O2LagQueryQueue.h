/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2LagQueryQueue.h
 * description	: 
 *
 */

#pragma once
#include "O2KeyDB.h"
#include "O2Profile.h"
#include "mutex.h"
#include "dataconv.h"
#include "thread.h"




class O2LagQueryQueue
	: public Mutex
{
protected:
	typedef std::map<hashT,O2Key> O2KeyMap;
	typedef std::map<hashT,O2Key>::iterator O2KeyMapIt;

	bool		Active;
	HANDLE		ThreadHandle;
	O2KeyMap	queries;
	O2Logger	*Logger;
	O2Profile	*Profile;
	O2KeyDB		*QueryDB;
	HWND		hwndBaloonCallback;
	UINT		msgBaloonCallback;

public:
	O2LagQueryQueue(O2Logger	*lgr
				  , O2Profile	*prof
				  , O2KeyDB		*qdb)
		: Active(false)
		, ThreadHandle(NULL)
		, Logger(lgr)
		, Profile(prof)
		, QueryDB(qdb)
		, hwndBaloonCallback(NULL)
		, msgBaloonCallback(0)
	{
		start();
	};
	~O2LagQueryQueue() {
		stop();
	};
	void SetBaloonCallbackMsg(HWND hwnd, UINT msg)
	{
		hwndBaloonCallback = hwnd;
		msgBaloonCallback = msg;
	}

public:
	bool add(const O2DatPath &datpath, const uint64 size)
	{
		O2Key querykey;
		querykey.ip = 0;
		querykey.port = 0;
		querykey.size = size;
		datpath.gethash(querykey.hash);
		datpath.geturl(querykey.url);
		datpath.gettitle(querykey.title);
		querykey.enable = true;

		Lock();
		{
			O2KeyMapIt it = queries.find(querykey.hash);
			if (it == queries.end()) {
				queries.insert(O2KeyMap::value_type(querykey.hash, querykey));
			}
			else {
				it->second.date = time(NULL);
			}
		}
		Unlock();

		return true;
	}

	bool remove(const O2DatPath &datpath)
	{
		hashT hash;
		datpath.gethash(hash);

		Lock();
		{
			O2KeyMapIt it = queries.find(hash);
			if (it != queries.end())
				queries.erase(it);
		}
		Unlock();
		return true;
	}

private:
	void start(void)
	{
		if (!ThreadHandle) {
			Active = true;
			ThreadHandle = (HANDLE)_beginthreadex(NULL, 0, StaticThread, (void*)this, 0, NULL);
		}
	}

	void stop(void)
	{
		if (ThreadHandle) {
			Active = false;
			WaitForSingleObject(ThreadHandle, INFINITE);
			CloseHandle(ThreadHandle);
			ThreadHandle = NULL;
		}
	}

	static uint WINAPI StaticThread(void *data)
	{
		O2LagQueryQueue *me = (O2LagQueryQueue*)data;

		CoInitialize(NULL);
		me->Checker();
		CoUninitialize();

		//_endthreadex(0);
		return (0);
	}

	void Checker(void)
	{
		while (Active) {
			Lock();
			for (O2KeyMapIt it = queries.begin(); it != queries.end(); ) {
				O2Key &querykey = it->second;

				TRACE(querykey.title.c_str());
				TRACE(L"\n");

				if (time(NULL) - querykey.date < 5) {
					it++;
					continue;
				}

				//ƒLƒ…[“o˜^‚©‚ç5•bŒo‰ß‚µ‚Ä‚¢‚½‚ç“o˜^
				querykey.date = 0;
				if (QueryDB->AddKey(querykey) == 1) {
					if (Logger) {
						Logger->AddLog(O2LT_INFO, L"LagQueryQueue", 0, 0,
							L"ŒŸõ“o˜^ %s", querykey.url.c_str());
					}

					if (hwndBaloonCallback && Profile->IsBaloon_Query()) {
						SendMessage(hwndBaloonCallback, msgBaloonCallback,
							(WPARAM)L"ŒŸõ“o˜^", (LPARAM)querykey.url.c_str());
					}
					QueryDB->Save(Profile->GetQueryFilePath());
				}
				it = queries.erase(it);
			}
			Unlock();
			Sleep(2500);
		}
	}
};
