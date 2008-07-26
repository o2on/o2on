/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Client.cpp
 * description	: o2on agent class implementation
 *
 */

#include <winsock2.h>
#include "O2Client.h"
#include "dataconv.h"
#include "thread.h"




O2Client::
O2Client(const wchar_t *name, O2Logger *lgr)
	: Logger(lgr)
	, ClientName(name)
	, SessionLimit(0x7fffffff)
	, SessionPeak(0)
	, RecvSizeLimit(_UI64_MAX)
	, TotalSessionCount(0)
	, TotalConnectError(0)
	, RecvByte(0)
	, SendByte(0)
	, Active(false)
	, LaunchThreadHandle(NULL)
	, NetIOThreadHandle(NULL)
	, hwndSetIconCallback(NULL)
	, msgSetIconCallback(0)
{
}




O2Client::
~O2Client()
{
}




bool
O2Client::
Start(void)
{
	if (Active) {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, ClientName.c_str(), 0, 0,
				L"起動済のため起動要求を無視");
		}
		return false;
	}

	Active = true;
	QueueExistSignal.Off();
	SessionExistSignal.Off();

	NetIOThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticNetIOThread, this, 0, NULL);
	LaunchThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticLaunchThread, this, 0, NULL);

	OnClientStart();

	if (Logger)
		Logger->AddLog(O2LT_INFO, ClientName.c_str(), 0, 0, L"起動");
	return true;
}




bool
O2Client::
Stop(void)
{
	if (!Active)
		return false;

	Active = false;

	ConnectSessionLock.Lock();
	{
		for (O2SocketSessionPSetIt ssit = connectss.begin(); ssit != connectss.end(); ssit++)
			closesocket((*ssit)->sock);
	}
	ConnectSessionLock.Unlock();

	SessionListLock.Lock();
	{
		for (O2SocketSessionPListIt ssit = sss.begin(); ssit != sss.end(); ssit++)
			closesocket((*ssit)->sock);
	}
	SessionListLock.Unlock();

	QueueExistSignal.On();
	SessionExistSignal.On();

	HANDLE handles[2] = { LaunchThreadHandle, NetIOThreadHandle };
	WaitForMultipleObjects(2, handles, TRUE, INFINITE);
	CloseHandle(LaunchThreadHandle);
	CloseHandle(NetIOThreadHandle);
	LaunchThreadHandle = 0;
	NetIOThreadHandle = 0;

	while (1) {
		ConnectSessionLock.Lock();
		size_t n = connectss.size();
		ConnectSessionLock.Unlock();
		if (n == 0) break;
		Sleep(THREAD_ALIVE_WAIT_MS);
	}

	OnClientStop();

	if (Logger)
		Logger->AddLog(O2LT_INFO, ClientName.c_str(), 0, 0, L"停止");
	return true;
}




bool
O2Client::
Restart(void)
{
	if (Logger)
		Logger->AddLog(O2LT_INFO, ClientName.c_str(), 0, 0, L"再起動...");
	if (!Stop())
		return false;
	if (!Start())
		return false;
	return true;
}




bool
O2Client::
IsActive(void)
{
	return (Active);
}




uint64
O2Client::
GetSessionLimit(void)
{
	return (SessionLimit);
}




bool
O2Client::
SetSessionLimit(uint64 limit)
{
	if (limit <= 0 || limit > 0x7fffffff)
		return false;
	SessionLimit = limit;
	return true;
}




uint64
O2Client::
GetRecvSizeLimit(void)
{
	return (RecvSizeLimit);
}




void
O2Client::
SetRecvSizeLimit(uint64 limit)
{
	RecvSizeLimit = limit;
}




uint64
O2Client::
GetSessionPeak(void)
{
	return (SessionPeak);
}




uint64
O2Client::
GetTotalSessionCount(void)
{
	return(TotalSessionCount);
}




uint64
O2Client::
GetRecvByte(void)
{
	return (RecvByte);
}




