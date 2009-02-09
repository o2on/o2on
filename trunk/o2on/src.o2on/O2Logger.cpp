/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Logger.cpp
 * description	: 
 *
 */

#include "O2Logger.h"
#include "dataconv.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEBUGOUT		0

#define MODULE			L"Logger"
#define MIN_RECORD		10
#define MAX_RECORD		99999
#define DEFAULT_LIMIT	500




// ---------------------------------------------------------------------------
//
//	O2Logger::
//	Constructor/Destructor
//
// ---------------------------------------------------------------------------

O2Logger::
O2Logger(const wchar_t *dir)
{
	if (dir)
		logdir = dir;

	LogLimit		= DEFAULT_LIMIT;
	NetLogLimit		= DEFAULT_LIMIT;
	HokanLogLimit	= DEFAULT_LIMIT;
	IPFLogLimit		= DEFAULT_LIMIT;

	long tzoffset;
	_get_timezone(&tzoffset);
	sessiontime = time(NULL) + tzoffset; //GMT
}




O2Logger::
~O2Logger()
{
}




// ---------------------------------------------------------------------------
//
//	O2Logger::
//	Local helper methods
//
// ---------------------------------------------------------------------------

O2LogRecords *
O2Logger::
ChooseLog(uint type, uint64 **limit)
{
	O2LogRecords *records;

	switch (type) {
	case LOGGER_LOG:		records = &Log;			*limit = &LogLimit;			break;
	case LOGGER_NETLOG:		records = &NetLog;		*limit = &NetLogLimit;		break;
	case LOGGER_HOKANLOG:	records = &HokanLog;	*limit = &HokanLogLimit;	break;
	case LOGGER_IPFLOG:		records = &IPFLog;		*limit = &IPFLogLimit;		break;
	default:				records = &Log;			*limit = &LogLimit;			break;
	}

	return (records);
}




// ---------------------------------------------------------------------------
//
//	O2Logger::
//	Get/Set DB parameter
//
// ---------------------------------------------------------------------------

bool
O2Logger::
SetLimit(uint type, uint n)
{
	if (n < MIN_RECORD)
		return false;

	uint64 *limit;
	O2LogRecords *records = ChooseLog(type, &limit);

	*limit = n;

	while (records->size() >= *limit) {
		records->pop_front();
	}
	return true;
}




uint64
O2Logger::
GetLogLimit(uint type)
{
	uint64 *limit;
	O2LogRecords *records = ChooseLog(type, &limit);
	return (*limit);
}




uint64
O2Logger::
Min(void)
{
	return (MIN_RECORD);
}




uint64
O2Logger::
Max(void)
{
	return (MAX_RECORD);
}




uint64
O2Logger::
Count(void)
{
	return (Log.size() + NetLog.size() + HokanLog.size() + IPFLog.size());
}




// ---------------------------------------------------------------------------
//
//	O2Logger::
//	File I/O
//
// ---------------------------------------------------------------------------

bool
O2Logger::
Save(bool clear)
{
#if 0
	if (records.empty())
		return false;

	struct _stat buf;
	if (_wstat(logdir.c_str(), &buf) == -1) {
		//create directory
		if (!CreateDirectory(logdir.c_str(), NULL))
			return false;
	}

	bool ret = false;

	long tzoffset;
	_get_timezone(&tzoffset);
	time_t t = sessiontime - tzoffset;

	struct tm tm;
	localtime_s(&tm, &t);

	wchar_t timestr[TIMESTR_BUFF_SIZE];
	wcsftime(timestr, TIMESTR_BUFF_SIZE, L"%Y-%m-%d-%H%M%S", &tm);
	wchar_t filename[MAX_PATH];
	swprintf(filename, MAX_PATH, L"%s\\%s.xml", logdir.c_str(), timestr);

	FILE *fp;
	if (_wfopen_s(&fp, filename, L"wb") == 0) {
		O2LogSelectCondition cond;

		cond.mask = LOG_XMLELM_ALL;
		cond.typemask = O2LT_ALL;
		cond.includefilelist = false;

		Lock();
		string out;
		InternalGet(cond, out);
		if (clear)
			records.clear();
		Unlock();

		fwrite(&out[0], 1, out.size(), fp);
		fclose(fp);
		ret = true;
	}
	return (ret);
#endif
	return false;
}




