/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2KeyDB.cpp
 * description	: 
 *
 */

#include "O2KeyDB.h"
#include "file.h"
#include "dataconv.h"
#include <boost/regex.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>

#define MODULE			L"KeyDB"
#define MIN_RECORDS		1000
#define MAX_RECORDS		15000
#define DEFAULT_LIMIT	3000




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	Constructor/Destructor
//
// ---------------------------------------------------------------------------

O2KeyDB::
O2KeyDB(const wchar_t *name, bool ip0port0, O2Logger *lgr)
	: DBName(name)
	, IP0Port0(ip0port0)
	, Limit(DEFAULT_LIMIT)
	, Logger(lgr)
{
	srand((unsigned)time(NULL));
}




O2KeyDB::
~O2KeyDB()
{
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	Local helper methods
//
// ---------------------------------------------------------------------------

bool
O2KeyDB::
CheckKey(O2Key &key)
{
	if (IP0Port0) {
		key.nodeid.clear();
		key.ip = 0;
		key.port = 0;
	}
	else if (key.nodeid == SelfNodeID
			|| !is_globalIP(key.ip)
			|| key.port == 0
			|| key.size == 0
			|| key.url.empty()) {
		return false;
	}
	return true;
}




void
O2KeyDB::
Expire(void)
{
	O2Keys::nth_index<2>::type& keys = Keys.get<2>();

	Lock();
	while (!keys.empty() && keys.size() > Limit) {
		O2Keys::nth_index_iterator<2>::type it = keys.end();
		it--;
		keys.erase(it);
	}
	Unlock();
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	Get/Set DB parameter
//
// ---------------------------------------------------------------------------

void
O2KeyDB::
SetSelfNodeID(const hashT &hash)
{
	SelfNodeID = hash;
}




bool
O2KeyDB::
SetLimit(uint64 n)
{
	//if (n < MIN_RECORDS)
	//	return false;

	Limit = n;
	if (Keys.size() > Limit) {
		Expire();
	}
	return true;
}




uint64
O2KeyDB::
GetLimit(void)
{
	return (Limit);
}




uint64
O2KeyDB::
Min(void)
{
	return (MIN_RECORDS);
}




uint64
O2KeyDB::
Max(void)
{
	return (MAX_RECORDS);
}




uint64
O2KeyDB::
Count(void)
{
	return (Keys.size());
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	Set key's property
//
// ---------------------------------------------------------------------------

bool
O2KeyDB::
SetNote(const hashT &hash, const wchar_t *title, uint64 datsize)
{
	//
	// この関数はQueryDBとして使われているとき＆補完時に呼ばれる
	//
	bool ret = false;

	Lock();
	{
		O2Keys::nth_index<1>::type& keys = Keys.get<1>();
		O2Keys::nth_index_iterator<1>::type it1 = keys.lower_bound(hash);
		O2Keys::nth_index_iterator<1>::type it2 = keys.upper_bound(hash);
		O2Keys::nth_index_iterator<1>::type it;
		for (it = it1; it != it2; it++) {
			O2Key key = *it;
			key.title = title;
			key.size = datsize;
			key.date = time(NULL);
			keys.replace(it, key);
			ret = true;
		}
	}
	Unlock();

	return (ret);
}

bool
O2KeyDB::
SetDate(const hashT &hash, time_t t)
{
	//
	// この関数はQueryDBとして使われているときに呼ばれる
	//
	bool ret = false;

	Lock();
	{
		O2Keys::nth_index<1>::type& keys = Keys.get<1>();
		O2Keys::nth_index_iterator<1>::type it1 = keys.lower_bound(hash);
		O2Keys::nth_index_iterator<1>::type it2 = keys.upper_bound(hash);
		O2Keys::nth_index_iterator<1>::type it;
		for (it = it1; it != it2; it++) {
			O2Key key = *it;
			key.date = t;
			keys.replace(it, key);
			ret = true;
		}
	}
	Unlock();

	return (ret);
}

bool
O2KeyDB::
SetEnable(const hashT &hash, bool flag)
{
	//
	// この関数はQueryDBとして使われているときに呼ばれる
	//
	bool ret = false;

	Lock();
	{
		O2Keys::nth_index<1>::type& keys = Keys.get<1>();
		O2Keys::nth_index_iterator<1>::type it1 = keys.lower_bound(hash);
		O2Keys::nth_index_iterator<1>::type it2 = keys.upper_bound(hash);
		O2Keys::nth_index_iterator<1>::type it;
		for (it = it1; it != it2; it++) {
			O2Key key = *it;
			key.enable = flag;
			keys.replace(it, key);
			ret = true;
		}
	}
	Unlock();

	return (ret);
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	Get/Add/Delete keys
//
// ---------------------------------------------------------------------------

uint64
O2KeyDB::
GetKeyList(O2KeyList &ret, time_t date_le)
{
	Lock();
	{
		O2Keys::iterator it;
		for (it = Keys.begin(); it != Keys.end(); it++) {
			if (date_le == 0 || it->date < date_le)
				ret.push_back(*it);
		}
	}
	Unlock();

	return (ret.size());
}

uint64
O2KeyDB::
GetKeyList(const hashT &target, O2KeyList &ret)
{
	Lock();
	{
		O2Keys::nth_index<1>::type& keys = Keys.get<1>();
		O2Keys::nth_index_iterator<1>::type it1 = keys.lower_bound(target);
		O2Keys::nth_index_iterator<1>::type it2 = keys.upper_bound(target);
		O2Keys::nth_index_iterator<1>::type it;
		for (it = it1; it != it2; it++) {
			ret.push_back(*it);
		}
	}
	Unlock();

	return (ret.size());
}




uint
O2KeyDB::
AddKey(O2Key &k)
{
	// ret
	//	1: new
	//	2: update
	uint ret = 0;

	if (!CheckKey(k))
		return (0);
	k.makeidkeyhash();

//	hashBitsetT d = k.hash.bits ^ SelfNodeID.bits;
//	k.distance = d.bit_length();
	k.distance = hash_xor_bitlength(k.hash, SelfNodeID);

	if (!IP0Port0)
		k.date = time(NULL);

	Lock();
	{
		O2Keys::iterator it = Keys.find(k.idkeyhash);
		if (it == Keys.end()) {
			Keys.insert(k);
			ret = 1;
		}
		else {
			k.marge(*it);
			Keys.replace(it, k);
			ret = 2;
		}
	}
	Unlock();
	return (ret);
}




uint64
O2KeyDB::
DeleteKey(const hashT &hash)
{
	uint64 count = 0;

	Lock();
	{
		O2Keys::nth_index<1>::type& keys = Keys.get<1>();
		count = keys.size();

		keys.erase(hash);
		count -= keys.size();
	}
	Unlock();
	return (count);
}

uint64
O2KeyDB::
DeleteKeyByNodeID(const hashT &nodeid)
{
	uint64 count = 0;

	Lock();
	{
		count = Keys.size();
		O2Keys::iterator it = Keys.begin();
		while (it != Keys.end()) {
			if (it->nodeid == nodeid)
				it = Keys.erase(it);
			else
				it++;
		}
		count -= Keys.size();
	}
	Unlock();
	return (count);
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB::
//	File I/O
//
// ---------------------------------------------------------------------------

bool
O2KeyDB::
Save(const wchar_t *filename)
{
	O2KeySelectCondition cond(KEY_XMLELM_COMMON | KEY_XMLELM_DATE | KEY_XMLELM_ENABLE);
	string out;
	ExportToXML(cond, out);
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
O2KeyDB::
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
//	O2KeyDB::
//	XML I/O
//
// ---------------------------------------------------------------------------

uint64
O2KeyDB::
ExportToXML(O2KeySelectCondition &cond, string &out)
{
	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;

	xml	+= L"<keys>"EOL;

	if (cond.mask & KEY_XMLELM_INFO) {
		xml += L"<info>"EOL;
		{
			wchar_t tmp[16];
			swprintf_s(tmp, 16, L"%d", Keys.size());
			xml += L" <count>";
			xml += tmp;
			xml += L"</count>"EOL;

			swprintf_s(tmp, 16, L"%d", Limit);
			xml += L" <limit>";
			xml += tmp;
			xml += L"</limit>"EOL;

			xml += L" <id>";
			wstring idstr;
			SelfNodeID.to_string(idstr);
			xml += idstr;
			xml += L"</id>"EOL;

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

	uint64 out_count = 0;
	Lock();
	{
		if (cond.orderbydate) {
			O2Keys::nth_index<3>::type& keys = Keys.get<3>();
			O2Keys::nth_index_iterator<3>::type it;

			for (it = keys.begin(); it != keys.end(); it++) {
				MakeKeyElement(*it, cond, xml);
				out_count++;
				if (cond.limit && out_count >= cond.limit)
					break;
			}
		}
		else {
			O2Keys::nth_index<2>::type& keys = Keys.get<2>();
			O2Keys::nth_index_iterator<2>::type it;

			for (it = keys.begin(); it != keys.end(); it++) {
				MakeKeyElement(*it, cond, xml);
				out_count++;
				if (cond.limit && out_count >= cond.limit)
					break;
			}
		}
	}
	Unlock();

	xml	+= L"</keys>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	return (out_count);
}

uint64
O2KeyDB::
ExportToXML(const O2KeyList &keys, string &out)
{
	O2KeySelectCondition cond;

	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	xml	+= L"<keys>"EOL;

	uint64 out_count = 0;
	O2KeyList::const_iterator it;
	for (it = keys.begin(); it != keys.end(); it++) {
		MakeKeyElement(*it, cond, xml);
		out_count++;
	}

	xml	+= L"</keys>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	return (out_count);
}

uint64
O2KeyDB::
ExportToXML(const O2Key &key, string &out)
{
	O2KeySelectCondition cond;

	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	xml	+= L"<keys>"EOL;

	MakeKeyElement(key, cond, xml);

	xml	+= L"</keys>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	return (1);
}

void
O2KeyDB::
MakeKeyElement(const O2Key &key, O2KeySelectCondition &cond, wstring &xml)
{
	wstring tmpstr;
	wchar_t tmp[32];

	xml += L"<key>"EOL;

	if (cond.mask & KEY_XMLELM_HASH) {
		key.hash.to_string(tmpstr);
		xml += L" <hash>";
		xml += tmpstr;
		xml += L"</hash>"EOL;
	}

	if (cond.mask & KEY_XMLELM_NODEID) {
		key.nodeid.to_string(tmpstr);
		xml += L" <nodeid>";
		xml += tmpstr;
		xml += L"</nodeid>"EOL;
	}

	if (cond.mask & KEY_XMLELM_IP) {
		ip2e(key.ip, tmpstr);
		xml += L" <ip>";
		xml += tmpstr;
		xml += L"</ip>"EOL;
	}

	if (cond.mask & KEY_XMLELM_PORT) {
		swprintf_s(tmp, 16, L"%d", key.port);
		xml += L" <port>";
		xml += tmp;
		xml += L"</port>"EOL;
	}

	if (cond.mask & KEY_XMLELM_SIZE) {
		xml += L" <size>";
		swprintf_s(tmp, 32, L"%I64u", key.size);
		xml += tmp;
		xml += L"</size>"EOL;
	}

	if (cond.mask & KEY_XMLELM_URL) {
		xml += L" <url>";
		xml += key.url;
		xml += L"</url>"EOL;
	}

	if (cond.mask & KEY_XMLELM_TITLE) {
		makeCDATA(key.title, tmpstr);
		xml += L" <title>";
		xml += tmpstr;
		xml += L"</title>"EOL;
	}

	if (cond.mask & KEY_XMLELM_NOTE) {
		makeCDATA(key.note, tmpstr);
		xml += L" <note>";
		xml += tmpstr;
		xml += L"</note>"EOL;
	}

	if (cond.mask & KEY_XMLELM_IDKEYHASH) {
		key.idkeyhash.to_string(tmpstr);
		xml += L" <idkeyhash>";
		xml += tmpstr;
		xml += L"</idkeyhash>"EOL;
	}

	if (cond.mask & KEY_XMLELM_DISTANCE) {
		swprintf_s(tmp, 16, L"%u", key.distance);
		xml += L" <distance>";
		xml += tmp;
		xml += L"</distance>"EOL;
	}

	if (cond.mask & KEY_XMLELM_DATE) {
		if (key.date == 0)
			xml += L" <date></date>"EOL;
		else {
			long tzoffset;
			_get_timezone(&tzoffset);
			if (!cond.timeformat.empty()) {
				time_t t = key.date - tzoffset;

				wchar_t timestr[TIMESTR_BUFF_SIZE];
				struct tm tm;
				gmtime_s(&tm, &t);
				wcsftime(timestr, TIMESTR_BUFF_SIZE, cond.timeformat.c_str(), &tm);
				xml += L" <date>";
				xml += timestr;
				xml += L"</date>"EOL;
			}
			else {
				time_t2datetime(key.date, -tzoffset, tmpstr);
				xml += L" <date>";
				xml += tmpstr;
				xml += L"</date>"EOL;
			}
		}
	}

	if (cond.mask & KEY_XMLELM_ENABLE) {
		xml += L" <enable>";
		xml += key.enable ? L"enable" : L"disable";
		xml += L"</enable>"EOL;
	}

	xml += L"</key>"EOL;
}




uint64
O2KeyDB::
ImportFromXML(const wchar_t *filename, const char *in, uint len)
{
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2KeyDB_SAX2Handler handler(Logger, this);
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
			Logger->AddLog(O2LT_ERROR, DBName.c_str(), 0, 0,
				L"SAX2: Out of Memory: %s", e.getMessage());
		}
	}
	catch (const XMLException &e) {
		Logger->AddLog(O2LT_ERROR, DBName.c_str(), 0, 0,
			L"SAX2: Exception: %s", e.getMessage());
	}
	catch (...) {
		Logger->AddLog(O2LT_ERROR, DBName.c_str(), 0, 0,
			L"SAX2: Unexpected exception during parsing.");
	}

	delete parser;
	Expire();

	return (handler.GetParseNum());
}




// ---------------------------------------------------------------------------
//
//	O2KeyDB_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

O2KeyDB_SAX2Handler::
O2KeyDB_SAX2Handler(O2Logger *lgr, O2KeyDB *kdb)
	: SAX2Handler(MODULE, lgr)
	, KeyDB(kdb)
	, CurKey(NULL)
{
}

O2KeyDB_SAX2Handler::
~O2KeyDB_SAX2Handler(void)
{
}

uint64
O2KeyDB_SAX2Handler::
GetParseNum(void)
{
	return (ParseNum);
}

void
O2KeyDB_SAX2Handler::
endDocument(void)
{
	if (CurKey) {
		delete CurKey;
		CurKey = NULL;
	}
}

void
O2KeyDB_SAX2Handler::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{
	CurElm = KEY_XMLELM_NONE;

	//common
	if (MATCHLNAME(L"key")) {
		if (CurKey)
			delete CurKey;
		CurKey = new O2Key();
		CurElm = KEY_XMLELM_NONE;
	}
	else if (MATCHLNAME(L"hash")) {
		CurElm = KEY_XMLELM_HASH;
	}
	else if (MATCHLNAME(L"ip")) {
		CurElm = KEY_XMLELM_IP;
	}
	else if (MATCHLNAME(L"nodeid")) {
		CurElm = KEY_XMLELM_NODEID;
	}
	else if (MATCHLNAME(L"port")) {
		CurElm = KEY_XMLELM_PORT;
	}
	else if (MATCHLNAME(L"size")) {
		CurElm = KEY_XMLELM_SIZE;
	}
	else if (MATCHLNAME(L"url")) {
		CurElm = KEY_XMLELM_URL;
	}
	else if (MATCHLNAME(L"title")) {
		CurElm = KEY_XMLELM_TITLE;
	}
	else if (MATCHLNAME(L"note")) {
		CurElm = KEY_XMLELM_NOTE;
	}
	else if (MATCHLNAME(L"date")) {
		CurElm = KEY_XMLELM_DATE;
	}
	else if (MATCHLNAME(L"enable")) {
		CurElm = KEY_XMLELM_ENABLE;
	}
}

void
O2KeyDB_SAX2Handler::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	switch (CurElm) {
		case KEY_XMLELM_HASH:
			CurKey->hash.assign(buf.c_str(), buf.size());
			break;
		case KEY_XMLELM_NODEID:
			CurKey->nodeid.assign(buf.c_str(), buf.size());
			break;
		case KEY_XMLELM_IP:
			CurKey->ip = e2ip(buf.c_str(), buf.size());
			break;
		case KEY_XMLELM_PORT:
			CurKey->port = (ushort)wcstoul(buf.c_str(), NULL, 10);
			break;
		case KEY_XMLELM_SIZE:
			CurKey->size = wcstoul(buf.c_str(), NULL, 10);
			break;
		case KEY_XMLELM_URL:
			CurKey->url = buf;
			break;
		case KEY_XMLELM_TITLE:
			if (buf.size() <= O2_MAX_KEY_TITLE_LEN)
				CurKey->title = buf;
			break;
		case KEY_XMLELM_NOTE:
			if (buf.size() <= O2_MAX_KEY_NOTE_LEN)
				CurKey->note = buf;
			break;
		case KEY_XMLELM_DATE:
			CurKey->date = datetime2time_t(buf.c_str(), buf.size());
			break;
		case KEY_XMLELM_ENABLE:
			CurKey->enable = buf[0] == L'e' ? true : false;
			break;
	}

	buf = L"";

	CurElm = KEY_XMLELM_NONE;
	if (!CurKey || !MATCHLNAME(L"key"))
		return;

	KeyDB->AddKey(*CurKey);
	ParseNum++;
}

void
O2KeyDB_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
	if (CurKey == NULL)
		return;

	if (CurElm != KEY_XMLELM_NONE)
		buf.append(chars, length);
}
