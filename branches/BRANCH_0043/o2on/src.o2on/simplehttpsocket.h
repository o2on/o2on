/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: simplehttpsocket.h
 * description	: simple http socket
 *
 */

#pragma once
#include "simpletcpsocket.h"
#include "httpheader.h"

#define LINESTR "--------------------------------------------------\r\n"




class HTTPSocket
	: public TCPSocket
{
protected:
	string useragent;
	string proxy;
	ushort proxyport;
	string *logbuffer;

public:
	HTTPSocket(int t_ms, const char *ua)
		: TCPSocket(t_ms)
		, useragent(ua)
		, logbuffer(NULL)
	{
	}
	~HTTPSocket()
	{
	}
	void setlogbuffer(string *buffer)
	{
		logbuffer = buffer;
	}

public:
	bool setproxy(const char *name, ushort port)
	{
		proxy = name;
		proxyport = port;
	}
	bool setproxy(const wchar_t *name, ushort port)
	{
		unicode2ascii(name, _tcslen(name), proxy);
		proxyport = port;
	}

	// -----------------------------------------------------------------------
	//	request
	//	URLのみを指定
	// -----------------------------------------------------------------------
	bool request(const char *url, const char *body, size_t bodylen, bool relative = false)
	{
		HTTPHeader hdr(HTTPHEADERTYPE_REQUEST);
		hdr.SplitURL(url);
		if (hdr.hostname.empty()) {
			if (logbuffer)
				*logbuffer += "HTTPSocket::request() hostname not found.\r\n";
			return false;
		}

		hdr.method = "GET";
		hdr.url = url;
		hdr.AddFieldString("Connection", "close");
		hdr.AddFieldString("Host", hdr.hostname.c_str());
		if (!useragent.empty())
			hdr.AddFieldString("User-Agent", useragent.c_str());
		if (body && bodylen)
			hdr.AddFieldNumeric("Content-Length", bodylen);

		string header;
		hdr.Make(header, relative);

		return (request(hdr.hostname.c_str(), hdr.port, header.c_str(), header.size(), body, bodylen));
	}

	// -----------------------------------------------------------------------
	//	request
	//	URLと追加のヘッダフィールドを指定
	// -----------------------------------------------------------------------
	bool request(const char *url, HTTPHeader &hdr, const char *body, size_t bodylen, bool relative = false)
	{
		hdr.SplitURL(url);
		if (hdr.hostname.empty()) {
			if (logbuffer)
				*logbuffer += "HTTPSocket::request() hostname not found.\r\n";
			return false;
		}

		if (hdr.method.empty())
			hdr.method = "GET";
		hdr.url = url;

		hdr.AddFieldString("Connection", "close", false);
		hdr.AddFieldString("Host", hdr.hostname.c_str(), false);
		if (!useragent.empty())
			hdr.AddFieldString("User-Agent", useragent.c_str(), false);
		if (body && bodylen)
			hdr.AddFieldNumeric("Content-Length", bodylen, false);

		string header;
		hdr.Make(header, relative);

		return (request(hdr.hostname.c_str(), hdr.port, header.c_str(), header.size(), body, bodylen));
	}

	// -----------------------------------------------------------------------
	//	request
	//	ホスト名、ポート、プレーンなヘッダ部を指定
	// -----------------------------------------------------------------------
	bool request(const char *hst, ushort pn, const char *header, size_t headerlen, const char *body, size_t bodylen)
	{
		string host;
		ushort port;
		if (proxy.empty()) {
			host = hst;
			port = pn;
		}
		else {
			host = proxy;
			port = proxyport;
		}
		
		if (!connect(host.c_str(), port)) {
			if (logbuffer)
				*logbuffer += "HTTPSocket::request() connect failed.\r\n";
			return false;
		}

		if (logbuffer) {
			*logbuffer += LINESTR;
			*logbuffer += header;
		}

		size_t offset = 0;
		int n;
		while ((n = send(&header[offset], headerlen-offset)) >= 0) {
			offset += n;
			if (offset >= headerlen)
				break;
		}
		if (n < 0) {
			if (logbuffer)
				*logbuffer += "HTTPSocket::request() send failed.\r\n";
			return false;
		}

		if (body) {
			if (logbuffer) {
				*logbuffer += body;
			}
			offset = 0;
			while ((n = send(&body[offset], bodylen-offset)) >= 0) {
				offset += n;
				if (offset >= bodylen)
					break;
			}
			if (n < 0) {
				if (logbuffer)
					*logbuffer += "HTTPSocket::request() send failed.\r\n";
				return false;
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------
	//	response 
	// -----------------------------------------------------------------------
	int response(string &out, HTTPHeader &outhdr)
	{
		int n = recv(out);
		if (n < 0) {
			if (logbuffer)
				*logbuffer += "HTTPSocket::response() recv failed.\r\n";
			return (n);
		}

		if (!outhdr.length) {
			size_t pos = out.find("\r\n\r\n");
			if (FOUND(pos))
				outhdr.Parse(out.c_str());
		}
		
		return (n);
	}
};
