/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2FriendDB.h
 * description	: 
 *
 */

#pragma once
#include "O2SAX2Parser.h"
#include "O2NodeDB.h"
#include "O2JobSchedule.h"
#include "file.h"
#include <tchar.h>




//
//	O2FriendDB
//
class O2FriendDB
{
protected:
	O2Logger			*Logger;
	O2NodeDB			*NodeDB;
	O2NodeDB::NodeListT	Friends;
	Mutex				FriendsLock;

public:
	O2FriendDB(O2Logger *lgr, O2NodeDB *ndb)
		: Logger(lgr)
		, NodeDB(ndb)
	{
	}
	~O2FriendDB(void)
	{
	}

	size_t Count(void)
	{
		FriendsLock.Lock();
		size_t count = Friends.size();
		FriendsLock.Unlock();
		return (count);
	}
	size_t GetFriends(O2NodeDB::NodeListT &out)
	{
		FriendsLock.Lock();
		out = Friends;
		FriendsLock.Unlock();
		return (Friends.size());
	}

	bool Add(const O2Node &node)
	{
		bool ret = false;

		FriendsLock.Lock();
		O2NodeDB::NodeListT::iterator it =
			std::find(Friends.begin(), Friends.end(), node);
		if (it == Friends.end()) {
			Friends.push_back(node);
			ret = true;
		}
		FriendsLock.Unlock();
		return (ret);
	}
	bool Update(const O2Node &node)
	{
		bool ret = false;

		FriendsLock.Lock();
		O2NodeDB::NodeListT::iterator it =
			std::find(Friends.begin(), Friends.end(), node);
		if (it != Friends.end()) {
			O2Node mnode(*it);
			mnode.marge(node);
			mnode.lastlink = time(NULL);
			Friends.erase(it);
			Friends.push_back(mnode);
			ret = true;
		}
		FriendsLock.Unlock();
		return (ret);
	}
	bool Delete(const hashT &id)
	{
		bool ret = false;

		O2Node node;
		node.id = id;

		FriendsLock.Lock();
		O2NodeDB::NodeListT::iterator it =
			std::find(Friends.begin(), Friends.end(), node);
		if (it != Friends.end()) {
			Friends.erase(it);
			ret = true;
		}
		FriendsLock.Unlock();
		return (ret);
	}

	bool Save(const wchar_t *filename)
	{
		O2NodeSelectCondition cond;
		string out;

		ExportToXML(cond, out);

		File f;
		if (!f.open(filename, MODE_W)) {
			if (Logger)
				Logger->AddLog(O2LT_ERROR, L"FriendDB", 0, 0,
				L"ファイルを開けません(%s)", filename);
			return false;
		}
		f.write((void*)&out[0], out.size());
		f.close();

		return true;
	}
	bool Load(const wchar_t *filename)
	{
		struct _stat st;
		if (_wstat(filename, &st) == -1)
			return false;
		if (st.st_size == 0)
			return false;
		ImportFromXML(filename, NULL, 0);
		return true;
	}
	size_t ExportToXML(const O2NodeSelectCondition &cond, string &out)
	{
		FriendsLock.Lock();
		O2NodeDB::NodeListT::iterator it;
		for (it = Friends.begin(); it != Friends.end(); it++) {
			if (time(NULL) - it->lastlink < O2_FRIEND_LIVING_TT)
				it->status = O2_NODESTATUS_LINKEDFROM;
			else if (time(NULL) - it->lastlink < O2_FRIEND_LIVING_TT*2)
				it->status = O2_NODESTATUS_PASTLINKEDFROM;
			else
				it->status = 0;
		}
		size_t ret = NodeDB->ExportToXML(Friends, cond, out);
		FriendsLock.Unlock();
		return (ret);
	}
	size_t ImportFromXML(const wchar_t *filename, const char *in, uint len)
	{
		FriendsLock.Lock();
		size_t ret = NodeDB->ImportFromXML(filename, in, len, &Friends);
		FriendsLock.Unlock();
		return (ret);
	}
};
