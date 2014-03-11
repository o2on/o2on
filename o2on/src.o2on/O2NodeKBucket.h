/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2NodeKBucket.h
 * description	: 
 *
 */

#pragma once
#include "KademliaKBucket.h"
#include "O2Profile.h"
#include "O2Client.h"
#include "O2Protocol.h"
#include "O2Protocol_Kademlia.h"




class O2NodeKBucket
    : public KademliaKBucket<O2Node>
{
protected:
	O2Profile	*Profile;
	O2Client	*Client;

	bool ping(const O2Node &node) const
	{
		O2Protocol_Kademlia kproto;

		O2SocketSession ss;
		ss.ip = node.ip;
		ss.port = node.port;
		kproto.MakeRequest_Kademlia_PING(&ss, Profile, ss.sbuff);

		Client->AddRequest(&ss, true);
		ss.Wait();

		HTTPHeader *header = (HTTPHeader*)ss.data;
		if (header)
			delete header;

		if (ss.rbuff.empty())
			return false;

		return true;
	}

public:
	O2NodeKBucket(void)
    {
    }
	~O2NodeKBucket()
	{
	}
	void SetObject(O2Profile *prof, O2Client *client)
    {
		Profile = prof;
		Client = client;
	}
};
