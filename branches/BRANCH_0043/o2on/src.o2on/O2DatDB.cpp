/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: o2on
 * filename		: O2DatDB.cpp
 * description	: 
 *
 */

#include "O2DatDB.h"
#include <windows.h>
#include <process.h>
#include "dataconv.h"
#include "stopwatch.h"
#include <time.h>

#define UPDATE_THREAD_INTERVAL_S	15
#define MAX_INSERT_QUEUE_SIZE		1000

#if defined(_DEBUG)
#define TRACE_SQL_EXEC_TIME			1
#else
#define TRACE_SQL_EXEC_TIME			0
#endif

#define COLUMNS	L"  hash" \
				L", domain" \
				L", bbsname" \
				L", datname" \
				L", size" \
				L", disksize" \
				L", url" \
				L", title" \
				L", res" \
				L", lastupdate" \
				L", lastpublish" \
				L" "



O2DatDB::
O2DatDB(O2Logger *lgr, const wchar_t *filename)
	: Logger(lgr)
	, dbfilename(filename)
	, UpdateThreadHandle(NULL)
	, UpdateThreadLoop(false)
{
	dbfilename_to_rebuild = dbfilename + L".rebuild";
}




O2DatDB::
~O2DatDB()
{
}



void
O2DatDB::
log(sqlite3 *db)
{
	Logger->AddLog(O2LT_ERROR, L"SQLite", 0, 0, "%s", sqlite3_errmsg(db));
}




bool
O2DatDB::
bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const uint64 num)
{
	int err = sqlite3_bind_int64(stmt, index, num);
	return (err == SQLITE_OK ? true : false);
}

bool
O2DatDB::
bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wchar_t *str)
{
	int err = sqlite3_bind_text16(
		stmt, index, str, wcslen(str)*sizeof(wchar_t), SQLITE_STATIC);
	return (err == SQLITE_OK ? true : false);
}

bool
O2DatDB::
bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const wstring &str)
{
	int err = sqlite3_bind_text16(
		stmt, index, str.c_str(), str.size()*sizeof(wchar_t), SQLITE_STATIC);
	return (err == SQLITE_OK ? true : false);
}

bool
O2DatDB::
bind(sqlite3 *db, sqlite3_stmt* stmt, int index, const hashT &hash)
{
	int err = sqlite3_bind_blob(
		stmt, index, hash.data(), hash.size(), SQLITE_STATIC);
	return (err == SQLITE_OK ? true : false);
}

void
O2DatDB::
get_columns(sqlite3_stmt* stmt, O2DatRec &rec)
{
	rec.hash.assign((byte*)sqlite3_column_blob(stmt, 0), HASHSIZE);
	rec.domain		= (wchar_t*)sqlite3_column_text16(stmt, 1);
	rec.bbsname		= (wchar_t*)sqlite3_column_text16(stmt, 2);
	rec.datname		= (wchar_t*)sqlite3_column_text16(stmt, 3);
	rec.size		=			sqlite3_column_int64 (stmt, 4);
	rec.disksize	=			sqlite3_column_int64 (stmt, 5);
	rec.url			= (wchar_t*)sqlite3_column_text16(stmt, 6);
	rec.title		= (wchar_t*)sqlite3_column_text16(stmt, 7);
	rec.res			=			sqlite3_column_int64 (stmt, 8);
	rec.lastupdate	=			sqlite3_column_int64 (stmt, 9);
	rec.lastpublish	=			sqlite3_column_int64 (stmt, 10);
}

void
O2DatDB::
get_columns(sqlite3_stmt* stmt, wstrarray &cols)
{
	__int64 t_int;
	double	t_float;
	wstring	t_text;
	byte	*t_byte;
	int		size;
	wchar_t tmp[1024];

	int column_count = sqlite3_column_count(stmt);
	for (int i = 0; i < column_count; i++) {
		switch (sqlite3_column_type(stmt, i)) {
			case SQLITE_INTEGER:
				t_int = sqlite3_column_int64 (stmt, i);
				swprintf_s(tmp, 1024, L"%I64u", t_int);
				cols.push_back(tmp);
				break;
			case SQLITE_FLOAT:
				t_float = sqlite3_column_double(stmt, i);
				swprintf_s(tmp, 1024, L"%lf", t_float);
				cols.push_back(tmp);
				break;
			case SQLITE_TEXT:
				t_text = (wchar_t*)sqlite3_column_text16(stmt, i);
				cols.push_back(t_text);
				break;
			case SQLITE_BLOB:
				size = sqlite3_column_bytes(stmt, i);
				t_byte = new byte[size];
				memcpy(t_byte, (byte*)sqlite3_column_blob(stmt, i), size);
				byte2whex(t_byte, size, t_text);
				cols.push_back(t_text);
				delete [] t_byte;
				break;
		}
	}
}

