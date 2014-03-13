/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Server_HTTP_Proxy.h
 * description	: o2on server class
 *
 */

#pragma once
#include "O2Server_HTTP.h"
#include "O2Profile.h"
#include "O2DatIO.h"
#include "O2LagQueryQueue.h"
#include "simplehttpsocket.h"
#include "zlib.h"
#include "thread.h"

#if defined(MODULE)
#undef MODULE
#endif
#define MODULE				L"ProxyServer"
#define RECENT_DAT_LIMIT	20




//
// O2Server_HTTP_Proxy
//
class O2Server_HTTP_Proxy
	: public O2Server_HTTP
{
private:
	O2Profile		*Profile;
	O2DatIO			*DatIO;
	O2Boards		*Boards;
	O2LagQueryQueue	*LagQueryQueue;
	uint64			ThreadNum;
	Mutex			ThreadNumLock;
	O2KeyList		RecentDatList;
	Mutex			RecentDatLock;

public:
	O2Server_HTTP_Proxy(O2Logger		*lgr
					  , O2IPFilter		*ipf
					  , O2Profile		*prof
					  , O2DatIO			*datio
					  , O2Boards		*boards
					  , O2LagQueryQueue	*qq)
		: O2Server_HTTP(MODULE, lgr, ipf)
		, Profile(prof)
		, DatIO(datio)
		, Boards(boards)
		, LagQueryQueue(qq)
		, ThreadNum(0)
	{
	}
	~O2Server_HTTP_Proxy()
	{
	}
	uint64 GetThreadNum(void)
	{
		return (ThreadNum);
	}

	void AddRecentDat(const hashT &hash, const wchar_t *url, const wchar_t *title)
	{
		RecentDatLock.Lock();
		{
			O2Key key;
			key.idkeyhash = hash;
			key.url = url;
			key.title = title;
			key.date = time(NULL);

			O2KeyListIt it = std::find(RecentDatList.begin(), RecentDatList.end(), key);
			if (it != RecentDatList.end())
				RecentDatList.erase(it);
			RecentDatList.push_back(key);
			while (RecentDatList.size() > RECENT_DAT_LIMIT)
				RecentDatList.pop_front();
		}
		RecentDatLock.Unlock();
	}

	void GetRecentDatList(O2KeyList &out)
	{
		RecentDatLock.Lock();
		out.assign(RecentDatList.begin(), RecentDatList.end());
		RecentDatLock.Unlock();
	}

protected:
	// -----------------------------------------------------------------------
	//	O2Server_HTTP methods (override)
	// -----------------------------------------------------------------------
	virtual void ParseRequest(O2SocketSession *ss)
	{
		ThreadParam *param = new ThreadParam;
		param->me = this;
		param->ss = ss;

		HANDLE handle =
			(HANDLE)_beginthreadex(NULL, 0, StaticParseThread, (void*)param, 0, NULL);
		CloseHandle(handle);
	}

private:
	// -----------------------------------------------------------------------
	//	Own methods
	// -----------------------------------------------------------------------
	struct ThreadParam {
		O2Server_HTTP_Proxy *me;
		O2SocketSession *ss;
	};

	static uint WINAPI StaticParseThread(void *data)
	{
		ThreadParam *param = (ThreadParam*)data;
		O2Server_HTTP_Proxy *me = param->me;
		O2SocketSession *ss = param->ss;
		delete param;

		me->ThreadNumLock.Lock();
		me->ThreadNum++;
		me->ThreadNumLock.Unlock();

		CoInitialize(NULL);
		me->ParseThread(ss);
		CoUninitialize();

		me->ThreadNumLock.Lock();
		me->ThreadNum--;
		me->ThreadNumLock.Unlock();

		//_endthreadex(0);
		return (0);
	}

	void ParseThread(O2SocketSession *ss)
	{
		HTTPHeader &hdr = *((HTTPHeader*)ss->data);
		string url = hdr.url;
		string host = hdr.hostname;
		strmap::iterator it;
		char tmp[32];

		O2DatPath datpath;
		uint urltype = datpath.set(url.c_str());
		if (urltype == URLTYPE_UNKNOWN || urltype == URLTYPE_NORMAL || urltype == URLTYPE_MACHI) {
			NormalProxy(ss);
			ss->SetCanDelete(true);
			return;
		}
		wstring tmpurl;
		datpath.geturl(tmpurl);

		uint64 request_first_byte = GetRequestFirstBytePos(hdr);

		if (urltype == URLTYPE_CRAWL) {
			request_first_byte = 0;
		}

		TRACEA("-------------------------------------------------------\n");
		TRACEA("■オリジナルリクエスト\n");
		TRACEA("-------------------------------------------------------\n");
		TRACEA(ss->rbuff.substr(0, hdr.length).c_str());
		
		// -------------------------------------------------------------------
		//
		//	request & get response
		//
		// -------------------------------------------------------------------
		bool has_unknown_host_str = false;
		bool return_original_response = false;
		HTTPHeader ohdr(HTTPHEADERTYPE_RESPONSE);
		string out;

		if (!datpath.is_origurl()) {
			ohdr.status = 404;
			ohdr.AddFieldDate("Date", 0);
			ohdr.AddFieldString("Server", Profile->GetUserAgentA());
			ohdr.AddFieldString("Content-Type", "text/plain; charset=shift_jis");
			ohdr.AddFieldString("Connection", "close");
			has_unknown_host_str = true;
		}
		else {
			RewriteConnectionField(hdr, ss->rbuff);
			HTTPSocket *hsock = new HTTPSocket(SELECT_TIMEOUT_MS, Profile->GetUserAgentA());
			if (!hsock->request(host.c_str(), hdr.port, ss->rbuff.c_str(), ss->rbuff.size(), NULL, 0)) {
				delete hsock;
				ss->SetCanDelete(true);
				return;
			}
			int offset = 0;
			int n;
			while ((n = hsock->response(out, ohdr)) >= 0) {
				if (n > 0) {
					if (urltype != URLTYPE_OFFLAW && (ohdr.status == 200 || ohdr.status == 206 || ohdr.status == 304)) {
						//●以外で200/206/304の場合は即時専ブラに返す
						ss->Lock();
						ss->sbuff.append(&out[offset], &out[offset+n]);
						ss->Unlock();
						offset += n;
						return_original_response = true;
					}
				}
				if (ohdr.contentlength) {
					if (out.size() - ohdr.length >= ohdr.contentlength)
						break;
				}
			}
			delete hsock;
		}
		
		TRACEA("-------------------------------------------------------\n");
		TRACEA("■オリジナルレスポンス\n");
		TRACEA("-------------------------------------------------------\n");
		TRACEA(out.substr(0, ohdr.length).c_str());
		//TRACEA(out.c_str());

		// -------------------------------------------------------------------
		//
		// ●専用の処理：返答内容に問題がなければ専ブラに返す
		//
		// -------------------------------------------------------------------
		if (urltype == URLTYPE_OFFLAW) {
			if (ohdr.status == 502) {
				if (Logger) {
					wstring tmp;
					datpath.get_o2_dat_path(tmp);
					Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
						"●でProxy Errorって返ってきた (%s)", tmp.c_str());
				}
			}
			else if (ohdr.status == 200 && ohdr.contentlength == 55) {
				it = ohdr.fields.find("Content-Encoding");
				if (it != ohdr.fields.end() && it->second == "gzip") {
					string decoded;
					ZLibUncompressor *gzu = new ZLibUncompressor(decoded, true);
					gzu->init(&out[ohdr.length], ohdr.contentlength);
					gzu->onetime();
					delete gzu;
					if (strncmp(decoded.c_str(), "-ERR", 4) == 0) {
						//-ERR そんな板orスレッドないです。
						if (Logger) {
							wstring tmp;
							datpath.get_o2_dat_path(tmp);
							Logger->AddLog(O2LT_WARNING, MODULE, 0, 0,
								"●で「-ERR そんな板orスレッドないです。」って返ってきた (%s)", tmp.c_str());
						}
						ohdr.status = 404;
					}
				}
			}
			if (ohdr.status == 200 || ohdr.status == 206 || ohdr.status == 304) {
				ss->Lock();
				ss->sbuff = out;
				ss->Unlock();
				return_original_response = true;
			}
		}

		// -------------------------------------------------------------------
		//	キャッシュから返すべきか判定
		// -------------------------------------------------------------------
		bool do_cache_loading = false;
		if (!return_original_response) {
			if (has_unknown_host_str
					|| (!Profile->IsMaruUser() && urltype == URLTYPE_KAKO_DAT)
					|| (Profile->IsMaruUser() && urltype == URLTYPE_OFFLAW)) {
				do_cache_loading = true;
			}
		}

		// -------------------------------------------------------------------
		//	キャッシュから取得
		// -------------------------------------------------------------------
		bool return_from_cache = false;
		if (do_cache_loading) {
			string cdat;

			if (DatIO->Load(datpath, request_first_byte, cdat)) {
				ohdr.status = request_first_byte ? 206 : 200;

				//remove Location, Vary
				ohdr.DeleteField("Location");
				ohdr.DeleteField("Vary");

				//add Last-Modified
				ohdr.AddFieldDate("Last-Modified", 0);

				//rewrite content-range
				if (request_first_byte == 0) {
					ohdr.DeleteField("Content-Range");
				}
				else {
					uint64 instance_length = request_first_byte + cdat.size();
					sprintf_s(tmp, 32, "bytes %d-%d/%d", request_first_byte, instance_length-1, instance_length);
					ohdr.AddFieldString("Content-Range", tmp);
				}

				//offlaw返答時の1行目を追加
				//※このタイミングじゃないとRangeがおかしくなるので注意
				if (urltype == URLTYPE_OFFLAW) {
					char firstline[64];
					sprintf_s(firstline, 64, "+OK %u/1024K\tLocation:temp/\n", cdat.size());
					cdat.insert(0, firstline);
				}

				//gzが要求されているときはgzipで返す
				bool is_request_gz = false;
				string gzbody;

				it = ohdr.fields.find("Content-Encoding");
				if (it != ohdr.fields.end() && it->second == "gzip") {
					is_request_gz = true;
					ohdr.AddFieldString("Content-Encoding", "gzip");
					ohdr.AddFieldString("Content-Type", "text/plain");
				}
				else if (strstr(url.c_str(), ".gz")) {
					is_request_gz = true;
					ohdr.AddFieldString("Content-Encoding", "gzip");
					ohdr.AddFieldString("Content-Type", "application/x-gzip");
				}
				else {
					ohdr.AddFieldString("Content-Type", "text/plain; charset=shift_jis");
					ohdr.DeleteField("Content-Encoding");
				}

				if (is_request_gz) {
					ZLibCompressor *gzc = new ZLibCompressor(gzbody, true);
					gzc->init(Z_DEFAULT_COMPRESSION, &cdat[0], cdat.size());
					gzc->onetime();
					delete gzc;
				}

				//rewrite content-length
				ohdr.AddFieldNumeric("Content-Length",
					(is_request_gz ? gzbody.size() : cdat.size()));

				//return cache
				ss->Lock();
				ohdr.Make(ss->sbuff);

				TRACEA("-------------------------------------------------------\n");
				TRACEA("■書換え後レスポンス\n");
				TRACEA("-------------------------------------------------------\n");
				TRACEA(ss->sbuff.c_str());
				if (is_request_gz)
					ss->sbuff.append(&gzbody[0], gzbody.size());
				else
					ss->sbuff.append(cdat);
				ss->Unlock();

				return_from_cache = true;
			}
		}

		if (!return_original_response && !return_from_cache) {
			ss->Lock();
			ohdr.Make(ss->sbuff);
			ss->Unlock();
		}

		ss->SetCanDelete(true);
		// -------------------------------------------------------------------
		//	↑candelete=trueにセットしたので、上流で開放が行われる可能性がある
		//	以降ssにアクセスしてはいけない
		// -------------------------------------------------------------------

		if (return_from_cache) {
			return;
		}

		if (ohdr.status != 200 && ohdr.status != 206 && ohdr.status != 304) {
			//検索クエリ候補として登録
			if (!DatIO->KakoHantei(datpath)) {
				DatIO->GetTitle(datpath);
				LagQueryQueue->add(datpath, DatIO->GetSize(datpath));
			}
			return;
		}
		else if (ohdr.status != 304) {
			//検索クエリ候補から削除
			LagQueryQueue->remove(datpath);
		}

		
		// -------------------------------------------------------------------
		//
		// キャッシュに保存
		//
		// -------------------------------------------------------------------
		wstring domain, bbsname, datname;
		datpath.element(domain, bbsname, datname);
		if (!Boards->IsEnabledEx(domain.c_str(), bbsname.c_str()))
			return;

		//decode chunked content
		string body;
		it = ohdr.fields.find("Transfer-Encoding");
		if (it != ohdr.fields.end() && it->second == "chunked") {

			bool chunkreadOK = false;
			char *p = &out[ohdr.length];

			while (p < &out[out.size()]) {
				uint remains = &out[out.size()] - p;
				if (remains < 3)
					break;
				
				if (p[0] == '0' && p[1] == '\r' && p[2] == '\n') {
					chunkreadOK = true;
					break;
				}

				char *crlf = strstr(p, "\r\n");
				if (!crlf)
					break;
				uint chunksize = strtol(p, &crlf, 16);
				if (chunksize == 0)
					break;
				p = crlf+2;

				remains = &out[out.size()] - p;
				if (remains < chunksize + 2)
					break;
				
				body.append(p, p+chunksize);

				if (p[chunksize] != '\r' || p[chunksize+1] != '\n')
					break;

				p += chunksize+2;
			}

			if (!chunkreadOK) {
				return;
			}
		}
		else
			body.assign(&out[ohdr.length], &out[out.size()]);

		//decode gzip
		it = ohdr.fields.find("Content-Encoding");
		if (it != ohdr.fields.end() && (it->second == "gzip" || it->second == "x-gzip")) {
			string decoded;
			ZLibUncompressor *gzu = new ZLibUncompressor(decoded, true);
			gzu->init(&body[0], body.size());
			gzu->onetime();
			delete gzu;
			body = decoded;
		}

		//delete first line (offlaw only)
		if (urltype == URLTYPE_OFFLAW) {
			uint pos = body.find("\n");
			body.erase(0, pos+1);
		}

		// 2012-06-15 簡易チェック
		if (!DatIO->CheckDat2(&body[0], body.size())) {
			return;
		}

		//キャッシュへ書き込み
		uint64 hokan_byte = DatIO->Put(
			datpath, &body[0], body.size(), request_first_byte);

		//
		if (hokan_byte) {
			if (Logger) {
				Logger->AddLog(O2LT_HOKAN, ServerName.c_str(), 0, 0,
					L"%s (%d)", tmpurl.c_str(), hokan_byte);
			}

			// 履歴に追加
			hashT hash;
			datpath.gethash(hash);
			wstring title;
			datpath.gettitle(title);
			AddRecentDat(hash, tmpurl.c_str(), title.c_str());
		}
	}

	inline void RewriteConnectionField(HTTPHeader &hdr, string &in)
	{
		//delete "Proxy-Connection" field
		hdr.DeleteField("Proxy-Connection");

		//add "Connection" field (Keep-Alive is not implemented)
		hdr.AddFieldString("Connection", "close");

		//rewrite
		uint headersize = hdr.length;
		string newheader;
		hdr.Make(newheader, true);
		in.replace(0, headersize, newheader);
	}

	inline uint64 GetRequestFirstBytePos(const HTTPHeader &hdr)
	{
		strmap::const_iterator it;
		if (hdr.paths[1] == "offlaw.cgi" ) {
			it = hdr.queries.find("raw");
			if (it != hdr.queries.end()) {
				if (it->second[0] == '.')
					return (_strtoui64(it->second.c_str()+1, NULL, 10));
				else if (it->second[0] == '0' && it->second[1] == '.')
					return (_strtoui64(it->second.c_str()+2, NULL, 10));
			}

		}
		else {
			it = hdr.fields.find("Range");
			if (it != hdr.fields.end()) {
				strarray sarray;
				if (split(it->second.c_str(), "=-", sarray) == 2) {
					return (_strtoui64(sarray[1].c_str(), NULL, 10));
				}
			}
		}
		return (0);
	}
	
	inline void NormalProxy(O2SocketSession *ss)
	{
		TRACEA("■NormalProxy\n");
		TRACEA(ss->rbuff.c_str());
		HTTPHeader &hdr = *((HTTPHeader*)ss->data);
	
		RewriteConnectionField(hdr, ss->rbuff);
		HTTPSocket *hsock = new HTTPSocket(SELECT_TIMEOUT_MS, Profile->GetUserAgentA());

		//proxy
		if (hsock->request(hdr.hostname.c_str(), hdr.port, ss->rbuff.c_str(), ss->rbuff.size(), NULL, 0)) {
			int n;
			char buff[RECVBUFFSIZE];
			while ((n = hsock->recv_fixed(buff, RECVBUFFSIZE)) >= 0) {
				if (n > 0) {
					ss->Lock();
					ss->sbuff.append(buff, n);
					ss->Unlock();
				}
			}
		}
		delete hsock;
	}
};
