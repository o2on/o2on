/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2ProgressInfo.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "mutex.h"




class O2ProgressInfo
	: public Mutex
{
public:
	bool	active;
	bool	stoppable;
	uint64	pos;
	uint64	maxpos;
	wstring	message;

public:
	O2ProgressInfo(void)
		: active(false)
		, stoppable(false)
		, pos(0)
		, maxpos(0)
	{
	}

	void Reset(bool act, bool stppable)
	{
		Lock();
		active = act;
		stoppable = stppable;
		if (active) {
			maxpos = 0;
			pos = 0;
			message = L"";
		}
		Unlock();
	}

	void AddPos(uint64 n)
	{
		Lock();
		pos += n;
		Unlock();
	}

	void AddMax(uint64 n)
	{
		Lock();
		maxpos += n;
		Unlock();
	}

	void SetMessage(const wchar_t *msg)
	{
		Lock();
		message = msg;
		Unlock();
	}
};