void
O2DatDB::
get_column_names(sqlite3_stmt* stmt, wstrarray &cols)
{
	int column_count = sqlite3_column_count(stmt);
	for (int i = 0; i < column_count; i++) {
		cols.push_back((wchar_t*)sqlite3_column_name16(stmt, i));
	}
}



bool
O2DatDB::
check_queue_size(O2DatRecList &reclist)
{
	return (reclist.size() < MAX_INSERT_QUEUE_SIZE ? true : false);
}




bool
O2DatDB::
before_rebuild(void)
{
	if (!DeleteFile(dbfilename_to_rebuild.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
		return false;
	if (!create_table(true))
		return false;

	return true;
}

bool
O2DatDB::
after_rebuild(void)
{
	if (!MoveFileEx(dbfilename.c_str(), (dbfilename + L".old").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return false;
	if (!MoveFileEx(dbfilename_to_rebuild.c_str(), dbfilename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return false;

	return true;
}




bool
O2DatDB::
create_table(bool to_rebuild)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("create table/index and analyze");
#endif

	sqlite3 *db = NULL;
	int err = sqlite3_open16(to_rebuild ? dbfilename_to_rebuild.c_str() : dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;

	char *sql =
		"create table if not exists dat ("
		"    hash        BLOB,"
		"    domain      TEXT,"
		"    bbsname     TEXT,"
		"    datname     TEXT,"
		"    size        INTEGER,"
		"    disksize    INTEGER,"
		"    url         TEXT,"
		"    title       TEXT,"
		"    res         INTEGER,"
		"    lastupdate  INTEGER,"
		"    lastpublish INTEGER,"
//		"    flags       INTEGER,"
//		"    matchbytes  INTEGER,"
//		"    matchbytes1 INTEGER,"
//		"    matchbytes2 INTEGER,"
		"    PRIMARY KEY (hash)"
		");"
		"drop index if exists idx_dat_domain;"
		"drop index if exists idx_dat_bbsname;"
//		"drop index if exists idx_dat_datname;"
		"create index if not exists idx_dat_domain_bbsname_datname on dat (domain, bbsname, datname);"
		"create index if not exists idx_dat_lastpublish on dat (lastpublish);"
		"create index if not exists idx_dat_datname on dat (datname);";
//		"analyze;";
	err = sqlite3_exec(db, sql, NULL, 0, 0);
	if (err != SQLITE_OK)
		goto error;
/*
	sql =
		"alter table dat add column flags INTEGER;"
		"alter table dat add column matchbytes INTEGER;"
		"alter table dat add column matchbytes1 INTEGER;"
		"alter table dat add column matchbytes2 INTEGER;";
	err = sqlite3_exec(db, sql, NULL, 0, 0);
	//if (err != SQLITE_OK)
	//	goto error;
*/
	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return true;

error:
	log(db);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
reindex(const char *target)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("reindex");
#endif

	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;

	char sql[64];
	sprintf_s(sql, 64, "reindex %s;", target);

	err = sqlite3_exec(db, sql, NULL, 0, 0);
	if (err != SQLITE_OK)
		goto error;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return true;

error:
	log(db);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
analyze(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("analyze/vacuum");
#endif

	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;

	char sql[] = "begin;analyze;end; vacuum dat;";

	err = sqlite3_exec(db, sql, NULL, 0, 0);
	if (err != SQLITE_OK)
		goto error;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return true;

error:
	log(db);
	if (db) sqlite3_close(db);
	return false;
}




size_t
O2DatDB::
select(const wchar_t *sql, SQLResultList &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select");
#endif
	wstrarray cols;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_reset(stmt);

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	if (out.empty()) {
		cols.clear();
		get_column_names(stmt, cols);
		out.push_back(cols);
	}
	for ( ; err == SQLITE_ROW; err = sqlite3_step(stmt)) {
		cols.clear();
		get_columns(stmt, cols);
		out.push_back(cols);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (out.size());

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return (0);
}



bool
O2DatDB::
select(O2DatRec &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select random 1");
#endif

	bool ret = true;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" order by random() limit 1;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_reset(stmt);

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	if (err == SQLITE_DONE)
		ret = false;
	if (err == SQLITE_ROW)
		get_columns(stmt, out);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (ret);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}

	
	
bool
O2DatDB::
select(O2DatRec &out, hashT hash)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by hash");
#endif

	bool ret = true;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where hash = ?;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, hash))
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;
	if (err == SQLITE_DONE)
		ret = false;
	if (err == SQLITE_ROW)
		get_columns(stmt, out);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (ret);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}

bool
O2DatDB::
select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by domain bbsname");
#endif

	bool ret = true;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where domain = ?"
		L"   and bbsname = ?"
		L" order by random() limit 1;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, domain))
		goto error;
	if (!bind(db, stmt, 2, bbsname))
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	if (err == SQLITE_DONE)
		ret = false;
	if (err == SQLITE_ROW)
		get_columns(stmt, out);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (ret);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname, const wchar_t *datname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by domain bbsname datname");
#endif

	bool ret = true;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where domain = ?"
		L"   and bbsname = ?"
		L"   and datname = ?;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, domain))
		goto error;
	if (!bind(db, stmt, 2, bbsname))
		goto error;
	if (!bind(db, stmt, 3, datname))
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	if (err == SQLITE_DONE)
		ret = false;
	if (err == SQLITE_ROW)
		get_columns(stmt, out);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (ret);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
select(O2DatRecList &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select all");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	O2DatRec rec;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		get_columns(stmt, rec);
		out.push_back(rec);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return true;

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
select(O2DatRecList &out, const wchar_t *domain, const wchar_t *bbsname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select domain bbsname order by datname");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	O2DatRec rec;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where domain = ?"
		L"   and bbsname = ?"
		L" order by datname;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, domain))
		goto error;
	if (!bind(db, stmt, 2, bbsname))
		goto error;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		get_columns(stmt, rec);
		out.push_back(rec);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return true;

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




bool
O2DatDB::
select(O2DatRecList &out, time_t publish_tt, size_t limit)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select lastpublish");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	O2DatRec rec;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where lastpublish < ?"
		L" order by lastpublish"
		L" limit ?;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, time(NULL)-publish_tt))
		goto error;
	if (!bind(db, stmt, 2, limit))
		goto error;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		get_columns(stmt, rec);
		out.push_back(rec);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return true;

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




uint64
O2DatDB::
select_datcount(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;

	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql = 
		L"begin;"
		L"select count(*) from dat;"
		L"end;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	uint64 count = sqlite3_column_int64(stmt,0);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (count);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return (0);
}




uint64
O2DatDB::
select_datcount(wstrnummap &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount group by domain bbsname");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	wstring domain_bbsname;
	uint64 total = 0;
	uint64 num;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select domain, bbsname, count(*) from dat group by domain, bbsname;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		domain_bbsname = (wchar_t*)sqlite3_column_text16(stmt, 0);
		domain_bbsname += L":";
		domain_bbsname += (wchar_t*)sqlite3_column_text16(stmt, 1);
		num = sqlite3_column_int64(stmt, 2);

		out.insert(wstrnummap::value_type(domain_bbsname, num));
		total += num;
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;
	return (total);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




uint64
O2DatDB::
select_totaldisksize(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select totakdisksize");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"begin;"
		L"select sum(disksize) from dat;"
		L"end;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	uint64 totalsize = sqlite3_column_int64(stmt,0);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (totalsize);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return (0);
}




uint64
O2DatDB::
select_publishcount(time_t publish_tt)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount by lastpublish");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql = L"select count(*) from dat where lastpublish > ?;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, time(NULL)-publish_tt))
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	uint64 count = sqlite3_column_int64(stmt,0);

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return (count);

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return (0);
}




void
O2DatDB::
insert(O2DatRecList &in, bool to_rebuild)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("insert by reclist");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt_insert = NULL;
	O2DatRec org;

	int err = sqlite3_open16(to_rebuild ? dbfilename_to_rebuild.c_str() : dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	sqlite3_exec(db, "pragma synchronous = OFF;", NULL, NULL, NULL);

	wchar_t *sql_insert =
		L"insert or replace into dat ("
		COLUMNS
		L") values ("
		L"?,?,?,?,?,?,?,?,?,?,?"
		L");";
	err = sqlite3_prepare16_v2(db, sql_insert, wcslen(sql_insert)*2, &stmt_insert, NULL);
	if (err != SQLITE_OK)
		goto error;

	//
	//	Loop
	//
	sqlite3_exec(db, "begin;", NULL, NULL, NULL);
	for (O2DatRecListIt it = in.begin(); it != in.end(); it++) {
		sqlite3_reset(stmt_insert);
		if (!bind(db, stmt_insert, 1, it->hash))
			goto error;
		if (!bind(db, stmt_insert, 2, it->domain))
			goto error;
		if (!bind(db, stmt_insert, 3, it->bbsname))
			goto error;
		if (!bind(db, stmt_insert, 4, it->datname))
			goto error;
		if (!bind(db, stmt_insert, 5, it->size))
			goto error;
		if (!bind(db, stmt_insert, 6, it->disksize))
			goto error;
		if (!bind(db, stmt_insert, 7, it->url))
			goto error;
		if (!bind(db, stmt_insert, 8, it->title))
			goto error;
		if (!bind(db, stmt_insert, 9, it->res))
			goto error;
		if (!bind(db, stmt_insert, 10, time(NULL)))
			goto error;
		if (!bind(db, stmt_insert, 11, (uint64)0))
			goto error;

		err = sqlite3_step(stmt_insert);
		if (err != SQLITE_ROW && err != SQLITE_DONE)
			goto error;
		Sleep(1);
	}
	sqlite3_exec(db, "commit;", NULL, NULL, NULL);

	sqlite3_finalize(stmt_insert);
	stmt_insert = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return;

error:
	log(db);
	if (stmt_insert) sqlite3_finalize(stmt_insert);
	if (db) sqlite3_close(db);
	return;
}




void
O2DatDB::
update(O2DatRecList &in)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("update by reclist");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt_insert = NULL;
	sqlite3_stmt *stmt_update = NULL;
	sqlite3_stmt *stmt_updatepublish = NULL;
	O2DatRec org;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql_insert =
		L"insert or replace into dat ("
		COLUMNS
		L") values ("
		L"?,?,?,?,?,?,?,?,?,?,?"
		L");";
	err = sqlite3_prepare16_v2(db, sql_insert, wcslen(sql_insert)*2, &stmt_insert, NULL);
	if (err != SQLITE_OK)
		goto error;

	wchar_t *sql_update =
		L"update or replace dat"
		L"   set size        = ?"
		L"     , disksize    = ?"
		L"     , url         = ?"
		L"     , res	     = ?"
		L"     , lastupdate  = ?"
//		L"     , lastpublish = 0"
		L" where hash = ?;";

	err = sqlite3_prepare16_v2(db, sql_update, wcslen(sql_update)*2, &stmt_update, NULL);
	if (err != SQLITE_OK)
		goto error;

	wchar_t *sql_updatepublish =
		L"update or replace dat"
		L"   set lastpublish = ?"
		L" where hash = ?;";

	err = sqlite3_prepare16_v2(db, sql_updatepublish, wcslen(sql_updatepublish)*2, &stmt_updatepublish, NULL);
	if (err != SQLITE_OK)
		goto error;

	//
	//	Loop
	//
	sqlite3_exec(db, "begin;", NULL, NULL, NULL);
	for (O2DatRecListIt it = in.begin(); it != in.end(); it++) {
		if (!select(org, it->hash)) {
			sqlite3_reset(stmt_insert);
			if (!bind(db, stmt_insert, 1, it->hash))
				goto error;
			if (!bind(db, stmt_insert, 2, it->domain))
				goto error;
			if (!bind(db, stmt_insert, 3, it->bbsname))
				goto error;
			if (!bind(db, stmt_insert, 4, it->datname))
				goto error;
			if (!bind(db, stmt_insert, 5, it->size))
				goto error;
			if (!bind(db, stmt_insert, 6, it->disksize))
				goto error;
			if (!bind(db, stmt_insert, 7, it->url))
				goto error;
			if (!bind(db, stmt_insert, 8, it->title))
				goto error;
			if (!bind(db, stmt_insert, 9, it->res))
				goto error;
			if (!bind(db, stmt_insert, 10, time(NULL)))
				goto error;
			if (!bind(db, stmt_insert, 11, (uint64)0))
				goto error;

			err = sqlite3_step(stmt_insert);
			if (err != SQLITE_ROW && err != SQLITE_DONE)
				goto error;
		}
		else if (it->userdata == 0) {
			sqlite3_reset(stmt_update);
			if (!bind(db, stmt_update, 1, it->size))
				goto error;
			if (!bind(db, stmt_update, 2, it->disksize))
				goto error;
			if (!bind(db, stmt_update, 3, (wcsstr(org.url.c_str(), L"xxx") == 0 ? it->url : org.url)))
				goto error;
			if (!bind(db, stmt_update, 4, it->res))
				goto error;
			if (!bind(db, stmt_update, 5, time(NULL)))
				goto error;
			if (!bind(db, stmt_update, 6, it->hash))
				goto error;

			err = sqlite3_step(stmt_update);
			if (err != SQLITE_ROW && err != SQLITE_DONE)
				goto error;
		}
		else {
			sqlite3_reset(stmt_updatepublish);
			if (!bind(db, stmt_updatepublish, 1, time(NULL)))
				goto error;
			if (!bind(db, stmt_updatepublish, 2, it->hash))
				goto error;

			err = sqlite3_step(stmt_updatepublish);
			if (err != SQLITE_ROW && err != SQLITE_DONE)
				goto error;
		}
		Sleep(1);
	}
	sqlite3_exec(db, "commit;", NULL, NULL, NULL);

	sqlite3_finalize(stmt_insert);
	sqlite3_finalize(stmt_update);
	sqlite3_finalize(stmt_updatepublish);
	stmt_insert = NULL;
	stmt_update = NULL;
	stmt_updatepublish = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return;

error:
	log(db);
	if (stmt_insert) sqlite3_finalize(stmt_insert);
	if (stmt_update) sqlite3_finalize(stmt_update);
	if (stmt_updatepublish) sqlite3_finalize(stmt_updatepublish);
	if (db) sqlite3_close(db);
	return;
}




bool
O2DatDB::
remove(const hashT &hash)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("remove");
#endif

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"delete from dat where hash = ?;";

	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, hash))
		goto error;

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	sqlite3_finalize(stmt);
	stmt = NULL;

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		goto error;

	return true;

error:
	log(db);
	if (stmt) sqlite3_finalize(stmt);
	if (db) sqlite3_close(db);
	return false;
}




void
O2DatDB::
AddUpdateQueue(O2DatRec &in)
{
	UpdateQueueLock.Lock();
	{
		UpdateQueue.push_back(in);
	}
	UpdateQueueLock.Unlock();
}
void
O2DatDB::
AddUpdateQueue(const hashT &hash)
{
	UpdateQueueLock.Lock();
	{
		O2DatRec rec;
		rec.hash = hash;
		rec.lastpublish = time(NULL);
		rec.userdata = 1;
		UpdateQueue.push_back(rec);
	}
	UpdateQueueLock.Unlock();
}




void
O2DatDB::
StartUpdateThread(void)
{
	Logger->AddLog(O2LT_INFO, L"UpdateThread", 0, 0, L"ŠJŽn");
	if (UpdateThreadHandle)
		return;

	UpdateThreadLoop = true;
	StopSignal.Off();
	UpdateThreadHandle = (HANDLE)_beginthreadex(
		NULL, 0, StaticUpdateThread, (void*)this, 0, NULL);
}
void
O2DatDB::
StopUpdateThread(void)
{
	Logger->AddLog(O2LT_INFO, L"UpdateThread", 0, 0, L"’âŽ~");
	if (!UpdateThreadHandle)
		return;
	UpdateThreadLoop = false;
	StopSignal.On();
	//Join
	WaitForSingleObject(UpdateThreadHandle, INFINITE);
	CloseHandle(UpdateThreadHandle);

	UpdateQueueLock.Lock();
	if (!UpdateQueue.empty()) {
		update(UpdateQueue);
		UpdateQueue.clear();
	}
	UpdateQueueLock.Unlock();

	UpdateThreadHandle = NULL;
}
uint WINAPI
O2DatDB::
StaticUpdateThread(void *data)
{
	O2DatDB *me = (O2DatDB*)data;

	CoInitialize(NULL);
	me->UpdateThread();
	CoUninitialize();

	//_endthreadex(0);
	return (0);
}
void
O2DatDB::
UpdateThread(void)
{
	time_t t = time(NULL);

	while (UpdateThreadLoop) {
		if (time(NULL) - t >= UPDATE_THREAD_INTERVAL_S) {
			UpdateQueueLock.Lock();
			O2DatRecList reclist(UpdateQueue);
			UpdateQueue.clear();
			UpdateQueueLock.Unlock();

			if (!reclist.empty())
				update(reclist);
			t = time(NULL);
			CLEAR_WORKSET;
			//TRACEA("+++++ UPDATE +++++\n");
		}
		StopSignal.Wait(3000);
	}
}
