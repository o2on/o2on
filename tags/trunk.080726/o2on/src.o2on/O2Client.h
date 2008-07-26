/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Client.h
 * description	: o2on agent class
 *
 */

#pragma once
#include "O2Logger.h"
#include "O2SocketSession.h"




/*
 *	O2Client
 */
class O2Client
{
protected:
	O2Logger	*Logger;

	wstring		ClientName;
	uint64		SessionLimit;
	uint64		SessionPeak;
	uint64		RecvSizeLimit;
	uint64		TotalSessionCount;
	uint64		TotalConnectError;
	uint64		RecvByte;
	uint64		SendByte;
	bool		Active;

	HANDLE		LaunchThreadHandle;
	HANDLE		NetIOThreadHandle;
	HWND		hwndSetIconCallback;
	UINT		msgSetIconCallback;

	O2SocketSessionPList	queue;
	O2SocketSessionPSet		connectss;
	O2SocketSessionPList	sss;

	Mutex		QueueLock;
	Mutex		ConnectSessionLock;
	Mutex		SessionListLock;
	EventObject	QueueExistSignal;
	EventObject	SessionExistSignal;

	struct ConnectThreadParam {
		O2Client *client;
		O2SocketSession *ss;
	};

public:
	O2Client(const wchar_t *name, O2Logger *lgr);
	~O2Client();

	bool	Start(void);
	bool	Stop(void);
	bool	Restart(void);
	bool	IsActive(void);

	uint64	GetSessionLimit(void);
	bool	SetSessionLimit(uint64 limit);
	uint64	GetRecvSizeLimit(void);
	void	SetRecvSizeLimit(uint64 limit);
	uint64	GetSessionPeak(void);
	uint64	GetTotalSessionCount(void);
	uint64	GetRecvByte(void);
	uint64	GetSendByte(void);
	void	ResetCounter(void);
	size_t	GetSessionList(O2SocketSessionPList &out);

	void SetIconCallbackMsg(HWND hwnd, UINT msg);
	void AddRequest(O2SocketSession *ss, bool high_priority = false);

protected:
	virtual void OnClientStart(void) = 0;
	virtual void OnClientStop(void) = 0;
	virtual void OnConnect(O2SocketSession *ss) = 0;
	virtual void OnRecv(O2SocketSession *ss) = 0;
	virtual void OnSend(O2SocketSession *ss) = 0;
	virtual void OnClose(O2SocketSession *ss) = 0;

private:
	static uint WINAPI StaticLaunchThread(void *data);
	static uint WINAPI StaticConnectionThread(void *data);
	static uint WINAPI StaticNetIOThread(void *data);
	void LaunchThread(void);
	void ConnectionThread(O2SocketSession *ss);
	void NetIOThread(void);
	int connect2(SOCKET s, const struct sockaddr *name, int namelen, int timeout);
};
