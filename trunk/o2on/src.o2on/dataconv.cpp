/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: dataconv.cpp
 * description	: 
 *
 */

#include "typedef.h"
#include "sha.h"
#include "debug.h"
#include "../cryptopp/aes.h"
#include "../cryptopp/modes.h"
#include <stdio.h>
#include <time.h>
#include <mlang.h>
#include <sstream>
#include <boost/regex.hpp>

const char *hex 	= "0123456789abcdef";
const wchar_t *whex = L"0123456789abcdef";




// ---------------------------------------------------------------------------
//	split 
//	wsplit
// ---------------------------------------------------------------------------

uint split(const char *in, const char *delim, strarray &token)
{
	token.clear();

	uint len = strlen(in);
	char *str = new char[len+1];
	strcpy_s(str, len+1, in);

	char *context;
	char *tok = strtok_s(str, delim, &context);
	while (tok) {
		token.push_back(tok);
		tok = strtok_s(NULL, delim, &context);
	}
	delete [] str;
	return (token.size());
}
uint wsplit(const wchar_t *in, const wchar_t *delim, wstrarray &token)
{
	token.clear();

	uint len = wcslen(in);
	wchar_t *str = new wchar_t[len+1];
	wcscpy_s(str, len+1, in);

	wchar_t *context;
	wchar_t *tok = wcstok_s(str, delim, &context);
	while (tok) {
		token.push_back(tok);
		tok = wcstok_s(NULL, delim, &context);
	}
	delete [] str;
	return (token.size());
}




// ---------------------------------------------------------------------------
//	splitstr
//	wsplitstr
// ---------------------------------------------------------------------------

uint splitstr(const char *in, const char *delim, strarray &token)
{
	token.clear();

	const char *p = in;
	const char *end;
	size_t len = strlen(in);
	size_t dlen = strlen(delim);

	while (p < in + len) {
		end = strstr(p, delim);
		if (!end) {
			token.push_back(p);
			break;
		}
		token.push_back(string(p, end));
		p = end + dlen;
	}
	return (token.size());
}
uint wsplitstr(const wchar_t *in, const wchar_t *delim, wstrarray &token)
{
	token.clear();

	const wchar_t *p = in;
	const wchar_t *end;
	size_t len = wcslen(in);
	size_t dlen = wcslen(delim);

	while (p < in + len) {
		end = wcsstr(p, delim);
		if (!end) {
			token.push_back(p);
			break;
		}
		token.push_back(wstring(p, end));
		p = end + dlen;
	}
	return (token.size());
}




// ---------------------------------------------------------------------------
//	byte2hex 
//	byte2whex
// ---------------------------------------------------------------------------

void byte2hex(const byte *in, uint len, string &out)
{

	out.erase();
	for (uint i = 0; i < len; i++) {
		out += hex[(in[i] >> 4)];
		out += hex[(in[i] & 0x0f)];
	}
}
void byte2whex(const byte *in, uint len, wstring &out)
{

	out.erase();
	for (uint i = 0; i < len; i++) {
		out += whex[(in[i] >> 4)];
		out += whex[(in[i] & 0x0f)];
	}
}




// ---------------------------------------------------------------------------
//	hex2byte 
//	whex2byte
// ---------------------------------------------------------------------------

void hex2byte(const char *in, uint len, byte *out)
{
	for (uint i = 0; i < len; i+=2) {
		char c0 = in[i+0];
		char c1 = in[i+1];
		byte c = (
			((c0 & 0x40 ? (c0 & 0x20 ? c0-0x57 : c0-0x37) : c0-0x30)<<4) |
			((c1 & 0x40 ? (c1 & 0x20 ? c1-0x57 : c1-0x37) : c1-0x30))
			);
		out[i/2] = c;
	}
}
void whex2byte(const wchar_t *in, uint len, byte *out)
{
	for (uint i = 0; i < len; i+=2) {
		wchar_t c0 = in[i+0];
		wchar_t c1 = in[i+1];
		byte c = (
			((c0 & 0x0040 ? (c0 & 0x0020 ? c0-0x0057 : c0-0x0037) : c0-0x0030)<<4) |
			((c1 & 0x0040 ? (c1 & 0x0020 ? c1-0x0057 : c1-0x0037) : c1-0x0030))
			);
		out[i/2] = c;
	}
}




