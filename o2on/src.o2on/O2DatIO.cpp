/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2DatIO.cpp
 * description	: dat I/O class
 *
 */

#pragma once
#include "O2DatIO.h"
#include "O2Key.h"
#include "httpheader.h"
#include "file.h"
#include "dataconv.h"
#include "thread.h"
#include "../cryptopp/osrng.h"
#include "StopWatch.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <fstream>

#define MODULE			L"DatIO"




/*
 *	O2DatIO()
 *	コンストラクタ
 */
O2DatIO::
O2DatIO(O2DatDB *db, O2Logger *lgr, O2Profile *prof, O2ProgressInfo *proginfo)
	: DatDB(db)
	, Logger(lgr)
	, Profile(prof)
	, hwndEmergencyHaltCallback(NULL)
	, msgEmergencyHaltCallback(0)
	, ProgressInfo(proginfo)
	, RebuildDBThreadHandle(NULL)
	, ReindexThreadHandle(NULL)
	, LoopRebuildDB(false)
	, AnalyzeThreadHandle(0)

{
	DWORD BytesPerSector;
	DWORD SectorsPerCluster;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;

	ClusterSize = 1;
	wchar_t abspath[_MAX_PATH];
	_wfullpath(abspath, Profile->GetCacheRootW(), _MAX_PATH);
	if (wcslen(abspath) >= 2) {
		wstring drive(abspath, abspath+2);
		drive += L"\\";

		if (GetDiskFreeSpace(
				drive.c_str(),
				//Profile->GetCacheRootW(),
				&SectorsPerCluster, &BytesPerSector,
				&NumberOfFreeClusters, &TotalNumberOfClusters)) {
			ClusterSize = (uint64)SectorsPerCluster * (uint64)BytesPerSector;
		}
	}
}




/*
 *	~O2DatIO()
 *	デストラクタ
 */
O2DatIO::
~O2DatIO()
{
}




uint64
O2DatIO::
GetDiskFileSize(uint64 size)
{
	uint64 mod = size % ClusterSize;
	if (mod == 0)
		return (size);
	return (size + (ClusterSize - mod));
}




/*
 *	CheckQuarterOverflow()
 *	
 */
bool
O2DatIO::
CheckQuarterOverflow(uint64 add_size)
{
	uint64 quarter_size = Profile->GetQuarterSize();

	if (quarter_size && DatDB->select_totaldisksize() + add_size >= quarter_size) {
		if (hwndEmergencyHaltCallback) {
			PostMessage(
				hwndEmergencyHaltCallback,
				msgEmergencyHaltCallback,
				0, 0);
		}
		return true;
	}
	return false;
}




// ---------------------------------------------------------------------------
//	SetEmergencyHaltMsg()
//	
// ---------------------------------------------------------------------------

void
O2DatIO::
SetEmergencyHaltCallbackMsg(HWND hwnd, UINT msg)
{
	hwndEmergencyHaltCallback = hwnd;
	msgEmergencyHaltCallback = msg;
}




// ---------------------------------------------------------------------------
//	KakoHantei()
//	
// ---------------------------------------------------------------------------

bool
O2DatIO::
KakoHantei(const O2DatPath &datpath)
{
	string path;
	datpath.getpath(Profile->GetCacheRootA(), path);

	MappedFile mf;
	char *p = (char*)mf.open(path.c_str(), 0, false);
	if (p == NULL)
		return false;

	return (KakoHantei(p, mf.size(), datpath.is_be()));
}

