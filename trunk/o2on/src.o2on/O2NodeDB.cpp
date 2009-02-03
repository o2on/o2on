/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2NodeDB.cpp
 * description	: Node Routing Table
 *
 */

#include "O2NodeDB.h"
#include "O2Version.h"
#include "file.h"
#include "dataconv.h"

#define MODULE L"NodeDB"
#define PORT0NODE_LIMIT	100




// ---------------------------------------------------------------------------
//
//	O2NodeDB::
//	Constructor/Destructor
//
// ---------------------------------------------------------------------------

O2NodeDB::
O2NodeDB(O2Logger *lgr, O2Profile *prof, O2Client *client)
	: Logger(lgr)
	, NewVerDetectionFlag(false)
{
	for (size_t i = 0; i < HASH_BITLEN; i++)
		KBuckets[i].SetObject(prof, client);
	swprintf_s(ver, 10, L"%1d.%02d.%04d", APP_VER_MAJOR, APP_VER_MINOR, APP_BUILDNO);
	srand((unsigned)time(NULL));
}




O2NodeDB::
~O2NodeDB()
{
}




bool
O2NodeDB::
touch_preprocessor(O2Node &node)
{
	// O2/0.2 (o2on/0.02.0027; Win32)
	if (node.ua.size() > 13) {
		wstring node_ver = node.ua.substr(13, 9);
		if (wcscmp(node_ver.c_str(), ver) > 0)
			NewVerDetectionFlag = true;
	}
	if (node.port == 0) {
		AddPort0Node(node);
		return false;
	}
	return true;
}

bool
O2NodeDB::
IsDetectNewVer(void)
{
	bool ret = NewVerDetectionFlag;
	NewVerDetectionFlag = false;
	return (ret);
}



// ---------------------------------------------------------------------------
//	AddEncodedNode
// ---------------------------------------------------------------------------
size_t
O2NodeDB::
AddEncodedNode(const char *in, size_t len)
{
	size_t totaladd = 0;
	string str(in, len);

	// 1行毎に処理
	size_t pos = 0;
	while (pos < str.size()) {
		size_t lf = str.find_first_of(" \t\r\n", pos);
		if (!FOUND(lf))
			lf = str.size();

		uint len = lf - pos;
		if (len == 52) { // id(40) + e_ip(8)+ port(4)
			string line(str.substr(pos, len));

			string idstr(line.substr(0, 40));
			string encipstr(line.substr(40, 8));
			string encportstr(line.substr(48, 4));

			O2Node node;

			node.id.assign(idstr.c_str(), idstr.size());
			node.ip = e2ip(encipstr.c_str(), encipstr.size());
			node.port = e2port(encportstr.c_str(), encportstr.size());

			if (touch(node))
				totaladd++;

#if defined(_DEBUG)
			char abc[256];
			in_addr addr;
			addr.S_un.S_addr = node.ip;
			sprintf_s(abc, 256, "%s:%d [%s]\n", inet_ntoa(addr), node.port, idstr.c_str());
			TRACEA(abc);
#endif
		}
		pos = str.find_first_not_of(" \t\r\n", lf);
	}
	return (totaladd);
}




// ---------------------------------------------------------------------------
//	AddPort0Node
// ---------------------------------------------------------------------------
void
O2NodeDB::
AddPort0Node(O2Node &node)
{
	Port0NodesLock.Lock();
	{
		NodeListT::iterator it = std::find(
			Port0Nodes.begin(), Port0Nodes.end(), node);

		if (it != Port0Nodes.end()) {
			O2Node mnode = *it;
			Port0Nodes.erase(it);

			mnode.marge(node);
			mnode.lastlink = time(NULL);
			Port0Nodes.push_back(mnode);
		}
		else {
			node.lastlink = time(NULL);
			Port0Nodes.push_back(node);
			while (Port0Nodes.size() > PORT0NODE_LIMIT)
				Port0Nodes.pop_front();
		}
	}
	Port0NodesLock.Unlock();
}




