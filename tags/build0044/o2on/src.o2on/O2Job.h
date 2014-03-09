/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job.h
 * description	: 
 *
 */

#pragma once
#include "O2Define.h"
#include "O2JobSchedule.h"
#include "O2Protocol.h"
#include "mutex.h"
#include <time.h>




class O2Job
	: public O2Protocol
{
protected:
	wstring	Name;
	bool	Active;
	bool	Working;
	time_t	Interval;
	bool	RunStartup;
	time_t	LastTime;
	uint64	TotalSessionCount;
	uint64	RecvByte;
	uint64	SendByte;
	Mutex	Lock_;

protected:
	bool CheckResponse(const O2SocketSession *ss
					 , const HTTPHeader *header
					 , O2NodeDB *nodedb
					 , O2Node &node)
	{
		if (ss->error)
			return false;

		if (ss->rbuff.empty())
			return false;

		if (!header)
			return false;

		if (header->status == 503) {
			node.lastlink = time(NULL);
			return false;
		}

		hashT org_id = node.id;

		if (!GetNodeInfoFromHeader(*header, ss->ip, ss->port, node))
			return false;

		TotalSessionCount++;
		RecvByte += ss->rbuff.size();
		SendByte += ss->sbuff.size();

		node.status = O2_NODESTATUS_PASTLINKEDTO;
		node.connection_me2n = 1;
		node.recvbyte_me2n = ss->rbuff.size();
		node.sendbyte_me2n = ss->sbuff.size();
		node.lastlink = time(NULL);

		if (nodedb && org_id != node.id) {
			O2Node tmpnode;
			tmpnode.id = org_id;
			nodedb->remove(tmpnode);
		}

		if (header->status != 200)
			return false;

		if (ss->rbuff.size() - header->length < header->contentlength)
			return false;

		return true;
	}

public:
	O2Job(const wchar_t *name, time_t interval, bool startup)
		: Name(name)
		, Active(false)
		, Working(false)
		, Interval(interval)
		, RunStartup(startup)
		, LastTime(0)
		, TotalSessionCount(0)
		, RecvByte(0)
		, SendByte(0)
	{
	}
	wchar_t *GetName(void)
	{
		return (&Name[0]);
	}
	void SetActive(bool f)
	{
		Lock_.Lock();
		Active = f;
		Lock_.Unlock();
	}
	bool IsActive(void)
	{
		Lock_.Lock();
		bool f = Active;
		Lock_.Unlock();
		return (f);
	}
	void SetWorking(bool f)
	{
		Lock_.Lock();
		Working = f;
		Lock_.Unlock();
	}
	bool IsWorking(void)
	{
		Lock_.Lock();
		bool f = Working;
		Lock_.Unlock();
		return (f);
	}
	time_t GetInterval(void)
	{
		return (Interval);
	}
	bool IsRunStartup(void)
	{
		return (RunStartup);
	}
	time_t GetLastTime(void)
	{
		return LastTime;
	}
	void SetLastTime(time_t t)
	{
		LastTime = t;
	}
	time_t GetRemain(void)
	{
		if (time(NULL) - LastTime >= Interval)
			return (0);
		return (Interval - (time(NULL) - LastTime));
	}
	void DoNow(void)
	{
		LastTime = 0;
	}
	virtual void JobThreadFunc(void) = 0;

public:
	uint64 GetTotalSessionCount(void)
	{
		return(TotalSessionCount);
	}
	uint64 GetRecvByte(void)
	{
		return (RecvByte);
	}
	uint64 GetSendByte(void)
	{
		return (SendByte);
	}
	void ResetCounter(void)
	{
		TotalSessionCount = 0;
		SendByte = 0;
		RecvByte = 0;
	}
};