bool
O2DatIO::
KakoHantei(const char *dat, uint64 len, bool is_be)
{
	int lf = 0;
	const char *p[2] = {NULL};

	for (__int64 i = len-1; i >= 0; i--) {
		if (dat[i] == '\n') { p[lf] = &dat[i]; lf++; }
		if (lf == 2) break;
	}

	if (lf != 2)
		return false;

	p[1]++;
	size_t linelen = p[0] - p[1];

	string encoding;
	if (is_be)
		sjis_or_euc(string(dat), encoding);
	if (encoding.empty())
		encoding = "shift_jis";

	string over1000 = "１００１<><>Over 1000 Thread";
	string stop = "<>停止したよ。";
	string rula = "<>移転";
	if (encoding == "euc-jp"){
		sjis2euc(over1000);
		sjis2euc(stop);
		sjis2euc(rula);
	}

	// -----------------------------------------------------------------------
	//	1000: 投稿日が「Over 1000 Thread」
	//	ex) １００１<><>Over 1000 Thread<>このスレッドは１０００を超えました。 <br> もう書けないので、新しいスレッドを立ててくださいです。。。 <>
	// -----------------------------------------------------------------------
	if (linelen >= 28 && strncmp(p[1], over1000.c_str(), 28) == 0)
		return true;
	
	// -----------------------------------------------------------------------
	//	スレスト: 行末が「<>停止したよ。」
	//	ex) 停止しました。。。<>停止<>停止<>真・スレッドストッパー。。。(￣ー￣)ﾆﾔﾘｯ<>停止したよ。
	// -----------------------------------------------------------------------
	if (linelen >= 14 && strncmp(p[0]-14, stop.c_str(), 14) == 0)
		return true;

	// -----------------------------------------------------------------------
	//	飛ばし: 行末が「<>移転」
	//	ex) 妄想族ψψψ ★<>sage<>移転＆停止<> <br> 妄想族ψψψ ★ さんが飛ばしました。(￣ー￣)ﾆﾔﾘｯ <br> <br> BE ポイント = 4780 から 20 消費しました。<br> <>移転
	// -----------------------------------------------------------------------
	if (linelen >= 6 && strncmp(p[0]-6, rula.c_str(), 6) == 0)
		return true;

	return false;
}




// ---------------------------------------------------------------------------
//	CheckDat()
//	
// ---------------------------------------------------------------------------

bool
O2DatIO::
CheckDat(const char *in, uint64 inlen)
{
	if (inlen < 2)
		return false;
	if (in[inlen-1] != '\n')
		return false;
	if (in[inlen-2] == '\r')
		return false;

	const char *lf = strchr(in, '\n');
	const char *p = in;
	int cnt = 0;
	while (p < lf) {
		if (*p == '<' && *(p+1) == '>') {
			cnt++;
			p += 2;
			continue;
		}
		p++;
	}
	if (cnt != 4)
		return false;
	return true;
}

//
// 2012-06-15 簡易チェック用　by fujisaki
//
bool
O2DatIO::
CheckDat2(const char *in, uint64 inlen)
{
	if (inlen < 2)
		return false;
	if (in[inlen-1] != '\n')
		return false;
	if (in[inlen-2] == '\r')
		return false;

	const char *end = in + inlen;
	const char *p = in + 2;
	int cnt = 0;

	while (p < end) {
		if (*p == '<' && *(p+1) == '>') {
			cnt++;
			p += 2;
			continue;
		}

		if (*p == '\n') {
			if (cnt != 4 && cnt != 5)
				return false;

			cnt = 0;
		}
		
		p++;
	}
	return true;
}


// ---------------------------------------------------------------------------
//	GetTitle
//
// ---------------------------------------------------------------------------

