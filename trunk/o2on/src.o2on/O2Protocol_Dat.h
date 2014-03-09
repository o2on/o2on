/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Protocol_Dat.h
 * description	: 
 *
 */

#pragma once
#include "O2Protocol.h"
#include "O2SocketSession.h"
#include "O2Logger.h"
#include "O2DatIO.h"
#include "O2Boards.h"
#include "O2KeyDB.h"
#include "dataconv.h"




class O2Protocol_Dat
{
protected:
	O2Protocol proto;

public:
	O2Protocol_Dat(void)
	{
	}

	~O2Protocol_Dat()
	{
	}

	// -----------------------------------------------------------------------
	//	MakeRequest_Dat
	// -----------------------------------------------------------------------
	void MakeRequest_Dat(const hashT *hash
					   , const wchar_t *board
					   , const O2SocketSession *ss
					   , const O2Profile *profile
					   , O2DatIO *datio
					   , string &out)
	{
		O2DatPath datpath;
		string rawdata;
		string proto_url;

		// おみやげのdat取得
		// TODO:ランダムじゃなくて、適切なのを。
		if (board) {
			wstrarray token;
			wsplit(board, L":", token);
			datio->RandomGetInBoard(token[0].c_str(), token[1].c_str(), rawdata, datpath);
		}
		else {
//			datio->RandomGet(rawdata, datpath);
		}

		// リクエストヘッダ生成
		//	GET http://1.2.3.4:1234/dat/ HTTP/1.1
		//	POST http://1.2.3.4:1234/dat/ HTTP/1.1
		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		header.method = rawdata.empty() ? "GET" : "POST";
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_DAT, proto_url);
		header.SetURL(proto_url.c_str());
		proto.AddRequestHeaderFields(header, profile);

		// 対象を指定している場合
		if (hash) {
			string hashstr;
			hash->to_string(hashstr);
			header.AddFieldString(X_O2_TARGET_KEY, hashstr.c_str());
		}
		else if (board) {
			string boardA;
			unicode2ascii(board, wcslen(board), boardA);
			header.AddFieldString(X_O2_TARGET_BOARD, boardA.c_str());
		}

		// おみやげのdat情報
		if (!rawdata.empty()) {
			// X-O2-DAT-Path: 2ch.net/news/1234567890
			string o2_dat_path;
			datpath.get_o2_dat_path(o2_dat_path);
			header.AddFieldString(X_O2_DATPATH, o2_dat_path.c_str());
			// X-O2-Original-DAT-URL: http://...
			if (datpath.is_origurl()) {
				string daturl;
				datpath.geturl(daturl);
				header.AddFieldString(X_O2_ORG_DAT_URL, daturl.c_str());
			}
			proto.AddContentFields(header, rawdata.size(), "text/plain", "shift_jis");
		}

