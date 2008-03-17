/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: httpheader.cpp
 * description	: 
 *
 */

#include "httpheader.h"
#include "dataconv.h"
#include <windows.h>
#include <algorithm>
#include <boost/regex.hpp>

//regexで楽してるので遅い




// ---------------------------------------------------------------------------
//
//	HTTPHeader::
//	Helper methods
//
// ---------------------------------------------------------------------------

time_t
HTTPHeader::
httpdate2time_t(const char *httpdate) const
{
	//supported RFC1123 HTTP-date format only
	// ex) Sun, 06 Nov 1994 08:49:37 GMT
	boost::regex regex("^([A-Z][a-z][a-z]), ([0-9][0-9]) ([A-Z][a-z][a-z]) ([0-9][0-9][0-9][0-9]) ([0-9][0-9]):([0-9][0-9]):([0-9][0-9]) GMT$");
	boost::cmatch m;
	if (!boost::regex_match(httpdate, m, regex))
		return (0);

	struct tm tms;
	tms.tm_year = atoi(m.str(4).c_str()) - 1900;
	tms.tm_mon	= string("JanFebMarAprMayJunJulAugSepOctNovDec").find(m.str(3)) / 3;
	tms.tm_mday = atoi(m.str(2).c_str());
	tms.tm_hour = atoi(m.str(5).c_str());
	tms.tm_min	= atoi(m.str(6).c_str());
	tms.tm_sec	= atoi(m.str(7).c_str());
	tms.tm_isdst = 0;

	long tzoffset;
	_get_timezone(&tzoffset);
	time_t gmt = mktime(&tms) - tzoffset; //gmmktime()
	
	if (gmt == -1)
		return (0);

	return (gmt);
}




void
HTTPHeader::
time_t2httpdate(time_t gmt, string &out)
{
	char tmp[32];
	if (gmt == 0)
		gmt = time(NULL);

	struct tm tm;
	gmtime_s(&tm, &gmt);
	strftime(tmp, 32,
		"%a, %d %b %Y %H:%M:%S GMT", &tm);
	out = tmp;
}




char *
HTTPHeader::
filename2contenttype(const char *filename)
{
	const char *p = strrchr(filename, '.');
	if (p == NULL || *(p+1) == 0)
		return ("application/octet-stream");

	string ext(p+1);
	std::transform(ext.begin(), ext.end(), ext.begin(), tolower);

	//主要な拡張子のみ対応
	switch (ext[0]) {
	case 'c':
		if (ext == "css")
			return ("text/css");
		break;
	case 'g':
		if (ext == "gif")
			return ("image/gif");
		break;
	case 'h':
		if (ext == "html" || ext == "htm")
			return ("text/html");
		break;
	case 'j':
		if (ext == "jpg" || ext == "jpeg")
			return ("image/jpeg");
		if (ext == "js")
			return ("text/javascript");
		break;
	case 'p':
		if (ext == "png")
			return ("image/png");
		break;
	case 't':
		if (ext == "txt")
			return ("text/plain");
		break;
	case 'x':
		if (ext == "xml" || ext == "xsl")
			return ("text/xml");
		break;
	}

	return ("application/octet-stream");
}
void
HTTPHeader::
dump(void)
{
#ifdef _DEBUG
	char tmp[16];
	OutputDebugStringA("Version: ");
	sprintf_s(tmp, 16, "%d.%d", ver/10, ver%10);
	OutputDebugStringA(tmp);
	OutputDebugStringA("\n");

	OutputDebugStringA("Method: ");
	OutputDebugStringA(method.c_str());
	OutputDebugStringA("\n");
	
	OutputDebugStringA("URL: ");
	OutputDebugStringA(url.c_str());
	OutputDebugStringA("\n");

	OutputDebugStringA("Status: ");
	sprintf_s(tmp, 16, "%d", status);
	OutputDebugStringA(tmp);
	OutputDebugStringA("\n");
	
	OutputDebugStringA("Reason: ");
	OutputDebugStringA(reason.c_str());
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Host: ");
	OutputDebugStringA(hostname.c_str());
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Port: ");
	sprintf_s(tmp, 16, "%d", port);
	OutputDebugStringA(tmp);
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Path: ");
	OutputDebugStringA(pathname.c_str());
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Paths:");
	for (uint i = 0; i < paths.size(); i++) {
		OutputDebugStringA(" ");
		OutputDebugStringA(paths[i].c_str());
	}
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Query: ");
	OutputDebugStringA(query.c_str());
	OutputDebugStringA("\n");

	OutputDebugStringA("URL:Querys:");
	for (strmap::const_iterator it = queries.begin(); it != queries.end(); it++) {
		OutputDebugStringA(" ");
		OutputDebugStringA(it->first.c_str());
		OutputDebugStringA("=");
		OutputDebugStringA(it->second.c_str());
	}
	OutputDebugStringA("\n");

	OutputDebugStringA("Fields:\n");
	for (strmap::const_iterator it = fields.begin(); it != fields.end(); it++) {
		OutputDebugStringA(" ");
		OutputDebugStringA(it->first.c_str());
		OutputDebugStringA(": ");
		OutputDebugStringA(it->second.c_str());
		OutputDebugStringA("\n");
	}
#endif
}