bool
O2DatIO::
GetTitle(O2DatPath &datpath)
{
	wstring path;
	datpath.getpath(Profile->GetCacheRootW(), path);

	MappedFile mf;
	char *p = (char*)mf.open(path.c_str(), mf.allocG(), false);
	if (p == NULL)
		return false;

	uint end = mf.allocG();
	if (mf.size() < end)
		end = (uint)mf.size();

	uint pos_lf = 0;
	bool lf_found = false;
	while (pos_lf < end && p[pos_lf] != '\n')
		pos_lf++;

	if (pos_lf == 0) {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
				"タイトル取得失敗:行頭にLF (%s)", path.c_str());
		}
		return false;
	}

	if (pos_lf == end || p[pos_lf] != '\n') {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
				"タイトル取得失敗:LF無 (%s)", path.c_str());
		}
		return false;
	}

	uint pos_gt = pos_lf-1;
	while (pos_gt > 0 && p[pos_gt] !=  '>')
		pos_gt--;

	if (pos_gt == 0 || p[pos_gt] != '>') {
		if (Logger) {
			Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
				"タイトル取得失敗:セパレータ無 (%s)", path.c_str());
		}
		return false;
	}

	uint pos_title = pos_gt+1;
	string tmp(&p[pos_title], &p[pos_lf]);
	
	wstring encoding;
	if (datpath.is_be())
		sjis_or_euc(string(p, end), encoding);
	if (encoding.empty())
		encoding = L"shift_jis";

	wstring title;
	ToUnicode(encoding.c_str(), tmp, title);

	//ASCIIコントロールコードが入っていたら?に置き換える
	//0x00 - 0x1f, 0x7f
	for (uint i = 0; i < title.size(); i++) {
		if ((wchar_t)title[i] <= 0x001f || (wchar_t)title[i] == 0x007f)
			title[i] = L'?';
	}

	if (title.size() > O2_MAX_KEY_TITLE_LEN) {
		title.resize(O2_MAX_KEY_TITLE_LEN - 1);
		title += L"…";
	}

	datpath.settitle(title.c_str());
	return true;
}




// ---------------------------------------------------------------------------
//	GetSize()
//
// ---------------------------------------------------------------------------

uint64
O2DatIO::
GetSize(const O2DatPath &datpath)
{
	string path;
	datpath.getpath(Profile->GetCacheRootA(), path);

	struct _stat st;
	if (_stat(path.c_str(), &st) == -1)
		return (0);

	return (st.st_size);
}




// ---------------------------------------------------------------------------
//	Load()
//
// ---------------------------------------------------------------------------

bool
O2DatIO::
Load(const O2DatPath &datpath, uint64 offset, string &out)
{
	bool ret = false;
	struct _stat64 st;
	FILE *fp = NULL;

	string path;
	datpath.getpath(Profile->GetCacheRootA(), path);

	if (_stat64(path.c_str(), &st) == -1)
		goto cleanup;

	__int64 filesize = st.st_size;
	if (filesize <= (int)offset)
		goto cleanup;

	if (fopen_s(&fp, path.c_str(), "rb") != 0)
		goto cleanup;

	if (_fseeki64(fp, (__int64)offset, SEEK_SET) != 0)
		goto cleanup;
	
	out.resize((size_t)(st.st_size - offset));
	if (!fread(&out[0], (size_t)(filesize - offset), 1, fp))
		goto cleanup;

	ret = true;

cleanup:
	if (fp)
		fclose(fp);

	return (ret);
}

bool
O2DatIO::
Load(const hashT &hash, uint64 offset, string &out, O2DatPath &datpath)
{
	O2DatRec rec;
	if (!DatDB->select(rec, hash))
		return false;

	if (rec.size <= offset)
		return false;

	datpath.set(rec.url.c_str());
	return (Load(datpath, 0, out)); //全部読む
}





// ---------------------------------------------------------------------------
//	Delete()
//
// ---------------------------------------------------------------------------

bool
O2DatIO::
Delete(const hashListT &hashlist)
{
	hashListT::const_iterator it;
	O2DatPath datpath;
	wstring path;
	O2DatRec rec;

	for (it = hashlist.begin(); it != hashlist.end(); it++) {
		if (!DatDB->select(rec, *it))
			continue;

		datpath.set(rec.domain.c_str(), rec.bbsname.c_str(), rec.datname.c_str());
		datpath.getpath(Profile->GetCacheRootW(), path);
#if 1
		DWORD attr = GetFileAttributesW(path.c_str());
		if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_READONLY)) {
			attr ^= FILE_ATTRIBUTE_READONLY;
			SetFileAttributesW(path.c_str(), attr);
		}
#endif
		if (!DeleteFile(path.c_str()))
			continue;

		DatDB->remove(*it);
	}

	return true;
}




// ---------------------------------------------------------------------------
//	RandomGet()
//	※この関数は将来使わなくする
// ---------------------------------------------------------------------------