uint64
O2Client::
GetSendByte(void)
{
	return (SendByte);
}

	
	
	
void
O2Client::
ResetCounter(void)
{
	TotalSessionCount = 0;
	SendByte = 0;
	RecvByte = 0;
}

	
	

size_t
O2Client::
GetSessionList(O2SocketSessionPList &out)
{
	SessionListLock.Lock();
	{
		for (O2SocketSessionPListIt it = sss.begin(); it != sss.end(); it++) {
			O2SocketSession *ss = new O2SocketSession();
			ss->ip			= (*it)->ip;
			ss->port		= (*it)->port;
			ss->connect_t	= (*it)->connect_t;
			ss->rbuffoffset	= (*it)->rbuff.size();
			ss->sbuffoffset	= (*it)->sbuff.size();
			out.push_back(ss);
		}
	}
	SessionListLock.Unlock();
	return (out.size());
}




void
O2Client::
SetIconCallbackMsg(HWND hwnd, UINT msg)
{
	hwndSetIconCallback = hwnd;
	msgSetIconCallback = msg;
}




// ---------------------------------------------------------------------------
//	AddRequest
// ---------------------------------------------------------------------------
void
O2Client::
AddRequest(O2SocketSession *ss, bool high_priority)
{
	if (!Active || ss->ip == 0 || ss->port == 0 || ss->sbuff.empty()) {
		ss->error = true;
		ss->Finish();
		return;
	}

	//add to queue
	QueueLock.Lock();
	{
		if (high_priority)
			queue.push_front(ss);
		else
			queue.push_back(ss);
	}
	QueueLock.Unlock();
	QueueExistSignal.On();
}





// ---------------------------------------------------------------------------
//	LaunchThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Client::
StaticLaunchThread(void *data)
{
	CoInitialize(NULL); 

	O2Client *me = (O2Client*)data;
	me->LaunchThread();

	CoUninitialize();

	//_endthreadex(0);
	return (0);
}

void
O2Client::
LaunchThread(void)
{
	while (Active) {

		// Signal Off
		QueueLock.Lock();
		{
			if (queue.empty())
				QueueExistSignal.Off();
		}
		QueueLock.Unlock();

		// Wait
		QueueExistSignal.Wait();
		if (!Active) break;

		while (1) {
			O2SocketSession *ss = NULL;
			bool empty = false;
			QueueLock.Lock();
			{
				empty = queue.empty();
				if (!empty) {
					ss = queue.front();
					queue.pop_front();
				}
			}
			QueueLock.Unlock();

			if (empty) break;

			ConnectThreadParam *param = new ConnectThreadParam;
			param->client = this;
			param->ss = ss;

			HANDLE handle =
				(HANDLE)_beginthreadex(NULL, 0, StaticConnectionThread, param, 0, NULL);
			CloseHandle(handle);
		}
	}

	// End
	QueueLock.Lock();
	for (O2SocketSessionPListIt it = queue.begin(); it != queue.end(); it++) {
		(*it)->error = true;
		(*it)->Finish();
	}
	queue.clear();
	QueueLock.Unlock();
}




// ---------------------------------------------------------------------------
//	ConnectionThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Client::
StaticConnectionThread(void *data)
{
	CoInitialize(NULL); 

	ConnectThreadParam *param = (ConnectThreadParam*)data;
	O2Client *client = param->client;
	O2SocketSession *ss = param->ss;
	delete param;

	client->ConnectSessionLock.Lock();
	client->connectss.insert(ss);
	client->ConnectSessionLock.Unlock();

	client->ConnectionThread(ss);

	client->ConnectSessionLock.Lock();
	client->connectss.erase(ss);
	client->ConnectSessionLock.Unlock();

	CoUninitialize();

	//_endthreadex(0);
	return (0);
}

