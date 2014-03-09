/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2Job_AutoSave.h
 * description	: 
 *
 */

#pragma once
#include "O2Job.h"
#include "O2Profile.h"
#include "O2NodeDB.h"




class O2Job_AutoSave
	: public O2Job
{
private:
	O2Profile	*Profile;
	O2NodeDB	*NodeDB;

public:
	O2Job_AutoSave(const wchar_t *name, time_t interval, bool startup, O2Profile *prof, O2NodeDB *ndb)
		: O2Job(name, interval, startup), Profile(prof), NodeDB(ndb)
	{
	}

	~O2Job_AutoSave()
	{
	}

	void JobThreadFunc(void)
	{
		NodeDB->Save(Profile->GetNodeFilePath());
	}
};