uint
HTTPHeader::
GetType(void) const
{
	return (type);
}


// ---------------------------------------------------------------------------
//
//	HTTPHeader::
//	Helper methods for Parsing
//
// ---------------------------------------------------------------------------

uint
HTTPHeader::
ParseRequestLine(const char *in)
{
	//
	//	Request Line
	//	ex) GET /foobar/index.html HTTP/1.1<cr><lf>
	//
	boost::regex regex("^([A-Z]+) ([^ ]+) HTTP/([0-9])\\.([0-9])\r\n");
	boost::cmatch m;

	if (!boost::regex_search(in, m, regex))
		return (0);

	method = m.str(1);
	url = m.str(2);
	ver = atoi(m.str(3).c_str())*10 + atoi(m.str(4).c_str());
	
	return (m.length(0));
}



uint
HTTPHeader::
ParseStatusLine(const char *in)
{
	//
	//	Status Line
	//	ex) HTTP/1.1 200 OK<cr><lf>
	//
	boost::regex regex("^HTTP/([0-9])\\.([0-9]) ([0-9][0-9][0-9]+) ([^\r\n]+)\r\n");
	boost::cmatch m;

	if (!boost::regex_search(in, m, regex))
		return (0);

	ver = atoi(m.str(1).c_str())*10 + atoi(m.str(2).c_str());
	status = atoi(m.str(3).c_str());
	reason = m.str(4);
	
	return (m.length(0));
}




uint
HTTPHeader::
ParseHeaderFields(const char *in)
{
	//
	//	Header Fields
	//	ex)
	//		Host: www.foobar.com<cr><lf>
	//		Content-Length: 12345<cr><lf>
	//		<cr><lf>
	//		<cr><lf>
	//

	//エンコード文字列は非対応
	const char *end = strstr(in, "\r\n\r\n");
	if (!end)
		return (0);

	char *p1 = const_cast<char*>(in);
	char *p2;
	string name;
	string value;

	while (p1 < end) {
		char *eol = strstr(p1, "\r\n");
		size_t valpos = strspn(p1, " \t");
		if (valpos == 0) {
			if (!name.empty()) {
				fields.insert(strmap::value_type(name, value));
			}
			name.erase();
			value.erase();

			//field-name
			if ((p2 = strchr(p1, ':')) == NULL)
				return (0);
			name.assign(p1, p2);
			//std::transform(name.begin(), name.end(), name.begin(), tolower);
			if (*(p2+1) == ' ')
				p1 = p2 + 2;
			else
				p1 = p2 + 1;
		}
		else
			p1 += valpos;

		//field-value
		value.append(p1, eol);
		p1 = eol + 2;
	}

	if (!name.empty() && !value.empty()) {
		fields.insert(strmap::value_type(name, value));
	}

	return ((end-in)+4);
}




void
HTTPHeader::
DecodePCT(const char *in, string &out)
{
	//
	// Decode Percent-Encoding (RFC 3986 2.1)
	// ex) %41%42%43 = ABC
	//
	string s;

	for (uint i = 0; i < strlen(in); i++) {
		if (in[i] == '%') {
			char c1 = in[i+1];
			char c2 = in[i+2];
			char c = (
				((c1 & 0x40 ? (c1 & 0x20 ? c1-0x57 : c1-0x37) : c1-0x30)<<4) |
				((c2 & 0x40 ? (c2 & 0x20 ? c2-0x57 : c2-0x37) : c2-0x30))
			);
			s += c;
			i += 2;
		}
		else if (in[i] == '+')
			s += ' ';
		else
			s += in[i];
	}
	out = s;
}




