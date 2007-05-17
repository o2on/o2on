/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Boards.h
 * description	: implementation of 2ch board class
 *
 */

#include "O2Boards.h"
#include "simplehttpsocket.h"
#include "file.h"
#include <sys/utime.h>
#include <boost/regex.hpp>

#define MODULE				L"Boards"
#define DEFAULT_BBSMENU_URL "http://menu.2ch.net/bbsmenu.html"

#define DOMAIN_2CH			"2ch.net"
#define DOMAIN_BBSPINK		"bbspink.com"
#define DOMAIN_MACHI		"machi.to"




/*
 *	O2Boards()
 *	コンストラクタ
 */
O2Boards::
O2Boards(O2Logger *lgr, O2Profile *profile, O2Client *client, const wchar_t *path, const wchar_t *expath)
	: SAX2Handler(MODULE, lgr)
	, Profile(profile)
	, Client(client)
	, filepath(path)
	, exfilepath(expath)
	, LastModified(0)
{
}




/*
 *	~O2Boards()
 *	デストラクタ
 */
O2Boards::
~O2Boards()
{
	ClearEx();
}




/*
 *	CheckDomain()
 *	
 */
wchar_t *
O2Boards::
host2domain(const wchar_t *host)
{
	if (wcsstr(host, _T(DOMAIN_2CH)))
		return (_T(DOMAIN_2CH));
	else if (wcsstr(host, _T(DOMAIN_BBSPINK)))
		return (_T(DOMAIN_BBSPINK));
//	else if (wcsstr(host, T(DOMAIN_MACHI)))
//		return (_T(DOMAIN_MACHI));
	return (NULL);
}




/*
 *	Get()
 *	URLから取得
 */
uint
O2Boards::
Get(const char *url)
{
	const char *target_url = url ? url : DEFAULT_BBSMENU_URL;
	HTTPSocket *socket = new HTTPSocket(SELECT_TIMEOUT_MS, Profile->GetUserAgentA());

	HTTPHeader h(HTTPHEADERTYPE_REQUEST);
	if (LastModified)
		h.AddFieldDate("If-Modified-Since", LastModified);

	string out;
	HTTPHeader oh(HTTPHEADERTYPE_RESPONSE);
	if (!socket->request(target_url, h, NULL, 0)) {
		delete socket;
		return (0);
	}
	while (socket->response(out, oh) >= 0)
	{
		if (oh.contentlength) {
			if (out.size() - oh.length >= oh.contentlength)
				break;
		}
	}
	delete socket;

	if (oh.status != 200)
		return (oh.status);

	if (!Update(&out[oh.length]))
		return (0);

	strmap::iterator it = oh.fields.find("Last-Modified");
	if (it == oh.fields.end())
		it = oh.fields.find("Date");
	if (it == oh.fields.end())
		return (0);

	time_t t = oh.httpdate2time_t(it->second.c_str());
	LastModified = t;
/*
	TRACEA(it->first.c_str());
	TRACEA(": ");
	TRACEA(it->second.c_str());
	TRACEA("\n");
	TRACEA(ctime(&LastModified));
*/
	return (oh.status);
}




/*
 *	Update()
 *	
 */
bool
O2Boards::
Update(const char *html)
{
	if (!html || html[0] == 0)
		return false;

	wstring unicode;
	if (!ToUnicode(L"shift_jis", html, strlen(html), unicode))
		return false;

	return (Update(unicode.c_str()));
}

bool
O2Boards::
Update(const wchar_t *html)
{
	std::wstringstream ss(html);
	std::wstring line;
	std::wstring category;
	O2BoardArray old(boards);
	O2BoardArrayIt it;

	if (!html || html[0] == 0)
		return false;

	BoardsLock.Lock();
	boards.clear();
	BoardsLock.Unlock();

	while (!ss.fail())
	{
		std::getline(ss, line);
		
		wstring pattern;
		pattern = L"^<A HREF=http://([a-z0-9.:]+)/([a-z0-9.:]+)/(?: TARGET=_blank)?>([^<]+)</A>";
		
		boost::wregex regex(pattern);
		boost::wsmatch m;
		if (!category.empty() && boost::regex_search(line, m, regex)) {
			const wchar_t *domain = host2domain(m.str(1).c_str());
			if (!domain)
				continue;
			
			O2Board brd;
			brd.bbsname = m.str(2);
			brd.title = m.str(3);
			brd.category = category;
			brd.host = m.str(1);
			brd.domain = domain;

			BoardsLock.Lock();
			O2BoardArrayIt it = std::find(boards.begin(), boards.end(), brd);
			if (it == boards.end())
				boards.push_back(brd);
			BoardsLock.Unlock();
/*
			TRACEW(brd.category.c_str());
			TRACEW(L" : ");
			TRACEW(brd.title.c_str());
			TRACEW(L" : http://");
			TRACEW(brd.host.c_str());
			TRACEW(L"/");
			TRACEW(brd.bbsname.c_str());
			TRACEW(L"/\n");
*/
			continue;
		}
		
		regex = L"^<BR><BR><B>([^<]+)</B>";
		if (boost::regex_search(line, m, regex)) {
			category = m.str(1);
			if (category == L"おすすめ" || category == L"特別企画")
				category.erase();
		}
	}

	return true;
}




