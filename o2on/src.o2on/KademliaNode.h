/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: KademliaNode.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "sha.h"




class KademliaNode
{
public:
	hashT	id;
	ulong	ip;
	ushort	port;

	KademliaNode(void)
		: ip(0)
		, port(0)
	{
	}

	virtual bool valid(void) const
	{
		if (!ip || !port)
			return false;
		return true;
	}

	virtual void marge(const KademliaNode &src)
	{
	}

	virtual bool operator==(const KademliaNode& src) const
	{
		if ((ip == src.ip && port == src.port) || id == src.id)
            return true;
        return false;
	}

	virtual bool operator<(const KademliaNode& src) const
	{
		return (id < src.id);
	}
};
