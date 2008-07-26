/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2IMDB.cpp
 * description	: 
 *
 */

#include "O2IMDB.h"
#include "file.h"
#include "dataconv.h"
#include <sys/types.h>
#include <sys/stat.h>

#define MODULE			L"IMDB"
#define MIN_RECORDS		1
#define MAX_RECORDS		UINT_MAX
#define DEFAULT_LIMIT	UINT_MAX




// ---------------------------------------------------------------------------
//
//	O2IMDB::
//	Constructor/Destructor
//
// ---------------------------------------------------------------------------

O2IMDB::
O2IMDB(O2Logger *lgr)
	: Logger(lgr)
	, Limit(DEFAULT_LIMIT)
	, NewMessageFlag(false)
{
}




O2IMDB::
~O2IMDB()
{
}




void
O2IMDB::
Expire(void)
{
	Lock();
	{
		while (Messages.size() > Limit)
			Messages.pop_front();
	}
	Unlock();
}




// ---------------------------------------------------------------------------
//
//	O2IMDB::
//	Get/Set DB parameter
//
// ---------------------------------------------------------------------------

bool
O2IMDB::
SetLimit(size_t n)
{
	if (n < MIN_RECORDS)
		return false;

	Limit = n;
	if (Messages.size() > Limit)
		Expire();
	return true;
}




size_t
O2IMDB::
GetLimit(void)
{
	return (Limit);
}




size_t
O2IMDB::
Min(void)
{
	return (MIN_RECORDS);
}




size_t
O2IMDB::
Max(void)
{
	return (MAX_RECORDS);
}




size_t
O2IMDB::
Count(void)
{
	return (Messages.size());
}




// ---------------------------------------------------------------------------
//
//	O2IMDB::
//	Add/Get methods
//
// ---------------------------------------------------------------------------

bool
O2IMDB::
AddMessage(O2IMessage &im)
{
	Lock();
	if (im.date == 0) {
		im.date = time(NULL);
		im.key.random();
	}
	Messages.push_back(im);
	NewMessageFlag = true;
	Unlock();

	Expire();
	return true;
}




bool
O2IMDB::
DeleteMessage(const hashListT &keylist)
{
	bool ret = false;

	Lock();
	{
		hashListT::const_iterator it;
		for (it = keylist.begin(); it != keylist.end(); it++) {
			O2IMessage dummy;
			dummy.key = *it;
			O2IMessagesIt it = std::find(Messages.begin(), Messages.end(), dummy);
			if (it != Messages.end()) {
				Messages.erase(it);
				ret = true;
			}
		}
	}
	Unlock();
	return (ret);
}




bool
O2IMDB::
Exist(const O2IMessage &im)
{
	bool ret = false;

	Lock();
	{
		O2IMessagesIt it = std::find(Messages.begin(), Messages.end(), im);
		if (it != Messages.end())
			ret = true;
	}
	Unlock();
	return (ret);
}




size_t
O2IMDB::
GetMessages(O2IMessages &out)
{
	Lock();
	{
		for (O2IMessagesIt it = Messages.begin(); it != Messages.end(); it++)
			out.push_back(*it);
	}
	Unlock();
	return (out.size());
}




bool
O2IMDB::
HaveNewMessage(void)
{
	Lock();
	bool ret = NewMessageFlag;
	NewMessageFlag = false;
	Unlock();

	return (ret);
}




