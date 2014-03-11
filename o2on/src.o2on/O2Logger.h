/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Logger.h
 * description	: 
 *
 */

#pragma once
#include "O2Define.h"
#include "mutex.h"
#include "O2SAX2Parser.h"
#include <tchar.h>
#include <list>




#define O2LT_INFO		0x00000001	//info
#define O2LT_WARNING	0x00000002	//warning
#define O2LT_ERROR		0x00000004	//error
#define O2LT_FATAL		0x00000008	//fatal
#define O2LT_IM			0x00000010	//im
#define O2LT_NET		0x00000020	//net
#define O2LT_NETERR		0x00000040	//net
#define O2LT_HOKAN		0x00000080	//hokan
#define O2LT_IPF		0x00000100	//ipf
#define O2LT_DEBUG		0x80000000	//debug




/*
 *	O2LogRecord
 */
struct O2LogRecord
{
	uint		type;
	time_t		date;
	wstring		module;
	ulong		ip;
	ushort		port;
	wstring		msg;

	O2LogRecord(void)
		: type(0)
		, date(0)
		, ip(0)
		, port(0)
	{
	}
};
typedef std::list<O2LogRecord> O2LogRecords;
typedef std::list<O2LogRecord>::iterator O2LogRecordsIt;
typedef std::list<O2LogRecord>::reverse_iterator O2LogRecordsRit;




/*
 *	Log record element bit
 */
#define LOG_XMLELM_NONE				0x00000000
#define LOG_XMLELM_TYPE				0x00000001
#define LOG_XMLELM_DATETIME			0x00000002
#define LOG_XMLELM_MODULE			0x00000004
#define LOG_XMLELM_IP				0x00000008
#define LOG_XMLELM_PORT				0x00000010
#define LOG_XMLELM_MSG				0x00000020
#define LOG_XMLELM_INFO				0x80000000
#define LOG_XMLELM_ALL				0x0000ffff




/*
 *	O2LogSelectCondition
 */
struct O2LogSelectCondition
{
	wstring		charset;
	uint		mask;
	uint		type;
	wstring		xsl;
	wstring		timeformat;
	wstring		filename;
	bool		includefilelist;
	wstring		message;
	wstring		message_type;

	O2LogSelectCondition(uint m = LOG_XMLELM_ALL)
		: charset(_T(DEFAULT_XML_CHARSET))
		, mask(m)
		, type(0)
		, includefilelist(false)
	{
	}
};




#define LOGGER_LOG			0
#define LOGGER_NETLOG		1
#define LOGGER_HOKANLOG		2
#define LOGGER_IPFLOG		3



/*
 *	O2Logger
 */
class O2Logger
	: public O2SAX2Parser
	, private Mutex
{
private:
	O2LogRecords Log;
	O2LogRecords NetLog;
	O2LogRecords HokanLog;
	O2LogRecords IPFLog;
	wstring logdir;
	uint64 LogLimit;
	uint64 NetLogLimit;
	uint64 HokanLogLimit;
	uint64 IPFLogLimit;
	time_t sessiontime;

private:
	O2LogRecords *ChooseLog(uint index, uint64 **limit);
	uint64 InternalGet(const O2LogSelectCondition &cond, string &out);

public:
	O2Logger(const wchar_t *dir);
	~O2Logger(void);
	bool SetLimit(uint index, uint n);
	uint64 GetLogLimit(uint index);
	uint64 Min(void);
	uint64 Max(void);

	uint64 Count(void);

	bool Save(bool clear);
	bool GetLogsFromFile(const O2LogSelectCondition &cond, string &out);
	bool AddLog(uint type, const wchar_t *module, ulong ip, ushort port, const char *format, ...);
	bool AddLog(uint type, const wchar_t *module, ulong ip, ushort port, const wchar_t *format, ...);
	bool GetLogMessage(uint type, const wchar_t *module, wstring &out);

	uint64 ExportToXML(const O2LogSelectCondition &cond, string &out);
	uint64 ImportFromXML(const wchar_t *charset, const char *in, uint inlen);
};




//
//	O2Logger_SAX2Handler
//
class O2Logger_SAX2Handler
	: public SAX2Handler
{
protected:
	O2LogRecord *CurRecord;
	uint		CurElm;
	uint64		ParseNum;

public:
	O2Logger_SAX2Handler(O2Logger *lgr);
	~O2Logger_SAX2Handler(void);

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