uint64
O2Logger::
InternalGet(const O2LogSelectCondition &cond, string &out)
{
	wchar_t tmp[16];
	wstring tmpstr;

	uint64 *limit;
	O2LogRecords *records = ChooseLog(cond.type, &limit);

	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	if (!cond.xsl.empty()) {
		xml += L"<?xml-stylesheet type=\"text/xsl\" href=\"";
		xml += cond.xsl;
		xml += L"\"?>"EOL;
	}

	xml += L"<logs>"EOL;

	//<info>
	if (cond.mask & LOG_XMLELM_INFO) {
		xml += L"<info>"EOL;
		{
			xml += L" <count>";
			swprintf_s(tmp, 16, L"%d", records->size());
			xml += tmp;
			xml += L"</count>"EOL;

			xml += L" <limit>";
			swprintf_s(tmp, 16, L"%d", *limit);
			xml += tmp;
			xml += L"</limit>"EOL;

			xml += L" <message>";
			xml += cond.message;
			xml += L"</message>"EOL;

			xml += L" <message_type>";
			xml += cond.message_type;
			xml += L"</message_type>"EOL;

			xml += L" <filename>";
			xml += cond.filename;
			xml += L"</filename>"EOL;
		}
		xml += L"</info>"EOL;
	}

	//<files>
	if (!logdir.empty() && cond.includefilelist) {
		xml += L"<files>"EOL;

		std::set<wstring> files;

		WIN32_FIND_DATA wfd;
		wchar_t filename[MAX_PATH];
		swprintf(filename, MAX_PATH, L"%s\\*.xml", logdir.c_str());
		HANDLE h = FindFirstFile(filename, &wfd);
		if (h != INVALID_HANDLE_VALUE) {
			do {
				if (wfd.nFileSizeLow > 0)
					files.insert(wfd.cFileName);
			} while (FindNextFile(h, &wfd));
			FindClose(h);
		}

		std::set<wstring>::iterator it;
		for (it = files.begin(); it != files.end(); it++) {
			xml += L" <file>";
			xml += *it;
			xml += L"</file>"EOL;
		}

		xml += L"</files>"EOL;
	}

	//<log>
	uint i = 0;
	for (O2LogRecordsRit it = records->rbegin(); it != records->rend(); it++) {
		O2LogRecord &rec = *it;

		xml += L"<log>"EOL;

		if (cond.mask & LOG_XMLELM_TYPE) {
			switch (rec.type) {
				case O2LT_INFO:
					xml += L" <type>info</type>"EOL;
					break;
				case O2LT_WARNING:
					xml += L" <type>warning</type>"EOL;
					break;
				case O2LT_ERROR:
					xml += L" <type>error</type>"EOL;
					break;
				case O2LT_FATAL:
					xml += L" <type>fatal</type>"EOL;
					break;
				case O2LT_IM:
					xml += L" <type>im</type>"EOL;
					break;
				case O2LT_NET:
					xml += L" <type>net</type>"EOL;
					break;
				case O2LT_NETERR:
					xml += L" <type>neterr</type>"EOL;
					break;
				case O2LT_HOKAN:
					xml += L" <type>hokan</type>"EOL;
					break;
				case O2LT_IPF:
					xml += L" <type>ipf</type>"EOL;
					break;
			}
		}

		if (cond.mask & LOG_XMLELM_DATETIME) {
			if (rec.date == 0)
				xml += L" <date></date>"EOL;
			else {
				long tzoffset;
				_get_timezone(&tzoffset);

				if (!cond.timeformat.empty()) {
					time_t t = rec.date - tzoffset;

					wchar_t timestr[TIMESTR_BUFF_SIZE];
					struct tm tm;
					gmtime_s(&tm, &t);
					wcsftime(timestr, TIMESTR_BUFF_SIZE, cond.timeformat.c_str(), &tm);
					xml += L" <date>";
					xml += timestr;
					xml += L"</date>"EOL;
				}
				else {
					time_t2datetime(rec.date, - tzoffset, tmpstr);
					xml += L" <date>";
					xml += tmpstr;
					xml += L"</date>"EOL;
				}
			}
		}

		if (cond.mask & LOG_XMLELM_MODULE) {
			makeCDATA(rec.module, tmpstr);
			xml += L" <module>";
			xml += tmpstr;
			xml += L"</module>"EOL;
		}

		if (cond.mask & LOG_XMLELM_IP) {
			if (rec.ip == 0)
				tmpstr = L"-";
#if !defined(_DEBUG)
			else if (rec.type != O2LT_IPF)
				ip2e(rec.ip, tmpstr);
#endif
			else
				ulong2ipstr(rec.ip, tmpstr);
			xml += L" <ip>";
			xml += tmpstr;
			xml += L"</ip>"EOL;
		}

		if (cond.mask & LOG_XMLELM_PORT) {
			if (rec.port == 0)
				wcscpy_s(tmp, 16, L"-");
			else
				swprintf_s(tmp, 16, L"%u", rec.port);
			xml += L" <port>";
			xml += tmp;
			xml += L"</port>"EOL;
		}

		if (cond.mask & LOG_XMLELM_MSG) {
			makeCDATA(rec.msg, tmpstr);
			xml += L" <msg>";
			xml += tmpstr;
			xml += L"</msg>"EOL;
		}

		xml += L"</log>"EOL;
		i++;
	}

	xml += L"</logs>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	
	return (records->size());
}