// ---------------------------------------------------------------------------
//	random_hex 
//	random_whex
// ---------------------------------------------------------------------------

void random_hex(uint len, string &out)
{
	out.erase();
	for (uint i = 0; i < len; i++)
		out += hex[rand()%16];
}
void random_whex(uint len, wstring &out)
{
	out.erase();
	for (uint i = 0; i < len; i++)
		out += whex[rand()%16];
}




// ---------------------------------------------------------------------------
//	datetime2time_t 
//	time_t2datetime
// ---------------------------------------------------------------------------

time_t datetime2time_t(const wchar_t *in, int len)
{
	//http://www.w3.org/TR/1998/NOTE-datetime-19980827
	//http://www2.airnet.ne.jp/sardine/docs/NOTE-datetime-19980827.html
	//
	//1994-11-05T08:15:30-05:00
	//1994-11-05T13:15:30Z
	//
	//YYYY-MM-DDThh:mm:ssTZD のみ対応
	//
	static wchar_t delim[] = {L"+-:.TZ"};
	struct tm tms = {0};
	time_t t = 0;
	time_t offset = 0;
	int sign = 0;

	if (len < 20)
		return (0);

	if (in[19] == L'+')
		sign = 1;
	else
		sign = -1;

	wchar_t *str = new wchar_t[len+1];
	wcsncpy_s(str, len+1, in, len);
	str[len] = 0;

	wchar_t *context;
	wchar_t *tok = wcstok_s(str, delim, &context);
	for (int i = 0; i < 8; i++) {
		if (!tok)
			break;
		switch (i) {
				case 0:
					tms.tm_year = _wtoi(tok) - 1900;
					break;
				case 1:
					tms.tm_mon = _wtoi(tok) - 1;
					break;
				case 2:
					tms.tm_mday = _wtoi(tok);
					break;
				case 3:
					tms.tm_hour = _wtoi(tok);
					break;
				case 4:
					tms.tm_min = _wtoi(tok);
					break;
				case 5:
					tms.tm_sec = _wtoi(tok);
					break;
				case 6:
					offset = _wtoi(tok)*60*60;
					break;
				case 7:
					offset += _wtoi(tok)*60;
					break;
		}
		tok = wcstok_s(NULL, delim, &context);
	}
	delete [] str;

	long tzoffset;
	_get_timezone(&tzoffset);
	t = mktime(&tms) - tzoffset; //gmmktime()

	if (sign)
		t -= sign*offset;

	return (t);
}

void time_t2datetime(time_t in, long tzoffset, wstring &out)
{
	//http://www.w3.org/TR/1998/NOTE-datetime-19980827
	//http://www2.airnet.ne.jp/sardine/docs/NOTE-datetime-19980827.html
	//
	//1994-11-05T08:15:30-05:00
	//1994-11-05T13:15:30Z
	//
	//YYYY-MM-DDThh:mm:ssTZD のみ対応
	//
	time_t t = in + tzoffset;

	wchar_t str[32];
	struct tm tm;
	gmtime_s(&tm, &t);
	if (tzoffset == 0)
		wcsftime(str, 32, L"%Y-%m-%dT%H:%M:%SZ", &tm);
	else {
		wcsftime(str, 32, L"%Y-%m-%dT%H:%M:%S", &tm);
		swprintf_s(str + 19, 32-19, L"%+03d:%02d", tzoffset/(60*60), (tzoffset % (60*60))/60);
	}
	out = str;
}




// ---------------------------------------------------------------------------
//	filetime2time_t 
//	time_t2filetime
// ---------------------------------------------------------------------------

time_t filetime2time_t(const FILETIME &ft)
{
	time_t t64 = ((_int64)ft.dwHighDateTime << 32) | (_int64)ft.dwLowDateTime;
	t64 = (t64 - 116444736000000000)/10000000;
	return (t64);
	//	VC8はtime_t型は64bitなので上記方法でOK
	//	time_t=32bitなコンパイラの場合は注意
}

