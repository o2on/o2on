/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server.cpp
 * description	: o2on server class implementation
 *
 */

#include <winsock2.h>
#include "O2Server.h"
#include "dataconv.h"
#include "thread.h"




O2Server::
O2Server(const wchar_t *name, O2Logger *lgr, O2IPFilter *ipf)
	: Logger(lgr)
	, IPFilter(ipf)
	, ServerName(name)
	, ServerPort(0)
	, ServerSocket(INVALID_SOCKET)
	, SessionLimit(0x7fffffff)
	, SessionPeak(0)
	, RecvSizeLimit(_UI64_MAX)
	, RejectMultiLink(false)
	, TotalSessionCount(0)
	, TotalSessionLimitOver(0)
	, TotalMultiLinkReject(0)
	, TotalIPFilterReject(0)
	, RecvByte(0)
	, SendByte(0)
	, LastAcceptTime(0)
	, Active(false)
	, ListenThreadHandle(NULL)
	, NetIOThreadHandle(NULL)
	, IPFilteringThreadCount(0)
	, hwndSetIconCallback(NULL)
	, msgSetIconCallback(0)
{
}




O2Server::
~O2Server()
{
}




bool
O2Server::
Start(void)
{
	if (Active) {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, ServerName.c_str(), 0, 0,
				L"起動済のため起動要求を無視");
		}
		return true;
	}

	if (!Bind()) {
		return false;
	}

	Active = true;
	SessionExistSignal.Off();

	NetIOThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticNetIOThread, this, 0, NULL);
	ListenThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticListenThread, this, 0, NULL);

	OnServerStart();

	if (Logger) {
		Logger->AddLog(O2LT_INFO,
			ServerName.c_str(), 0, 0, L"起動 (port:%d)", ServerPort);
	}

	return true;
}




bool
O2Server::
Stop(void)
{
	if (!Active)
		return false;

	Active = false;

	SessionMapLock.Lock();
	{
		for (O2SocketSessionPMapIt ssit = sss.begin(); ssit != sss.end(); ssit++) {
			closesocket(ssit->second->sock);
		}
	}
	SessionMapLock.Unlock();

	SessionExistSignal.On();

	HANDLE handles[2] = { ListenThreadHandle, NetIOThreadHandle };
	WaitForMultipleObjects(2, handles, TRUE, INFINITE);
	CloseHandle(ListenThreadHandle);
	CloseHandle(NetIOThreadHandle);
	ListenThreadHandle = NULL;
	NetIOThreadHandle = NULL;

	while (1) {
		IPFilteringThreadCountLock.Lock();
		uint64 n = IPFilteringThreadCount;
		IPFilteringThreadCountLock.Unlock();
		if (n == 0) break;
		Sleep(THREAD_ALIVE_WAIT_MS);
	}

	OnServerStop();

	if (Logger) {
		Logger->AddLog(O2LT_INFO,
			ServerName.c_str(), 0, 0, L"停止 (port:%d)", ServerPort);
	}
	return true;
}




bool
O2Server::
Restart(void)
{
	if (Logger)
		Logger->AddLog(O2LT_INFO, ServerName.c_str(), 0, 0, L"再起動...");
	if (!Stop())
		return false;
	if (!Start())
		return false;
	return true;
}




bool
O2Server::
IsActive(void)
{
	return (Active);
}




wchar_t *
O2Server::
GetName(void)
{
	return (&ServerName[0]);
}




bool
O2Server::
SetPort(ushort pn)
{
	if (Active)
		return false;
	ServerPort = pn;
	return true;
}




void
O2Server::
SetMultiLinkRejection(bool f)
{
	RejectMultiLink = f;
}




bool
O2Server::
IsMultiLinkReject(void)
{
	return (RejectMultiLink);
}

	
	
	
uint64
O2Server::
GetSessionLimit(void)
{
	return (SessionLimit);
}




bool
O2Server::
SetSessionLimit(uint64 limit)
{
	if (limit <= 0 || limit > 0x7fffffff)
		return false;
	SessionLimit = limit;
	return true;
}




uint64
O2Server::
GetRecvSizeLimit(void)
{
	return (RecvSizeLimit);
}




void
O2Server::
SetRecvSizeLimit(uint64 limit)
{
	RecvSizeLimit = limit;
}




