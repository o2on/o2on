/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2DatPath.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "dataconv.h"
#include "sha.h"
#include "httpheader.h"
#include <tchar.h>
#include <io.h>
#include <direct.h>

#define DOMAIN_2CH		"2ch.net"
#define DOMAIN_BBSPINK	"bbspink.com"
#define DOMAIN_MACHI	"machi.to"
#define FAKE_HOST_STR	"xxx"

enum DOMAINTYPE : uint
{
	DOMAINTYPE_UNKNOWN,
	DOMAINTYPE_2CH,
	DOMAINTYPE_BBSPINK,
	DOMAINTYPE_MACHI,
};

enum URLTYPE : uint
{
	URLTYPE_UNKNOWN,
	URLTYPE_NORMAL,
	URLTYPE_DAT,
	URLTYPE_KAKO_DAT,
	URLTYPE_KAKO_GZ,
	URLTYPE_OFFLAW,
	URLTYPE_MACHI,
	URLTYPE_CRAWL
};




class O2DatPath
{
private:
	string hostnameA;
	string domainA;
	string bbsnameA;
	string datnameA;
	string titleA;

	wstring hostnameW;
	wstring domainW;
	wstring bbsnameW;
	wstring datnameW;
	wstring titleW;

public:
	O2DatPath(void)
	{
	}

	uint set(const char *url)
	{
		uint urltype = splitURL(url, hostnameA, domainA, bbsnameA, datnameA);
		ascii2unicode(hostnameA, hostnameW);
		ascii2unicode(domainA,   domainW);
		ascii2unicode(bbsnameA,  bbsnameW);
		ascii2unicode(datnameA,  datnameW);

		if (!check(domainA.c_str(), bbsnameA.c_str(), datnameA.c_str()))
			return (0);
		return (urltype);
	}

	uint set(const wchar_t *url)
	{
		string urlA;
		unicode2ascii(url, urlA);
		uint urltype = splitURL(urlA.c_str(), hostnameA, domainA, bbsnameA, datnameA);
		ascii2unicode(hostnameA, hostnameW);
		ascii2unicode(domainA,   domainW);
		ascii2unicode(bbsnameA,  bbsnameW);
		ascii2unicode(datnameA,  datnameW);

		if (!check(domainA.c_str(), bbsnameA.c_str(), datnameA.c_str()))
			return (0);
		return (urltype);
	}

	bool set(const char *dom, const char *bbs, const char *dat)
	{
		domainA = dom;
		bbsnameA = bbs;
		datnameA = dat;
		ascii2unicode(domainA, domainW);
		ascii2unicode(bbsnameA, bbsnameW);
		ascii2unicode(datnameA, datnameW);
		if (!check(domainA.c_str(), bbsnameA.c_str(), datnameA.c_str()))
			return false;
		hostnameA = FAKE_HOST_STR"." + domainA;
		hostnameW = _T(FAKE_HOST_STR)_T(".") + domainW;
		return true;
	}

	bool set(const wchar_t *dom, const wchar_t *bbs, const wchar_t *dat)
	{
		domainW = dom;
		bbsnameW = bbs;
		datnameW = dat;
		unicode2ascii(domainW, domainA);
		unicode2ascii(bbsnameW, bbsnameA);
		unicode2ascii(datnameW, datnameA);
		if (!check(domainA.c_str(), bbsnameA.c_str(), datnameA.c_str()))
			return false;
		hostnameA = FAKE_HOST_STR"." + domainA;
		hostnameW = _T(FAKE_HOST_STR)_T(".") + domainW;
		return true;
	}

	bool is_origurl(void) const
	{
		if (strstr(hostnameA.c_str(), FAKE_HOST_STR))
			return false;
		return true;
	}

	uint domaintype(void) const
	{
		if (strcmp(domainA.c_str(), DOMAIN_2CH))
			return (DOMAINTYPE_2CH);
		else if (strcmp(domainA.c_str(), DOMAIN_BBSPINK))
			return (DOMAINTYPE_BBSPINK);
		else if (strcmp(domainA.c_str(), DOMAIN_MACHI))
			return (DOMAINTYPE_MACHI);
		return (DOMAINTYPE_UNKNOWN);
	}

	void element(string &dom, string &bbs, string &dat) const
	{
		dom = domainA;
		bbs = bbsnameA;
		dat = datnameA;
	}

	void element(wstring &dom, wstring &bbs, wstring &dat) const
	{
		dom = domainW;
		bbs = bbsnameW;
		dat = datnameW;
	}

	void getdir(const char* root, string &out) const
	{
		out = root;
		out += "\\";
		out += domainA;
		out += "\\";
		out += bbsnameA;
		out += "\\";
		out += datnameA.substr(0,4);
	}