// ---------------------------------------------------------------------------
//	Size()
//	
// ---------------------------------------------------------------------------
size_t
O2Boards::
Size(void)
{
	return (boards.size());
}




// ---------------------------------------------------------------------------
//	SizeEx()
//	
// ---------------------------------------------------------------------------
size_t
O2Boards::
SizeEx(void)
{
	ExLock.Lock();
	size_t size = exmap.size();
	ExLock.Unlock();
	return (size);
}




// ---------------------------------------------------------------------------
//	IsEnableEx()
//	
// ---------------------------------------------------------------------------
bool
O2Boards::
IsEnabledEx(const wchar_t *domain, const wchar_t *bbsname)
{
	wstring domain_bbsname;
	domain_bbsname = domain;
	domain_bbsname += L":";
	domain_bbsname += bbsname;

	ExLock.Lock();
	O2BoardExMapIt it = exmap.find(domain_bbsname);
	bool enable =  it != exmap.end() && it->second->enable ? true : false;
	ExLock.Unlock();

	return (enable);
}




// ---------------------------------------------------------------------------
//	EnableEx()
//	
// ---------------------------------------------------------------------------
void
O2Boards::
EnableEx(wstrarray &enableboards)
{
	O2BoardExMapIt exit;

	ExLock.Lock();
	for (exit = exmap.begin(); exit != exmap.end(); exit++) {
		exit->second->enable = false;
	}
	ExLock.Unlock();

	for (size_t i = 0; i < enableboards.size(); i++) {
		ExLock.Lock();
		{
			exit = exmap.find(enableboards[i]);
			if (exit == exmap.end()) {
				O2BoardEx *ex = new O2BoardEx();
				ex->collectors.SetObject(Profile, Client);
				ex->enable = true;
				exmap.insert(O2BoardExMap::value_type(enableboards[i], ex));
			}
			else
				exit->second->enable = true;
		}
		ExLock.Unlock();
	}
}




// ---------------------------------------------------------------------------
//	ClearEx()
//	
// ---------------------------------------------------------------------------
void
O2Boards::
ClearEx(void)
{
	ExLock.Lock();
	for (O2BoardExMapIt exit = exmap.begin(); exit != exmap.end(); exit++) {
		delete exit->second;
	}
	exmap.clear();
	ExLock.Unlock();
}




// ---------------------------------------------------------------------------
//	AddEx()
//	
// ---------------------------------------------------------------------------
bool
O2Boards::
AddEx(const char *url)
{
	HTTPHeader h(0);
	h.SplitURL(url);

	if (h.paths.empty() || h.paths[0] == "/")
		return false;

	wstring hostname;
	ascii2unicode(h.hostname, hostname);

	wchar_t *domain = host2domain(hostname.c_str());
	if (!domain)
		return false;

	wstring bbsname;
	ascii2unicode(h.paths[0], bbsname);

	wstring domain_bbsname;
	domain_bbsname = domain;
	domain_bbsname += L':';
	domain_bbsname += bbsname;

	O2BoardExMapIt exit = exmap.find(domain_bbsname);
	if (exit != exmap.end())
		return false;

	ExLock.Lock();
	{
		O2BoardEx *ex = new O2BoardEx();
		ex->collectors.SetObject(Profile, Client);
		exmap.insert(O2BoardExMap::value_type(domain_bbsname, ex));
	}
	ExLock.Unlock();
	return true;
}