uint64
O2Server::
GetSessionPeak(void)
{
	return (SessionPeak);
}




uint64
O2Server::
GetTotalSessionCount(void)
{
	return (TotalSessionCount);
}




uint64
O2Server::
GetRecvByte(void)
{
	return (RecvByte);
}




uint64
O2Server::
GetSendByte(void)
{
	return (SendByte);
}

	
	
	
void
O2Server::
ResetCounter(void)
{
	LastAcceptTime = false;
	TotalSessionCount = 0;
	SendByte = 0;
	RecvByte = 0;
}

	
	
	
time_t
O2Server::
GetLastAcceptTime(void)
{
	return (LastAcceptTime);
}

	
	

size_t
O2Server::
GetSessionList(O2SocketSessionPList &out)
{
	SessionMapLock.Lock();
	{
		for (O2SocketSessionPMapIt it = sss.begin(); it != sss.end(); it++) {
			O2SocketSession *ss = new O2SocketSession();
			ss->ip			= it->second->ip;
			ss->port		= it->second->port;
			ss->connect_t	= it->second->connect_t;
			ss->rbuffoffset	= it->second->rbuff.size();
			ss->sbuffoffset	= it->second->sbuff.size();
			it->second->Lock();
			ss->rbuff = it->second->rbuff.substr(0, 256);
			it->second->Unlock();
			out.push_back(ss);
		}
	}
	SessionMapLock.Unlock();
	return (out.size());
}




void
O2Server::
SetIconCallbackMsg(HWND hwnd, UINT msg)
{
	hwndSetIconCallback = hwnd;
	msgSetIconCallback = msg;
}




// ---------------------------------------------------------------------------
//	Bind
//
// ---------------------------------------------------------------------------
bool
O2Server::
Bind(void)
{
	if (ServerPort == 0) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, ServerName.c_str(), 0, 0, L"ポートが0");
		}
		return false;
	}

	ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (ServerSocket == INVALID_SOCKET) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, ServerName.c_str(), 0, 0, L"socket生成失敗");
		}
		return false;
	}

	int one = 1;
	// SO_REUSEADDR: socketがTIME_WAITでもbindできるようにする ←うそです
	// ※Winsockの場合LISTENINGでも上書きbindされてしまう
	// ※SO_REUSEADDRをセットしなくてもTIME_WAITならbindできる
//	setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(int));
    setsockopt(ServerSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&one, sizeof(int));
    setsockopt(ServerSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(int));

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(ServerPort);

	if (bind(ServerSocket, (struct sockaddr*)(&sin), sizeof(sockaddr_in)) != 0) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, ServerName.c_str(), 0, 0, 
				L"bind失敗 (port:%d)", ServerPort);
		}
		closesocket(ServerSocket);
		ServerSocket = INVALID_SOCKET;
		return false;
	}

	if (listen(ServerSocket, (int)SessionLimit) != 0) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, ServerName.c_str(), 0, 0,
				L"listen失敗 (port:%d)", ServerPort);
		}
		closesocket(ServerSocket);
		ServerSocket = INVALID_SOCKET;
		return false;
	}

	return true;
}




// ---------------------------------------------------------------------------
//	ListenThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Server::
StaticListenThread(void *data)
{
	CoInitialize(NULL); 

	O2Server *me = (O2Server*)data;
	me->ListenThread();

	CoUninitialize();

	_endthreadex(0);
	return (0);
}