bool
O2DatIO::
RandomGet(string &out, O2DatPath &datpath)
{
	O2DatRec rec;
	if (!DatDB->select(rec))
		return false;

	datpath.set(rec.url.c_str());
	return (Load(datpath, 0, out));
}




// ---------------------------------------------------------------------------
//	RandomGetInBoard()
//	
// ---------------------------------------------------------------------------
bool
O2DatIO::
RandomGetInBoard(const wchar_t *domain, const wchar_t *bbsname, string &out, O2DatPath &datpath)
{
	O2DatRec rec;
	if (!DatDB->select(rec, domain, bbsname))
		return false;

	datpath.set(rec.url.c_str());
	return (Load(datpath, 0, out));
}




/*
 *	Put()
 *	
 */
uint64
O2DatIO::
Put(O2DatPath &datpath, const char *dat, uint64 len, uint64 startpos)
{
	if (datpath.domaintype() == DOMAINTYPE_MACHI)
		return (0);

	string path;
	datpath.getpath(Profile->GetCacheRootA(), path);
	if (!datpath.makedir(Profile->GetCacheRootA()))
		return false;

	struct _stat64 st;
	//FILE *fp;
	strmap::const_iterator it;
	uint64 hokan_byte = 0;
	uint64 total_size = 0;

	//write
	if (_stat64(path.c_str(), &st) == -1 || st.st_atime == 0) {
		//dat無し
		if (startpos != 0) {
			//dat無し&差分：キャッシュしない
			TRACEA(">error (1)\n");
			return (0);
		}

		hokan_byte = len;
		if (CheckQuarterOverflow(hokan_byte))
			return (0);
/*
		if (fopen_s(&fp, path.c_str(), "wb") != 0)
			return (0);
		fwrite(dat, 1, (size_t)len, fp);
		fclose(fp);
*/
		File f;
		if (!f.open(path.c_str(), MODE_W))
			return (0);
		f.write((void*)dat, (size_t)len);
		f.close();

		total_size = len;
		TRACEA(">cached (1)\n");
	}
	else {

#if 1
		DWORD attr = GetFileAttributesA(path.c_str());
		if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_READONLY)) {
			attr ^= FILE_ATTRIBUTE_READONLY;
			SetFileAttributesA(path.c_str(), attr);
		}
#endif
		//dat有り
		if (st.st_size < (__int64)startpos) {
			//dat有り&差分&足りない：キャッシュしない
			TRACEA(">error (2)\n");
			return (0);
		}
		else if (st.st_size == startpos) {
			//dat有り&差分&サイズぴったり：追加する
			hokan_byte = len;
			if (CheckQuarterOverflow(hokan_byte))
				return (0);
/*
			if (fopen_s(&fp, path.c_str(), "a+b") != 0)
				return (0);
			fwrite(dat, 1, (size_t)len, fp);
			fclose(fp);
*/
			File f;
			if (!f.open(path.c_str(), MODE_A))
				return (0);
			f.write((void*)dat, (size_t)len);
			f.close();

			total_size = st.st_size + len;
			TRACEA(">cached (2)\n");
		}
		else {
			//dat有り&重複有り：diff
			string cache;
			if (!Load(datpath, 0, cache)) {
				TRACEA(">error (3)\n");
				return (0);
			}
			if (memcmp(&cache[(size_t)startpos], &dat[0], cache.size()-(size_t)startpos) != 0) {
				TRACEA(">error (5)\n");
				return (0);
			}

			len = startpos + len - cache.size();
			if (len <= 0) {
				TRACEA(">same\n");
				return (0);
			}

			hokan_byte = len;
			if (CheckQuarterOverflow(hokan_byte))
				return (0);
/*
			if (fopen_s(&fp, path.c_str(), "a+b") != 0)
				return (0);
			fwrite(&dat[cache.size() - startpos], 1, (size_t)len, fp);
			fclose(fp);
*/
			File f;
			if (!f.open(path.c_str(), MODE_A))
				return (0);
			f.write((void*)&dat[cache.size() - startpos], (size_t)len);
			f.close();

			total_size = startpos + len;
			TRACEA(">cached (3)\n");
		}
	}

	if (hokan_byte) {

		O2DatRec rec;
		datpath.gethash(rec.hash);
		datpath.element(rec.domain, rec.bbsname, rec.datname);
		datpath.geturl(rec.url);
		GetTitle(datpath);
		datpath.gettitle(rec.title);
		rec.size = total_size;
		rec.disksize = GetDiskFileSize(total_size);
		rec.res = 0;
		DatDB->AddUpdateQueue(rec);
	}

	return (hokan_byte);
}




