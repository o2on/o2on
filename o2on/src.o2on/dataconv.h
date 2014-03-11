/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: dataconv.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "sha.h"
#include <windows.h>




extern const char *hex;
extern const wchar_t *whex;

extern uint split(const char *in, const char *delim, strarray &token);
extern uint wsplit(const wchar_t *in, const wchar_t *delim, wstrarray &token);
extern uint splitstr(const char *in, const char *delim, strarray &token);
extern uint wsplitstr(const wchar_t *in, const wchar_t *delim, wstrarray &token);
extern void byte2hex(const byte *in, uint len, string &out);
extern void byte2whex(const byte *in, uint len, wstring &out);
extern void hex2byte(const char *in, uint len, byte *out);
extern void whex2byte(const wchar_t *in, uint len, byte *out);
extern void random_hex(uint len, string &out);
extern void random_whex(uint len, wstring &out);
extern time_t datetime2time_t(const wchar_t *in, int len);
extern void time_t2datetime(time_t in, long tzoffset, wstring &out);
extern time_t filetime2time_t(const FILETIME &ft);
extern void time_t2filetime(time_t t, FILETIME &ft);
extern ulong ipstr2ulong(const wchar_t *in, int len);
extern void ulong2ipstr(ulong ip, string &out);
extern void ulong2ipstr(ulong ip, wstring &out);
extern void simple_aes_ctr(byte *in, uint inlen, byte *out);

extern void hash_xor(hashT &out, const hashT &h1, const hashT &h2);
extern size_t hash_bitlength(const hashT &hash);
extern bool hash_bittest(const hashT &hash, size_t pos);
extern size_t hash_xor_bitlength(const hashT &h1, const hashT &h2);
extern bool less_xor_bitlength(const hashT &target, const hashT &h1, const hashT &h2);
extern void bench(void);


extern void ip2e(ulong ip, string &out);
extern void ip2e(ulong ip, wstring &out);
extern void port2e(ushort port, string &out);
extern void port2e(ushort port, wstring &out);
extern ulong e2ip(const char *str, uint len);
extern ulong e2ip(const wchar_t *str, uint len);
extern ushort e2port(const char *str, uint len);
extern ushort e2port(const wchar_t *str, uint len);

extern bool is_globalIP(ulong ip);

extern bool ToUnicode(const wchar_t *charset, const char *in, const uint len, wstring &out);
extern bool FromUnicode(const wchar_t *charset, const wchar_t *in, uint len, string &out);
extern bool ToUnicode(const wchar_t *charset, const string &in, wstring &out);
extern bool FromUnicode(const wchar_t *charset, const wstring &in, string &out);
#ifdef _WIN64
extern bool ToUnicode(const wchar_t *charset, const char *in, const size_t len, wstring &out);
extern bool FromUnicode(const wchar_t *charset, const wchar_t *in, size_t len, string &out);
#endif

void ascii2unicode(const char *a, size_t len, wstring &w);
void ascii2unicode(const string &a, wstring &w);
void unicode2ascii(const wchar_t *w, size_t len, string &a);
void unicode2ascii(const wstring &w, string &a);

extern bool sjis_or_euc(const string &in, string &encoding);
extern bool sjis_or_euc(const string &in, wstring &encoding);
extern void sjis2euc(string &inout);

void convertGTLT(const string &in, string &out);
void convertGTLT(const wstring &in, wstring &out);
extern void makeCDATA(const string &in, string &out);
extern void makeCDATA(const wstring &in, wstring &out);

extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, const wchar_t *val, bool escape = false);
extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, int val);
extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, uint val);
extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, __int64 val);
extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, uint64 val);
extern void xml_AddElement(wstring &xml, const wchar_t *tag, const wchar_t *attr, double val);
