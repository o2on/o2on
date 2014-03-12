/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2IPFilter.h
 * description	: 
 *
 */

#pragma once
#include "O2SAX2Parser.h"
#include "O2Logger.h"
#include "mutex.h"
#include <windows.h>
#include <map>
#include <vector>

#define O2_ALLOW	0x00000000
#define O2_DENY		0x00000001




/*
 *	O2IPFilterRecord
 */
struct O2IPFilterRecord
{
	bool		enable;

	uint		flag;
	wstring		cond;
	ulong		ip;
	ulong		mask;
	wstring		host;

	O2IPFilterRecord(void)
		: enable(false)
		, flag(0)
		, ip(0)
		, mask(0)
	{
	}
};
typedef std::vector<O2IPFilterRecord> O2IPFilterRecords;
typedef std::vector<O2IPFilterRecord>::iterator O2IPFilterRecordsIt;




#define IPF_XMLELM_NONE				0x00000000
#define IPF_XMLELM_ENABLE			0x00000001
#define IPF_XMLELM_FLAG				0x00000002
#define IPF_XMLELM_COND				0x00000004

#define IPF_XMLELM_ALL				0x0000ffff
#define IPF_XMLELM_DEFAULT			0x80000000




/*
 *	O2IPFilter
 */
class O2IPFilter
	: public O2SAX2Parser
	, private Mutex
{
private:
	wstring name;
	O2Logger *Logger;
	O2IPFilterRecords records;
	uint DefaultFlag;

public:
	O2IPFilter(const wchar_t *nm, O2Logger *lgr);
	~O2IPFilter(void);

	uint64 Count(void);

	wchar_t *getname(void);
	bool getdefault(void);
	void setdefault(uint flag);
	bool get(uint i, O2IPFilterRecord &rec);
	bool add(bool enable, uint flag, const wchar_t *cond, bool do_add = true);
	bool check(bool enable, uint flag, const wchar_t *cond);
	bool erase(uint index);
	void clear(void);
	bool up(uint index);
	bool down(uint index);
	bool enable(uint index);
	bool disable(uint index);
	uint filtering(ulong ip, const wstrarray &hostnames);

	bool Save(const wchar_t *filename);
	bool Load(const wchar_t *filename);

	uint64 ExportToXML(string &out);
	uint64 ImportFromXML(const wchar_t *filename, const char *in, uint len);
};




//
//	O2IPFilter_SAX2Handler
//
class O2IPFilter_SAX2Handler
	: public SAX2Handler
{
protected:
	O2IPFilter			*IPFilter;
	O2IPFilterRecord	*CurRecord;
	uint				CurElm;
	uint64				ParseNum;

public:
	O2IPFilter_SAX2Handler(const wchar_t *nm, O2Logger *lgr, O2IPFilter *ipf);
	~O2IPFilter_SAX2Handler(void);

	uint64 GetParseNum(void);

	void endDocument(void);
	void startElement(const XMLCh* const uri
					, const XMLCh* const localname
					, const XMLCh* const qname
					, const Attributes& attrs);
	void endElement(const XMLCh* const uri
				  , const XMLCh* const localname
				  , const XMLCh* const qname);
	void characters(const XMLCh* const chars
				  , const unsigned int length);
};