bool
O2DatIO::
ExportToXML(const wchar_t *domain, const wchar_t *bbsname, string &out)
{
	wchar_t tmp[32];
	wstring tmpstr;
	struct tm tm;
	time_t t;
	uint64 totaldisksize = 0;

	wstring dir(Profile->GetCacheRootW());
	dir += L"\\";
	dir += domain;
	dir += L"\\";
	dir += bbsname;

	wstring xml;
	xml += L"<dir>";
	xml += domain;
	xml += L"/";
	xml += bbsname;
	xml += L"</dir>";

	O2DatRecList reclist;
	DatDB->select(reclist, domain, bbsname);
	for (O2DatRecListIt it = reclist.begin(); it != reclist.end(); it++) {
		xml += L"<thread>"EOL;
		xml += L"<domain>";
		xml += domain;
		xml += L"</domain>"EOL;
		xml += L"<bbsname>";
		xml += bbsname;
		xml += L"</bbsname>"EOL;
		xml += L"<datname>";
		xml += it->datname;
		xml += L"</datname>"EOL;
		xml += L"<url>";
		xml += it->url;
		xml += L"</url>"EOL;
		makeCDATA(it->title, tmpstr);
		xml += L"<title>";
		xml += tmpstr;
		xml += L"</title>"EOL;
		swprintf_s(tmp, 32, L"%I64u", it->size);
		xml += L"<size>";
		xml += tmp;
		xml += L"</size>"EOL;
		it->hash.to_string(tmpstr);
		xml += L"<hash>";
		xml += tmpstr;
		xml += L"</hash>"EOL;
		t = _wcstoui64(it->datname.c_str(), NULL, 10);
		localtime_s(&tm, &t);
		wcsftime(tmp, 32, L"%Y/%m/%d %H:%M:%S ", &tm);
		xml += L"<date>";
		xml += tmp;
		xml += L"</date>"EOL;

		xml += L"</thread>"EOL;
		totaldisksize += it->disksize;
	}

	swprintf_s(tmp, 32, L"%d", reclist.size());
	xml += L"<datnum>";
	xml += tmp;
	xml += L"</datnum>";
	swprintf_s(tmp, 32, L"%I64u", totaldisksize);
	xml += L"<datsize>";
	xml += tmp;
	xml += L"</datsize>";

	FromUnicode(_T(DEFAULT_XML_CHARSET), xml, out);
	return true;
}

