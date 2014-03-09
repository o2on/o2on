/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: typedef.h
 * description	: 
 *
 */

#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>




/* unsigned */
typedef unsigned char				byte;
typedef unsigned short				ushort;
typedef unsigned int				uint;
typedef unsigned long				ulong;
typedef unsigned __int64			uint64;

/* char and strings */
typedef std::string					string;
typedef std::wstring				wstring;

/* string container */
typedef std::vector<string>			strarray;
typedef std::vector<wstring>		wstrarray;
typedef std::set<string>			stringset;
typedef std::set<wstring>			wstringset;
typedef std::map<string,string>		strmap;
typedef std::map<wstring,wstring>	wstrmap;
typedef std::map<string,uint64>		strnummap;
typedef std::map<wstring,uint64>	wstrnummap;

/* tstring */
#if defined(UNICODE) || defined(_UNICODE)
typedef std::wstring				tstring;
typedef std::wfstream				tfstream;
typedef std::wifstream				tifstream;
typedef std::wofstream				tofstream;
#else
typedef std::string					tstring;
typedef std::fstream				tfstream;
typedef std::ifstream				tifstream;
typedef std::ofstream				tofstream;
#endif

#define FOUND(i) ((i) != tstring::npos)