void time_t2filetime(time_t t, FILETIME &ft)
{
	 time_t t64;

	 t64 = t * 10000000 + 116444736000000000;
	 ft.dwLowDateTime = (DWORD)t64;
	 ft.dwHighDateTime = (DWORD)(t64 >> 32);
}




// ---------------------------------------------------------------------------
//	hash_xor 
//	hash_bitlength
//	hash_bittest
//	hash_xor_bitlength 
//	less_xor_bitlength
// ---------------------------------------------------------------------------
void hash_xor(hashT &out, const hashT &h1, const hashT &h2)
{
	for (size_t i = 0; i < h1.block_length; i++)
		out.block[i] = h1.block[i] ^ h2.block[i];
}
size_t hash_bitlength(const hashT &h)
{
	for (size_t i = h.block_length-1; ; i--) {
		if (h.block[i] & 0x80) return (i*8+8);
		if (h.block[i] & 0x40) return (i*8+7);
		if (h.block[i] & 0x20) return (i*8+6);
		if (h.block[i] & 0x10) return (i*8+5);
		if (h.block[i] & 0x08) return (i*8+4);
		if (h.block[i] & 0x04) return (i*8+3);
		if (h.block[i] & 0x02) return (i*8+2);
		if (h.block[i] & 0x01) return (i*8+1);
		if (i == 0) break;
	}
	return (0);
}
bool hash_bittest(const hashT &hash, size_t pos)
{
	return ((hash.block[pos/8] >> (pos%8)) & 0x01 ? true : false);
}
size_t hash_xor_bitlength(const hashT &h1, const hashT &h2)
{
	byte xor;
	for (size_t i = h1.block_length-1; ; i--) {
		xor = h1.block[i] ^ h2.block[i];
		if (xor & 0x80) return (i*8+8);
		if (xor & 0x40) return (i*8+7);
		if (xor & 0x20) return (i*8+6);
		if (xor & 0x10) return (i*8+5);
		if (xor & 0x08) return (i*8+4);
		if (xor & 0x04) return (i*8+3);
		if (xor & 0x02) return (i*8+2);
		if (xor & 0x01) return (i*8+1);
		if (i == 0) break;
	}
	return (0);
}
bool less_xor_bitlength(const hashT &target, const hashT &h1, const hashT &h2)
{
	byte xor1;
	byte xor2;
	for (size_t i = h1.block_length-1; ; i--) {
		xor1 = target.block[i] ^ h1.block[i];
		xor2 = target.block[i] ^ h2.block[i];
		if (xor1 != xor2) return (xor1 < xor2);
		if (i == 0) break;
	}
	return false;
}

#include "stopwatch.h"
void bench(void)
{
	stopwatch *sw;

#if 0
	sw = new stopwatch("bitlength");
	for (size_t i = 0; i < 1000000; i++) {
		hashT a;
		a.random();
		size_t len = hash_bitlength(a);
	}
#endif
#if 0
	sw = new stopwatch("xor");
	for (size_t i = 0; i < 1000000; i++) {
		hashT a;
		hashT b;
		a.random();
		b.random();
		hashT c;
		hash_xor(c, a, b);
	}
#endif
#if 1
	sw = new stopwatch("xor_bitlength");
	for (size_t i = 0; i < 1000000; i++) {
		hashT a;
		hashT b;
		a.random();
		b.random();
		size_t len = hash_xor_bitlength(a, b);
	}
#endif
	delete sw;
}




// ---------------------------------------------------------------------------
//	ipstr2ulong 
//	ulong2ipstr
// ---------------------------------------------------------------------------

ulong ipstr2ulong(const wchar_t *in, int len)
{
	ulong ul = 0;

	wchar_t *str = new wchar_t[len+1];
	wcsncpy_s(str, len+1, in, len);
	str[len] = 0;

	wchar_t *context;
	wchar_t *tok = wcstok_s(str, L".", &context);
	int i;
	for (i = 0; i < 4; i++) {
		if (!tok)
			break;
		//ul |= (((ulong)_wtoi(tok))<<(8*(3-i)));
		ul |= (((ulong)_wtoi(tok))<<(8*i));
		tok = wcstok_s(NULL, L".", &context);
	}
	delete [] str;
	return (i != 4 ? 0 : ul);
}

