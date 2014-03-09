/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Agent.h
 * description	: 
 *
 */

#pragma once
#include "O2Client_HTTP.h"
#include "O2Scheduler.h"




class O2Agent
	: public O2Client_HTTP
	, public O2Scheduler
{
private:

public:
	O2Agent(const wchar_t *name, O2Logger *lgr)
		: O2Client_HTTP(name, lgr)
	{
	}

	~O2Agent()
	{
	}

	void ClientStart(void)
	{
		O2Client_HTTP::Start();
	}

	void SchedulerStart(void)
	{
		O2Scheduler::Start();
	}

	void ClientStop(void)
	{
		O2Client_HTTP::Stop();
	}

	void SchedulerStop(void)
	{
		O2Scheduler::Stop();
	}
};
