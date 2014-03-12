/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2IPFilter.cpp
 * description	: 
 *
 */

#include "O2IPFilter.h"
#include "file.h"
#include "dataconv.h"
#include <boost/regex.hpp>

#define MODULE	L"IPFilter"



O2IPFilter::
O2IPFilter(const wchar_t *nm, O2Logger *lgr)
	: name(nm)
	, Logger(lgr)
	, DefaultFlag(O2_DENY)
{
}




O2IPFilter::
~O2IPFilter()
{
}




uint64
O2IPFilter::
Count(void)
{
	return (records.size());
}




wchar_t *
O2IPFilter::
getname(void)
{
	return (&name[0]);
}




bool
O2IPFilter::
getdefault(void)
{
	return (DefaultFlag != 0);
}




void
O2IPFilter::
setdefault(uint flag)
{
	DefaultFlag = flag;
}




bool
O2IPFilter::
get(uint i, O2IPFilterRecord &r)
{
	bool ret = true;
	Lock();

	if (i < records.size()) {
		r.enable = records[i].enable;
		r.flag = records[i].flag;
		r.cond = records[i].cond;
		r.ip = records[i].ip;
		r.mask = records[i].mask;
		r.host = records[i].host;
	}
	else
		ret = false;

	Unlock();
	return (ret);
}




bool
O2IPFilter::
add(bool enable, uint flag, const wchar_t *cond, bool do_add)
{
	O2IPFilterRecord filter;

	if (flag != O2_ALLOW && flag != O2_DENY)
		return false;
	filter.enable = enable;
	filter.flag = flag;
	filter.cond = cond;

	string condstr;
	unicode2ascii(cond, wcslen(cond), condstr);

	size_t pos = condstr.find_first_not_of("0123456789./");
	if (!FOUND(pos)) {
		string ip;
		string mask;
		pos = condstr.find('/');
		if (FOUND(pos)) {
			ip = condstr.substr(0, pos);
			mask = condstr.substr(pos+1);
		}
		else
			ip = condstr;

		strarray iptoken;
		if (split(ip.c_str(), ".", iptoken) == 0)
			return false;
		switch (iptoken.size()) {
			case 1:
				ip += ".0.0.0";
				filter.ip = inet_addr(ip.c_str());
				filter.mask = inet_addr("255.0.0.0");
				break;
			case 2:
				ip += ".0.0";
				filter.ip = inet_addr(ip.c_str());
				filter.mask = inet_addr("255.255.0.0");
				break;
			case 3:
				ip += ".0";
				filter.ip = inet_addr(ip.c_str());
				filter.mask = inet_addr("255.255.255.0");
				break;
			case 4:
				filter.ip = inet_addr(ip.c_str());
				filter.mask = inet_addr("255.255.255.255");
				break;
		}
		if (filter.ip == INADDR_NONE)
			return false;

		if (!mask.empty()) {
			if (!FOUND(mask.find('.'))) {
				int bitnum = atoi(mask.c_str());
				if (bitnum > 32)
					return false;
				ulong m = 0;
				for (int i = 0; i < bitnum; i++)
					m |= (1<<(31-i));
				filter.mask = htonl(m);
					
			}
			else {
				if (mask == "255.255.255.255")
					filter.mask = 0xffffffff;
				else {
					filter.mask = inet_addr(mask.c_str());
					if (filter.mask == INADDR_NONE)
						return false;
				}
			}
		}
	}
	else {
		boost::wregex::flag_type f = boost::regex_constants::normal
									| boost::regex_constants::icase;
		boost::wregex regex(L"^[a-z0-9][a-z0-9.-]+[a-z0-9]$", f);
		if (!boost::regex_search(cond, regex))
			return false;
		filter.host = L".";
		filter.host += cond;
	}

	if (do_add) {
		Lock();
		records.push_back(filter);
		Unlock();
	}

	/*
	wchar_t abc[256];
	wstring is, ms;
	ulong2ipstr(filter.ip, is);
	ulong2ipstr(filter.mask, ms);
	swprintf(abc, L"IP:%s MASK:%s HOST:%s", is.c_str(), ms.c_str(), filter.host.c_str());
	MessageBoxW(NULL,abc,NULL,MB_OK);
	*/

	return true;
}

