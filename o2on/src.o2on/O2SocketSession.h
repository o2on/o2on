/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2SocketSession.h
 * description	: client session
 *
 */

#pragma once
#include "typedef.h"
#include "O2Define.h"
#include "O2Node.h"
#include "mutex.h"
#include "event.h"
#include <time.h>
#include <set>
#include <map>
#include <list>




//
// O2SocketSession
//
struct O2SocketSession
	: public Mutex
	, public EventObject
{
	SOCKET		sock;
	time_t		connect_timeout_s;
	time_t		timeout_s;
	ulong		ip;
	ushort		port;
	void		*data;
	bool		active;
	bool		error;
	bool		can_recv;
	bool		can_send;
	bool		can_delete;
	size_t		rbuffoffset;
	size_t		sbuffoffset;
	string		rbuff;
	string		sbuff;
	time_t		connect_t;
	time_t		t;
	O2Node		node;

	O2SocketSession(time_t conn_timeout_s = CONNECT_TIMEOUT_S, time_t timeout = SOCKET_TIMEOUT_S)
		: sock(INVALID_SOCKET)
		, connect_timeout_s(conn_timeout_s)
		, timeout_s(timeout)
		, ip(0)
		, port(0)
		, data(NULL)
		, active(false)
		, error(false)
		, can_recv(false)
		, can_send(false)
		, can_delete(true)
		, rbuffoffset(0)
		, sbuffoffset(0)
		, connect_t(0)
		, t(0)
	{
	}

	void AppendRecv(const char *buff, uint len)
	{
		Lock();
		rbuffoffset = rbuff.size();
		rbuff.append(buff, len);
		Unlock();
	}

	void UpdateSend(uint len)
	{
		Lock();
		sbuffoffset += len;
		Unlock();
	}

	const char *GetNextSend(int &len)
	{
		const char *p = NULL;

		if (!LockTest()) {
			len = 0;
			return (NULL);
		}
		if (sbuffoffset >= sbuff.size()) {
			len = 0;
		}
		else {
			len = sbuff.size() - sbuffoffset;
			p  = &sbuff[sbuffoffset];
		}
		Unlock();
		return (p);
	}

	void Activate(void)
	{
		Lock();
		active = true;
		Unlock();
	}

	void Deactivate(void)
	{
		Lock();
		active = false;
		Unlock();
	}

	bool IsActive(void)
	{
		return (active);
	}

	void SetCanDelete(bool f)
	{
		Lock();
		can_delete = f;
		Unlock();
	}

	bool CanDelete(void)
	{
		if (!LockTest())
			return false;
		bool f = can_delete;
		Unlock();
		return (f);
	}

	void SetConnectTime(void)
	{
		connect_t = time(NULL);
	}

	void UpdateTimer(void)
	{
		Lock();
		t = time(NULL);
		Unlock();
	}

	time_t GetPastTime(void)
	{
		Lock();
		time_t pasttime = time(NULL) - t;
		Unlock();
		return (pasttime);
	}

	void Finish(void)
	{
		On(); //signaled
	}
};

typedef std::multimap<ulong,O2SocketSession*> O2SocketSessionPMap;
typedef std::multimap<ulong,O2SocketSession*>::iterator O2SocketSessionPMapIt;

typedef std::list<O2SocketSession*> O2SocketSessionPList;
typedef std::list<O2SocketSession*>::iterator O2SocketSessionPListIt;

typedef std::set<O2SocketSession*> O2SocketSessionPSet;
typedef std::set<O2SocketSession*>::iterator O2SocketSessionPSetIt;