// ---------------------------------------------------------------------------
//	GetExList()
//	
// ---------------------------------------------------------------------------
size_t
O2Boards::
GetExList(wstrarray &boards)
{
	ExLock.Lock();
	for (O2BoardExMapIt exit = exmap.begin(); exit != exmap.end(); exit++) {
		if (exit->second->collectors.count())
			boards.push_back(exit->first);
	}
	ExLock.Unlock();

	return (boards.size());
}




// ---------------------------------------------------------------------------
//	GetExNodeList()
//	
// ---------------------------------------------------------------------------
size_t
O2Boards::
GetExNodeList(const wchar_t *board, O2NodeKBucket::NodeListT &nodelist)
{
	ExLock.Lock();
	O2BoardExMapIt exit = exmap.find(board);
	if (exit != exmap.end()) {
		exit->second->collectors.get_nodes(nodelist);
	}
	ExLock.Unlock();

	return (nodelist.size());
}




// ---------------------------------------------------------------------------
//	RemoveExNode()
//	
// ---------------------------------------------------------------------------
void
O2Boards::
RemoveExNode(const wchar_t *board, const O2Node &node)
{
	ExLock.Lock();
	O2BoardExMapIt exit = exmap.find(board);
	if (exit != exmap.end()) {
		exit->second->collectors.remove(node);
	}
	ExLock.Unlock();
}




// ---------------------------------------------------------------------------
//	ImportNodeFromXML()
//	
// ---------------------------------------------------------------------------
void
O2Boards::
ImportNodeFromXML(const O2Node &node, const char *in, size_t len)
{
	wstring xml;
	size_t p = 0;
	size_t p1;
	size_t p2;
	ascii2unicode(in, len, xml);
	wstring domain_bbsname;
	O2BoardExMapIt exit;

	while (p < len) {
		p1 = xml.find(L"<board>", p);
		if (p1 == wstring::npos) break;
		p1 += 7;

		p2 = xml.find(L"</board>", p1);
		if (p2 == wstring::npos) break;

		if (p2 > p1) {
			domain_bbsname.assign(xml, p1, p2-p1);
			ExLock.Lock();
			{
				exit = exmap.find(domain_bbsname);
				if (exit != exmap.end()) {
					exit->second->collectors.push(node);
				}
			}
			ExLock.Unlock();
		}
		p = p2 + 8;
	}
}




// ---------------------------------------------------------------------------
//	ExportToXML()
//	
// ---------------------------------------------------------------------------
void
O2Boards::
ExportToXML(string &out)
{
	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += _T(DEFAULT_XML_CHARSET);
	xml += L"\"?>"EOL;
	xml += L"<boards>"EOL;
	ExLock.Lock();
	{
		for (O2BoardExMapIt it = exmap.begin(); it != exmap.end(); it++) {
			xml_AddElement(xml, L"board", NULL, it->first.c_str());
		}
	}
	ExLock.Unlock();
	xml += L"<boards>"EOL;
	unicode2ascii(xml, out);
}




