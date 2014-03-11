/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.s69.xrea.com/
 */

/*
 * project		: o2on
 * filename		: O2Agent.cpp
 * description	: o2on agent class implementation
 *
 */

#include <winsock2.h>
#include "O2Agent.h"
#include "dataconv.h"
#include <process.h>

#define MODULE						L"Agent"
#define DEBUG_SESSION_COUNT			0
#define DEBUG_THREADLOOP			0

#define DEFAULT_CONNECT_INTERVAL	10000
#define GIP_CONNECT_TIMEOUT_MS		2000




O2Agent::
O2Agent(O2Logger *lgr, O2Profile *prof, O2NodeDB *ndb)
	: Agent_GetGlobalIP(NULL)
	, Logger(lgr), Profile(prof), NodeDB(ndb)
	, LaunchThreadHandle(NULL)
	, NetIOThreadHandle(NULL)
	, SessionAvailableEvent(NULL)
	, hwndSetIconCallback(NULL)
	, msgSetIconCallback(0)
	, Interval(DEFAULT_CONNECT_INTERVAL)
	, Active(false)
{
}




O2Agent::
~O2Agent()
{
}




bool
O2Agent::
Start(void)
{
	if (Active) {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
				L"起動済のため起動要求を無視");
		}
		return false;
	}

	Lock();
	uint servicecount = Services.size();
	Unlock();

	if (servicecount == 0 && !Agent_GetGlobalIP) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"サービスが無いため起動に失敗");
		}
		return false;
	}
	if (!Agent_GetGlobalIP) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"Agent_GetGlobalIPが無いため起動に失敗");
		}
		return false;
	}

	SessionAvailableEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	Active = true;

	NetIOThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticNetIOThread, this, 0, NULL);
	LaunchThreadHandle =
		(HANDLE)_beginthreadex(NULL, 0, StaticLaunchThread, this, 0, NULL);

	return true;
}




bool
O2Agent::
Stop(void)
{
	Active = false;

	while (LaunchThreadHandle || NetIOThreadHandle) {
		Sleep(THREAD_ALIVE_WAIT_MS);
	}
	CloseHandle(SessionAvailableEvent);
	SessionAvailableEvent = NULL;

	return true;
}




bool
O2Agent::
Restart(void)
{
	if (Logger)
		Logger->AddLog(O2LT_INFO, MODULE, 0, 0, L"再起動...");
	if (!Stop())
		return false;
	if (!Start())
		return false;
	return true;
}




bool
O2Agent::
IsActive(void)
{
	return (Active);
}




bool
O2Agent::
AddService(O2AgentService *as)
{
	Lock();
	Services.push_back(as);
	Unlock();

	if (Logger) {
		Logger->AddLog(O2LT_INFO, MODULE, 0, 0,
			L"サービス追加(%s)", as->GetName());
	}

	return true;
}
bool
O2Agent::
AddService_GetGlobalIP(O2AgentService *as)
{
	Lock();
	Agent_GetGlobalIP = as;
	Unlock();

	if (Logger) {
		Logger->AddLog(O2LT_INFO, MODULE, 0, 0,
			L"サービス追加(%s)", as->GetName());
	}

	return true;
}




bool
O2Agent::
SetInterval(uint ms)
{
	Interval = ms;
	return true;
}




uint
O2Agent::
GetInterval(void)
{
	return (Interval);
}




void
O2Agent::
SetIconCallbackMsg(HWND hwnd, UINT msg)
{
	hwndSetIconCallback = hwnd;
	msgSetIconCallback = msg;
}




// ---------------------------------------------------------------------------
//	LaunchThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Agent::
StaticLaunchThread(void *data)
{
	CoInitialize(NULL); 

	O2Agent *me = (O2Agent*)data;
	uint ret = me->LaunchThread();

	CoUninitialize();

	CloseHandle(me->LaunchThreadHandle);
	me->LaunchThreadHandle = NULL;
	//_endthreadex(0);
	return (ret);
}