bool
O2Logger::
GetLogsFromFile(const O2LogSelectCondition &cond, string &out)
{
	bool ret = false;
	uint size = 0;
	struct _stat buf;
	FILE *fp = NULL;
	O2Logger *flog = NULL;
	string in;

	if (logdir.empty())
		return (false);

	wchar_t path[MAX_PATH];
	swprintf(path, MAX_PATH, L"%s\\%s", logdir.c_str(), cond.filename.c_str());

	//get filesize
	if (_wstat(path, &buf) == -1)
		goto cleanup;
	size = buf.st_size;
	if (size == 0)
		goto cleanup;

	in.resize(size);

	//load xml
	if (_wfopen_s(&fp, path, L"rb") != 0)
		goto cleanup;
	fread(&in[0], 1, size, fp);
	fclose(fp);
	fp = NULL;

	//log
	flog = new O2Logger(logdir.c_str());
	flog->ImportFromXML(_T(DEFAULT_XML_CHARSET), in.c_str(), in.size());
	flog->ExportToXML(cond, out);

	ret = true;

cleanup:
	if (flog) delete flog;
	if (fp) fclose(fp);
	return (ret);
}




// ---------------------------------------------------------------------------
//	AddLog (A)
// ---------------------------------------------------------------------------
bool
O2Logger::
AddLog(uint type, const wchar_t *module, ulong ip, ushort port, const char *format, ...)
{
	if (!O2DEBUG && type == O2LT_DEBUG)
		return true;

	va_list arg_ptr;
	va_start(arg_ptr, format);	   

	char msg[1024];
	vsprintf_s(msg, format, arg_ptr);
	wstring msgW;
	ToUnicode(L"shift_jis", msg, strlen(msg), msgW);

	O2LogRecord rec;
	rec.type = type;
	rec.date = time(NULL);
	rec.module = module;
	rec.ip = ip;
	rec.port = port;
	rec.msg = msgW;

	uint64 *limit;
	O2LogRecords *records;
	switch (type) {
		case O2LT_NET:
		case O2LT_NETERR:
			records = ChooseLog(LOGGER_NETLOG, &limit);
			break;
		case O2LT_HOKAN:
			records = ChooseLog(LOGGER_HOKANLOG, &limit);
			break;
		case O2LT_IPF:
			records = ChooseLog(LOGGER_IPFLOG, &limit);
			break;
		default:
			records = ChooseLog(LOGGER_LOG, &limit);
			break;
	}

	Lock();
	records->push_back(rec);
	Unlock();

	if (records->size() > *limit) {
		if (!logdir.empty() && Save(true)) {
			Lock();
			records->clear();
			Unlock();
		}
		else {
			while (records->size() > *limit) {
				records->pop_front();
			}
		}
	}

#if DEBUGOUT && defined(_DEBUG)
	wchar_t tmp[1024];
	struct tm tm;
	gmtime_s(&tm, &rec.date);
	wcsftime(tmp, 1024, L"%Y%m%d-%H:%M:%S ", &tm);
	TRACEW(tmp);
	TRACEW(L"(");
	TRACEW(module);
	TRACEW(L"):");
	TRACEW(msgW.c_str());
	TRACEW(L"\n");
#endif

	return true;
}