// ---------------------------------------------------------------------------
//	MakeBBSMenuXML()
//	
// ---------------------------------------------------------------------------
bool
O2Boards::
MakeBBSMenuXML(string &out, O2DatDB *db)
{
	wstring xml;
	wstring category;
	wstring domain_bbsname;
	wstrarray token;
	wstrnummap::iterator nmit;
	O2BoardExMapIt exit;
	uint64 total;
	uint64 num;

	if (boards.empty())
		return false;

	wstrnummap nummap;
	if (db) total = db->select_datcount(nummap);

	ExLock.Lock();
	for (exit = exmap.begin(); exit != exmap.end(); exit++) {
		nummap.insert(wstrnummap::value_type(exit->first, 0));
	}
	ExLock.Unlock();

	for (size_t i = 0; i < boards.size(); i++) {
		if (category != boards[i].category) {
			category = boards[i].category;
			if (i != 0)
				xml += L"</category>"EOL;
			xml += L"<category>"EOL;
			xml_AddElement(xml, L"categoryname", NULL, category.c_str());
		}
		xml += L"<board>"EOL;

		xml_AddElement(xml, L"bbsname",	NULL, boards[i].bbsname.c_str());
		xml_AddElement(xml, L"title",	NULL, boards[i].title.c_str(), true);
		xml_AddElement(xml, L"host",	NULL, boards[i].host.c_str());
		xml_AddElement(xml, L"domain",	NULL, boards[i].domain.c_str());

		domain_bbsname = boards[i].domain + L":" + boards[i].bbsname;
		nmit = nummap.find(domain_bbsname);
		num = nmit == nummap.end() ? 0 : nmit->second;
		xml_AddElement(xml, L"datnum",	NULL, num);
		//xml_AddElement(xml, L"ratio",	NULL, total == 0 ? 0 : (uint)((double)num/total*100.0));
		xml_AddElement(xml, L"ratio",	NULL, num > 1000 ? 100 : (uint)((double)num/1000*100.0));
		if (nmit != nummap.end())
			nummap.erase(nmit);

		ExLock.Lock();
		exit = exmap.find(domain_bbsname);
		xml_AddElement(xml, L"enable",	NULL, exit != exmap.end() && exit->second->enable ? L"1" : L"0");
		ExLock.Unlock();

		xml += L"</board>"EOL;
	}
	xml += L"</category>"EOL;

	//
	xml += L"<category>"EOL;
	xml_AddElement(xml, L"categoryname", NULL, L"不明");
	for (nmit = nummap.begin(); nmit != nummap.end(); nmit++) {
		wsplit(nmit->first.c_str(), L":", token);
		xml += L"<board>"EOL;
		xml_AddElement(xml, L"bbsname",	NULL, token[1].c_str());
		xml_AddElement(xml, L"title",	NULL, token[1].c_str(), true);
		xml_AddElement(xml, L"host",	NULL, L"");
		xml_AddElement(xml, L"domain",	NULL, token[0].c_str());
		xml_AddElement(xml, L"datnum",	NULL, nmit->second);
		xml_AddElement(xml, L"ratio",	NULL, total == 0 ? 0 : (uint)((double)nmit->second/total*100.0));
		ExLock.Lock();
		exit = exmap.find(nmit->first);
		xml_AddElement(xml, L"enable",	NULL, exit != exmap.end() ? L"1" : L"0");
		ExLock.Unlock();
		xml += L"</board>"EOL;
	}
	xml += L"</category>"EOL;

	FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
	return true;
}




// ---------------------------------------------------------------------------
//	Save()
//	2channel.brd形式でファイルに保存
// ---------------------------------------------------------------------------
bool
O2Boards::
Save(void)
{
	if (boards.empty())
		return false;
	
	wstring s;
	BoardsLock.Lock();
	{
		wstring category;
		s += _T("0,0\r\n");
		for (uint i = 0; i < boards.size(); i++) {
			if (category != boards[i].category) {
				category = boards[i].category;
				s += category;
				s += L"\t0\r\n";
			}
			s += L"\t";
			s += boards[i].host;
			s += L"\t";
			s += boards[i].bbsname;
			s += L"\t";
			s += boards[i].title;
			s += L"\r\n";
		}
	}
	BoardsLock.Unlock();

	string sjis;
	FromUnicode(L"shift_jis", s, sjis);

/*
	FILE *fp;
	if (_tfopen_s(&fp, filepath.c_str(), _T("wb")) != 0)
		return false;
	fwrite(&sjis[0], sjis.size(), 1, fp);
	fclose(fp);
*/
	File f;
	if (!f.open(filepath.c_str(), MODE_W))
		return false;
	f.write((void*)&sjis[0], sjis.size());
	f.close();

	struct _utimbuf times;
	times.actime  = LastModified;
	times.modtime = LastModified;
	_wutime(filepath.c_str(), &times);

	return true;
}

// ---------------------------------------------------------------------------
//	SaveEx()
//	独自の情報を別ファイルに保存
// ---------------------------------------------------------------------------
bool
O2Boards::
SaveEx(void)
{
	wstring xml;
	xml += L"<?xml version=\"1.0\" encoding=\"";
	xml += _T(DEFAULT_XML_CHARSET);
	xml += L"\"?>"EOL;
	xml += L"<boards>"EOL;

	ExLock.Lock();
	{
		for (O2BoardExMapIt it = exmap.begin(); it != exmap.end(); it++) {
			xml += L"<board>"EOL;
			xml_AddElement(xml, L"name",	NULL, it->first.c_str());
			xml_AddElement(xml, L"enable",	NULL, it->second->enable ? L"1" : L"0");
			xml += L"</board>"EOL;
		}
	}
	ExLock.Unlock();
	xml += L"</boards>"EOL;

	string out;
	FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
/*
	FILE *fp;
	if (_wfopen_s(&fp, exfilepath.c_str(), L"wb") != 0)
		return false;
	fwrite(&out[0], 1, out.size(), fp);
	fclose(fp);
*/
	File f;
	if (!f.open(exfilepath.c_str(), MODE_W))
		return false;
	f.write((void*)&out[0], out.size());
	f.close();

	return true;
}




