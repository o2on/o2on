/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: httpheader.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"

#define DEFUALT_HTTP_VER_MAJOR		1
#define DEFUALT_HTTP_VER_MINOR		1

#define HTTPHEADERTYPE_UNKNOWN		0
#define HTTPHEADERTYPE_REQUEST		1
#define HTTPHEADERTYPE_RESPONSE		2




class HTTPHeader
{
private:
	struct stricmpPred {
		const char *dest;
		stricmpPred(const char *dst) : dest(dst) {}
		bool operator ()(const strmap::value_type &src) {
			return (_stricmp(dest, src.first.c_str()) == 0 ? true : false);
		}
	};
private:
	uint		type;
public:
	//Common
	uint		ver;
	uint		length;
	uint		contentlength;
	strmap		fields;
	//Request
	string		method;
	string		url;
	//Response
	uint		status;
	string		reason;
	//URL Elements
	string		base_url;
	string		host;
	string		hostname;
	ushort		port;
	string		pathquery;
	string		pathname;
	strarray	paths;
	string		query;
	strmap		queries;

public:
	HTTPHeader(uint type = HTTPHEADERTYPE_UNKNOWN)
		: type(type)
		, ver(DEFUALT_HTTP_VER_MAJOR*10+DEFUALT_HTTP_VER_MINOR)
		, length(0)
		, contentlength(0)
		, status(0)
		, port(0)
	{
	}

	time_t httpdate2time_t(const char *httpdate) const;
	void time_t2httpdate(time_t gmt, string &out);
	char *filename2contenttype(const char *filename);
	void dump(void);

	uint GetType(void) const;

	uint ParseRequestLine(const char *in);
	uint ParseStatusLine(const char *in);
	uint ParseHeaderFields(const char *in);
	void DecodePCT(const char *in, string &out);
	uint ParseQueryString(const char *in);
	bool SplitURL(const char *url);
	bool MakeStatusLine(ushort code, uint ver, string &out);

	uint MakeRequest(string &out, bool relative);
	uint MakeResponse(string &out);

	uint Parse(const char *in);
	uint Make(string &out, bool relative = false);

	void SetURL(const char *url);
	void AddFieldString(const char *name, const char *val, bool overwrite = true);
	void AddFieldNumeric(const char *name, uint n, bool overwrite = true);
	void AddFieldDate(const char *name, time_t gmt, bool overwrite = true);
	void DeleteField(const char *name);

	const char *GetFieldString(const char *name) const;
	const uint GetFieldNumeric(const char *name) const;
	const time_t GetFieldDate(const char *name) const;
	const char *GetQueryString(const char *name) const;
	const uint GetQueryNumeric(const char *name) const;
};