bool
O2DatIO::
Dat2HTML(const hashT &hash, string &out, string &encoding)
{
	char tmp[32];

	O2DatPath datpath;
	string dat;
	if (!Load(hash, 0, dat, datpath)) {
		encoding = "shift_jis";
		return false;
	}

	if (datpath.is_be())
		sjis_or_euc(dat, encoding);
	if (encoding.empty())
		encoding = "shift_jis";

	string colon = "：";
	string broken = "壊れてる？";
	if (encoding == "euc-jp"){
		sjis2euc(colon);
		sjis2euc(broken);
	}

	string url;
	datpath.geturl(url);

	bool firstres = false;
	size_t pos1 = 0;
	size_t pos2 = 0;
	uint n = 1;
	while (pos1 < dat.size()) {
		pos2 = dat.find("\n", pos1);
		if (pos2 == string::npos)
			break;
		string line(dat, pos1, pos2-pos1);
		strarray token;
		splitstr(line.c_str(), "<>", token);

		if (!firstres && token.size() == 5) {
			out += "<html>\r\n";
			out += "<head>\r\n";
			out += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=";
			out += encoding;
			out += "\">\r\n";
			out += "<title>";
			out += token[4];
			out += "</title>\r\n";
			out += "</head>\r\n";
			out += "<body bgcolor=#efefef text=black link=blue alink=red vlink=#660099>\r\n";

			out += url;
			out += "<hr>";

			out += "<font size=\"+1\" color=\"red\">";
			out += token[4];
			out += "</font>\r\n";
			out += "<dl class=\"thread\">\r\n";
			firstres = true;
		}

		out += "<dt>";
		sprintf_s(tmp, 32, "%d %s", n, colon.c_str());
		out += tmp;

		if (token.size() >= 4) {
			if (token[1].empty()) {
				out += "<font color=green><b>";
				out += token[0];
				out += "</b></font>";
			}
			else {
				out += "<a href=\"mailto:";
				out += token[1];
				out += "\"><b>";
				out += token[0];
				out += "</b></a>";
			}
			out += colon;
			out += token[2];

			out += "<dd>";
			out += token[3];
			out += "<br><br>\r\n";
		}
		else {
			out += broken;
			out += "<dd>";
			out += line;
			out += "<br><br>\r\n";
		}

		pos1 = pos2 + 1;
		n++;
	}
	out += "</dl>\r\n";
	out += "<font color=red face=\"Arial\"><b>";
	sprintf_s(tmp, 32, "%d", dat.size()/1024);
	out += tmp;
	out += " KB</b></font>\r\n";

	return true;
}




// ---------------------------------------------------------------------------
//	GetLocalFileKeys
//
// ---------------------------------------------------------------------------
size_t
O2DatIO::
GetLocalFileKeys(O2KeyList &keylist, time_t publish_tt, size_t limit)
{
	O2DatRecList reclist;
	DatDB->select(reclist, publish_tt, limit);

	O2Key key;
	Profile->GetID(key.nodeid);
	key.ip = Profile->GetIP();
	key.port = Profile->GetP2PPort();
	key.date = 0;

	for (O2DatRecListIt it = reclist.begin(); it != reclist.end(); it++) {
		key.hash = it->hash;
		key.size = it->size;
		key.url = it->url;
		key.title = it->title;
		keylist.push_back(key);
	}

	return (keylist.size());
}




// ---------------------------------------------------------------------------
//	RebuildDB
//
// ---------------------------------------------------------------------------
void
O2DatIO::
RebuildDB(void)
{
	if (RebuildDBThreadHandle)
		return;
	LoopRebuildDB = true;

	RebuildDBThreadHandle = (HANDLE)_beginthreadex(
		NULL, 0, StaticRebuildDBThread, (void*)this, 0, NULL);
}
void
O2DatIO::
StopRebuildDB(void)
{
	if (!RebuildDBThreadHandle)
		return;
	LoopRebuildDB = false;
	WaitForSingleObject(RebuildDBThreadHandle, INFINITE);
}

uint WINAPI
O2DatIO::
StaticRebuildDBThread(void *data)
{
	O2DatIO *me = (O2DatIO*)data;

	CoInitialize(NULL);
	me->DatDB->StopUpdateThread();
	O2DatRecList reclist;
	if (me->DatDB->before_rebuild()) {
		me->RebuildDBThread(me->Profile->GetCacheRootW(), 0, reclist);
		bool manualstop = !me->LoopRebuildDB;
		// 手動で停止した場合は差し替えなどをしない
		if (!manualstop) {
			if (me->DatDB->after_rebuild())
				me->DatDB->analyze();
			else
				me->Logger->AddLog(O2LT_ERROR, L"DB再構築", 0, 0, "再構築済みDB差し替え失敗");
		}
	}
	else {
		me->Logger->AddLog(O2LT_ERROR, L"DB再構築", 0, 0, "再構築用DB作成失敗");
		me->ProgressInfo->Reset(false, false);
	}
	me->DatDB->StartUpdateThread();
	CoUninitialize();

	CloseHandle(me->RebuildDBThreadHandle);
	me->RebuildDBThreadHandle = NULL;

	//_endthreadex(0);
	return (0);
}