void
O2Client::
ConnectionThread(O2SocketSession *ss)
{
	// IP文字列
	wstring ipstr;
	if (O2DEBUG)
		ulong2ipstr(ss->ip, ipstr);
	else
		ip2e(ss->ip, ipstr);

	// Create Socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		if (Logger) {
			Logger->AddLog(O2LT_NETERR,
				ClientName.c_str(), 0, 0, L"ソケット生成に失敗");
		}
		ss->error = true;
		ss->Finish();
		return;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(ss->port);
	sin.sin_addr.S_un.S_addr = ss->ip;

	// Connect
	if (connect2(sock, (struct sockaddr*)&sin, sizeof(sin), (int)(ss->connect_timeout_s*1000)) != 0) {
		if (Logger) {
			Logger->AddLog(O2LT_NETERR, ClientName.c_str(),
				ss->ip, ss->port, L"connect失敗");
		}
		closesocket(sock);
		TotalConnectError++;
		ss->error = true;
		ss->Finish();
		return;
	}
	if (hwndSetIconCallback) {
		PostMessage(hwndSetIconCallback, msgSetIconCallback, 1, 0);
	}
	ss->sock = sock;
	ss->SetConnectTime();
	ss->UpdateTimer();
	ss->Activate();

	TotalSessionCount++;
	OnConnect(ss);

	// push to session list
	SessionListLock.Lock();
	{
		sss.push_back(ss);
		if (sss.size() > SessionPeak)
			SessionPeak = sss.size();
	}
	SessionListLock.Unlock();

	SessionExistSignal.On();

	if (Logger) {
		Logger->AddLog(O2LT_NET, ClientName.c_str(),
			ss->ip, ss->port, L"connect");
	}
}




// ---------------------------------------------------------------------------
//	NetIOThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Client::
StaticNetIOThread(void *data)
{
	CoInitialize(NULL); 

	O2Client *me = (O2Client*)data;
	me->NetIOThread();

	CoUninitialize();

	//_endthreadex(0);
	return (0);
}