bool
O2IPFilter::
check(bool enable, uint flag, const wchar_t *cond)
{
	return (add(enable, flag, cond, false));
}




bool
O2IPFilter::
erase(uint index)
{
	bool ret = true;
	Lock();
	{
		if (index < records.size()) {
			records.erase(records.begin() + index);
		}
		else
			ret = false;
	}
	Unlock();
	return (ret);
}




void
O2IPFilter::
clear(void)
{
	records.clear();
}




bool
O2IPFilter::
up(uint index)
{
	bool ret = true;
	Lock();

	if (index > 0 && index < records.size())
		std::iter_swap(&records[index], &records[index-1]);
	else
		ret = false;

	Unlock();
	return (ret);
}




bool
O2IPFilter::
down(uint index)
{
	bool ret = true;
	Lock();

	if (index < records.size()-1)
		std::iter_swap(&records[index], &records[index+1]);
	else
		ret = false;

	Unlock();
	return (ret);
}




bool
O2IPFilter::
enable(uint index)
{
	bool ret = true;
	Lock();

	if (index < records.size())
		records[index].enable = true;
	else
		ret = false;

	Unlock();
	return (ret);
}




bool
O2IPFilter::
disable(uint index)
{
	bool ret = true;
	Lock();

	if (index < records.size())
		records[index].enable = false;
	else
		ret = false;

	Unlock();
	return (ret);
}




#include "StopWatch.h"
uint
O2IPFilter::
filtering(ulong ip, const wstrarray &hostnames)
{
	uint ret = DefaultFlag;

	Lock();
	for (uint i = 0; i < records.size(); i++) {
		if (records[i].host.empty()) {
			if ((ip & records[i].mask) == records[i].ip)
				ret = records[i].flag;
		}
		else if (!hostnames.empty()) {
			for (uint j = 0; j < hostnames.size(); j++) {
				int offset = hostnames[j].size() - records[i].host.size();
				if (offset >= 0 && wcsncmp(&hostnames[j][offset], records[i].host.c_str(), records[i].host.size()) == 0)
					ret = records[i].flag;
			}
		}
	}
	Unlock();

	return (ret);
}




// ---------------------------------------------------------------------------
//
//	O2IPFilter::
//	File I/O
//
// ---------------------------------------------------------------------------

bool
O2IPFilter::
Save(const wchar_t *filename)
{
	string out;
	ExportToXML(out);
/*	
	FILE *fp;
	if (_wfopen_s(&fp, filename, L"wb") != 0)
		return false;
	fwrite(&out[0], 1, out.size(), fp);
	fclose(fp);
*/
	File f;
	if (!f.open(filename, MODE_W)) {
		if (Logger)
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0, L"ファイルを開けません(%s)", filename);
		return false;
	}
	f.write((void*)&out[0], out.size());
	f.close();

	return true;
}




bool
O2IPFilter::
Load(const wchar_t *filename)
{
	struct _stat st;
	if (_wstat(filename, &st) == -1)
		return false;
	if (st.st_size == 0)
		return false;
	ImportFromXML(filename, NULL, 0);
	return true;
}




// ---------------------------------------------------------------------------
//
//	O2IPFilter::
//	XML I/O
//
// ---------------------------------------------------------------------------

uint64
O2IPFilter::
ExportToXML(string &out)
{
	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += _T(DEFAULT_XML_CHARSET);
	xml += L"\"?>"EOL;
	xml += L"<ipf>"EOL;

	wstring message;
	wstring message_type;
	GetXMLMessage(message, message_type);

	xml += L" <message>";
	xml += message;
	xml += L"</message>"EOL;

	xml += L" <message_type>";
	xml += message_type;
	xml += L"</message_type>"EOL;

	xml += L"<type>";
	xml += name.c_str();
	xml += L"</type>"EOL;
	xml += L"<default>";
	xml += DefaultFlag == O2_ALLOW ? L"allow" : L"deny";
	xml += L"</default>"EOL;

	Lock();
	for (uint i = 0; i < records.size(); i++) {
		xml += L"<filter>"EOL;
		xml += L" <enable>";
		xml += records[i].enable ? L"true" : L"false";
		xml += L"</enable>"EOL;
		xml += L" <flag>";
		xml += records[i].flag == O2_ALLOW ? L"allow" : L"deny";
		xml += L"</flag>"EOL;
		xml += L" <cond>";
		xml += records[i].cond;
		xml += L"</cond>"EOL;
		xml += L"</filter>"EOL;
	}
	for (uint i = 0; i < 10; i++) {
		//dummy
		xml += L"<filter>"EOL;
		xml += L" <enable/>";
		xml += L" <flag/>";
		xml += L" <cond/>";
		xml += L"</filter>"EOL;
	}
	xml += L"</ipf>"EOL;
	Unlock();
	
	if (!FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out))
		return (0);

	return (records.size());
}




