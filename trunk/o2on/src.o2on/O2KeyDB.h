/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2KeyDB.h
 * description	: 
 *
 */

#pragma once
#include "O2Key.h"
#include "O2Logger.h"
#include "mutex.h"
#include "O2SAX2Parser.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

using namespace boost::multi_index;




/*
 *	element bit
 */
#define KEY_XMLELM_NONE					0x00000000
#define KEY_XMLELM_HASH					0x00000001
#define KEY_XMLELM_IP					0x00000002
#define KEY_XMLELM_NODEID				0x00000004
#define KEY_XMLELM_PORT					0x00000008
#define KEY_XMLELM_SIZE					0x00000010
#define KEY_XMLELM_URL					0x00000020
#define KEY_XMLELM_TITLE				0x00000040
#define KEY_XMLELM_NOTE					0x00000080
#define KEY_XMLELM_IDKEYHASH			0x00010000
#define KEY_XMLELM_DISTANCE				0x00020000
#define KEY_XMLELM_DATE					0x00040000
#define KEY_XMLELM_ENABLE				0x00080000
#define KEY_XMLELM_INFO					0x80000000

#define KEY_XMLELM_COMMON				0x0000ffff
#define KEY_XMLELM_ALL					0x0fffffff




/*
 *	multi_index_container
 */
typedef multi_index_container<
	O2Key,
	indexed_by<
		ordered_unique<member<O2Key, hashT, &O2Key::idkeyhash> >,
		ordered_non_unique<member<O2Key, hashT, &O2Key::hash> >,
		ordered_non_unique<identity<O2Key> >,
		ordered_non_unique<member<O2Key, time_t, &O2Key::date>, std::greater<time_t> >
	>
> O2Keys;




/*
 *	O2KeySelectCondition
 */
struct O2KeySelectCondition
{
	wstring		charset;
	uint		mask;
	uint64		limit;
	wstring		timeformat;
	bool		orderbydate;

	O2KeySelectCondition(uint m = KEY_XMLELM_COMMON)
		: charset(_T(DEFAULT_XML_CHARSET))
		, mask(m)
		, limit(0)
		, orderbydate(false)
	{
	}
};




//
//	O2KeyDB
//
class O2KeyDB
	: public O2SAX2Parser
	, private Mutex
{
private:
	O2Logger	*Logger;
	O2Keys		Keys;
	uint64		Limit;
	wstring		DBName;
	hashT		SelfNodeID;
	bool		IP0Port0;

private:
	bool CheckKey(O2Key &key);
	void Expire(void);
	void MakeKeyElement(const O2Key &key, O2KeySelectCondition &cond, wstring &xml);

public:
	O2KeyDB(const wchar_t *name, bool ip0port0, O2Logger *lgr);
	~O2KeyDB(void);

	void SetSelfNodeID(const hashT &hash);
	uint64 GetLimit(void);
	bool SetLimit(uint64 n);
	uint64 Min(void);
	uint64 Max(void);
	uint64 Count(void);

	bool SetNote(const hashT &hash, const wchar_t *title, uint64 datsize);
	bool SetDate(const hashT &hash, time_t t);
	bool SetEnable(const hashT &hash, bool flag);

	uint64 GetKeyList(O2KeyList &ret, time_t date_le);
	uint64 GetKeyList(const hashT &target, O2KeyList &ret);
	uint AddKey(O2Key &k);
	uint64 DeleteKey(const hashT &hash);
	uint64 DeleteKeyByNodeID(const hashT &nodeid);

	bool Save(const wchar_t *filename);
	bool Load(const wchar_t *filename);

	uint64 ExportToXML(O2KeySelectCondition &cond, string &out);
	uint64 ExportToXML(const O2KeyList &keys, string &out);
	uint64 ExportToXML(const O2Key &key, string &out);
	uint64 ImportFromXML(const wchar_t *filename, const char *in, uint len);
};




//
//	O2KeyDB_SAX2Handler
//
class O2KeyDB_SAX2Handler
	: public SAX2Handler
{
protected:
	O2KeyDB		*KeyDB;
	O2Key		*CurKey;
	uint		CurElm;
	uint64		ParseNum;
	wstring		buf;

public:
	O2KeyDB_SAX2Handler(O2Logger *lgr, O2KeyDB *kdb);
	~O2KeyDB_SAX2Handler(void);

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