void ulong2ipstr(ulong ip, string &out)
{
	char tmp[20];
	sprintf_s(tmp, 20, "%d.%d.%d.%d",
		(byte)(ip		  & 0x000000ff),
		(byte)((ip >>  8) & 0x000000ff),
		(byte)((ip >> 16) & 0x000000ff),
		(byte)((ip >> 24) & 0x000000ff));
	/*
	swprintf_s(tmp, 20, L"%d.%d.%d.%d",
	(byte)((ip >> 24) & 0x000000ff),
	(byte)((ip >> 16) & 0x000000ff),
	(byte)((ip >>  8) & 0x000000ff),
	(byte)(ip		  & 0x000000ff));
	*/
	out = tmp;
}

void ulong2ipstr(ulong ip, wstring &out)
{
	wchar_t tmp[20];
	swprintf_s(tmp, 20, L"%d.%d.%d.%d",
		(byte)(ip		  & 0x000000ff),
		(byte)((ip >>  8) & 0x000000ff),
		(byte)((ip >> 16) & 0x000000ff),
		(byte)((ip >> 24) & 0x000000ff));
	/*
	swprintf_s(tmp, 20, L"%d.%d.%d.%d",
	(byte)((ip >> 24) & 0x000000ff),
	(byte)((ip >> 16) & 0x000000ff),
	(byte)((ip >>  8) & 0x000000ff),
	(byte)(ip		  & 0x000000ff));
	*/
	out = tmp;
}




// ---------------------------------------------------------------------------
//	simple_aes_ctr 
//	
// ---------------------------------------------------------------------------

void simple_aes_ctr(byte *in, uint inlen, byte *out)
{
	static const uint keylen = 16;
	static const byte *key = (byte*)"0000000000000000";
	static const byte *iv = NULL;

	CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption aes(key, keylen, iv);
	aes.ProcessString(out, in, inlen);
}




// ---------------------------------------------------------------------------
//	ip2e
//	port2e
// ---------------------------------------------------------------------------

void ip2e(ulong ip, string &out)
{
	byte enc[4];
	simple_aes_ctr((byte*)(&ip), 4, enc);
	byte2hex(enc, 4, out);
}
void ip2e(ulong ip, wstring &out)
{
	byte enc[4];
	simple_aes_ctr((byte*)(&ip), 4, enc);
	byte2whex(enc, 4, out);
}
void port2e(ushort port, string &out)
{
	byte enc[2];
	simple_aes_ctr((byte*)(&port), 2, enc);
	byte2hex(enc, 2, out);
}
void port2e(ushort port, wstring &out)
{
	byte enc[2];
	simple_aes_ctr((byte*)(&port), 2, enc);
	byte2whex(enc, 2, out);
}




// ---------------------------------------------------------------------------
//	e2ip
//	e2port
// ---------------------------------------------------------------------------

ulong e2ip(const char *str, uint len)
{
	if (len != 8)
		return (0);
	byte buf[4];
	hex2byte(str, len, buf);
	ulong ip;
	simple_aes_ctr(buf, 4, (byte*)(&ip));
	return (ip);
}
ulong e2ip(const wchar_t *str, uint len)
{
	if (len != 8)
		return (0);
	byte buf[4];
	whex2byte(str, len, buf);
	ulong ip;
	simple_aes_ctr(buf, 4, (byte*)(&ip));
	return (ip);
}
ushort e2port(const char *str, uint len)
{
	if (len != 4)
		return (0);
	byte buf[2];
	hex2byte(str, len, buf);
	ushort port;
	simple_aes_ctr(buf, 2, (byte*)(&port));
	return (port);
}
ushort e2port(const wchar_t *str, uint len)
{
	if (len != 4)
		return (0);
	byte buf[2];
	whex2byte(str, len, buf);
	ushort port;
	simple_aes_ctr(buf, 2, (byte*)(&port));
	return (port);
}