void
O2DatIO::
RebuildDBThread(const wchar_t *dir, uint level, O2DatRecList &reclist)
{
	if (level == 0) {
		ProgressInfo->Reset(true, true);
	}
	if (level == 3) {
		ProgressInfo->SetMessage(dir+wcslen(Profile->GetCacheRootW()));
	}

	WIN32_FIND_DATAW wfd;
	HANDLE handle;
	wstrarray dirs;
	wchar_t findpath[MAX_PATH];
	swprintf_s(findpath, MAX_PATH, L"%s\\*.*", dir);
	O2DatPath datpath;
	O2DatRec rec;
	wstrarray paths;
	wchar_t path[MAX_PATH];

	handle = FindFirstFileW(findpath, &wfd);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	do {
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (wfd.cFileName[0] != L'.') {
				if (level == 3)
					Logger->AddLog(O2LT_WARNING, L"DB再構築", 0, 0,
						L"余計なディレクトリがあるよ(%s\\%s)", dir, wfd.cFileName);
				else
					dirs.push_back(wfd.cFileName);
			}
		}
		else {
			if (wcscmp(wfd.cFileName, L".index") == 0) {
				swprintf_s(path, MAX_PATH, L"%s\\%s", dir, wfd.cFileName);
				DeleteFile(path);
				continue;
			}
			else if (level != 3) {
				Logger->AddLog(O2LT_WARNING, L"DB再構築", 0, 0,
					L"変なファイルがあるよ(%s\\%s)", dir, wfd.cFileName);
				continue;
			}

			if (level != 3) continue;

			wsplit(dir+wcslen(Profile->GetCacheRootW()), L"\\", paths);
			if (!datpath.set(paths[0].c_str(), paths[1].c_str(), wfd.cFileName)) {
				Logger->AddLog(O2LT_WARNING, L"DB再構築", 0, 0,
					L"datじゃないファイル？(%s\\%s)", dir, wfd.cFileName);
				continue;
			}
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
				swprintf_s(path, MAX_PATH, L"%s\\%s", dir, wfd.cFileName);
				wfd.dwFileAttributes ^= FILE_ATTRIBUTE_READONLY;
				SetFileAttributes(path, wfd.dwFileAttributes);
			}

			datpath.gethash(rec.hash);
			rec.domain = paths[0];
			rec.bbsname = paths[1];
			rec.datname = wfd.cFileName;
			rec.size = ((uint64)wfd.nFileSizeHigh << 32) | (uint64)wfd.nFileSizeLow;
			rec.disksize = GetDiskFileSize(rec.size);
			datpath.geturl(rec.url);
			GetTitle(datpath);
			datpath.gettitle(rec.title);
			rec.res = 0;
			reclist.push_back(rec);
			if (!DatDB->check_queue_size(reclist)) {
				DatDB->insert(reclist, true);
				reclist.clear();
			}
		}
	} while (LoopRebuildDB && FindNextFileW(handle, &wfd));
	FindClose(handle);

	if (!dirs.empty()) {
		if (level == 1)
			ProgressInfo->AddMax(dirs.size());

		for (uint i = 0; i < dirs.size() && LoopRebuildDB; i++) {
			const wchar_t *s = dirs[i].c_str();
			bool invalid = false;
			switch (level) {
				case 0:
					if (wcscmp(s, _T(DOMAIN_2CH)) != 0
						&& wcscmp(s, _T(DOMAIN_BBSPINK)) != 0
						&& wcscmp(s, _T(DOMAIN_MACHI)) != 0) {
							invalid = true;
TRACEA("おかしい(0)\n");
TRACEW(s);
TRACEA("\n");
					}
					break;
				case 1:
					// 板フォルダ名のチェック
					for (uint j = 0; j < wcslen(s); j++) {
						if (!(s[j] >= 0x30 && s[j] <= 0x39)
							&& !(s[j] >= 0x41 && s[j] <= 0x5A) // 2013-09-12 Fujisaki
							&& !(s[j] >= 0x61 && s[j] <= 0x7a)) {
								invalid = true;
								Logger->AddLog(O2LT_WARNING, L"DB再構築", 0, 0,
									L"変なディレクトリがあるよ(%s\\%s)", dir, wfd.cFileName);
TRACEA("おかしい(1)\n");
TRACEW(s);
TRACEA("\n");
								break;
						}
					}
					break;
				case 2:
					// 4桁の数字フォルダ名のチェック
					if (wcslen(s) != 4
						|| !(s[0] >= 0x30 && s[0] <= 0x39)
						|| !(s[1] >= 0x30 && s[1] <= 0x39)
						|| !(s[2] >= 0x30 && s[2] <= 0x39)
						|| !(s[3] >= 0x30 && s[3] <= 0x39)) {
							Logger->AddLog(O2LT_WARNING, L"DB再構築", 0, 0,
								L"変なディレクトリがあるよ(%s\\%s)", dir, wfd.cFileName);
							invalid = true;
TRACEA("おかしい(2)\n");
TRACEW(s);
TRACEA("\n");
					}
					break;
				default:
					invalid = true;
			}

			if (invalid)
				continue;

			swprintf_s(path, MAX_PATH, L"%s\\%s", dir, s);
			RebuildDBThread(path, level+1, reclist); //再帰

			if (level == 1)
				ProgressInfo->AddPos(1);
		}
	}

	if (level == 0) {
		if (LoopRebuildDB && !reclist.empty()) {
			DatDB->insert(reclist, true);
			reclist.clear();
		}
		ProgressInfo->Reset(false, true);
		CLEAR_WORKSET;
	}
}