void
O2Client::
NetIOThread(void)
{
	O2SocketSessionPListIt ssit;
	int lasterror;

	while (Active) {

		// Signal Off
		SessionListLock.Lock();
		{
			if (sss.empty())
				SessionExistSignal.Off();
		}
		SessionListLock.Unlock();

		// Wait
		SessionExistSignal.Wait();
		if (!Active) break;

		// Setup FD
		fd_set readfds;
		fd_set writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		SessionListLock.Lock();
		{
			for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
				O2SocketSession *ss = *ssit;
				if (ss->can_send)
					FD_SET(ss->sock, &writefds);
				if (ss->can_recv)
					FD_SET(ss->sock, &readfds);
			}
		}
		SessionListLock.Unlock();

		// select
		struct timeval tv;
		tv.tv_sec = SELECT_TIMEOUT_MS/1000;
		tv.tv_usec = SELECT_TIMEOUT_MS%1000;
		select(0, &readfds, &writefds, NULL, &tv);
		if (!Active) break;

		//
		//	既存セッションとの送受信
		//
		SessionListLock.Lock();
		ssit = sss.begin();
		SessionListLock.Unlock();

		while (Active) {
			bool end = false;
			SessionListLock.Lock();
			{
				if (ssit == sss.end())
					end = true;
			}
			SessionListLock.Unlock();
			if (end) break;

			O2SocketSession *ss = *ssit;

			// IP文字列
			wstring ipstr;
			if (O2DEBUG)
				ulong2ipstr(ss->ip, ipstr);
			else
				ip2e(ss->ip, ipstr);

			// Send
			if (FD_ISSET(ss->sock, &writefds)) {
				int len;
				const char *buff = ss->GetNextSend(len);
				if (len > 0) {
					int n = send(ss->sock, buff, len, 0);
					if (n > 0) {
						ss->UpdateSend(n);
						ss->UpdateTimer();

						SendByte += n;
						OnSend(ss);

						if (hwndSetIconCallback) {
							PostMessage(hwndSetIconCallback, msgSetIconCallback, 1, 0);
						}
					}
					else if (n == 0) {
						;
					}
					else if ((lasterror = WSAGetLastError()) != WSAEWOULDBLOCK) {
						if (Logger) {
							Logger->AddLog(O2LT_NETERR, ClientName.c_str(),
								ss->ip, ss->port, L"送信エラー(%d)", lasterror);
						}
						ss->error = true;
						ss->Deactivate();
					}
				}
			}

			// Recv
			if (FD_ISSET(ss->sock, &readfds)) {
				char buff[RECVBUFFSIZE];
				int n = recv(ss->sock, buff, RECVBUFFSIZE, 0);
				if (n > 0) {
					ss->AppendRecv(buff, n);
					ss->UpdateTimer();

					RecvByte += n;
					OnRecv(ss);

					if (hwndSetIconCallback) {
						PostMessage(hwndSetIconCallback, msgSetIconCallback, 0, 0);
					}
				}
				else if (n == 0) {
					/*if (Logger) {
						Logger->AddLog(O2LT_NETERR, ClientName.c_str(),
							ss->ip, ss->port, L"受信0");
					}*/
					ss->Deactivate();
				}
				else if ((lasterror = WSAGetLastError()) != WSAEWOULDBLOCK) {
					if (Logger) {
						Logger->AddLog(O2LT_NETERR, ClientName.c_str(),
							ss->ip, ss->port, L"受信エラー(%d)", lasterror);
					}
					ss->error = true;
					ss->Deactivate();
				}
			}

			// Delete
			if (ss->CanDelete()) {
				bool timeout = ss->GetPastTime() >= ss->timeout_s ? true : false;
				if (!ss->IsActive() || timeout) {
					if (timeout) {
						if (Logger) {
							Logger->AddLog(O2LT_NETERR, ClientName.c_str(),
								ss->ip, ss->port, L"timeout");
						}
					}

					closesocket(ss->sock);
					ss->sock = 0;

					SessionListLock.Lock();
					ssit = sss.erase(ssit);
					SessionListLock.Unlock();

					OnClose(ss);
					continue;
				}
			}

			Sleep(1);

			SessionListLock.Lock();
			ssit++;
			SessionListLock.Unlock();
		}
	}

	// End
	SessionListLock.Lock();
	for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
		O2SocketSession *ss = *ssit;
		closesocket(ss->sock);
		ss->sock = 0;
		OnClose(ss);
	}
	sss.clear();
	SessionListLock.Unlock();
}




int
O2Client::
connect2(SOCKET s, const struct sockaddr *name, int namelen, int timeout)
{
	bool err = false;

	WSAEVENT event = WSACreateEvent();
	if (event == WSA_INVALID_EVENT)
		err = true;

	if (!err && WSAEventSelect(s, event, FD_CONNECT) == SOCKET_ERROR)
		err = true;

	if (!err && connect(s, name, namelen) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			err = true;
		//非ブロッキングでconnectした場合、通常はSOCKET_ERRORになり
		//WSAGetLastError() == WSAEWOULDBLOCKが返ってくる。
		//それ以外のエラーは本当にconnect失敗
	}
	else
		err = true;

	if (!err && WSAWaitForMultipleEvents(1, &event, FALSE, timeout, FALSE) != WSA_WAIT_EVENT_0)
		err = true;

	WSANETWORKEVENTS events;
	if (!err && WSAEnumNetworkEvents(s, event, &events) == SOCKET_ERROR)
		err = true;
	if (!err && (!(events.lNetworkEvents & FD_CONNECT) || events.iErrorCode[FD_CONNECT_BIT] != 0))
		err = true;

	WSAEventSelect(s, NULL, 0);
	WSACloseEvent(event);

	/* back to blocking mode
	ulong argp = 0;
	ioctlsocket(s, FIONBIO, &argp);
	*/

	return (err ? SOCKET_ERROR : 0);
}