uint
HTTPHeader::
ParseQueryString(const char *in)
{
	//
	// QueryString
	// ex) foo=bar&abc=123
	//

	strarray pair;

	if (split(in, "&", pair) == 0)
		return (0);

	for (uint i = 0; i < pair.size(); i++) {
		strarray nameval;
		if (split(pair[i].c_str(), "=", nameval) == 0)
			continue;
		if (nameval.size() == 1) {
			DecodePCT(nameval[0].c_str(), nameval[0]);
			queries.insert(strmap::value_type(nameval[0], ""));
		}
		else {
			DecodePCT(nameval[0].c_str(), nameval[0]);
			DecodePCT(nameval[1].c_str(), nameval[1]);
			queries.insert(strmap::value_type(nameval[0], nameval[1]));
		}
	}

	return (queries.size());
}




bool
HTTPHeader::
SplitURL(const char *url)
{
	//
	//	Split URL(URI)
	//	ex) http://www.foobar.com:8001/path0/path1/path2.html?abc=123
	//		base_url	= http://www.foobar.com:8001
	//		host		= www.foobar.com:8001
	//		hostname	= www.foobar.com
	//		port		= 8001
	//		pathquery	= /path1/path2/path3.html?abc=123
	//		pathname	= /path1/path2/path3.html
	//		paths[0]	= path0
	//		paths[1]	= path1
	//		paths[2]	= path2.html
	//		query		= abc=123
	//		queries 	= see ParseQueryString()
	//

	//host, port
	boost::regex regex("^([a-z]+://)?([A-Za-z0-9.-]+):?([0-9]+)?/?");
	boost::cmatch m;
	uint offset = 0;
	if (boost::regex_search(url, m, regex)) {
		host = m.str(3).empty() ? m.str(2) : m.str(2) + ":" + m.str(3);
		hostname = m.str(2);
		base_url = m.str(1) + host;
		if (method == "CONNECT" && m.str(1).empty())
			m.str(3).empty() ? port = 443 : port = atoi(m.str(3).c_str());
		else
			m.str(3).empty() ? port = 80 : port = atoi(m.str(3).c_str());
		offset = m.length(0) - 1;
	}

	//pathquery
	pathquery = &url[offset];

	//pathname
	strarray tmp;
	if (split(pathquery.c_str(), "?", tmp) == 0) {
		pathname = "/";
		paths.push_back("/");
		return true;
	}
	pathname = tmp[0];

	//paths
	if (split(pathname.c_str(), "/", paths) == 0) {
		pathname = "/";
		paths.push_back("/");
	}

	//query
	if (tmp.size() == 1)
		return true;
	query = tmp[1];

	//querys
	ParseQueryString(query.c_str());

	return true;
}




bool
HTTPHeader::
MakeStatusLine(ushort code, uint ver, string &out)
{
	char *reason = NULL;

	//主要コードのみ対応
	switch (code) {
		case 200: reason = "OK"; break;
		case 206: reason = "Partial Content"; break;
		case 302: reason = "Found"; break;
		case 304: reason = "Not Modified"; break;
		case 400: reason = "Bad Request"; break;
		case 401: reason = "Authorization Required"; break;
		case 403: reason = "Forbidden"; break;
		case 404: reason = "Not Found"; break;
		case 500: reason = "Internal Server Error"; break;
		case 501: reason = "Not Implemented"; break;
		case 502: reason = "Bad Gateway"; break;
		case 503: reason = "Service Temporary Unavailable"; break;
		default: return false;
	}

	char line[64];
	sprintf_s(line, 64, "HTTP/%1d.%1d %d %s\r\n", ver/10, ver%10, code, reason);
	out = line;

	return true;
}




// ---------------------------------------------------------------------------
//
//	HTTPHeader::
//	Parsing methods
//
// ---------------------------------------------------------------------------