uint64
O2IPFilter::
ImportFromXML(const wchar_t *filename, const char *in, uint len)
{
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2IPFilter_SAX2Handler handler(name.c_str(), Logger, this);
	parser->setContentHandler(&handler);
	parser->setErrorHandler(&handler);

	try {
		if (filename) {
			LocalFileInputSource source(filename);
			parser->parse(source);
		}
		else {
			MemBufInputSource source((const XMLByte*)in, len, "");
			parser->parse(source);
		}
	}
	catch (const OutOfMemoryException &e) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, name.c_str(), 0, 0,
				L"SAX2: Out of Memory: %s", e.getMessage());
		}
	}
	catch (const XMLException &e) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, name.c_str(), 0, 0,
				L"SAX2: Exception: %s", e.getMessage());
		}
	}
	catch (...) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, name.c_str(), 0, 0,
				L"SAX2: Unexpected exception during parsing.");
		}
	}

	delete parser;
	return (handler.GetParseNum());
}




// ---------------------------------------------------------------------------
//
//	O2IPFilter_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

O2IPFilter_SAX2Handler::
O2IPFilter_SAX2Handler(const wchar_t *nm, O2Logger *lgr, O2IPFilter *ipf)
	: SAX2Handler(nm, lgr)
	, IPFilter(ipf)
	, CurRecord(NULL)
{
}

O2IPFilter_SAX2Handler::
~O2IPFilter_SAX2Handler(void)
{
}

uint64
O2IPFilter_SAX2Handler::
GetParseNum(void)
{
	return (ParseNum);
}

void
O2IPFilter_SAX2Handler::
endDocument(void)
{
	if (CurRecord) {
		delete CurRecord;
		CurRecord = NULL;
	}
}

void
O2IPFilter_SAX2Handler::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{
	CurElm = IPF_XMLELM_NONE;

	if (MATCHLNAME(L"filter")) {
		if (CurRecord)
			delete CurRecord;
		CurRecord = new O2IPFilterRecord();
		CurElm = IPF_XMLELM_NONE;
	}
	else if (MATCHLNAME(L"default")) {
		CurElm = IPF_XMLELM_DEFAULT;
	}
	else if (MATCHLNAME(L"enable")) {
		CurElm = IPF_XMLELM_ENABLE;
	}
	else if (MATCHLNAME(L"flag")) {
		CurElm = IPF_XMLELM_FLAG;
	}
	else if (MATCHLNAME(L"cond")) {
		CurElm = IPF_XMLELM_COND;
	}
}




void
O2IPFilter_SAX2Handler::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	CurElm = IPF_XMLELM_NONE;

	if (!CurRecord || !MATCHLNAME(L"filter"))
		return;
	if (CurRecord->cond.empty())
		return;

	IPFilter->add(CurRecord->enable, CurRecord->flag, CurRecord->cond.c_str());
}




void
O2IPFilter_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
	switch (CurElm) {
		case IPF_XMLELM_DEFAULT:
			if (_wcsnicmp(chars, L"allow", 5) == 0)
				IPFilter->setdefault(O2_ALLOW);
			else
				IPFilter->setdefault(O2_DENY);
			break;
		case IPF_XMLELM_ENABLE:
			if (_wcsnicmp(chars, L"true", 4) == 0)
				CurRecord->enable = true;
			else
				CurRecord->enable = false;
			break;
		case IPF_XMLELM_FLAG:
			if (_wcsnicmp(chars, L"allow", 5) == 0)
				CurRecord->flag = O2_ALLOW;
			else
				CurRecord->flag = O2_DENY;
			break;
		case IPF_XMLELM_COND:
			CurRecord->cond.assign(chars, length);
			break;
	}
}