// ---------------------------------------------------------------------------
//	File I/O
// ---------------------------------------------------------------------------
bool
O2NodeDB::
Save(const wchar_t *filename)
{
	O2NodeSelectCondition cond;
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
O2NodeDB::
Load(const wchar_t *filename)
{
	struct _stat st;
	if (_wstat(filename, &st) == -1)
		return false;
	if (st.st_size == 0)
		return false;
	ImportFromXML(filename, NULL, 0, NULL);
	return true;
}




// ---------------------------------------------------------------------------
//	XML I/O
// ---------------------------------------------------------------------------

size_t
O2NodeDB::
ExportToXML(const O2NodeSelectCondition &cond, string &out)
{
	wstring xml;

	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	xml	+= L"<nodes>"EOL;

	if (cond.mask & NODE_XMLELM_INFO) {
		xml += L"<info>"EOL;
		{
			xml += L" <count>";
			wchar_t tmp[16];
			swprintf_s(tmp, 16, L"%d", count());
			xml += tmp;
			xml += L"</count>"EOL;

			xml += L" <id>";
			wstring idstr;
			SelfKademliaNode.id.to_string(idstr);
			xml += idstr;
			xml += L"</id>"EOL;

			for (uint i = 0; i < HASH_BITLEN; i++) {
				wchar_t str[256];
				swprintf_s(str, 256,
					L"<kbucket d=\"%u\">%d</kbucket>\r\n",
					i, (int)((double)KBuckets[i].count()/(i+1)*100.0));
				xml += str;
			}

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

	for (size_t i = 0 ;i < HASH_BITLEN; i++) {
		NodeListT nodes;
		KBuckets[i].get_nodes(nodes);

		NodeListT::iterator it;
		for (it = nodes.begin(); it != nodes.end(); it++) {
			MakeNodeElement(*it, cond, xml);
			out_count++;
			if (cond.limit && out_count >= cond.limit) {
				i = HASH_BITLEN;
				break;
			}
		}
	}
	if (cond.include_port0) {
		for (NodeListT::iterator it = Port0Nodes.begin(); it != Port0Nodes.end(); it++) {
			if (cond.limit && out_count >= cond.limit)
				break;
			MakeNodeElement(*it, cond, xml);
			out_count++;
		}
	}

	xml	+= L"</nodes>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	return (out_count);
}




size_t
O2NodeDB::
ExportToXML(const NodeListT &nodelist, const O2NodeSelectCondition &cond, string &out)
{
	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += cond.charset;
	xml += L"\"?>"EOL;
	xml	+= L"<nodes>"EOL;

	size_t out_count = 0;
	NodeListT::const_iterator it;
	for (it = nodelist.begin(); it != nodelist.end(); it++) {
		MakeNodeElement(*it, cond, xml);
		out_count++;
	}

	xml	+= L"</nodes>"EOL;

	if (!FromUnicode(cond.charset.c_str(), xml, out))
		return (0);
	return (out_count);
}




void
O2NodeDB::
MakeNodeElement(const O2Node &node, const O2NodeSelectCondition &cond, wstring &xml)
{
	wchar_t tmp[32];
	wstring tmpstr;

	xml += L"<node>"EOL;

	if (cond.mask & NODE_XMLELM_ID) {
		node.id.to_string(tmpstr);
		xml += L" <id>";
		xml += tmpstr;
		xml += L"</id>"EOL;
	}
	if (cond.mask & NODE_XMLELM_IP) {
		ip2e(node.ip, tmpstr);
		xml += L" <ip>";
		xml += tmpstr;
		xml += L"</ip>"EOL;
	}
	if (cond.mask & NODE_XMLELM_PORT) {
		swprintf_s(tmp, 32, L"%d", node.port);
		xml += L" <port>";
		xml += tmp;
		xml += L"</port>"EOL;
	}
	if (cond.mask & NODE_XMLELM_NAME) {
		xml += L" <name><![CDATA[";
		xml += node.name;
		xml += L"]]></name>"EOL;
	}
	if (cond.mask & NODE_XMLELM_PUBKEY) {
		node.pubkey.to_string(tmpstr);
		xml += L" <pubkey>";
		xml += tmpstr;
		xml += L"</pubkey>"EOL;
	}

	if (cond.mask & NODE_XMLELM_UA) {
		xml += L" <ua>";
		xml += node.ua;
		xml += L"</ua>"EOL;
	}
	if (cond.mask & NODE_XMLELM_STATUS) {
		xml += L" <status";
		if (node.status & O2_NODESTATUS_LINKEDFROM)
			xml += L" linkedfrom=\"1\"";
		if (node.status & O2_NODESTATUS_LINKEDTO)
			xml += L" linkedto=\"1\"";
		if (node.status & O2_NODESTATUS_PASTLINKEDFROM)
			xml += L" pastlinkedfrom=\"1\"";
		if (node.status & O2_NODESTATUS_PASTLINKEDTO)
			xml += L" pastlinkedto=\"1\"";
		xml += L"/>"EOL;
	}
	if (cond.mask & NODE_XMLELM_LASTLINK) {
		if (node.lastlink == 0)
			xml += L" <lastlink>-</lastlink>"EOL;
		else {
			long tzoffset;
			_get_timezone(&tzoffset);

			if (!cond.timeformat.empty()) {
				time_t t = node.lastlink - tzoffset;

				wchar_t timestr[TIMESTR_BUFF_SIZE];
				struct tm tm;
				gmtime_s(&tm, &t);
				wcsftime(timestr, TIMESTR_BUFF_SIZE, cond.timeformat.c_str(), &tm);
				xml += L" <lastlink>";
				xml += timestr;
				xml += L"</lastlink>"EOL;
			}
			else {
				time_t2datetime(node.lastlink, - tzoffset, tmpstr);
				xml += L" <lastlink>";
				xml += tmpstr;
				xml += L"</lastlink>"EOL;
			}
		}
	}
	if (cond.mask & NODE_XMLELM_SENDBYTE_ME2N) {
		swprintf_s(tmp, 32, L"%I64u", node.sendbyte_me2n);
		xml += L" <sendbyte_me2n>";
		xml += tmp;
		xml += L"</sendbyte_me2n>"EOL;
	}
	if (cond.mask & NODE_XMLELM_RECVBYTE_ME2N) {
		swprintf_s(tmp, 32, L"%I64u", node.recvbyte_me2n);
		xml += L" <recvbyte_me2n>";
		xml += tmp;
		xml += L"</recvbyte_me2n>"EOL;
	}
	if (cond.mask & NODE_XMLELM_SENDBYTE_N2ME) {
		swprintf_s(tmp, 32, L"%I64u", node.sendbyte_n2me);
		xml += L" <sendbyte_n2me>";
		xml += tmp;
		xml += L"</sendbyte_n2me>"EOL;
	}
	if (cond.mask & NODE_XMLELM_RECVBYTE_N2ME) {
		swprintf_s(tmp, 32, L"%I64u", node.recvbyte_n2me);
		xml += L" <recvbyte_n2me>";
		xml += tmp;
		xml += L"</recvbyte_n2me>"EOL;
	}
	if (cond.mask & NODE_XMLELM_DISTANCE) {
		if (node.port == 0) {
			xml += L" <distance>999</distance>"EOL;
		}
		else {
//			hashBitsetT d = node.id.bits ^ SelfKademliaNode.id.bits;
//			swprintf_s(tmp, 32, L"%u", d.bit_length());
			swprintf_s(tmp, 32, L"%u", hash_xor_bitlength(node.id, SelfKademliaNode.id));
			xml += L" <distance>";
			xml += tmp;
			xml += L"</distance>"EOL;
		}
	}
	if (cond.mask & NODE_XMLELM_FLAGS) {
		xml += L" <flags>";
		xml += node.flags;
		xml += L"</flags>"EOL;
	}
	xml += L"</node>"EOL;
}




size_t
O2NodeDB::
ImportFromXML(const wchar_t *filename, const char *in, uint len, NodeListT *rlist)
{
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	O2NodeDB_SAX2Handler handler(Logger, this, rlist);
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
//	O2NodeDB_SAX2Handler::
//	Implementation of the SAX2 Handler interface
//
// ---------------------------------------------------------------------------

O2NodeDB_SAX2Handler::
O2NodeDB_SAX2Handler(O2Logger *lgr, O2NodeDB *ndb, O2NodeDB::NodeListT *rlist)
	: SAX2Handler(MODULE, lgr)
	, NodeDB(ndb)
	, ReceiveList(rlist)
	, CurNode(NULL)
	, ParseNum(0)
{
}

O2NodeDB_SAX2Handler::
~O2NodeDB_SAX2Handler(void)
{
}

size_t
O2NodeDB_SAX2Handler::
GetParseNum(void)
{
	return (ParseNum);
}

void
O2NodeDB_SAX2Handler::
startDocument(void)
{
	ParseNum = 0;
}
void
O2NodeDB_SAX2Handler::
endDocument(void)
{
	if (CurNode) {
		delete CurNode;
		CurNode = NULL;
	}
}

void
O2NodeDB_SAX2Handler::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{
	CurElm = NODE_XMLELM_NONE;

	if (MATCHLNAME(L"node")) {
		if (CurNode)
			delete CurNode;
		CurNode = new O2Node();
		CurElm = NODE_XMLELM_NONE;
	}
	else if (MATCHLNAME(L"id")) {
		CurElm = NODE_XMLELM_ID;
	}
	else if (MATCHLNAME(L"ip")) {
		CurElm = NODE_XMLELM_IP;
	}
	else if (MATCHLNAME(L"port")) {
		CurElm = NODE_XMLELM_PORT;
	}
	else if (MATCHLNAME(L"name")) {
		CurElm = NODE_XMLELM_NAME;
	}
	else if (MATCHLNAME(L"pubkey")) {
		CurElm = NODE_XMLELM_PUBKEY;
	}
	else if (MATCHLNAME(L"str")) {
		CurElm = NODE_XMLELM_STR;
	}
}

void
O2NodeDB_SAX2Handler::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	string tmpstr;

	switch (CurElm) {
		case NODE_XMLELM_ID:
			CurNode->id.assign(buf.c_str(), buf.size());
			break;
		case NODE_XMLELM_IP:
			CurNode->ip = e2ip(buf.c_str(), buf.size());
			break;
		case NODE_XMLELM_PORT:
			CurNode->port = (ushort)wcstoul(buf.c_str(), NULL, 10);
			break;
		case NODE_XMLELM_NAME:
			if (buf.size() <= O2_MAX_NAME_LEN)
				CurNode->name = buf;
			break;
		case NODE_XMLELM_PUBKEY:
			CurNode->pubkey.assign(buf.c_str(), buf.size());
			break;
		case NODE_XMLELM_STR:
			unicode2ascii(buf, tmpstr);
			if (NodeDB->AddEncodedNode(tmpstr.c_str(), tmpstr.size()))
				ParseNum++;
			break;
	}

	buf = L"";

	CurElm = NODE_XMLELM_NONE;
	if (!CurNode || !MATCHLNAME(L"node"))
		return;

	if (ReceiveList)
		ReceiveList->push_back(*CurNode);
	else
		NodeDB->touch(*CurNode);
	ParseNum++;
}

void
O2NodeDB_SAX2Handler::
characters(const XMLCh* const chars, const unsigned int length)
{
	if (CurNode == NULL && CurElm != NODE_XMLELM_STR)
		return;

	if (CurElm != NODE_XMLELM_NONE)
		buf.append(chars, length);
}