uint
O2Agent::
LaunchThread(void)
{
	for (uint i = 0; i < Services.size(); i++) {
		Services[i]->SetActive(true);
		Services[i]->OnServiceStart();
	}
	Agent_GetGlobalIP->SetActive(true);
	Agent_GetGlobalIP->OnServiceStart();

	if (Logger)
		Logger->AddLog(O2LT_INFO, MODULE, L"エージェント正常起動");

	time_t prevconnecttime = time(NULL);
	uint index = 0;

	while (Active) {

		index %= Services.size();

		/*
		 *	ウェイト処理
		 */
		if (NodeDB->Count() == 0) {
#if DEBUG_THREADLOOP && defined(_DEBUG)
			TRACEA("■Node 0\n");
#endif
			Sleep(1000);
			continue;
		}

		uint pasttime_ms = ((uint)time(NULL) - (uint)prevconnecttime) * 1000;
		if (pasttime_ms < Interval) {
			uint sleeptime_ms = Interval - pasttime_ms;
#if DEBUG_THREADLOOP && defined(_DEBUG)
			TRACEA("Wait interval\n");
#endif
			if (Agent_GetGlobalIP->IsActive()) {
				;
				//グローバルIP未確定のときは急ぐ
			}
			else {
				if (sleeptime_ms < 1000)
					Sleep(sleeptime_ms);
				else
					Sleep(1000);
				continue;
			}
		}

		/*
		 *	サービス起動
		 */
		O2AgentService *service = Services[index];
		if (Agent_GetGlobalIP->IsActive())	//グローバルIP未決定のとき
			service = Agent_GetGlobalIP;	//強制的にAgent_GetGlobalIP起動

#if DEBUG_SESSION_COUNT && defined(_DEBUG)
		wchar_t abc[256]; swprintf(abc, L"● count:%d  limit:%d  %s\n",
			service->GetSessionCount(), service->GetSessionLimit(), service->GetName());
		TRACEW(abc);
#endif
		
		if (!service->IsActive()) {
			index++;
			continue;
		}
		if (service->GetSessionCount() >= service->GetSessionLimit()) {
			Sleep(500);
			index++;
			continue;
		}
		
		O2SocketSession *ss = new O2SocketSession();
		ss->service = (void*)service;
		
		ulong ip;
		ushort port;
		wstring ipstr;

		//接続先ノード決定
		//接続 TODO:クラスタリング
		if (service->HaveNodeDecision()) {
			service->OnLaunch(ss);
			if (!ss->IsActive()) {
				delete ss;
				index++;
				continue;
			}
			ip = ss->sin.sin_addr.S_un.S_addr;
			port = htons(ss->sin.sin_port);
		}
		else if (!NodeDB->GetNodeByPriority(ip, port)) {
			if (Logger) {
				Logger->AddLog(O2LT_WARNING, MODULE,
					L"有効なノード情報が無いので接続不可(%s)",
					service->GetName());
			}
			delete ss;
			index++;
			continue;
		}

#if !defined(_DEBUG)
		if (ip == Profile->GetIP() || ip == 0x0100007f) {
#if 0
			struct in_addr addr;
			addr.S_un.S_addr = ip;
			if (Logger) {
				Logger->AddLog(O2LT_WARNING, MODULE,
					"自ノード情報破棄 IP: %s Port: %d",
					inet_ntoa(addr), port);
			}
#endif
			NodeDB->DeleteNode(ip);
			delete ss;
			index++;
			continue;
		}
#endif

#if defined(_DEBUG)
		ulong2ipstr(ip, ipstr);
#else
		ip2e(ip, ipstr);
#endif
		
		/*
		 *	Socket生成
		 */
		ss->sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ss->sock == INVALID_SOCKET) {
			if (Logger)
				Logger->AddLog(O2LT_ERROR, MODULE, L"ソケット生成に失敗");
			delete ss;
			index++;
			continue;
		}

		ss->sin.sin_family = AF_INET;
		ss->sin.sin_port = htons(port);
		ss->sin.sin_addr.S_un.S_addr = ip;
		

		/*
		 *	Connect
		 */
		int timeout = service == Agent_GetGlobalIP ? GIP_CONNECT_TIMEOUT_MS : CONNECT_TIMEOUT_MS;
		if (connect2(ss->sock, (struct sockaddr*)&ss->sin, sizeof(ss->sin), timeout) != 0) {
			//NodeDB->DeleteNode(ip);
			NodeDB->IncrementErrorCount(ip, 1);
			if (Logger) {
				Logger->AddLog(O2LT_NET, MODULE,
					L"connect失敗(%s %s:%d)",
					service->GetName(), ipstr.c_str(), port);
			}
			closesocket(ss->sock);
			delete ss;
			index++;
			continue;
		}
		if (hwndSetIconCallback) {
			PostMessage(hwndSetIconCallback, msgSetIconCallback, 1, 0);
		}

		//launch
		if (!service->HaveNodeDecision()) {
			service->OnLaunch(ss);
			if (!ss->IsActive()) {
				closesocket(ss->sock);
				delete ss;
				index++;
				continue;
			}
		}
		ss->UpdateTimer();
		service->IncrementCount();

		Lock();
		sss.push_back(ss);
		Unlock();
		SetEvent(SessionAvailableEvent);
		
		NodeDB->AddStatusBit(ip, O2_NODESTATUS_LINKEDTO);
		prevconnecttime = time(NULL);
		index++;
		
#if DEBUG_SESSION_COUNT && defined(_DEBUG)
		swprintf(abc, L"●● count:%d  limit:%d  %s\n",
			service->GetSessionCount(), service->GetSessionLimit(), service->GetName());
		TRACEW(abc);
#endif
		if (Logger) {
			Logger->AddLog(O2LT_NET, MODULE,
				L"connect成功 (%s %s:%d)",
				service->GetName(), ipstr.c_str(), port);
		}
	}

	//end
	Lock();
	SetEvent(SessionAvailableEvent);
	Unlock();

	for (uint i = 0; i < Services.size(); i++) {
		O2AgentService *service = Services[i];
		if (service->IsActive()) {
			service->SetActive(false);
			service->OnServiceEnd();
		}
	}

	if (Logger)
		Logger->AddLog(O2LT_INFO, MODULE, L"エージェント正常終了");
	return (0);
}