bool
O2IMDB::
MakeSendXML(O2Profile *profile, const wchar_t *charset, const wchar_t *msg, string &out)
{
	wstring xml;
	wstring tmpstr;
	wchar_t tmp[16];

	if (profile->GetIP() == 0)
		return false;

	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += charset;
	xml += L"\"?>"EOL;

	xml	+= L"<messages>"EOL;
	xml	+= L"<message>"EOL;

	ip2e(profile->GetIP(), tmpstr);
	xml += L" <ip>";
	xml += tmpstr;
	xml += L"</ip>"EOL;

	swprintf_s(tmp, 16, L"%d", profile->GetP2PPort());
	xml += L" <port>";
	xml += tmp;
	xml += L"</port>"EOL;
	
	hashT id;
	profile->GetID(id);
	id.to_string(tmpstr);
	xml += L" <id>";
	xml += tmpstr;
	xml += L"</id>"EOL;

	wstring pubkey;
	profile->GetPubkeyW(pubkey);
	xml += L" <pubkey>";
	xml += pubkey;
	xml += L"</pubkey>"EOL;

	xml += L" <name>";
	xml += profile->GetNodeNameW();
	xml += L"</name>"EOL;

	xml += L" <msg><![CDATA[";
	xml += msg;
	xml += L"]]></msg>"EOL;

	xml	+= L"</message>"EOL;
	xml	+= L"</messages>"EOL;

	FromUnicode(charset, xml, out);
	return true;
}




bool
O2IMDB::
MakeSendXML(const O2IMessage &im, string &out)
{
	wstring xml;
	wstring tmpstr;
	wchar_t tmp[16];

	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += _T(DEFAULT_XML_CHARSET);
	xml += L"\"?>"EOL;

	xml	+= L"<messages>"EOL;
	xml	+= L"<message>"EOL;

	ip2e(im.ip, tmpstr);
	xml += L" <ip>";
	xml += tmpstr;
	xml += L"</ip>"EOL;

	swprintf_s(tmp, 16, L"%d", im.port);
	xml += L" <port>";
	xml += tmp;
	xml += L"</port>"EOL;
	
	im.id.to_string(tmpstr);
	xml += L" <id>";
	xml += tmpstr;
	xml += L"</id>"EOL;

	im.pubkey.to_string(tmpstr);
	xml += L" <pubkey>";
	xml += tmpstr;
	xml += L"</pubkey>"EOL;

	xml += L" <name>";
	xml += im.name;
	xml += L"</name>"EOL;

	xml += L" <msg><![CDATA[";
	xml += im.msg;
	xml += L"]]></msg>"EOL;

	im.key.to_string(tmpstr);
	xml += L" <key>";
	xml += tmpstr;
	xml += L"</key>"EOL;

	for (hashListT::const_iterator it = im.paths.begin(); it != im.paths.end(); it++) {
		it->to_string(tmpstr);
		xml += L" <path>";
		xml += tmpstr;
		xml += L"</path>"EOL;
	}

	xml	+= L"</message>"EOL;
	xml	+= L"</messages>"EOL;

	FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
	return true;
}




// ---------------------------------------------------------------------------
//
//	O2IMDB::
//	File I/O
//
// ---------------------------------------------------------------------------

