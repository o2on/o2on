/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Node.h
 * description	: o2on node
 *
 */

#pragma once
#include "O2Define.h"
#include "KademliaNode.h"
#include "dataconv.h"
#include "rsa.h"
#include <time.h>




#define O2_NODESTATUS_PASTLINKEDFROM	0x00000001
#define O2_NODESTATUS_PASTLINKEDTO		0x00000002
#define O2_NODESTATUS_LINKEDFROM		0x00000004
#define O2_NODESTATUS_LINKEDTO			0x00000008

#define O2_NODESTATUSMASK_PASTLINKED	0x00000003
#define O2_NODESTATUSMASK_LINKED		0x0000000c




struct O2Node
	: public KademliaNode
{
	//common
	//id
	//ip
	//port
	wstring		name;
	pubT		pubkey;
	//local
	wstring		ua;
	uint		status;
	wstring		flags;
	time_t		lastlink;
	uint64		connection_me2n;
	uint64		sendbyte_me2n;
	uint64		recvbyte_me2n;
	uint64		connection_n2me;
	uint64		sendbyte_n2me;
	uint64		recvbyte_n2me;

	O2Node(void)
		: status(0)
		, lastlink(0)
		, connection_me2n(0)
		, sendbyte_me2n(0)
		, recvbyte_me2n(0)
		, connection_n2me(0)
		, sendbyte_n2me(0)
		, recvbyte_n2me(0)
	{
	}

	double o2ver(void)
	{
		if (ua.size() >= 6
				&& ua[0] == 'O'
				&& ua[1] == '2'
				&& ua[2] == '/') {
			return (wcstod(&ua[3], NULL));
		}
		return (0.0);
	}

	bool valid(void) const
	{
#if !defined(_DEBUG)
		if (!is_globalIP(ip) || port == 0)
			return false;
#endif
		return true;
	}

	void marge(const O2Node &src)
	{
		if (*this == src) {
			if (ip != src.ip)
				ip = src.ip;
			if (port != src.port)
				port = src.port;
			if (id != src.id)
				id = src.id;
			if (pubkey != src.pubkey)
				pubkey = src.pubkey;
			if (!src.ua.empty() && ua != src.ua)
				ua = src.ua;
			if (!src.name.empty() && name != src.name)
				name = src.name;
			if (!src.flags.empty() && flags != src.flags)
				flags = src.flags;
			status |= src.status;
			connection_me2n	+= src.connection_me2n;
			sendbyte_me2n	+= src.sendbyte_me2n;
			recvbyte_me2n	+= src.recvbyte_me2n;
			connection_n2me	+= src.connection_n2me;
			sendbyte_n2me	+= src.sendbyte_n2me;
			recvbyte_n2me	+= src.recvbyte_n2me;
		}
	}

	void reset(void)
	{
		connection_me2n	= 0;
		sendbyte_me2n	= 0;
		recvbyte_me2n	= 0;
		connection_n2me	= 0;
		sendbyte_n2me	= 0;
		recvbyte_n2me	= 0;
	}
};
