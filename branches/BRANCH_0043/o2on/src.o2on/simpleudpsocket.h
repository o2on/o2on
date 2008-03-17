/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: simpleudpsocket.h
 * description	: simple UDP non-blocking socket
 *
 */

#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include "dataconv.h"
#include "debug.h"
#include <tchar.h>

#define BUFFSIZE		5120




class UDPSocket
{
public:
	SOCKET sock;
	struct timeval	tv;

	UDPSocket(int t_ms)
		: sock(INVALID_SOCKET)
	{
		tv.tv_sec  = t_ms/1000;
		tv.tv_usec = t_ms%1000;
	}

	~UDPSocket()
	{
		close();
	}

public:
	SOCKET getsock(void)
	{
		return (sock);
	}
	
	// -----------------------------------------------------------------------
	//	bind
	// -----------------------------------------------------------------------
	bool bind(ushort port)
	{
		if (sock != INVALID_SOCKET)
			return false;
		
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock == INVALID_SOCKET)
			return false;

		sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		sin.sin_port = htons(port);

		if (::bind(sock, (sockaddr*)(&sin), sizeof(sockaddr_in)) != 0) {
			close();
			return false;
		}
		
		//non blocking mode
		unsigned long argp = 1;
		ioctlsocket(sock, FIONBIO, &argp);

		return true;
	}

	// -----------------------------------------------------------------------
	//	close
	// -----------------------------------------------------------------------
	void close(void)
	{
		if (sock) {
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	// -----------------------------------------------------------------------
	//	recvfrom
	// -----------------------------------------------------------------------
	int recvfrom(string &out, ulong &ip, ushort &port)
	{
		if (sock == INVALID_SOCKET)
			return (-1);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int r;
		if ((r = select(0, &fds, NULL, NULL, &tv)) <= 0)
			return (r);

		char buff[BUFFSIZE];
		sockaddr_in addr;
		int addrlen = sizeof(sockaddr_in);

		int n = ::recvfrom(sock, buff, BUFFSIZE, 0, (sockaddr*)&addr, &addrlen);
		ip = addr.sin_addr.S_un.S_addr;
		port = htons(addr.sin_port);

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

	// -----------------------------------------------------------------------
	//	sendto
	// -----------------------------------------------------------------------
	int sendto(ulong ip, ushort port, const char *in, int len)
	{
		if (sock == INVALID_SOCKET || !len)
			return (-1);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		int ret;
		if ((ret = select(0, NULL, &fds, NULL, &tv)) <= 0)
			return (ret);

		sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_addr.S_un.S_addr = ip;
		sin.sin_port = htons(port);

		int n = ::sendto(sock, in, len, 0, (sockaddr*)&sin, sizeof(sockaddr_in));

		if (n < 0) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)
				return (0);
			else
				return (-1);
		}
		return (n);
	}

	// -----------------------------------------------------------------------
	//	multicast_sendto
	// -----------------------------------------------------------------------
	int multicast_sendto(ulong ip, ushort port, const char *in, int len)
	{
		if (sock == INVALID_SOCKET || !len)
			return (-1);

		// get interface address list
		DWORD size = 0;
		WSAIoctl(sock, SIO_ADDRESS_LIST_QUERY, NULL, 0, NULL, 0,&size, NULL, NULL);
		if (size == 0)
			return (-1);
		SOCKET_ADDRESS_LIST *if_list = new SOCKET_ADDRESS_LIST[size];
		int ret = WSAIoctl(sock, SIO_ADDRESS_LIST_QUERY, NULL, 0, if_list, size, &size, NULL, NULL);
		if (ret != 0) {
			delete [] if_list;
			return (-1);
		}

		// multicast
		int send_complete = 0;
		for (int i = 0; i < if_list->iAddressCount; i++) {
			in_addr if_addr;
			if_addr.s_addr = ((sockaddr_in*)if_list->Address[i].lpSockaddr)->sin_addr.s_addr;
			if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char*)&if_addr, sizeof(in_addr)) != 0)
				continue;

			int sendbyte = 0;
			int n;
			while  ((n = sendto(ip, port, in+sendbyte, len-sendbyte)) >= 0) {
				sendbyte += n;
				if (sendbyte >= len) {
					send_complete++;
					break;
				}
			}
		}
		delete [] if_list;
		return (send_complete);
	}
};