bool
O2IMDB::
Save(const wchar_t *filename, bool clear)
{
	if (Messages.empty())
		return false;

	O2IMSelectCondition cond(IM_XMLELM_ALL);
	string out;
	ExportToXML(cond, out);

	Lock();
	if (clear)
		Messages.clear();
	Unlock();
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
O2IMDB::
Load(const wchar_t *filename)
{
	struct _stat st;
	if (_wstat(filename, &st) == -1)
		return false;
	if (st.st_size == 0)
		return false;
	ImportFromXML(filename, NULL, 0);
	NewMessageFlag = false;
	return true;
}




// ---------------------------------------------------------------------------
//
//	O2IMDB::
//	XML I/O
//
// ---------------------------------------------------------------------------

size_t
O2IMDB::
ExportToXML(O2IMSelectCondition &cond, string &out)
{
	wchar_t tmp[16];
	wstring xml;

	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	if (!cond.xsl.empty()) {
		xml += L"<?xml-stylesheet type=\"text/xsl\" href=\"";
		xml += cond.xsl;
		xml += L"\"?>"EOL;
	}
	xml	+= L"<messages>"EOL;

	if (cond.mask & IM_XMLELM_INFO) {
		xml += L"<info>"EOL;
		{
			xml += L" <count>";
			swprintf_s(tmp, 16, L"%d", Messages.size());
			xml += tmp;
			xml += L"</count>"EOL;

			xml += L" <sort>";
			xml += cond.sort;
			xml += L"</sort>"EOL;

			wstring message;
			wstring message_type;
			GetXMLMessage(message, message_type);

			xml += L" <message>";
			xml += message;
			xml += L"</message>"EOL;

			xml += L" <message_type>";
			xml += message_type;
			xml += L"</message_type>"EOL;
		}
		xml += L"</info>"EOL;
	}

	size_t out_count = 0;

	Lock();
	if (!cond.desc) {
		O2IMessages::iterator it;
		for (it = Messages.begin(); it != Messages.end(); it++) {
			MakeIMElement(*it, cond, xml);
			out_count++;
			if (cond.limit && out_count >= cond.limit)
				break;
		}
	}
	else {
		O2IMessages::reverse_iterator it;
		for (it = Messages.rbegin(); it != Messages.rend(); it++) {
			MakeIMElement(*it, cond, xml);
			out_count++;
			if (cond.limit && out_count >= cond.limit)
				break;
		}
	}
	Unlock();

	xml	+= L"</messages>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	
	return (out_count);
}

void
O2IMDB::
MakeIMElement(O2IMessage &im, O2IMSelectCondition &cond, wstring &xml)
{
	wchar_t tmp[16];
	wstring tmpstr;

	xml += L"<message>"EOL;

	if (cond.mask & IM_XMLELM_IP) {
		ip2e(im.ip, tmpstr);
		xml += L" <ip>";
		xml += tmpstr;
		xml += L"</ip>"EOL;
	}

	if (cond.mask & IM_XMLELM_PORT) {
		swprintf_s(tmp, 16, L"%d", im.port);
		xml += L" <port>";
		xml += tmp;
		xml += L"</port>"EOL;
	}

	if (cond.mask & IM_XMLELM_ID) {
		im.id.to_string(tmpstr);
		xml += L" <id>";
		xml += tmpstr;
		xml += L"</id>"EOL;
	}

	if (cond.mask & IM_XMLELM_PUBKEY) {
		im.pubkey.to_string(tmpstr);
		xml += L" <pubkey>";
		xml += tmpstr;
		xml += L"</pubkey>"EOL;
	}

	if (cond.mask & IM_XMLELM_NAME) {
		xml += L" <name>";
		xml += im.name;
		xml += L"</name>"EOL;
	}

	if (cond.mask & IM_XMLELM_DATE) {
		if (im.date == 0)
			xml += L" <date></date>"EOL;
		else {
			long tzoffset;
			_get_timezone(&tzoffset);
			if (!cond.timeformat.empty()) {
				time_t t = im.date - tzoffset;

				wchar_t timestr[TIMESTR_BUFF_SIZE];
				struct tm tm;
				gmtime_s(&tm, &t);
				wcsftime(timestr, TIMESTR_BUFF_SIZE, cond.timeformat.c_str(), &tm);
				xml += L" <date>";
				xml += timestr;
				xml += L"</date>"EOL;
			}
			else {
				time_t2datetime(im.date, - tzoffset, tmpstr);
				xml += L" <date>";
				xml += tmpstr;
				xml += L"</date>"EOL;
			}
		}
	}

	if (cond.mask & IM_XMLELM_MSG) {
		xml += L" <msg><![CDATA[";
		xml += im.msg;
		xml += L"]]></msg>"EOL;
	}

	if (cond.mask & IM_XMLELM_KEY) {
		im.key.to_string(tmpstr);
		xml += L" <key>";
		xml += tmpstr;
		xml += L"</key>"EOL;
	}

	if (cond.mask & IM_XMLELM_MINE) {
		xml += L" <mine>";
		xml += im.mine ? L"1" : L"0";
		xml += L"</mine>"EOL;
	}

	xml += L"</message>"EOL;
}




size_t
O2IMDB::
ImportFromXML(const wchar_t *filename, const char *in, uint len)
{
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2IMDB_SAX2Handler handler(Logger, this);
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
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Out of Memory: %s", e.getMessage());
		}
	}
	catch (const XMLException &e) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Exception: %s", e.getMessage());
		}
	}
	catch (...) {
		if (Logger) {
			Logger->AddLog(O2LT_ERROR, MODULE, 0, 0,
				L"SAX2: Unexpected exception during parsing.");
		}
	}

	delete parser;
	return (handler.GetParseNum());
}




