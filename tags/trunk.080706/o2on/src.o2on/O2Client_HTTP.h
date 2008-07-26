/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Client_HTTP.h
 * description	: o2on server class
 *
 */

#pragma once
#include "O2Client.h"
#include "httpheader.h"




//
//	O2Client_HTTP
//
class O2Client_HTTP
	: public O2Client
{
public:
	O2Client_HTTP(const wchar_t *name, O2Logger *lgr)
		: O2Client(name, lgr)
	{
	}
	~O2Client_HTTP()
	{
	}

protected:
	// -----------------------------------------------------------------------
	//	O2Client methods (override)
	// -----------------------------------------------------------------------
	virtual void OnClientStart(void)
	{
	}
	virtual void OnClientStop(void)
	{
	}
	virtual void OnConnect(O2SocketSession *ss)
	{
		ss->can_recv = false;
		ss->can_send = true;
	}
	virtual void OnRecv(O2SocketSession *ss)
	{
		HTTPHeader *header = (HTTPHeader*)ss->data;
		if (!header) {
			if (ss->rbuff.size() >= 4 && (
					ss->rbuff[0] != 'H'
					|| ss->rbuff[1] != 'T'
					|| ss->rbuff[2] != 'T'
					|| ss->rbuff[3] != 'P')) {
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
			else if (ss->rbuff.size() - header->length >= header->contentlength)
				ss->Deactivate();
		}
	}
	virtual void OnSend(O2SocketSession *ss)
	{
		if (ss->sbuffoffset >= ss->sbuff.size()) {
			ss->can_send = false;
			ss->can_recv = true;
		}
	}
	virtual void OnClose(O2SocketSession *ss)
	{
/*
		HTTPHeader *header = (HTTPHeader*)ss->data;
		if (header) {
			delete header;
			ss->data = NULL;
		}
*/
		ss->Finish();
		//delete ss;
	}
};