// ---------------------------------------------------------------------------
//	NetIOThread
//
// ---------------------------------------------------------------------------

uint WINAPI
O2Agent::
StaticNetIOThread(void *data)
{
	CoInitialize(NULL); 

	O2Agent *me = (O2Agent*)data;
	uint ret = me->NetIOThread();

	CoUninitialize();

	CloseHandle(me->NetIOThreadHandle);
	me->NetIOThreadHandle = NULL;
	//_endthreadex(0);
	return (ret);
}

uint
O2Agent::
NetIOThread(void)
{
	O2SocketSessionListIt ssit;
	int lasterror;

	while (Active) {
		//setup FD
		fd_set readfds;
		fd_set writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		//wait
		Lock();
		uint n = sss.size();
		Unlock();
		if (n == 0) {
			ResetEvent(SessionAvailableEvent);
		}
		WaitForSingleObject(SessionAvailableEvent, INFINITE);
		if (!Active)
			break;

		//FD
		Lock();
		for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
			O2SocketSession *ss = *ssit;
			if (!ss->sendcomplete)
				FD_SET(ss->sock, &writefds);
			else
				FD_SET(ss->sock, &readfds);
		}
		Unlock();

		//select
		struct timeval tv;
		tv.tv_sec = SELECT_TIMEOUT_MS/1000;
		tv.tv_usec = SELECT_TIMEOUT_MS%1000;
		select(0, &readfds, &writefds, NULL, &tv);

		//
		//	既存セッションとの送受信
		//
		Lock();
		ssit = sss.begin();
		Unlock();
		while (1) {
			bool loopend = false;
			Lock();
			if (ssit == sss.end())
				loopend = true;
			Unlock();
			if (loopend)
				break;

#if DEBUG_THREADLOOP && defined(_DEBUG)
			TRACEA("[NetIO]\n");
#endif
			O2SocketSession *ss = *ssit;
			O2AgentService *service = (O2AgentService*)ss->service;

			ushort port = htons(ss->sin.sin_port);
			wstring ipstr;
#if defined (_DEBUG)
			ulong2ipstr(ss->sin.sin_addr.S_un.S_addr, ipstr);
#else
			ip2e(ss->sin.sin_addr.S_un.S_addr, ipstr);
#endif
			//send
			if (FD_ISSET(ss->sock, &writefds)) {
				int len;
				const char *buff = ss->GetSendBuffer(len);
				if (len > 0) {
					int n = send(ss->sock, buff, len, 0);
#if DEBUG_THREADLOOP && defined(_DEBUG)
					TRACEA("[A]send\n");
#endif
					if (n > 0) {
						ss->UpdateSend(n);
						NodeDB->IncrementSendByte_me2n(ss->sin.sin_addr.S_un.S_addr, n);
						service->IncrementSendByte(n);
						service->OnSend(ss);
						ss->UpdateTimer();

						if (hwndSetIconCallback) {
							PostMessage(hwndSetIconCallback, msgSetIconCallback, 1, 0);
						}
					}
					else if (n == 0) {
						;
					}
					else if ((lasterror = WSAGetLastError()) != WSAEWOULDBLOCK) {
						if (Logger) {
							Logger->AddLog(O2LT_NET, MODULE,
								L"送信エラー(%d) (%s %s:%d %dbyte)",
								lasterror, service->GetName(), ipstr.c_str(), port, ss->sbuff.size());
						}
						ss->SetActive(false);
					}
				}
			}

			//recv
			if (FD_ISSET(ss->sock, &readfds)) {
				char buff[RECVBUFFSIZE];
				int n = recv(ss->sock, buff, RECVBUFFSIZE, 0);
#if DEBUG_THREADLOOP && defined(_DEBUG)
				TRACEA("[A]recv\n");
#endif
				if (n > 0) {
					ss->AppendRecv(buff, n);
					NodeDB->IncrementRecvByte_me2n(ss->sin.sin_addr.S_un.S_addr, n);
					service->IncrementRecvByte(n);
					service->OnRecv(ss);
					ss->UpdateTimer();

					if (hwndSetIconCallback) {
						PostMessage(hwndSetIconCallback, msgSetIconCallback, 0, 0);
					}
				}
				else if (n == 0) {
					if (Logger) {
						Logger->AddLog(O2LT_NET, MODULE,
							L"受信0 (%s %s:%d)",
								service->GetName(), ipstr.c_str(), port);
					}
					ss->SetActive(false);

				}
				else if ((lasterror = WSAGetLastError()) != WSAEWOULDBLOCK) {
					if (Logger) {
						Logger->AddLog(O2LT_NET, MODULE,
						L"受信エラー(%d) (%s %s:%d %dbyte)",
						lasterror, service->GetName(), ipstr.c_str(), port, ss->rbuff.size());
					}
					ss->SetActive(false);
				}
			}

			//delete
			if (!ss->IsActive() || ss->GetPastTime() >= SOCKET_TIMEOUT_S) {
				void *databk = ss->data;
				ulong ip = ss->sin.sin_addr.S_un.S_addr;
				closesocket(ss->sock);
				service->OnClose(ss);
				NodeDB->RemoveStatusBit(ip, O2_NODESTATUS_LINKEDTO);
				NodeDB->AddStatusBit(ip, O2_NODESTATUS_PASTLINKEDTO);
				service->DecrementCount();

				if (!ss->rbuff.empty() && !ss->sbuff.empty()) {
					NodeDB->UpdateLastModified(ip, 2);
					service->IncrementTotalSessionCount();
				}
				else if (ss->rbuff.empty())
					NodeDB->IncrementErrorCount(ip, 1);
				if (!ss->rbuff.empty() && (!databk || ss->rbuff[0] != 'H')) {
					NodeDB->DeleteNode(ip);

					if (Logger) {
						Logger->AddLog(O2LT_WARNING, MODULE,
							L"不正データ受信 (ノード削除: %s:%d)",
							ipstr.c_str(), htons(ss->sin.sin_port));
					}

					if (O2DEBUG) {
						char filename[256];
						sprintf_s(filename, 256, "dump\\A_%d.txt", rand());
						FILE *fp;
						fopen_s(&fp, filename, "wb");
						fwrite(ss->rbuff.c_str(), ss->rbuff.size(), 1, fp);
						fclose(fp);
					}
				}
#if DEBUG_SESSION_COUNT && defined(_DEBUG)
				wchar_t abc[256]; swprintf(abc, L"■■削除■■ count:%d  limit:%d  %s\n",
					service->GetSessionCount(), service->GetSessionLimit(), service->GetName());
				TRACEW(abc);
#endif
				Lock();
				ssit = sss.erase(ssit);
				Unlock();
				delete ss;
				continue;
			}

			Sleep(1);
			ssit++;
		}
	}

	//end
	Lock();
	for (ssit = sss.begin(); ssit != sss.end(); ssit++) {
		O2SocketSession *ss = *ssit;
		closesocket(ss->sock);
		ss->sock = 0;
		((O2AgentService*)((O2AgentService*)ss->service))->OnClose(ss);
		delete ss;
	}
	sss.clear();
	Unlock();

	return (0);
}




int
O2Agent::
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