// ---------------------------------------------------------------------------
//	Load()
//	2channel.brd形式のファイルから読み込む
// ---------------------------------------------------------------------------
bool
O2Boards::
Load(const wchar_t *fn)
{
	const wchar_t *filename = fn ? fn : filepath.c_str();

	struct _stat st;
	if (_tstat(filename, &st) == -1)
		return false;

	if (st.st_size == 0)
		return false;

	FILE *fp;
	if (_tfopen_s(&fp, filename, _T("rb")) != 0)
		return false;

	string buff;
	buff.resize(st.st_size);
	fread(&buff[0], 1, st.st_size, fp);
	fclose(fp);

	wstring unicode;
	if (!ToUnicode(L"shift_jis", buff, unicode))
		return false;

	BoardsLock.Lock();
	boards.clear();
	BoardsLock.Unlock();

	std::wstringstream ss(unicode);
	std::wstring line;
	std::wstring category;

	std::getline(ss, line); //skip first line
	while (!ss.fail()) {
		std::getline(ss, line);
		if (line.empty())
			continue;
		if (*(line.end()-1) == L'\r')
			line.erase(line.end()-1);

		boost::wregex regex(L"^\t?([^[\t]+)\t([^[\t]+)(?:\t([^[\t]+))?$");
		boost::wsmatch m;
		boost::regex_match(line, m, regex);

		if (line[0] != L'\t') {
			category = m.str(1);
		}
		else {
			const wchar_t *domain = host2domain(m.str(1).c_str());
			if (!domain)
				continue;
			O2Board brd;
			brd.bbsname = m.str(2);
			brd.title = m.str(3);
			brd.category = category;
			brd.host = m.str(1);
			brd.domain = domain;

			BoardsLock.Lock();
			O2BoardArrayIt it = std::find(boards.begin(), boards.end(), brd);
			if (it == boards.end())
				boards.push_back(brd);
			BoardsLock.Unlock();
		}
	}
	LastModified = st.st_mtime;

	/*
	TRACEA("st.st_mtime:");
	TRACEA(ctime(&st.st_mtime));
	TRACEA("bbsmenu_lastmodified:");
	TRACEA(ctime(&bbsmenu_lastmodified));
	*/

	return true;
}

// ---------------------------------------------------------------------------
//	LoadEx()
//	独自の情報を別ファイルから読む
// ---------------------------------------------------------------------------
bool
O2Boards::
LoadEx(void)
{
	bool ret = false;

	struct _stat st;
	if (_wstat(exfilepath.c_str(), &st) == -1)
		return false;
	if (st.st_size == 0)
		return false;

	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	parser->setContentHandler(this);
	parser->setErrorHandler(this);

	try {
		LocalFileInputSource source(exfilepath.c_str());
		parser->parse(source);
		ret = true;
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
	return (ret);
}




void
O2Boards::
endDocument(void)
{
}

void
O2Boards::
startElement(const XMLCh* const uri
		   , const XMLCh* const localname
		   , const XMLCh* const qname
		   , const Attributes& attrs)
{
	if (MATCHLNAME(L"name")) {
		parse_elm = 1;
	}
	else if (MATCHLNAME(L"enable")) {
		parse_elm = 2;
	}
	else {
		parse_elm = 0;
	}
}

void
O2Boards::
endElement(const XMLCh* const uri
		 , const XMLCh* const localname
		 , const XMLCh* const qname)
{
	parse_elm = 0;
	if (MATCHLNAME(L"board")) {
		ExLock.Lock();
		{
			O2BoardExMapIt exit = exmap.find(parse_name);
			if (exit == exmap.end()) {
				O2BoardEx *ex = new O2BoardEx();
				ex->enable = parse_enable;
				ex->collectors.SetObject(Profile, Client);
				exmap.insert(O2BoardExMap::value_type(parse_name, ex));
			}
		}
		ExLock.Unlock();
	}
}

void
O2Boards::
characters(const XMLCh* const chars
		 , const unsigned int length)
{
	switch (parse_elm) {
		case 1:
			parse_name.assign(chars, length);
			break;
		case 2:
			parse_enable = chars[0] == L'0' ? false : true;
			break;
	}
}
