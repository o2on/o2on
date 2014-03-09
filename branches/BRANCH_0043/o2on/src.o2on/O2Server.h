/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server.h
 * description	: o2on server class
 *
 */

//注
//このヘッダをincludeする際は、手前にwinsock2.hのincludeを記述すること

#pragma once
#include "O2Logger.h"
#include "O2IPFilter.h"
#include "O2SocketSession.h"




//
// O2Server
//
class O2Server
{
protected:
	O2Logger	*Logger;
	O2IPFilter	*IPFilter;

	wstring		ServerName;
	ushort		ServerPort;
	SOCKET		ServerSocket;
	uint64		SessionLimit;
	uint64		SessionPeak;
	uint64		RecvSizeLimit;
	bool		RejectMultiLink;
	uint64		TotalSessionCount;
	uint64		TotalSessionLimitOver;
	uint64		TotalMultiLinkReject;
	uint64		TotalIPFilterReject;
	uint64		RecvByte;
	uint64		SendByte;
	time_t		LastAcceptTime;
	bool		Active;

	HANDLE		ListenThreadHandle;
	HANDLE		NetIOThreadHandle;
	uint64		IPFilteringThreadCount;
	HWND		hwndSetIconCallback;
	UINT		msgSetIconCallback;

   	O2SocketSessionPMap	sss;
	Mutex		SessionMapLock;
	EventObject	SessionExistSignal;
	Mutex		IPFilteringThreadCountLock;

	struct IPFilteringThreadParam {
		O2Server *server;
		O2SocketSession *ss;
	};

public:
	O2Server(const wchar_t *name, O2Logger *lgr, O2IPFilter *ipf);
	~O2Server();

	bool	Start(void);
	bool	Stop(void);
	bool	Restart(void);
	bool	IsActive(void);

	wchar_t	*GetName(void);
	bool	SetPort(ushort pn);
	void	SetMultiLinkRejection(bool f);
	bool	IsMultiLinkReject(void);
	uint64	GetSessionLimit(void);
	bool	SetSessionLimit(uint64 limit);
	uint64	GetRecvSizeLimit(void);
	void	SetRecvSizeLimit(uint64 limit);
	uint64	GetSessionPeak(void);
	uint64	GetTotalSessionCount(void);
	uint64	GetRecvByte(void);
	uint64	GetSendByte(void);
	void	ResetCounter(void);
	time_t	GetLastAcceptTime(void);
	size_t	GetSessionList(O2SocketSessionPList &out);

	void	SetIconCallbackMsg(HWND hwnd, UINT msg);

protected:
	virtual void OnServerStart(void) = 0;
	virtual void OnServerStop(void) = 0;
	virtual void OnSessionLimit(O2SocketSession *ss) = 0;
	virtual void OnAccept(O2SocketSession *ss) = 0;
	virtual void OnRecv(O2SocketSession *ss) = 0;
	virtual void OnSend(O2SocketSession *ss) = 0;
	virtual void OnClose(O2SocketSession *ss) = 0;

private:
	bool Bind(void);
	static uint WINAPI StaticListenThread(void *data);
	void ListenThread(void);
	static uint WINAPI StaticNetIOThread(void *data);
	void NetIOThread(void);
	static uint WINAPI StaticIPFilteringThread(void *data);
	void IPFilteringThread(O2SocketSession *ss);
};
