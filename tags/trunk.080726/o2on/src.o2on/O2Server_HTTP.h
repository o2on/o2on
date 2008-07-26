/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server_HTTP.h
 * description	: o2on server class
 *
 */

#pragma once
#include "O2Server.h"
#include "httpheader.h"




//
// O2Server_HTTP
//
class O2Server_HTTP
	: public O2Server
{
public:
	O2Server_HTTP(const wchar_t *name, O2Logger *lgr, O2IPFilter *ipf)
		: O2Server(name, lgr, ipf)
	{
	}
	~O2Server_HTTP()
	{
	}

protected:
	// -----------------------------------------------------------------------
	//	O2Server methods (override)
	// -----------------------------------------------------------------------
	virtual void OnServerStart(void)
	{
	}
	virtual void OnServerStop(void)
	{
	}
	virtual void OnSessionLimit(O2SocketSession *ss)
	{
		closesocket(ss->sock);
		delete ss;
	}
	virtual void OnAccept(O2SocketSession *ss)
	{
		ss->can_recv = true;
		ss->can_send = false;
	}
	virtual void OnRecv(O2SocketSession *ss)
	{
		HTTPHeader *header = (HTTPHeader*)ss->data;
		if (!header) {
			if (ss->rbuff[0] < 'A' || ss->rbuff[0] > 'Z') {
				ss->error = true;
				ss->Deactivate();
				return;
			}
			if (!strstr(ss->rbuff.c_str(), "\r\n\r\n"))
				return;
			header = new HTTPHeader();
			if (header->Parse(ss->rbuff.c_str()) == 0
					|| header->contentlength > RecvSizeLimit) {
				ss->error = true;
				ss->Deactivate();
				delete header;
				return;
			}
			ss->data = (void*)header;
		}
		if (header) {
			if (ss->rbuff.size() > RecvSizeLimit)
				ss->Deactivate();
			else if (ss->rbuff.size() - header->length >= header->contentlength) {
				ss->SetCanDelete(false);
				ParseRequest(ss);
				ss->can_recv = false;
				ss->can_send = true;
			}
		}
	}
	virtual void OnSend(O2SocketSession *ss)
	{
		if (ss->CanDelete() && ss->sbuffoffset >= ss->sbuff.size()) {
			ss->Deactivate();
		}
	}
	virtual void OnClose(O2SocketSession *ss)
	{
		HTTPHeader *header = (HTTPHeader*)ss->data;
		if (header) {
			delete header;
			ss->data = NULL;
		}
		ss->Finish();
		delete ss;
	}

protected:
	// -----------------------------------------------------------------------
	//	Own methods
	// -----------------------------------------------------------------------
	virtual void ParseRequest(O2SocketSession *ss)
	{
		ss->SetCanDelete(true);
	}
};
