/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Boards.h
 * description	: 
 *
 */

#pragma once
#include "O2Define.h"
#include "O2DatDB.h"
#include "O2NodeKBucket.h"
#include "mutex.h"
#include <vector>
#include <list>
#include <sstream>




// ---------------------------------------------------------------------------
//	O2Board
//
// ---------------------------------------------------------------------------
struct O2Board {
	wstring		bbsname;
	wstring		title;
	wstring		category;
	wstring		host;
	wstring		domain;

	bool operator ==(const O2Board &src) {
		return (domain == src.domain && bbsname == src.bbsname ? true : false);
	}
};
typedef std::vector<O2Board> O2BoardArray;
typedef std::vector<O2Board>::iterator O2BoardArrayIt;




// ---------------------------------------------------------------------------
//	O2BoardEx
//
// ---------------------------------------------------------------------------
#define COLLECTORS_LIMIT 10

struct O2BoardEx {
	O2NodeKBucket	collectors;
	bool			enable;

	O2BoardEx(void)
		: enable(true)
	{
		collectors.set_capacity(COLLECTORS_LIMIT);
	}
	~O2BoardEx()
	{
	}
};
typedef std::map<wstring,O2BoardEx*> O2BoardExMap;
typedef O2BoardExMap::iterator O2BoardExMapIt;




// ---------------------------------------------------------------------------
//	O2Boards
//
// ---------------------------------------------------------------------------
class O2Boards
	: public O2SAX2Parser
	, public SAX2Handler
{
private:
	O2Profile		*Profile;
	O2Client		*Client;
	wstring			filepath;
	wstring			exfilepath;
	time_t			LastModified;
	O2BoardArray	boards;
	O2BoardExMap	exmap;
	Mutex			BoardsLock;
	Mutex			ExLock;

	uint			parse_elm;
	wstring			parse_name;
	bool			parse_enable;
	wstring			buf;

	wchar_t *host2domain(const wchar_t *host);

public:
	O2Boards(O2Logger *lgr, O2Profile *profile, O2Client *client, const wchar_t *path, const wchar_t *expath);
	~O2Boards();

	uint Get(const char *url = NULL);
	bool Update(const char *html);
	bool Update(const wchar_t *html);
	size_t Size(void);
	size_t SizeEx(void);
	bool IsEnabledEx(const wchar_t *domain, const wchar_t *bbsname); 
	void EnableEx(wstrarray &enableboards);
	void EnableExAll(void);
	void ClearEx(void);
	bool AddEx(const char *url);
	size_t GetExList(wstrarray &boards);
	size_t GetExEnList(wstrarray &boards);
	size_t GetExNodeList(const wchar_t *board, O2NodeKBucket::NodeListT &nodelist);
	void RemoveExNode(const wchar_t *board, const O2Node &node);
	void ImportNodeFromXML(const O2Node &node, const char *in, size_t len);
	void ExportToXML(string &out);
	bool MakeBBSMenuXML(string &out, O2DatDB *db);
	bool Save(void);
	bool SaveEx(void);
	bool Load(const wchar_t *fn = NULL);
	bool LoadEx(void);

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