// ---------------------------------------------------------------------------
//	is_globalIP 
// ---------------------------------------------------------------------------
bool is_globalIP(ulong ip)
{
	ulong ipL = htonl(ip);
	if ((ipL & 0xff000000) == 0x7f000000) { //loopback
		TRACEA("＊＊＊＊＊＊ 127.xxx.xxx.xxx ＊＊＊＊＊＊\n");
		return false;
	}
	if ((ipL & 0x000000ff) == 0x00000000) { //network
		TRACEA("＊＊＊＊＊＊ xxx.xxx.xxx.0 ＊＊＊＊＊＊\n");
		return false;
	}
	if ((ipL & 0x000000ff) == 0x000000ff) { //broadcast
		TRACEA("＊＊＊＊＊＊ xxx.xxx.xxx.255 ＊＊＊＊＊＊\n");
		return false;
	}
#if !defined(_DEBUG)
	if ((ipL & 0xffff0000) == 0xc0a80000) { //class C
		TRACEA("＊＊＊＊＊＊ class C ＊＊＊＊＊＊\n");
		return false;
	}
	if ((ipL & 0xfff00000) == 0xac100000) { //class B
		TRACEA("＊＊＊＊＊＊ class B ＊＊＊＊＊＊\n");
		return false;
	}
	if ((ipL & 0xff000000) == 0x0a000000) { //class A
		TRACEA("＊＊＊＊＊＊ class A ＊＊＊＊＊＊\n");
		return false;
	}
#else
#pragma message("＃＃＃＃＃ IPチェックがオフになってるよ！！！ ＃＃＃＃＃")
#endif
	return true;
}




// ---------------------------------------------------------------------------
//	ToUnicode 
//	FromUnicode
// ---------------------------------------------------------------------------

bool ToUnicode(const wchar_t *charset, const char *in, const uint len, wstring &out)
{
	//IMultiLanguage生成
	IMultiLanguage *lang = NULL;
	HRESULT hr = CoCreateInstance(CLSID_CMultiLanguage, NULL,
		CLSCTX_ALL, IID_IMultiLanguage, (LPVOID*)&lang);

	//Charset情報取得
	MIMECSETINFO csetinfo;
	if (SUCCEEDED(hr))
		hr = lang->GetCharsetInfo(const_cast<wchar_t*>(charset), &csetinfo);

	//変換後の文字長を取得
	DWORD pdwMode = 0;
	UINT inlen = len;
	UINT outlen = 0;
	if (SUCCEEDED(hr)) {
		hr = lang->ConvertStringToUnicode(
			&pdwMode, csetinfo.uiInternetEncoding,
			const_cast<char*>(in), &inlen,
			NULL, &outlen);

		out.resize(outlen);
	}

	//convert to unicode
	if (SUCCEEDED(hr)) {
		hr = lang->ConvertStringToUnicode(
			&pdwMode, csetinfo.uiInternetEncoding,
			const_cast<char*>(in), &inlen,
			&out[0], &outlen);
	}

	if (lang)
		lang->Release();

	return (SUCCEEDED(hr) ? true : false);
}

bool FromUnicode(const wchar_t *charset, const wchar_t *in, uint len, string &out)
{
	//IMultiLanguage生成
	IMultiLanguage *lang = NULL;
	HRESULT hr = CoCreateInstance(CLSID_CMultiLanguage, NULL,
		CLSCTX_ALL, IID_IMultiLanguage, (LPVOID*)&lang);

	//Charset情報取得
	MIMECSETINFO csetinfo;
	if (SUCCEEDED(hr))
		hr = lang->GetCharsetInfo(const_cast<wchar_t*>(charset), &csetinfo);

	//変換後の文字長を取得
	DWORD pdwMode = 0;
	UINT inlen = len;
	UINT outlen = 0;
	if (SUCCEEDED(hr)) {
		hr = lang->ConvertStringFromUnicode(
			&pdwMode, csetinfo.uiInternetEncoding,
			const_cast<wchar_t*>(in), &inlen,
			NULL, &outlen);

		out.resize(outlen);
	}

	//convert from unicode
	if (SUCCEEDED(hr)) {
		hr = lang->ConvertStringFromUnicode(
			&pdwMode, csetinfo.uiInternetEncoding,
			const_cast<wchar_t*>(in), &inlen,
			(char*)(&out[0]), &outlen);
	}

	if (lang)
		lang->Release();

	return (SUCCEEDED(hr) ? true : false);
}