void
O2Server::
ListenThread(void)
{
	// -----------------------------------------------------------------------
	//	待ち受けLoop
	// -----------------------------------------------------------------------
	while (Active) {
		// FD
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(ServerSocket, &readfds);

		// select
		struct timeval tv;
		tv.tv_sec = SELECT_TIMEOUT_MS/1000;
		tv.tv_usec = SELECT_TIMEOUT_MS%1000;
		select(0, &readfds, NULL, NULL, &tv);
		if (!Active) break;

		// 接続きた？
		if (!FD_ISSET(ServerSocket, &readfds))
			continue;

		// accept
		sockaddr_in sin;
		int len = sizeof(sockaddr_in);
		SOCKET sock = accept(ServerSocket, (struct sockaddr*)&sin, &len);
		if (sock == INVALID_SOCKET) {
			if (Logger) {
				Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
						0, 0, L"accept error");
			}
			continue;
		}
		LastAcceptTime = time(NULL);
		TotalSessionCount++;

		ulong ip = sin.sin_addr.S_un.S_addr;
		ushort port = htons(sin.sin_port);

		// to non-blocking mode
		unsigned long argp = 1;
		ioctlsocket(sock, FIONBIO, &argp);

		wstring ipstr;
		if (O2DEBUG)
			ulong2ipstr(ip, ipstr);
		else
			ip2e(ip, ipstr);

		//
		O2SocketSession *ss = new O2SocketSession();
		ss->sock = sock;
		ss->ip = ip;
		ss->port = port;
		ss->SetConnectTime();

		SessionMapLock.Lock();
		uint64 session_count = sss.size();
		bool session_exist = sss.find(ip) != sss.end() ? true : false;
		SessionMapLock.Unlock();

		// セッション数確認
		if (session_count >= SessionLimit) {
			// セッション数オーバー
			if (Logger) {
				Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
						ip, port, L"Reject(session limit)");
			}
			TotalSessionLimitOver++;
			OnSessionLimit(ss);
			continue;
		}

		// 複数接続確認
		if (RejectMultiLink && session_exist) {
			if (Logger) {
				Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
					ip, port, L"Reject(multi link) %s:%d");
			}
			TotalMultiLinkReject++;
			closesocket(ss->sock);
			delete ss;
			continue;
		}

		if (!Active) {
			closesocket(ss->sock);
			delete ss;
			break;
		}

		// lookup & IP Filtering thread
		IPFilteringThreadParam *param = new IPFilteringThreadParam;
		param->server = this;
		param->ss = ss;
		HANDLE handle = (HANDLE)_beginthreadex(NULL, 0, StaticIPFilteringThread, param, 0, NULL);
		CloseHandle(handle);
	}

	closesocket(ServerSocket);
	ServerSocket = INVALID_SOCKET;
}




// ---------------------------------------------------------------------------
//	NetIOThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Server::
StaticNetIOThread(void *data)
{
	CoInitialize(NULL); 

	O2Server *me = (O2Server*)data;
	me->NetIOThread();

	CoUninitialize();

	_endthreadex(0);
	return (0);
}

void
O2Server::
NetIOThread(void)
{
	O2SocketSessionPMapIt ssit;
	int lasterror;

	while (Active) {
		// Signal Off
		SessionMapLock.Lock();
		{
			if (sss.empty())
				SessionExistSignal.Off();
		}
		SessionMapLock.Unlock();

		// Wait
		SessionExistSignal.Wait();
		if (!Active) break;

		// Setup FD
		fd_set readfds;
		fd_set writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		SessionMapLock.Lock();
		{
			for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
				O2SocketSession *ss = ssit->second;
				if (ss->can_recv)
					FD_SET(ss->sock, &readfds);
				if (ss->can_send)
					FD_SET(ss->sock, &writefds);
			}
		}
		SessionMapLock.Unlock();

		// select
		struct timeval tv;
		tv.tv_sec = SELECT_TIMEOUT_MS/1000;
		tv.tv_usec = SELECT_TIMEOUT_MS%1000;
		select(0, &readfds, &writefds, NULL, &tv);
		if (!Active) break;

		//
		//	既存セッションとの送受信
		//
		SessionMapLock.Lock();
		ssit = sss.begin();
		SessionMapLock.Unlock();

		while (Active) {
			bool end = false;
			SessionMapLock.Lock();
			{
				if (ssit == sss.end())
					end = true;
			}
			SessionMapLock.Unlock();
			if (end) break;

			O2SocketSession *ss = ssit->second;

			// IP文字列
			wstring ipstr;
			if (O2DEBUG)
				ulong2ipstr(ss->ip, ipstr);
			else
				ip2e(ss->ip, ipstr);

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
					if (Logger) {
						Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
							ss->ip, ss->port, L"受信0");
					}
					ss->Deactivate();
				}
				else if ((lasterror = WSAGetLastError()) != WSAEWOULDBLOCK) {
					if (Logger) {
						Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
							ss->ip, ss->port, L"受信エラー(%d)", lasterror);
					}
					ss->error = true;
					ss->Deactivate();
				}
			}

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
							Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
								ss->ip, ss->port, L"送信エラー(%d)", lasterror);
						}
						ss->error = true;
						ss->Deactivate();
					}
				}
				else
					OnSend(ss);
			}

			// Delete
			if (ss->CanDelete()) {
				bool timeout = ss->GetPastTime() >= ss->timeout_s ? true : false;
				if (!ss->IsActive() || timeout) {
					if (timeout) {
						if (Logger) {
							Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
								ss->ip, ss->port, L"timeout");
						}
					}

					closesocket(ss->sock);
					ss->sock = 0;

					SessionMapLock.Lock();
					sss.erase(ssit++);
					SessionMapLock.Unlock();

					OnClose(ss);
					continue;
				}
			}

			Sleep(1);

			SessionMapLock.Lock();
			ssit++;
			SessionMapLock.Unlock();
		}
	}

	// End
	SessionMapLock.Lock();
	for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
		O2SocketSession *ss = ssit->second;
		closesocket(ss->sock);
		ss->sock = 0;

		while (!ss->CanDelete())
			Sleep(THREAD_ALIVE_WAIT_MS);

		OnClose(ss);
	}
	sss.clear();
	SessionMapLock.Unlock();
}