	void getdir(const wchar_t* root, wstring &out) const
	{
		out = root;
		out += L"\\";
		out += domainW;
		out += L"\\";
		out += bbsnameW;
		out += L"\\";
		out += datnameW.substr(0,4);
	}

	void getpath(const char* root, string &out) const
	{
		getdir(root, out);
		out += "\\";
		out += datnameA;
	}

	void getpath(const wchar_t* root, wstring &out) const
	{
		getdir(root, out);
		out += L"\\";
		out += datnameW;
	}

	void gethash(hashT &hash) const
	{
		wchar_t a[64];
		swprintf_s(a, 64, L"%s/%s/%I64u", domainW.c_str(), bbsnameW.c_str(), _wcstoui64(datnameW.c_str(), NULL, 10));
		byte b[HASHSIZE];
		CryptoPP::SHA1().CalculateDigest(b, (byte*)(a), wcslen(a)*sizeof(wchar_t));
		hash.assign(b, HASHSIZE);
	}
		
	bool makedir(const char* root) const
	{
		string tmp = root;
		if (!mkdir(tmp.c_str()))
			return false;

		tmp += "\\";
		tmp += domainA;
		if (!mkdir(tmp.c_str()))
			return false;

		tmp += "\\";
		tmp += bbsnameA;
		if (!mkdir(tmp.c_str()))
			return false;

		tmp += "\\";
		tmp += datnameA.substr(0,4);
		if (!mkdir(tmp.c_str()))
			return false;

		return true;
	}

	bool makedir(const wchar_t* root) const
	{
		wstring tmp = root;
		if (!_wmkdir(tmp.c_str()))
			return false;

		tmp += L"\\";
		tmp += domainW;
		if (!_wmkdir(tmp.c_str()))
			return false;

		tmp += L"\\";
		tmp += bbsnameW;
		if (!_wmkdir(tmp.c_str()))
			return false;

		tmp += L"\\";
		tmp += datnameW.substr(0,4);
		if (!_wmkdir(tmp.c_str()))
			return false;

		return true;
	}

	void geturl(string &url) const
	{
		char datstr[32];
		sprintf_s(datstr, 32, "%I64u", _strtoui64(datnameA.c_str(), NULL, 10));
		url = "http://";
		url += hostnameA;
		url += "/test/read.cgi/";
		url += bbsnameA;
		url += "/";
		url += datstr;
		url += "/";
	}

	void geturl(wstring &url) const
	{
		wchar_t datstr[32];
		swprintf_s(datstr, 32, L"%I64u", _wcstoui64(datnameW.c_str(), NULL, 10));
		url = L"http://";
		url += hostnameW;
		url += L"/test/read.cgi/";
		url += bbsnameW;
		url += L"/";
		url += datstr;
		url += L"/";
	}

	void get_o2_dat_path(string &path)
	{
		char datstr[32];
		sprintf_s(datstr, 32, "%I64u", _strtoui64(datnameA.c_str(), NULL, 10));
		path += domainA;
		path += "/";
		path += bbsnameA;
		path += "/";
		path += datstr;
	}

	void get_o2_dat_path(wstring &path)
	{
		wchar_t datstr[32];
		swprintf_s(datstr, 32, L"%I64u", _wcstoui64(datnameW.c_str(), NULL, 10));
		path += domainW;
		path += L"/";
		path += bbsnameW;
		path += L"/";
		path += datstr;
	}

	void settitle(const char *title)
	{
		titleA = title;
		ToUnicode(L"shift_jis", titleA, titleW);
	}

	void settitle(const wchar_t *title)
	{
		titleW = title;
		FromUnicode(L"shift_jis", titleW, titleA);
	}

	void gettitle(string &title) const
	{
		title = titleA;
	}

	void gettitle(wstring &title) const
	{
		title = titleW;
	}

	bool is_be(void) const
	{
		return bbsnameA == "be";
	}


private:
	bool check(const char *dom, const char *bbs, const char *dat)
	{
		if (!dom || !dom[0] || !bbs || !bbs[0] || !dat || !dat[0])
			return false;
		if (!checkdatname(datnameA.c_str()))
			return false;
		return true;
	}

	bool checkdatname(const char *dat)
	{
		if (strlen(dat) < 13 || strlen(dat) > 15)
			return false;

		char *endptr;
		time_t t = strtoul(dat, &endptr, 10);
		if (t < 928034732)
			return false;

		if (*endptr != '.'
				|| *(endptr+1) != 'd'
				|| *(endptr+2) != 'a'
				|| *(endptr+3) != 't') {
			return false;
		}

		return true;
	}