uint
HTTPHeader::
Parse(const char *in)
{
	if (strlen(in) < 4)
		return (0);

	uint offset = 0;

	// Request or Response or ?
	if (in[0] == 'H' && in[1] == 'T'
			&& in[2] == 'T' && in[3] == 'P') {
		type = HTTPHEADERTYPE_RESPONSE;

		offset = ParseStatusLine(in);
		if (offset == 0)
			return (0);
	}
	else if (in[0] >= 0x41 && in[1] <= 0x5a) {
		type = HTTPHEADERTYPE_REQUEST;

		offset = ParseRequestLine(in);
		if (offset == 0)
			return (0);

		if (!SplitURL(url.c_str()))
			return (0);
	}
	else {
		type = HTTPHEADERTYPE_UNKNOWN;
		return (0);
	}

	// Parse Header Fields
	offset += ParseHeaderFields(&in[offset]);
	if (offset == 0)
		return (0);

	length = offset;
	contentlength = GetFieldNumeric("Content-Length");

	return (offset);
}




uint
HTTPHeader::
Make(string &out, bool relative)
{
	char tmp[16];

	switch (type) {
		case HTTPHEADERTYPE_REQUEST:
			out = method;
			out += " ";
			if (relative) {
				out += pathquery;
			}
			else
				out += url;
			out += " HTTP/";
			sprintf_s(tmp, 16, "%d.%d", ver/10, ver%10);
			out += tmp;
			out += "\r\n";
			break;
		case HTTPHEADERTYPE_RESPONSE:
			MakeStatusLine(status, ver, out);
			break;
		default:
			return (0);
	}

	strmap::iterator it;
	for (it = fields.begin(); it != fields.end(); it++) {
		out += it->first;
		out += ": ";
		out += it->second;
		out += "\r\n";
	}
	out += "\r\n";

	return (out.size());
}




// ---------------------------------------------------------------------------
//
//	HTTPHeader::
//	Get/Set/Add methods
//
// ---------------------------------------------------------------------------

void
HTTPHeader::
SetURL(const char *in)
{
	url = in;
	SplitURL(in);
}

void
HTTPHeader::
AddFieldString(const char *name, const char *val, bool overwrite)
{
	stricmpPred pred(name);
	strmap::iterator it = std::find_if(fields.begin(), fields.end(), pred);
	if (it == fields.end())
		fields.insert(strmap::value_type(name, val));
	else if (overwrite)
		it->second = val;
}

void
HTTPHeader::
AddFieldNumeric(const char *name, uint n, bool overwrite)
{
	char tmp[16];
	sprintf_s(tmp, 16, "%u", n);
	AddFieldString(name, tmp, overwrite);
}

void
HTTPHeader::
AddFieldDate(const char *name, time_t gmt, bool overwrite)
{
	string timestr;
	time_t2httpdate(gmt, timestr);
	AddFieldString(name, timestr.c_str(), overwrite);
}

void
HTTPHeader::
DeleteField(const char *name)
{
	stricmpPred pred(name);
	strmap::iterator it = std::find_if(fields.begin(), fields.end(), pred);
	if (it != fields.end())
		fields.erase(it);
}

const char *
HTTPHeader::
GetFieldString(const char *name) const
{
	stricmpPred pred(name);
	strmap::const_iterator it = std::find_if(fields.begin(), fields.end(), pred);
	if (it == fields.end())
		return (NULL);
	return (&it->second[0]);
}

const uint
HTTPHeader::
GetFieldNumeric(const char *name) const
{
	stricmpPred pred(name);
	strmap::const_iterator it = std::find_if(fields.begin(), fields.end(), pred);
	if (it == fields.end())
		return (0);

	return (strtoul(it->second.c_str(), NULL, 10));
}

const time_t
HTTPHeader::
GetFieldDate(const char *name) const
{
	stricmpPred pred(name);
	strmap::const_iterator it = std::find_if(fields.begin(), fields.end(), pred);
	if (it == fields.end())
		return (0);

	return (httpdate2time_t(it->second.c_str()));
}


const char *
HTTPHeader::
GetQueryString(const char *name) const
{
	strmap::const_iterator it = queries.find(name);
	if (it == queries.end())
		return (NULL);
	return (&it->second[0]);
}




const uint
HTTPHeader::
GetQueryNumeric(const char *name) const
{
	strmap::const_iterator it = queries.find(name);
	if (it == queries.end())
		return (0);
	return (strtoul(it->second.c_str(), NULL, 10));
}
