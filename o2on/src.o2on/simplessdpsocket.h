/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: simplessdpsocket.h
 * description	: simple SSDP socket
 *
 */

#pragma once
#include "simpleudpsocket.h"
#include "httpheader.h"

#define LINESTR "--------------------------------------------------\r\n"




class SSDPSocket
	: public UDPSocket
{
protected:
	struct responseT {
		ulong ip;
		ushort port;
		string buff;
		bool operator==(const responseT &src) {
			return (ip == src.ip && port == src.port ? true : false);
		}
	};
	typedef std::vector<responseT> ResponseArray;
	typedef ResponseArray::iterator ResponseArrayIt;

	string *logbuffer;

public:
	SSDPSocket(int t_ms)
		: UDPSocket(t_ms)
		, logbuffer(NULL)
	{
	}
	~SSDPSocket()
	{
	}
	void setlogbuffer(string *buffer)
	{
		logbuffer = buffer;
	}

public:
	bool initialize(ushort port)
	{
		bool ret = bind(port);
		if (!ret && logbuffer)
			*logbuffer += "SSDPSocket::initialize() bind error.\r\n";
		return (ret);
	}

	size_t discover(const char *st, strarray &locations)
	{
		HTTPHeader hdr(HTTPHEADERTYPE_REQUEST);

		hdr.method = "M-SEARCH";
		hdr.url = "*";
		hdr.ver = 11;
		hdr.AddFieldString("MX",	"3");
		hdr.AddFieldString("Host",	"239.255.255.250:1900");
		hdr.AddFieldString("Man",	"\"ssdp:discover\"");
		hdr.AddFieldString("ST",	st);

		string header;
		hdr.Make(header);

		ulong ip = inet_addr("239.255.255.250");
		ushort port = 1900;

		if (logbuffer) {
			*logbuffer += "SSDPSocket::discover() start multicast.\r\n";
			*logbuffer += LINESTR;
			*logbuffer += header;
		}

		if (multicast_sendto(ip, port, header.c_str(), header.size()) <= 0) {
			*logbuffer += "SSDPSocket::discover() multicast error.\r\n";
			return (0);
		}

		ResponseArray rarray;
		ResponseArrayIt it;
		responseT response;
		string buff;
		time_t t = time(NULL);
		time_t wait_s = 1;
		int ret;

		if (logbuffer) {
			*logbuffer += "SSDPSocket::discover() wait for multicast response.\r\n";
		}

		while ((ret = recvfrom(response.buff, ip, port)) >= 0) {
			if (ret > 0) {
				response.ip = ip;
				response.port = port;
				it = std::find(rarray.begin(), rarray.end(), response);
				if (it == rarray.end())
					rarray.push_back(response);
				else
					it->buff.append(response.buff);
			}
			if (time(NULL) - t >= wait_s) {
				int complete_count = 0;
				for (it = rarray.begin(); it != rarray.end(); it++) {
					if (strstr(it->buff.c_str(), "\r\n\r\n"))
						complete_count++;
				}
				if (complete_count == rarray.size())
					break;
			}
			response.buff.clear();
			Sleep(1);
		}

		for (it = rarray.begin(); it != rarray.end(); it++) {
			if (logbuffer) {
				*logbuffer += LINESTR;
				*logbuffer += it->buff;
			}
			HTTPHeader rhdr(HTTPHEADERTYPE_RESPONSE);
			if (rhdr.Parse(it->buff.c_str())) {
				const char *location = rhdr.GetFieldString("Location");
				if (location)
					locations.push_back(location);
			}
		}

		if (locations.empty() && logbuffer) {
			*logbuffer += "SSDPSocket::discover() location not found.\r\n";
		}

		return (locations.size());
	}
};
