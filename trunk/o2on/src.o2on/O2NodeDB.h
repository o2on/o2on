/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2NodeDB.h
 * description	: Node Routing Table
 *
 */

#pragma once
#include "O2SAX2Parser.h"
#include "O2Node.h"
#include "O2Logger.h"
#include "O2NodeKBucket.h"
#include "KademliaRoutingTable.h"
#include <tchar.h>




// XML Element bit
#define NODE_XMLELM_NONE				0x00000000
#define NODE_XMLELM_ID					0x00000001
#define NODE_XMLELM_IP					0x00000002
#define NODE_XMLELM_PORT				0x00000004
#define NODE_XMLELM_NAME				0x00000008
#define NODE_XMLELM_PUBKEY				0x00000010
#define NODE_XMLELM_UA					0x00010000
#define NODE_XMLELM_STATUS				0x00020000
#define NODE_XMLELM_LASTLINK			0x00040000
#define NODE_XMLELM_CONNECTION_ME2N		0x00080000
#define NODE_XMLELM_SENDBYTE_ME2N		0x00100000
#define NODE_XMLELM_RECVBYTE_ME2N		0x00200000
#define NODE_XMLELM_CONNECTION_N2ME		0x00400000
#define NODE_XMLELM_SENDBYTE_N2ME		0x00800000
#define NODE_XMLELM_RECVBYTE_N2ME		0x01000000
#define NODE_XMLELM_DISTANCE			0x02000000
#define NODE_XMLELM_FLAGS				0x04000000
#define NODE_XMLELM_STR					0x40000000
#define NODE_XMLELM_INFO				0x80000000

#define NODE_XMLELM_COMMON				0x0000ffff
#define NODE_XMLELM_ALL					0x0fffffff




// O2NodeSelectCondition
struct O2NodeSelectCondition
{
	wstring		charset;
	uint		mask;
	uint64		limit;
	wstring		timeformat;
	bool		include_port0;

	O2NodeSelectCondition(uint m = NODE_XMLELM_COMMON)
		: charset(_T(DEFAULT_XML_CHARSET))
		, mask(m)
		, limit(0)
		, include_port0(false)
	{
	}
};




//
//	O2NodeDB
//
class O2NodeDB
	: public KademliaRoutingTable<O2NodeKBucket, O2Node>
	, public O2SAX2Parser
{
protected:
	O2Logger	*Logger;
	wchar_t		ver[10];
	bool		NewVerDetectionFlag;
	NodeListT	Port0Nodes;
	Mutex		Port0NodesLock;

protected:
	virtual bool touch_preprocessor(O2Node &node);
	void MakeNodeElement(const O2Node &node, const O2NodeSelectCondition &cond, wstring &xml);

public:
	O2NodeDB(O2Logger *lgr, O2Profile *prof, O2Client *client);
	~O2NodeDB(void);

	bool IsDetectNewVer(void);
	size_t AddEncodedNode(const char *in, size_t len);
	void AddPort0Node(O2Node &node);

	bool Save(const wchar_t *filename);
	bool Load(const wchar_t *filename);

	size_t ExportToXML(const O2NodeSelectCondition &cond, string &out);
	size_t ExportToXML(const NodeListT &nodelist, const O2NodeSelectCondition &cond, string &out);
	size_t ImportFromXML(const wchar_t *filename, const char *in, uint len, NodeListT *rlist);
};




//
//	O2NodeDB_SAX2Handler
//
class O2NodeDB_SAX2Handler
	: public SAX2Handler
{
protected:
	O2NodeDB			*NodeDB;
	O2NodeDB::NodeListT	*ReceiveList;
	O2Node				*CurNode;
	uint				CurElm;
	size_t				ParseNum;
	wstring				buf;

public:
	O2NodeDB_SAX2Handler(O2Logger *lgr, O2NodeDB *ndb, O2NodeDB::NodeListT *rlist);
	~O2NodeDB_SAX2Handler(void);

	size_t GetParseNum(void);

	void startDocument(void);
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