// ---------------------------------------------------------------------------
//	IPFilteringThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Server::
StaticIPFilteringThread(void *data)
{
	CoInitialize(NULL); 


	IPFilteringThreadParam *param = (IPFilteringThreadParam*)data;
	O2Server *server = param->server;
	O2SocketSession *ss = param->ss;
	delete param;

	server->IPFilteringThreadCountLock.Lock();
	server->IPFilteringThreadCount++;
	server->IPFilteringThreadCountLock.Unlock();

	server->IPFilteringThread(ss);

	server->IPFilteringThreadCountLock.Lock();
	server->IPFilteringThreadCount--;
	server->IPFilteringThreadCountLock.Unlock();

	CoUninitialize();

	_endthreadex(0);
	return (0);
}

void
O2Server::
IPFilteringThread(O2SocketSession *ss)
{
	wstrarray hostnames;
	wstring hostname;
	wstring ipstr;

#if !defined(_DEBUG)
	// Lookup
	if (is_globalIP(ss->ip)) {
		HOSTENT *hostent = gethostbyaddr((char*)&ss->ip, sizeof(ulong), AF_INET);
		if (hostent) {
			ascii2unicode(hostent->h_name, strlen(hostent->h_name), hostname);
			hostname = L"." + hostname;
			hostnames.push_back(hostname);

			//TRACEW(L" >");TRACEW(hostname.c_str());TRACEW(L"<\n");

			for (uint i = 0; hostent->h_aliases[i]; i++) {
				ascii2unicode(hostent->h_aliases[i], strlen(hostent->h_aliases[i]), hostname);
				hostname = L"." + hostname;
				hostnames.push_back(hostname);

				//TRACEW(L" >");TRACEW(hostname.c_str());TRACEW(L"<\n");
			}
		}
	}

	// IP Filter
	if (IPFilter && IPFilter->filtering(ss->ip, hostnames) == O2_DENY) {
		if (Logger) {
			ulong2ipstr(ss->ip, ipstr);
			Logger->AddLog(O2LT_NETERR, ServerName.c_str(),
				ss->ip, ss->port, L"Reject(IPFilter)");
			Logger->AddLog(O2LT_IPF, ServerName.c_str(),
				ss->ip, ss->port, L"Reject(IPFilter)");
		}

		TotalIPFilterReject++;
		closesocket(ss->sock);
		delete ss;
		return;
	}
#endif

	// add to session map
	SessionMapLock.Lock();
	if (Active) {
		ss->UpdateTimer();
		ss->Activate();
		OnAccept(ss);
		sss.insert(O2SocketSessionPMap::value_type(ss->ip, ss));
		if (sss.size() > SessionPeak)
			SessionPeak = sss.size();
	}
	else {
		closesocket(ss->sock);
		delete ss;
		ss = NULL;
	}
	SessionMapLock.Unlock();

	if (!ss) return;
	SessionExistSignal.On();

	// 新規コネクションを記録
	if (htonl(ss->ip) != 0x7f000001) {
		if (O2DEBUG)
			ulong2ipstr(ss->ip, ipstr);
		else
			ip2e(ss->ip, ipstr);

		if (Logger) {
			Logger->AddLog(O2LT_NET, ServerName.c_str(),
				ss->ip, ss->port, L"accept");
		}
	}
}
