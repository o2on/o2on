/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: simpletcpsocket.h
 * description	: simple TCP non-blocking socket
 *
 */

#pragma once
#include "dataconv.h"
#include "debug.h"
#include <tchar.h>

#define BUFFSIZE		5120
#define SOCKET_TRACE	0




class TCPSocket
{
public:
	SOCKET sock;
	struct timeval	tv;

	TCPSocket(int t_ms)
		: sock(INVALID_SOCKET)
	{
		tv.tv_sec  = t_ms/1000;
		tv.tv_usec = t_ms%1000;
	}

	~TCPSocket()
	{
		close();
	}

public:
	SOCKET getsock(void)
	{
		return (sock);
	}
	
	bool connect(const char *name, ushort port)
	{
		ulong ip = inet_addr(name);
		if (ip == INADDR_NONE) {
			struct hostent *host;
			host = gethostbyname(name);
			if (!host)
				return false;
			ip = *((ulong*)host->h_addr_list[0]);
		}
		return (connect(ip, port));
	}

	bool connect(const wchar_t *name, ushort port)
	{
		string s;
		unicode2ascii(name, _tcslen(name), s);
		return (connect(s.c_str(), port));
	}
	
	bool connect(ulong ip, ushort port)
	{
		if (sock != INVALID_SOCKET)
			return false;
		
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET)
			return false;

		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_addr.S_un.S_addr = ip;
		sin.sin_port = htons(port);
		
		if (::connect(sock, (struct sockaddr*)&sin, sizeof(sin)) != 0) {
			close();
			return false;
		}
		//non blocking mode
		unsigned long argp = 1;
		ioctlsocket(sock, FIONBIO, &argp);

		return true;
	}

	void close(void)
	{
		if (sock) {
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	int recv(string &out)
	{
		if (sock == INVALID_SOCKET)
			return (-1);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, &fds, NULL, NULL, &tv)) <= 0)
			return (ret);

		char buff[BUFFSIZE];
		int n = ::recv(sock, buff, BUFFSIZE, 0);

#if SOCKET_TRACE && defined(_DEBUG)
		char tmp[16];
		sprintf_s(tmp, 16, "recv:%d\n", n);
		TRACEA(tmp);
#endif

		if (n > 0)
			out.append(buff, buff+n);
		else if (n == 0)
			return (-1);
		else if (n < 0) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return (0);
			else
				return (-1);
		}
		return (n);
	}

	int recv_fixed(char *buff, uint buffsize)
	{
		if (sock == INVALID_SOCKET)
			return (-1);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, &fds, NULL, NULL, &tv)) <= 0)
			return (ret);

		int n = ::recv(sock, buff, buffsize, 0);

#if SOCKET_TRACE && defined(_DEBUG)
		char tmp[16];
		sprintf_s(tmp, 16, "recv:%d\n", n);
		TRACEA(tmp);
#endif

		if (n == 0)
			return (-1);
		else if (n < 0) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return (0);
			else
				return (-1);
		}
		return (n);
	}

	int send(const char *in, int len)
	{
		if (sock == INVALID_SOCKET || !len)
			return (-1);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, NULL, &fds, NULL, &tv)) <= 0)
			return (ret);

		int n = ::send(sock, in, len, 0);

#if SOCKET_TRACE && defined(_DEBUG)
		char tmp[16];
		sprintf_s(tmp, 16, "send:%d\n", n);
		TRACEA(tmp);
#endif

		if (n < 0) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return (0);
			else
				return (-1);
		}
		return (n);
	}
};