bool ToUnicode(const wchar_t *charset, const string &in, wstring &out)
{
	return (ToUnicode(charset, in.c_str(), in.size(), out));
}
bool FromUnicode(const wchar_t *charset, const wstring &in, string &out)
{
	return (FromUnicode(charset, in.c_str(), in.size(), out));
}

#ifdef _WIN64
bool ToUnicode(const wchar_t *charset, const char *in, const size_t len, wstring &out)
{
	if (len >= UINT_MAX)
		return false;
	return (ToUnicode(charset, in, (uint)len, out));
}
bool FromUnicode(const wchar_t *charset, const wchar_t *in, size_t len, string &out)
{
	if (len >= UINT_MAX)
		return false;
	return (FromUnicode(charset, in, (uint)len, out));
}
#endif

// ---------------------------------------------------------------------------
//	ascii2unicode 
//	unicod2ascii
// ---------------------------------------------------------------------------
void ascii2unicode(const char *a, size_t len, wstring &w)
{
	w.resize(len);
	for (size_t i = 0; i < len; i++)
		w[i] = a[i];
}
void ascii2unicode(const string &a, wstring &w)
{
	ascii2unicode(a.c_str(), a.size(), w);
}
void unicode2ascii(const wchar_t *w, size_t len, string &a)
{
	a.resize(len);
	for (size_t i = 0; i < len; i++)
		a[i] = (char)w[i];
}
void unicode2ascii(const wstring &w, string &a)
{
	unicode2ascii(w.c_str(), w.size(), a);
}

// ---------------------------------------------------------------------------
//	convertGTLT 
//	
// ---------------------------------------------------------------------------
void convertGTLT(const string &in, string &out)
{
	out.erase();

	std::ostringstream t(std::ios::out | std::ios::binary);
	std::ostream_iterator<char, char> oi(t);

	boost::regex e("(<)|(>)|(\r\n)");
	boost::regex_replace(oi, in.begin(), in.end(),
		e, "(?1&lt;)(?2&gt;)(?3<br>)", boost::match_default | boost::format_all);

	out = t.str();
}
void convertGTLT(const wstring &in, wstring &out)
{
	out.erase();

	std::wostringstream t(std::ios::out | std::ios::binary);
	std::ostream_iterator<wchar_t, wchar_t> oi(t);

	boost::wregex e(L"(<)|(>)|(\r\n)");
	boost::regex_replace(oi, in.begin(), in.end(),
		e, L"(?1&lt;)(?2&gt;)(?3<br>)", boost::match_default | boost::format_all);

	out = t.str();
}




// ---------------------------------------------------------------------------
//	xml_AddElement
//	
// ---------------------------------------------------------------------------

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, const wchar_t *val, bool escape)
{
	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	if (escape) xml += L"<![CDATA[";
	xml += val;
	if (escape) xml += L"]]>";
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, int val)
{
	wstring s;
	wchar_t tmp[16];

	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	swprintf_s(tmp, 16, L"%d", val);
	xml += tmp;
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, uint val)
{
	wstring s;
	wchar_t tmp[16];

	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	swprintf_s(tmp, 16, L"%u", val);
	xml += tmp;
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, __int64 val)
{
	wstring s;
	wchar_t tmp[16];

	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	swprintf_s(tmp, 16, L"%I64d", val);
	xml += tmp;
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, uint64 val)
{
	wstring s;
	wchar_t tmp[16];

	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	swprintf_s(tmp, 16, L"%I64u", val);
	xml += tmp;
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}

void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, double val)
{
	wstring s;
	wchar_t tmp[16];

	xml += L'<';
	xml += tag;
	if (attr) {
		xml += L' ';
		xml += attr;
	}
	xml += L'>';
	swprintf_s(tmp, 16, L"%.2lf", val);
	xml += tmp;
	xml += L"</";
	xml += tag;
	xml += L">\r\n";
}