void
O2DatIO::
Reindex(void)
{
	if (ReindexThreadHandle)
		return;
	ReindexThreadHandle = (HANDLE)_beginthreadex(
		NULL, 0, StaticReindexThread, (void*)this, 0, NULL);
}

uint WINAPI
O2DatIO::
StaticReindexThread(void *data)
{
	static char *targets[] = {"dat","idx_dat_domain_bbsname_datname","idx_dat_lastpublish","idx_dat_datname"};

	O2DatIO *me = (O2DatIO*)data;

	CoInitialize(NULL);
	me->ProgressInfo->Reset(true, false);
	me->ProgressInfo->SetMessage(L"reindex...");
	me->ProgressInfo->AddMax(4);

	for (size_t i = 0; i < 4; i++) {
		wstring tmp;
		ascii2unicode(targets[i], strlen(targets[i]), tmp);
		tmp.insert(0, L"reindex ");
		tmp.append(L"...");
		me->ProgressInfo->SetMessage(tmp.c_str());
		me->DatDB->reindex(targets[i]);
		me->ProgressInfo->AddPos(1);
	}

	me->ProgressInfo->Reset(false, false);
	CoUninitialize();

	CloseHandle(me->ReindexThreadHandle);
	me->ReindexThreadHandle = NULL;

	//_endthreadex(0);
	return (0);
}

void
O2DatIO::
Analyze(void)
{
	if (AnalyzeThreadHandle)
		return;
	AnalyzeThreadHandle = (HANDLE)_beginthreadex(
		NULL, 0, StaticAnalyzeThread, (void*)this, 0, NULL);
}

uint WINAPI
O2DatIO::
StaticAnalyzeThread(void *data)
{
	O2DatIO *me = (O2DatIO*)data;

	CoInitialize(NULL);
	me->ProgressInfo->Reset(true, false);
	me->ProgressInfo->SetMessage(L"analyze...");
	me->ProgressInfo->AddMax(2);
	me->ProgressInfo->pos = 1;
	me->DatDB->analyze();
	me->ProgressInfo->Reset(false, false);
	CoUninitialize();

	CloseHandle(me->AnalyzeThreadHandle);
	me->AnalyzeThreadHandle = NULL;

	return (0);
}