// ---------------------------------------------------------------------------
//	AddLog (W)
// ---------------------------------------------------------------------------

bool
O2Logger::
AddLog(uint type, const wchar_t *module, ulong ip, ushort port, const wchar_t *format, ...)
{
	if (!O2DEBUG && type == O2LT_DEBUG)
		return true;

	va_list arg_ptr;
	va_start(arg_ptr, format);	   

	wchar_t msg[1024];
	vswprintf(msg, 1024, format, arg_ptr);

	O2LogRecord rec;
	rec.type = type;
	rec.date = time(NULL);
	rec.module = module;
	rec.ip = ip;
	rec.port = port;
	rec.msg = msg;

	uint64 *limit;
	O2LogRecords *records;
	switch (type) {
		case O2LT_NET:
		case O2LT_NETERR:
			records = ChooseLog(LOGGER_NETLOG, &limit);
			break;
		case O2LT_HOKAN:
			records = ChooseLog(LOGGER_HOKANLOG, &limit);
			break;
		case O2LT_IPF:
			records = ChooseLog(LOGGER_IPFLOG, &limit);
			break;
		default:
			records = ChooseLog(LOGGER_LOG, &limit);
			break;
	}

	Lock();
	records->push_back(rec);
	Unlock();


	if (records->size() > *limit) {
		if (!logdir.empty() && Save(true)) {
			Lock();
			records->clear();
			Unlock();
		}
		else {
			while (records->size() > *limit) {
				Lock();
				records->pop_front();
				Unlock();
			}
		}
	}

#if DEBUGOUT && defined(_DEBUG)
	wchar_t tmp[1024];
	struct tm tm;
	gmtime_s(&tm, &rec.date);
	wcsftime(tmp, 1024, L"%Y%m%d-%H:%M:%S ", &tm);
	TRACEW(tmp);
	TRACEW(L"(");
	TRACEW(module);
	TRACEW(L"):");
	TRACEW(msg);
	TRACEW(L"\n");
#endif

	return true;
}




bool
O2Logger::
GetLogMessage(uint type, const wchar_t *module, wstring &out)
{
	bool ret = false;

	Lock();
	uint64 *limit;
	O2LogRecords *records = ChooseLog(type, &limit);
	O2LogRecords::reverse_iterator rit;
	for (rit = records->rbegin(); rit != records->rend(); rit++) {
		if (rit->module == module) {
			out = rit->msg;
			ret = true;
			break;
		}
	}
	Unlock();

	return (ret);
}




uint64
O2Logger::
ExportToXML(const O2LogSelectCondition &cond, string &out)
{
	Lock();
	uint64 ret = InternalGet(cond, out);
	Unlock();
	
	return (ret);
}




uint64
O2Logger::
ImportFromXML(const wchar_t *charset, const char *in, uint inlen)
{
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2Logger_SAX2Handler handler(this);
	parser->setContentHandler(&handler);
	parser->setErrorHandler(&handler);

	MemBufInputSource source((const XMLByte*)in, inlen, "");

	Lock();
	{
		try {
			parser->parse(source);
		}
		catch (const OutOfMemoryException &e) {
			AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Out of Memory: %s", e.getMessage());
		}
		catch (const XMLException &e) {
			AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Exception: %s", e.getMessage());
		}
		catch (...) {
			AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Unexpected exception during parsing.");
		}
	}
	Unlock();

	delete parser;
	return (handler.GetParseNum());
}




// ---------------------------------------------------------------------------
//
//	O2Logger_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

