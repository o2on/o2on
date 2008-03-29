/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2DatDB.h
 * description	: 
 *
 */

#pragma once
#ifdef O2_DB_FIREBIRD
#include "ibase.h"
#else
#include "sqlite3.h"
#endif
#include "sha.h"
#include "O2Logger.h"




struct O2DatRec
{
	hashT		hash;
	wstring		domain;
	wstring		bbsname;
	wstring		datname;
	uint64		size;
	uint64		disksize;
	wstring		url;
	wstring		title;
	uint64		res;
	time_t		lastupdate;
	time_t		lastpublish;
	uint		userdata;

	O2DatRec(void)
		: size(0)
		, disksize(0)
		, res(0)
		, lastupdate(0)
		, lastpublish(0)
		, userdata(0)
	{
	}
};

typedef std::list<O2DatRec> O2DatRecList;
typedef O2DatRecList::iterator O2DatRecListIt;

typedef std::vector<wstrarray> SQLResultList;




class O2DatDB
{
protected:
	O2Logger		*Logger;
	wstring			dbfilename;
#ifdef O2_DB_FIREBIRD
	string			dbfilenameA;
	ISC_SCHAR		dpb_buff[128];
	short			dpblen;
#endif

	O2DatRecList	UpdateQueue;
	Mutex			UpdateQueueLock;
	HANDLE			UpdateThreadHandle;
	bool			UpdateThreadLoop;

protected:
#ifdef O2_DB_FIREBIRD
	void log(ISC_STATUS_ARRAY &status);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const uint64 num);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wchar_t *str);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wstring &str);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const hashT &hash);
	void get_columns(XSQLDA *sqlda, O2DatRec &rec);
	void get_columns(isc_stmt_handle stmt, XSQLDA *sqlda, wstrarray &cols);
	void get_column_names(XSQLDA *sqlda, wstrarray &cols);
#else
	void log(sqlite3 *db);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const uint64 num);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wchar_t *str);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wstring &str);
	bool bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const hashT &hash);
	void get_columns(sqlite3_stmt* stmt, O2DatRec &rec);
	void get_columns(sqlite3_stmt* stmt, wstrarray &cols);
	void get_column_names(sqlite3_stmt* stmt, wstrarray &cols);
#endif

public:
	O2DatDB(O2Logger *lgr, const wchar_t *filename);
	~O2DatDB();

#ifdef O2_DB_FIREBIRD
	bool create_table(void);
	//bool reindex(const char *target);

	//size_t select(const wchar_t *sql, SQLResultList &out);
	//bool select(O2DatRec &out);
	//bool select(O2DatRec &out, const hashT hash);
	//bool select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname);
	bool select(O2DatRecList &out);
	//bool select(O2DatRecList &out, const wchar_t *domain, const wchar_t *bbsname);
	//bool select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname, const wchar_t *datname);
	//bool select(O2DatRecList &out, time_t publish_tt, size_t limit);
	//uint64 select_datcount(void);
	//uint64 select_datcount(wstrnummap &out);
	//uint64 select_totaldisksize(void);
	//uint64 select_publishcount(time_t publish_tt);

//	bool update(O2DatRec &in, bool is_origurl);
	//void update(O2DatRecList &in);
//	bool update_lastpublish(const hashT &hash);

	//bool remove(const hashT &hash);
#else
	bool create_table(void);
	bool reindex(const char *target);

	size_t select(const wchar_t *sql, SQLResultList &out);
	bool select(O2DatRec &out);
	bool select(O2DatRec &out, const hashT hash);
	bool select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname);
	bool select(O2DatRecList &out);
	bool select(O2DatRecList &out, const wchar_t *domain, const wchar_t *bbsname);
	bool select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname, const wchar_t *datname);
	bool select(O2DatRecList &out, time_t publish_tt, size_t limit);
	uint64 select_datcount(void);
	uint64 select_datcount(wstrnummap &out);
	uint64 select_totaldisksize(void);
	uint64 select_publishcount(time_t publish_tt);

//	bool update(O2DatRec &in, bool is_origurl);
	void update(O2DatRecList &in);
//	bool update_lastpublish(const hashT &hash);

	bool remove(const hashT &hash);
#endif

	void AddUpdateQueue(O2DatRec &in);
	void AddUpdateQueue(const hashT &hash);
	void StartUpdateThread(void);
	void StopUpdateThread(void);

	static uint WINAPI StaticUpdateThread(void *data);
	void UpdateThread(void);
};
