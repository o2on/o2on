/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_ClearWorkset.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"




class O2Job_ClearWorkset
	: public O2Job
{
public:
	O2Job_ClearWorkset(const wchar_t *name, time_t interval, bool startup)
		: O2Job(name, interval, startup)
	{
	}

	~O2Job_ClearWorkset()
	{
	}

	void JobThreadFunc(void)
	{
		CLEAR_WORKSET;
	}
};