		header.Make(out);
		out += rawdata;
	}

	// -----------------------------------------------------------------------
	//	MakeResponse_Dat
	// -----------------------------------------------------------------------
	void MakeResponse_Dat(const hashT *target
						, const wchar_t *board
						, const O2Profile *profile
					    , O2DatIO *datio
					    , string &out)
	{
		O2DatPath datpath;
		string rawdata;

		// 返すdat取得
		if (target) {
			// ハッシュ指定されている
			if (!datio->Load(*target, 0, rawdata, datpath)) {
				proto.MakeResponse_404(profile, out);
				return;
			}
		}
		else if (board) {
			// 板指定されている
			wstrarray token;
			wsplit(board, L":", token);
			datio->RandomGetInBoard(token[0].c_str(), token[1].c_str(), rawdata, datpath);
		}
		else {
			// 指定されていない場合
			// TODO:ランダムじゃなくて、適切なのを。
			datio->RandomGet(rawdata, datpath);
		}

		// レスポンスヘッダ生成
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);

		// 返すdat取得
		if (!rawdata.empty()) {
			// X-O2-DAT-Path: 2ch.net/news/1234567890
			string o2_dat_path;
			datpath.get_o2_dat_path(o2_dat_path);
			header.AddFieldString(X_O2_DATPATH, o2_dat_path.c_str());
			// X-O2-Original-DAT-URL: http://...
			if (datpath.is_origurl()) {
				string daturl;
				datpath.geturl(daturl);
				header.AddFieldString(X_O2_ORG_DAT_URL, daturl.c_str());
			}
			proto.AddContentFields(header, rawdata.size(), "text/plain", "shift_jis");
		}

		header.Make(out);
		out += rawdata;
	}

	// -----------------------------------------------------------------------
	//	ImportDat
	// -----------------------------------------------------------------------
	bool ImportDat(O2DatIO *datio
				 , const O2Profile *profile
				 , O2Boards *boards
				 , const HTTPHeader &header
				 , const char *rawdata
				 , const uint64 size
				 , O2Logger *Logger
				 , ulong ip
				 , ushort port
				 , O2KeyDB *QueryDB
				 , HWND hwndBaloonCallback
				 , UINT msgBaloonCallback)
	{
		O2DatPath datpath;

		strmap:: const_iterator it = header.fields.find(X_O2_ORG_DAT_URL);
		if (it != header.fields.end()) {
			// X-O2-Original-DAT-URL: http://...
			size_t pos = it->second.find("..");
			if (FOUND(pos))
				return false;

			datpath.set(it->second.c_str());
		}
		else {
			// X-O2-DAT-Path: 2ch.net/news/1234567890
			it = header.fields.find(X_O2_DATPATH);
			if (it == header.fields.end())
				return false;

			size_t pos = it->second.find("..");
			if (FOUND(pos))
				return false;

			strarray token;
			if (split(it->second.c_str(), "/", token) < 3)
				return false;
			datpath.set(token[0].c_str(), token[1].c_str(), string(token[2]+".dat").c_str());
		}

		// 保存すべきか？
		if (boards) {
			wstring domain, bbsname, datname;
			datpath.element(domain, bbsname, datname);
			if (!boards->IsEnabledEx(domain.c_str(), bbsname.c_str())) {
				Logger->AddLog(O2LT_WARNING, L"dat", ip, port,
					L"(ﾟ⊿ﾟ)ｲﾗﾈ (%s/%s/%s)", domain.c_str(), bbsname.c_str(), datname.c_str());
				return false;
			}
		}

		// 検証
		if (!datio->CheckDat(rawdata, size))
			return false;

		// 保存
		uint64 hokan_byte = datio->Put(datpath, rawdata, size, 0);


		// 補完された
		if (hokan_byte) {
			if (Logger) {
				wstring url;
				datpath.geturl(url);
				Logger->AddLog(O2LT_HOKAN, L"Job", ip, port,
					L"%s (%d)", url.c_str(), hokan_byte);
			}
			if (QueryDB) {
				hashT hash;
				datpath.gethash(hash);
				wstring title;
				datpath.gettitle(title);
				if (QueryDB->SetNote(hash, title.c_str(), size) && hwndBaloonCallback && profile->IsBaloon_Hokan()) {
					wchar_t msg[256];
					swprintf_s(msg, 256, L"%s\nが補完されました", title.c_str());
					SendMessage(
						hwndBaloonCallback, msgBaloonCallback,
						(WPARAM)L"o2on", (LPARAM)msg);
				}
			}
		}

		return true;
	}

	// -----------------------------------------------------------------------
	//	MakeRequest_Collection
	// -----------------------------------------------------------------------
	void MakeRequest_Collection(const O2SocketSession *ss
							  , const O2Profile *profile
							  , O2Boards *boards
							  , string &out)
	{
		string proto_url;

		// 自分のコレクション情報
		string xml;
		boards->ExportToXML(xml);

		// リクエストヘッダ生成
		//	GET http://1.2.3.4:1234/collection/ HTTP/1.1
		HTTPHeader header(HTTPHEADERTYPE_REQUEST);
		proto.MakeURL(ss->ip, ss->port, O2PROTOPATH_COLLECTION, proto_url);
		header.method = "POST";
		header.SetURL(proto_url.c_str());
		proto.AddRequestHeaderFields(header, profile);
		proto.AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		header.Make(out);
		out += xml;
	}

	// -----------------------------------------------------------------------
	//	MakeResponse_Collection
	// -----------------------------------------------------------------------
	void MakeResponse_Collection(const O2Profile *profile
							   , O2Boards *boards
							   , string &out)
	{
		// 返す情報
		string xml;
		boards->ExportToXML(xml);

		// レスポンスヘッダ生成
		HTTPHeader header(HTTPHEADERTYPE_RESPONSE);
		header.status = 200;
		proto.AddResponseHeaderFields(header, profile);
		proto.AddContentFields(header, xml.size(), "text/xml", DEFAULT_XML_CHARSET);

		header.Make(out);
		out += xml;
	}
};