// ---------------------------------------------------------------------------
//
//	O2IMDB_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

O2IMDB_SAX2Handler::
O2IMDB_SAX2Handler(O2Logger *lgr, O2IMDB *imdb)
	: SAX2Handler(MODULE, lgr)
	, IMDB(imdb)
	, CurIM(NULL)
{
}

O2IMDB_SAX2Handler::
~O2IMDB_SAX2Handler(void)
{
}

size_t
O2IMDB_SAX2Handler::
GetParseNum(void)
{
	return (ParseNum);
}

void
O2IMDB_SAX2Handler::
endDocument(void)
{
	if (CurIM) {
		delete CurIM;
		CurIM = NULL;
	}
}

void
O2IMDB_SAX2Handler::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{

	CurElm = IM_XMLELM_NONE;

	if (MATCHLNAME(L"message")) {
		if (CurIM)
			delete CurIM;
		CurIM = new O2IMessage();
		CurElm = IM_XMLELM_NONE;
	}
	else if (MATCHLNAME(L"ip")) {
		CurElm = IM_XMLELM_IP;
	}
	else if (MATCHLNAME(L"port")) {
		CurElm = IM_XMLELM_PORT;
	}
	else if (MATCHLNAME(L"id")) {
		CurElm = IM_XMLELM_ID;
	}
	else if (MATCHLNAME(L"pubkey")) {
		CurElm = IM_XMLELM_PUBKEY;
	}
	else if (MATCHLNAME(L"name")) {
		CurElm = IM_XMLELM_NAME;
	}
	else if (MATCHLNAME(L"date")) {
		CurElm = IM_XMLELM_DATE;
	}
	else if (MATCHLNAME(L"msg")) {
		CurElm = IM_XMLELM_MSG;
	}
	else if (MATCHLNAME(L"key")) {
		CurElm = IM_XMLELM_KEY;
	}
	else if (MATCHLNAME(L"mine")) {
		CurElm = IM_XMLELM_MINE;
	}
	else if (MATCHLNAME(L"path")) {
		CurElm = IM_XMLELM_PATH;
	}
}

void
O2IMDB_SAX2Handler::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	CurElm = IM_XMLELM_NONE;
	if (!CurIM || !MATCHLNAME(L"message"))
		return;

	IMDB->AddMessage(*CurIM);
}




void
O2IMDB_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
	if (!CurIM)
		return;

	switch (CurElm) {
		case IM_XMLELM_IP:
			CurIM->ip = e2ip(chars, length);
			break;
		case IM_XMLELM_PORT:
			CurIM->port = (ushort)wcstoul(chars, NULL, 10);
			break;
		case IM_XMLELM_ID:
			CurIM->id.assign(chars, length);
			break;
		case IM_XMLELM_PUBKEY:
			CurIM->pubkey.assign(chars, length);
			break;
		case IM_XMLELM_NAME:
			CurIM->name.assign(chars, length);
			break;
		case IM_XMLELM_DATE:
			CurIM->date = datetime2time_t(chars, length);
			break;
		case IM_XMLELM_MSG:
			CurIM->msg.assign(chars, length);
			break;
		case IM_XMLELM_KEY:
			CurIM->key.assign(chars, length);
			break;
		case IM_XMLELM_MINE:
			CurIM->mine = chars[0] == L'0' ? false : true;
			break;
		case IM_XMLELM_PATH:
			{
				hashT id;
				id.assign(chars, length);
				CurIM->paths.push_back(id);
				while (CurIM->paths.size() > O2_BROADCAST_PATH_LIMIT)
					CurIM->paths.pop_front();
			}
			break;
	}
}