#if 0
HRESULT STDMETHODCALLTYPE
O2Logger::
startElement( 
	/* [in] */ const wchar_t __RPC_FAR *nmspace,
	/* [in] */ int nmspace_len,
	/* [in] */ const wchar_t __RPC_FAR *localname,
	/* [in] */ int localname_len,
	/* [in] */ const wchar_t __RPC_FAR *qname,
	/* [in] */ int qname_len,
	/* [in] */ ISAXAttributes __RPC_FAR *attr)
{
	if (wcsncmp(localname, L"log", localname_len) == 0) {
		if (CurRecord) delete CurRecord;
		CurRecord = new O2LogRecord();
		CurElm = LOG_XMLELM_NONE;
	}
	else if (wcsncmp(localname, L"type", localname_len) == 0) {
		CurElm = LOG_XMLELM_TYPE;
	}
	else if (wcsncmp(localname, L"date", localname_len) == 0) {
		CurElm = LOG_XMLELM_DATETIME;
	}
	else if (wcsncmp(localname, L"module", localname_len) == 0) {
		CurElm = LOG_XMLELM_MODULE;
	}
	else if (wcsncmp(localname, L"msg", localname_len) == 0) {
		CurElm = LOG_XMLELM_MSG;
	}
	else {
		CurElm = LOG_XMLELM_NONE;
	}
	return S_OK;
}




HRESULT STDMETHODCALLTYPE
O2Logger::
endElement( 
	/* [in] */ const wchar_t __RPC_FAR *nmspace,
	/* [in] */ int nmspace_len,
	/* [in] */ const wchar_t __RPC_FAR *localname,
	/* [in] */ int localname_len,
	/* [in] */ const wchar_t __RPC_FAR *qname,
	/* [in] */ int qname_len)
{
	if (wcsncmp(localname, L"log", localname_len) == 0) {
		bool error = false;
		
		if (!error && CurRecord->type == 0)
			error = true;
		if (!error && CurRecord->date == 0)
			error = true;
		if (!error && CurRecord->module.empty())
			error = true;
		if (!error && CurRecord->msg.empty())
			error = true;

		if (!error) {
			Lock();
			records.push_back(*CurRecord);
			Unlock();
		}
	}
	CurElm = LOG_XMLELM_NONE;
	return S_OK;
}




HRESULT STDMETHODCALLTYPE
O2Logger::
characters( 
	/* [in] */ const wchar_t __RPC_FAR *chars,
	/* [in] */ int len)
{
	switch (CurElm) {
		case LOG_XMLELM_TYPE:
			if (CurRecord && CurRecord->type == 0) {
				if (wcsncmp(chars, L"info", len) == 0)
					CurRecord->type = O2LT_INFO;
				else if (wcsncmp(chars, L"warning", len) == 0)
					CurRecord->type = O2LT_WARNING;
				else if (wcsncmp(chars, L"error", len) == 0)
					CurRecord->type = O2LT_ERROR;
				else if (wcsncmp(chars, L"fatal", len) == 0)
					CurRecord->type = O2LT_FATAL;
				else if (wcsncmp(chars, L"net", len) == 0)
					CurRecord->type = O2LT_NET;
				else if (wcsncmp(chars, L"debug", len) == 0)
					CurRecord->type = O2LT_DEBUG;
			}
			break;
		case LOG_XMLELM_DATETIME:
			if (CurRecord)
				CurRecord->date = datetime2time_t(chars, len);
			break;
		case LOG_XMLELM_MODULE:
			if (CurRecord) {
				CurRecord->module.assign(chars, len);
			}
			break;
		case LOG_XMLELM_MSG:
			if (CurRecord) {
				uint size = CurRecord->msg.size();
				CurRecord->msg.resize(size + len);
				wcsncpy_s(&CurRecord->msg[size], len, chars, len);
			}
			break;
	}
	return S_OK;
}
#endif

O2Logger_SAX2Handler::
O2Logger_SAX2Handler(O2Logger *lgr)
	: SAX2Handler(MODULE, lgr)
	, CurRecord(NULL)
{
}

O2Logger_SAX2Handler::
~O2Logger_SAX2Handler(void)
{
}

uint64
O2Logger_SAX2Handler::
GetParseNum(void)
{
	return (ParseNum);
}

void
O2Logger_SAX2Handler::
endDocument(void)
{
	if (CurRecord) {
		delete CurRecord;
		CurRecord = NULL;
	}
}

void
O2Logger_SAX2Handler::
startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes& attrs)
{
}

void
O2Logger_SAX2Handler::
endElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname)
{
}

void
O2Logger_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
}
