/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Protocol_Kademlia.h
 * description	: 
 *
 */

#pragma once
#include "O2Protocol.h"
#include "O2SocketSession.h"
#include "dataconv.h"




class O2Protocol_Kademlia
{
protected:
	O2Protocol proto;

public:
	O2Protocol_Kademlia(void)
	{
	}

	~O2Protocol_Kademlia()
	{
	}

	// -----------------------------------------------------------------------
	//	PING
	// -----------------------------------------------------------------------
	void MakeRequest_Kademlia_PING(const O2SocketSession *ss
								 , const O2Profile *profile
								 , string &out)
	{
		string url;
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_KADEMLIA_PING, url);

		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		header.method = "GET";
		header.SetURL(url.c_str());
		proto.AddRequestHeaderFields(header, profile);

		header.Make(out);
	}
	void MakeResponse_Kademlia_PING(const O2Profile *profile
								  , string &out)
	{
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);
		proto.AddContentFields(header, sizeof(ulong)*2, "text/plain", NULL);

		header.Make(out);
	}

	// -----------------------------------------------------------------------
	//	STORE
	// -----------------------------------------------------------------------
	void MakeRequest_Kademlia_STORE(const O2SocketSession *ss
								  , const O2Profile *profile
								  , const char *category
								  , const size_t content_length
								  , string &out)
	{
		string url;
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_KADEMLIA_STORE, url);

		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		header.method = "POST";
		header.SetURL(url.c_str());
		header.AddFieldString(X_O2_KEY_CATEGORY, category);
		proto.AddRequestHeaderFields(header, profile);
		proto.AddContentFields(header, content_length, "text/xml", DEFAULT_XML_CHARSET);

		header.Make(out);
	}
	void MakeResponse_Kademlia_STORE(const O2Profile *profile
								   , string &out)
	{
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);

		header.Make(out);
	}

	// -----------------------------------------------------------------------
	//	FIND_NODE
	// -----------------------------------------------------------------------
	void MakeRequest_Kademlia_FINDNODE(const O2SocketSession *ss
									 , const O2Profile *profile
									 , const hashT target
									 , string &out)
	{
		string url;
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_KADEMLIA_FINDNODE, url);

		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		header.method = "GET";
		header.SetURL(url.c_str());
		proto.AddRequestHeaderFields(header, profile);

		string hashstr;
		target.to_string(hashstr);
		header.AddFieldString(X_O2_TARGET_KEY, hashstr.c_str());

		header.Make(out);
	}
	void MakeResponse_Kademlia_FINDNODE(const O2Profile *profile
									  , const size_t content_length
									  , string &out)
	{
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);
		proto.AddContentFields(header, content_length, "text/xml", DEFAULT_XML_CHARSET);

		header.Make(out);
	}


	// -----------------------------------------------------------------------
	//	FIND_VALUE
	// -----------------------------------------------------------------------
	void MakeRequest_Kademlia_FINDVALUE(const O2SocketSession *ss
									  , const O2Profile *profile
									  , const hashT target
									  , string &out)
	{
		string url;
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_KADEMLIA_FINDVALUE, url);

		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		header.method = "GET";
		header.SetURL(url.c_str());
		proto.AddRequestHeaderFields(header, profile);

		string hashstr;
		target.to_string(hashstr);
		header.AddFieldString(X_O2_TARGET_KEY, hashstr.c_str());

		header.Make(out);
	}
	void MakeResponse_Kademlia_FINDVALUE(const O2Profile *profile
									   , const size_t content_length
									   , string &out)
	{
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);
		proto.AddContentFields(header, content_length, "text/xml", DEFAULT_XML_CHARSET);

		header.Make(out);
	}
};