	uint splitURL(const char *url, string &host, string &dom, string &bbs, string &dat)
	{
		string tmp(url);
		size_t pos = tmp.find_first_of(" \t\r\n");
		if (FOUND(pos))
			tmp.erase(pos);

		HTTPHeader h(0);
		h.SplitURL(tmp.c_str());

		const char *dm = host2domain(h.hostname.c_str());
		if (!dm)
			return (0);
		dom = dm;
		host = h.hostname;

		uint type = 0;

		// URLTYPE_NORMAL	http://news20.2ch.net/test/read.cgi/news/1165620938/
		// URLTYPE_DAT		http://news20.2ch.net:80/news/dat/1163402471.dat
		// URLTYPE_KAKO_DAT http://news20.2ch.net:80/news/kako/1163/11631/1163170977.dat
		// URLTYPE_KAKO_GZ	http://news20.2ch.net:80/news/kako/1163/11631/1163170977.dat.gz
		// URLTYPE_OFFLAW	http://news20.2ch.net:80/test/offlaw.cgi/news/1163170977/?raw=...
		// URLTYPE_MACHI	http://hokkaido.machi.to:80/bbs/read.pl?BBS=hokkaidou&KEY=1162641850
		// URLTYPE_CRAWL    http://bg20.2ch.net/test/r.so/society6.2ch.net/gline/1169964276/
		strmap::iterator it;
		switch (h.paths.size()) {
			case 3:
				if (h.paths[1] == "dat"
						&& strstr(h.paths[2].c_str(), ".dat")) {
					// URLTYPE_DAT
					bbs = h.paths[0];
					dat = h.paths[2];
					type = URLTYPE_DAT;
				}
				break;
			case 4:
				if (h.paths[1] == "kako") {
					if (strstr(h.paths[3].c_str(), ".dat.gz")) {
						bbs = h.paths[0];
						dat = h.paths[3].substr(0, h.paths[3].size()-3);
						type = URLTYPE_KAKO_GZ;
					}
					else if (strstr(h.paths[3].c_str(), ".dat")) {
						// URLTYPE_KAKO_DAT
						bbs = h.paths[0];
						dat = h.paths[3];
						type = URLTYPE_KAKO_DAT;
					}
				}
				else if (h.paths[0] == "test"
						&& h.paths[1] == "offlaw.cgi") {
					// URLTYPE_OFFLAW
					bbs = h.paths[2];
					dat = h.paths[3] + ".dat";
					type = URLTYPE_OFFLAW;
				}
				else if (h.paths[0] == "test"
						&& h.paths[1] == "read.cgi") {
					// URLTYPE_NORMAL
					bbs = h.paths[2];
					dat = h.paths[3] + ".dat";
					type = URLTYPE_NORMAL;
				}
				break;
			case 5:
				if (h.paths[1] == "kako"
						&& h.paths[2].size() == 4
						&& h.paths[3].size() == 5) {
					if (strstr(h.paths[4].c_str(), ".dat.gz")) {
						// URLTYPE_KAKO_GZ
						bbs = h.paths[0];
						dat = h.paths[4].substr(0, h.paths[4].size()-3);
						type = URLTYPE_KAKO_GZ;
					}
					else if (strstr(h.paths[4].c_str(), ".dat")) {
						// URLTYPE_KAKO_DAT
						bbs = h.paths[0];
						dat = h.paths[4];
						type = URLTYPE_KAKO_DAT;
					}
				}
				else if (h.paths[0] == "test"
					&& h.paths[1] == "r.so") {
						bbs = h.paths[3];
						dat = h.paths[4] + ".dat";
						type = URLTYPE_CRAWL;
				}
				break;
			case 2:
				if (h.paths[0] == "bbs"
						&& h.paths[1] == "read.pl") {
					// URLTYPE_MACHI
					it = h.queries.find("BBS");
					if (it != h.queries.end())
						bbs = it->second;
					it = h.queries.find("KEY");
					if (it != h.queries.end())
						dat = it->second + ".dat";
					type = URLTYPE_MACHI;
				}
				break;
		}

		return (type);
	}

	char *host2domain(const char *hostname)
	{
		if (strstr(hostname, DOMAIN_2CH))
			return (DOMAIN_2CH);
		else if (strstr(hostname, DOMAIN_BBSPINK))
			return (DOMAIN_BBSPINK);
		else if (strstr(hostname, DOMAIN_MACHI))
			return (DOMAIN_MACHI);
		return (NULL);
	}

	bool mkdir(const char* path) const
	{
		if (_access_s(path, 0) != 0) {
			if (_mkdir(path) != 0)
				return false;
		}
		return true;
	}
};
