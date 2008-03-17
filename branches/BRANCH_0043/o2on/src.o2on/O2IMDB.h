/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2IMDB.h
 * description	: Instant message
 *
 */

#pragma once
#include "O2Define.h"
#include "O2Logger.h"
#include "O2Profile.h"
#include "mutex.h"
#include "O2SAX2Parser.h"
#include <tchar.h>




/*
 *	O2Message
 */
struct O2IMessage
{
	//common
	ulong		ip;
	ushort		port;
	hashT		id;
	pubT		pubkey;
	wstring		name;
	time_t		date;
	wstring		msg;
	hashT		key;
	bool		mine;
	hashListT	paths;

	O2IMessage(void)
		: ip(0)
		, port(0)
		, date(0)
		, mine(false)
	{
	}

	bool operator==(const O2IMessage &src)
	{
		//この部分のコメントアウトを解除
		//return (key == src.key ? true : false);

		//note:ブロードキャストメッセージの件の暫定対応
		// この構造体はIMDBでも使用しているため、
		// 修正によってメッセンジャーの方にも影響あり。
		// メッセージ内容(IP/PORT/本文が同じ)だと一つのメッセージになる。
		if(ip == src.ip && port == src.port && msg == src.msg)
			return true;
		else
			return false;
	}
};
typedef std::list<O2IMessage> O2IMessages;
typedef std::list<O2IMessage>::iterator O2IMessagesIt;




/*
 *	element bit
 */
#define IM_XMLELM_NONE				0x00000000
#define IM_XMLELM_IP				0x00000001
#define IM_XMLELM_PORT				0x00000002
#define IM_XMLELM_ID				0x00000004
#define IM_XMLELM_PUBKEY			0x00000008
#define IM_XMLELM_NAME				0x00000010
#define IM_XMLELM_DATE				0x00000020
#define IM_XMLELM_MSG				0x00000040
#define IM_XMLELM_KEY				0x00010000
#define IM_XMLELM_MINE				0x00020000
#define IM_XMLELM_PATH				0x00040000
#define IM_XMLELM_INFO				0x80000000

#define IM_XMLELM_COMMON			0x0000ffff
#define IM_XMLELM_ALL				0x0fffffff




/*
 *	O2LogSelectCondition
 */
struct O2IMSelectCondition
{
	wstring		charset;
	uint		mask;
	uint		limit;
	wstring		sort;
	wstring		xsl;
	wstring		timeformat;

	pubT		pubkey;
	bool		has_pubkey;
	bool		desc;

	O2IMSelectCondition(uint m = IM_XMLELM_COMMON)
		: charset(_T(DEFAULT_XML_CHARSET))
		, mask(m)
		, limit(0)
		, has_pubkey(false)
		, desc(false)
	{
	}
};




/*
 *	O2IMDB
 */
class O2IMDB
	: public O2SAX2Parser
	, private Mutex
{
private:
	O2Logger *Logger;
	O2IMessages Messages;
	size_t Limit;
	bool NewMessageFlag;

	void MakeIMElement(O2IMessage &im, O2IMSelectCondition &cond, wstring &xml);

public:
	O2IMDB(O2Logger *lgr);
	~O2IMDB(void);

	void Expire(void);

	size_t GetLimit(void);
	bool SetLimit(size_t n);
	size_t Min(void);
	size_t Max(void);

	size_t Count(void);

	bool AddMessage(O2IMessage &im);
	bool DeleteMessage(const hashListT &keylist);
	bool Exist(const O2IMessage &im);
	size_t GetMessages(O2IMessages &out);
	bool HaveNewMessage(void);
	bool MakeSendXML(O2Profile *profile, const wchar_t *charset, const wchar_t *msg, string &out);
	bool MakeSendXML(const O2IMessage &im, string &out);


	bool Save(const wchar_t *filename, bool clear);
	bool Load(const wchar_t *filename);

	size_t ExportToXML(O2IMSelectCondition &cond, string &out);
	size_t ImportFromXML(const wchar_t *filename, const char *in, uint len);

public:
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




//
//	O2IMDB_SAX2Handler
//
class O2IMDB_SAX2Handler
	: public SAX2Handler
{
protected:
	O2IMDB		*IMDB;
	O2IMessage	*CurIM;
	uint		CurElm;
	size_t		ParseNum;

public:
	O2IMDB_SAX2Handler(O2Logger *lgr, O2IMDB *imdb);
	~O2IMDB_SAX2Handler(void);

	size_t GetParseNum(void);

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
