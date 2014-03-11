/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Key.h
 * description	: o2on p2p key
 *
 */

#pragma once
#include "O2Define.h"
#include "sha.h"
#include <time.h>
#include <tchar.h>
#include <list>




struct O2Key
{
	//common
	hashT		hash;
	hashT		nodeid;
	ulong		ip;
	ushort		port;
	uint64		size;
	wstring		url;
	wstring		title;
	wstring		note;
	//local
	hashT		idkeyhash;
	uint		distance;
	time_t		date;
	bool		enable;

	O2Key(void)
		: ip(0)
		, port(0)
		, size(0)
		, distance(0)
		, date(0)
		, enable(false)
	{
	}

	void makeidkeyhash(void)
	{
		byte a[HASHSIZE+HASHSIZE];
		memcpy(&a[0], hash.data(), HASHSIZE);
		memcpy(&a[HASHSIZE], nodeid.data(), HASHSIZE);
		byte b[HASHSIZE];
		CryptoPP::SHA1().CalculateDigest(b, a, sizeof(a));
		idkeyhash.assign(b, HASHSIZE);
	}

	void marge(const O2Key &src)
	{
		if (*this == src) {
			if (ip != src.ip)
				ip = src.ip;
			if (port != src.port)
				port = src.port;
			if (size < src.size)
				size = src.size;
			if (!src.url.empty() && url != src.url)
				url = src.url;
			if (!src.title.empty() && title != src.title)
				title = src.title;
			if (!src.note.empty() && title != src.note)
				note = src.note;
		}
	}

	bool operator==(const O2Key& src) const
	{
		return (idkeyhash == src.idkeyhash ? true : false);
	}

	bool operator<(const O2Key& key) const
	{
		return distance < key.distance; //~‡
	}
};

typedef std::list<O2Key> O2KeyList;
typedef std::list<O2Key>::iterator O2KeyListIt;
