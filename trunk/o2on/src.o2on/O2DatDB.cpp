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

#if defined(_DEBUG)
#define TRACE_SQL_EXEC_TIME			1
#else
#define TRACE_SQL_EXEC_TIME			0
#endif

#define COLUMNSA	"  hash" \
				", domainname" \
				", bbsname" \
				", datname" \
				", filesize" \
				", disksize" \
				", url" \
				", title" \
				", res" \
				", lastupdate" \
				", lastpublish" \
				" "
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
#ifdef O2_DB_FIREBIRD
#define SQL_CHAR_NONE	0
#define SQL_CHAR_OCTETS	1
#define SQL_CHAR_ASCII	2
#define SQL_CHAR_UNICODE_FSS	3
#define SQL_CHAR_UTF8	4
#define SQL_CHAR_SJIS	5
#define SQL_CHAR_EUCJ	6
#endif



O2DatDB::
O2DatDB(O2Logger *lgr, const wchar_t *filename)
	: Logger(lgr)
	, dbfilename(filename)
	, UpdateThreadHandle(NULL)
	, UpdateThreadLoop(false)
{
#ifdef O2_DB_FIREBIRD
	FromUnicode(L"shift_jis", filename, dbfilenameA);

	char user_name[] = "SYSDBA";
	char user_password[] = "masterkey";
	char *dpb, *p;
	dpb = dpb_buff;
	*dpb++ = isc_dpb_version1;
	*dpb++ = isc_dpb_user_name;
	*dpb++ = strlen(user_name);
	for (p = user_name; *p;)
		*dpb++ = *p++;
	*dpb++ = isc_dpb_password;
	*dpb++ = strlen(user_password);
	for (p = user_password; *p;)
		*dpb++ = *p++;
	dpblen = dpb - dpb_buff;
#endif
}




O2DatDB::
~O2DatDB()
{
}


#ifdef O2_DB_FIREBIRD
void
O2DatDB::
log(ISC_STATUS_ARRAY &status)
{
	string errmsg;
	char msg[512];
	const ISC_STATUS* st = status;
	while(fb_interpret(msg, sizeof(msg), &st)){
		errmsg += msg;
		errmsg += " ";
    }
	Logger->AddLog(O2LT_ERROR, L"Firebird", 0, 0, "%s", errmsg.c_str());
}
#else
void
O2DatDB::
log(sqlite3 *db)
{
	Logger->AddLog(O2LT_ERROR, L"SQLite", 0, 0, sqlite3_errmsg(db));
}
#endif




#ifdef O2_DB_FIREBIRD
bool
O2DatDB::
bind(XSQLDA *sqlda, int index, const uint64 num)
{
	XSQLVAR *var = sqlda->sqlvar;
	if (sqlda->sqln > index && (var[index].sqltype & ~1) == SQL_INT64) {
		var[index].sqltype = SQL_INT64;
		*(ISC_INT64*)var[index].sqldata = num;
		return true;
	} else
		return false;
}

bool
O2DatDB::
bind(XSQLDA *sqlda, int index, const wchar_t *str)
{
	string tmpstr;
	XSQLVAR *var = sqlda->sqlvar;
	if (sqlda->sqln > index && (var[index].sqltype & ~1) == SQL_VARYING) {
		FromUnicode(L"UTF-8", str, wcslen(str)*2, tmpstr);
		var[index].sqltype = SQL_TEXT;
		strcpy_s(var[index].sqldata, var[index].sqllen, tmpstr.c_str());
		//tmpstr.copy(var[index].sqldata, tmpstr.length(), 0);
		var[index].sqllen = wcslen(str);
		return true;
	} else
		return false;
}

bool
O2DatDB::
bind(XSQLDA *sqlda, int index, const wstring &str)
{
	string tmpstr;
	XSQLVAR *var = sqlda->sqlvar;
	if (sqlda->sqln > index && (var[index].sqltype & ~1 )== SQL_VARYING) {
		FromUnicode(L"UTF-8", str, tmpstr);
		var[index].sqltype = SQL_TEXT;
		strcpy_s(var[index].sqldata, var[index].sqllen, tmpstr.c_str());
		//tmpstr.copy(var[index].sqldata, tmpstr.length(), 0);
		var[index].sqllen = strlen(tmpstr.c_str());
		return true;
	} else
		return false;
}

bool
O2DatDB::
bind(XSQLDA *sqlda, int index, const hashT &hash)
{
	XSQLVAR *var = sqlda->sqlvar;
	if (sqlda->sqln > index && var[index].sqllen == 20 && (var[index].sqltype & ~1)== SQL_TEXT) {
		memcpy(var[index].sqldata, hash.data(), hash.size());
		return true;
	} else
		return false;
}

void
O2DatDB::
get_columns(XSQLDA *sqlda, O2DatRec &rec)
{
	XSQLVAR *var = sqlda->sqlvar;

	rec.hash.assign((byte*)var->sqldata, HASHSIZE); var++;
	ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, rec.domain); var++;
	ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, rec.bbsname); var++;
	ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, rec.datname); var++;
	rec.size = *(long*)(var->sqldata); var++;
	rec.disksize = *(long*)var->sqldata; var++;
	ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, rec.url); var++;
	ToUnicode(L"UTF-8", var->sqldata+2, *(ISC_SHORT*)var->sqldata, rec.title); var++;
	rec.res = 0; var++; // always 0
	rec.lastupdate =  *(long*)var->sqldata; var++;
	rec.lastpublish =  *(long*)var->sqldata;
}

void
O2DatDB::
get_columns(XSQLDA* sqlda, wstrarray &cols)
{
	int i;
	wstring tmpstr;
	wchar_t tmp[1024];
	XSQLVAR *var;

	for (i = 0, var = sqlda->sqlvar; i < sqlda->sqld; i++, var++) {
		tmpstr.clear();
		switch (var->sqltype & ~1) {
		case SQL_VARYING:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				if (var->sqlsubtype == SQL_CHAR_OCTETS){
					byte2whex((const byte *)var->sqldata+2, *(ISC_SHORT*)var->sqldata, tmpstr);
					cols.push_back(tmpstr);
				} else {
					ToUnicode(L"UTF-8", var->sqldata+2, *(ISC_SHORT*)var->sqldata, tmpstr);
					//ascii2unicode(var->sqldata+2, var->sqllen, tmpstr);
					cols.push_back(tmpstr);
				}
			}
			break;
		case SQL_TEXT:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else if (var->sqlsubtype == SQL_CHAR_OCTETS){
				byte2whex((const byte *)var->sqldata, var->sqllen, tmpstr);
				cols.push_back(tmpstr);
			} else {
				ToUnicode(L"UTF-8", var->sqldata, var->sqllen, tmpstr);
				//ascii2unicode(var->sqldata, *(ISC_SHORT*)var->sqldata, tmpstr);
				cols.push_back(tmpstr);
			}
			break;
		case SQL_LONG:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				swprintf_s(tmp, 1024, L"%d", *(ISC_LONG*)var->sqldata);
				cols.push_back(tmp);
			}
			break;
		case SQL_SHORT:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				swprintf_s(tmp, 1024, L"%d", *(ISC_SHORT*)var->sqldata);
				cols.push_back(tmp);
			}
			break;
		case SQL_FLOAT:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				swprintf_s(tmp, 1024, L"%lf", *(float*)var->sqldata);
				cols.push_back(tmp);
			}
			break;
		case SQL_DOUBLE:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				swprintf_s(tmp, 1024, L"%lf", *(double*)var->sqldata);
				cols.push_back(tmp);
			}
			break;
		case SQL_INT64:
			if ((var->sqltype & 1) && (*(var->sqlind) == -1)) {
				cols.push_back(L"[NULL]");
			} else {
				swprintf_s(tmp, 1024, L"%I64d", *(ISC_INT64*)var->sqldata);
				cols.push_back(tmp);
			}
			break;
		case SQL_ARRAY:
		case SQL_BLOB:
		default:
			cols.push_back(L"[unsupported data type]");
			break;
		}
	}
}

void
O2DatDB::
get_column_names(XSQLDA *sqlda, wstrarray &cols)
{
	wstring tmpstr;
	XSQLVAR *var = sqlda->sqlvar;
	for (int i = 0; i < sqlda->sqln; i++, var++) {
		ascii2unicode(var->sqlname, var->sqlname_length, tmpstr);
		cols.push_back(tmpstr);
	}
}

XSQLDA*
O2DatDB::
prepare_xsqlda(isc_stmt_handle stmt, bool bind)
{
	ISC_STATUS_ARRAY status;
	XSQLVAR *var;
	XSQLDA *outda = (XSQLDA *)new char[(XSQLDA_LENGTH(1))];
	outda->version = SQLDA_VERSION1;
	outda->sqln = 1;
	if (bind) {
		if (isc_dsql_describe_bind(status, &stmt, 3, outda)) goto error;
	} else {
		if (isc_dsql_describe(status, &stmt, 3, outda)) goto error;
	}
	if (outda->sqld > outda->sqln) {
		int n = outda->sqld;
		delete outda;
		outda = (XSQLDA *)new char[XSQLDA_LENGTH(n)];
		outda->version = SQLDA_VERSION1;
		outda->sqln = n;
		if (bind)
			isc_dsql_describe_bind(status, &stmt, 3, outda);
		else
			isc_dsql_describe(status, &stmt, 3, outda);
	} else if (outda->sqld ==0) {
		delete(outda);
		return NULL;
	}
	int i;
	for (var = outda->sqlvar, i=0; i < outda->sqld; i++, var++) {
		switch (var->sqltype & ~1) {
		case SQL_VARYING:
			var->sqldata = (char *)new char[var->sqllen + 2];
			break;
		default:
			var->sqldata = (char *)new char[var->sqllen];
			break;
		}
		if (!bind && (var->sqltype & 1)) {
			var->sqlind = (short *)new short;
		}
	}
	return outda;
error:
	if (outda) delete outda;
	log(status);
	return NULL;
}

void
O2DatDB::
free_xsqlda(XSQLDA* &sqlda)
{
	if (sqlda == NULL)
		return;
	XSQLVAR* var = sqlda->sqlvar;
	for (int i=0; i < sqlda->sqln; i++,var++){
		delete(var->sqldata);
		if (var->sqltype & 1) {
			delete(var->sqlind);
		}
	}
	delete(sqlda);
	sqlda = NULL;
	return;
}
#else
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
#endif




bool
O2DatDB::
create_table(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("create table/index and analyze");
#endif
#ifdef O2_DB_FIREBIRD
	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	isc_stmt_handle stmt = NULL;
	ISC_STATUS_ARRAY status;

	char create_db[512];
	sprintf_s(create_db, 512, "CREATE DATABASE '%s' USER 'SYSDBA' PASSWORD 'masterkey';", dbfilenameA.c_str());
	if (!isc_dsql_execute_immediate(status, &db, &tr, 0, create_db, 3, NULL))
		isc_detach_database(status, &db);
	
	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql0 = 
		"EXECUTE BLOCK AS BEGIN "
		"if (not exists(select 1 from rdb$relations where rdb$relation_name = 'DAT')) then "
		"execute statement 'create table DAT ("
		"    hash         CHAR(20) CHARACTER SET OCTETS NOT NULL PRIMARY KEY,"//length is HASHSIZE in sha.h
		"    domainname   VARCHAR(16) CHARACTER SET ASCII,"
		"    bbsname      VARCHAR(32) CHARACTER SET ASCII,"
		"    datname      VARCHAR(32) CHARACTER SET ASCII,"
		"    filesize     BIGINT,"
		"    disksize     BIGINT,"
		"    url          VARCHAR(256) CHARACTER SET ASCII,"
		"    title        VARCHAR(512) CHARACTER SET UTF8,"
		"    res          BIGINT,"
		"    lastupdate   BIGINT,"
		"    lastpublish  BIGINT"
		");';"
		"END";

	char *sql1 =
		"EXECUTE BLOCK AS BEGIN "
		"if (not exists(select 1 from RDB$INDICES where RDB$INDEX_NAME = 'IDX_DAT_LASTPUBLISH')) then "
		"execute statement 'create index idx_dat_lastpublish on dat (lastpublish);';"
		"END";

	char *sql2 =
		"EXECUTE BLOCK AS BEGIN "
		"if (not exists(select 1 from RDB$INDICES where RDB$INDEX_NAME = 'IDX_DAT_DOMAIN_BBSNAME_DATNAME')) then "
		"execute statement 'create index idx_dat_domain_bbsname_datname on dat (bbsname, domainname, datname);';"
		"END";

	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql0, 3, NULL))
		goto error;
	if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql1, 3, NULL))
		goto error;
	if (isc_dsql_execute_immediate(status, &db, &tr, 0, sql2, 3, NULL))
		goto error;
	if (isc_commit_transaction(status, &tr))
		goto error;
	if (isc_detach_database(status, &db))
		goto error;
	return (true);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (db) isc_detach_database(status, &db);
	return (false);
#else
	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
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
		"drop index if exists idx_dat_datname;"
		"create index if not exists idx_dat_domain_bbsname_datname on dat (domain, bbsname, datname);"
		"create index if not exists idx_dat_lastpublish on dat (lastpublish);"
		"analyze;";
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
#endif
}




#ifdef O2_DB_FIREBIRD
bool
O2DatDB::
reindex(const char *target)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("reindex");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;

	char sql[64];
	sprintf_s(sql, 64, "ALTER INDEX %s INACTIVE;", target);
	if (isc_execute_immediate(status, &db, &tr, 0, sql))
		goto error;
	sprintf_s(sql, 64, "ALTER INDEX %s ACTIVE;", target);
	if (isc_execute_immediate(status, &db, &tr, 0, sql))
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (isc_detach_database(status, &db))
		goto error;

	return (true);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (db) isc_detach_database(status, &db);
	return (false);
}




size_t
O2DatDB::
select(const wchar_t *sql, SQLResultList &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL;
	string w_str, sqlstr;
	wstrarray cols;
	long fetch;
	size_t ct = 0; 

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	FromUnicode(L"UTF-8", sql, sqlstr);

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sqlstr.c_str(), 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);

	//if (outda == NULL) {
	//	if(isc_dsql_execute(status, &tr, &stmt,3, NULL))
	//		goto error;
	//	goto fin;
	//} else if (isc_dsql_execute2(status, &tr, &stmt, 3, outda))
	//	goto error;
	fetch = isc_dsql_execute2(status, &tr, &stmt, 3, NULL, outda);
	if(fetch !=0 &&isc_sqlcode(status) != 100L)
		goto error;

	if (out.empty()) {
		//àÍçsñ⁄
		cols.clear();
		get_column_names(outda, cols);
		out.push_back(cols);
		cols.clear();
		get_columns(outda, cols);
		out.push_back(cols);
		ct++;
	}
	while (fetch = isc_dsql_fetch(status, &stmt, 3, outda) == 0){
		//2çsñ⁄à»ç~
		cols.clear();
		get_columns(outda, cols);
		out.push_back(cols);
		ct++;
	}

fin:
	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(outda);
	return (ct);

error:
	out.clear();
	cols.clear();
	cols.push_back(L"SQL error");
	out.push_back(cols);
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(outda);
	return (0);
}



bool
O2DatDB::
select(O2DatRec &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select random 1");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL;
	string w_str;
	int w_row = 0;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql = 
		"select first 1 "
		COLUMNSA
		" from dat order by rand();";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, outda))
		goto error;
	XSQLDA *sqlda = prepare_xsqlda(stmt, false);

	if (isc_dsql_execute(status, &tr, &stmt, 1, NULL))
		goto error;
	if (isc_dsql_fetch(status, &stmt, 1, sqlda))
		goto error;
	get_columns(sqlda, out);

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);
	return (true);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);
	return (false);
}




bool
O2DatDB::
select(O2DatRec &out, hashT hash)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by hash");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;
	bool ret = false;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select"
		COLUMNSA
		" from dat"
		" where hash = ?;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);

	if(!bind(inda, 0, hash))
		goto error;

	if (!isc_dsql_execute2(status, &tr, &stmt, 3, inda, outda)) {
		get_columns(outda, out);
		ret = true;
	} else if (isc_sqlcode(status) != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (ret);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (false);
}

bool
O2DatDB::
select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by domain bbsname");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;
	bool ret = false;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select first 1"
		COLUMNSA
		" from dat"
		" where domainname = ?"
		"   and bbsname = ?"
		" order by rand();";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);

	if (!bind(inda, 0, domain))
		goto error;
	if (!bind(inda, 1, bbsname))
		goto error;
	
	if (!isc_dsql_execute2(status, &tr, &stmt, 3, inda, outda)) {
		get_columns(outda, out);
		ret = true;
	} else if (isc_sqlcode(status) != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (ret);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (false);
/*

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where domain = ?"
		L"   and bbsname = ?"
		L" order by random() limit 1;";

*/
}




bool
O2DatDB::
select(O2DatRec &out, const wchar_t *domain, const wchar_t *bbsname, const wchar_t *datname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select by domain bbsname datname");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;
	bool ret = false;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select"
		COLUMNSA
		" from dat"
		" where domainname = ?"
		"   and bbsname = ?"
		"   and datname = ?;";
	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);

	if (!bind(inda, 0, domain))
		goto error;
	if (!bind(inda, 1, bbsname))
		goto error;
	if (!bind(inda, 2, datname))
		goto error;
	
	if (!isc_dsql_execute2(status, &tr, &stmt, 3, inda, outda)) {
		get_columns(outda, out);
		ret = true;
	} else if (isc_sqlcode(status) != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (ret);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (false);
/*

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where domain = ?"
		L"   and bbsname = ?"
		L"   and datname = ?;";
*/
}




bool
O2DatDB::
select(O2DatRecList &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select all");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	O2DatRec rec;
	XSQLDA *outda = NULL;
	string w_str;
	int w_row = 0;
	long fetch_stat;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select"
		COLUMNSA
		" from dat;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	if (isc_dsql_execute(status, &tr, &stmt, 3, NULL))
		goto error;

	while ((fetch_stat = isc_dsql_fetch(status, &stmt, 1, outda)) == 0) {
		get_columns(outda, rec);
		out.push_back(rec);
	}
	
	if (isc_commit_transaction(status, &tr))
		goto error;
	if (isc_dsql_free_statement(status, &stmt, DSQL_drop))
		goto error;
	if (isc_detach_database(status, &db))
		goto error;

	free_xsqlda(outda);
	return true;

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(outda);
	return false;
}




bool
O2DatDB::
select(O2DatRecList &out, const wchar_t *domain, const wchar_t *bbsname)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select domain bbsname order by datname");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;
	O2DatRec rec;
	long fetch;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select"
		COLUMNSA
		" from dat"
		" where domainname = ?"
		"   and bbsname = ?"
		" order by datname;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	XSQLDA tmpda;
	tmpda.version = SQLDA_VERSION1;
	tmpda.sqln = 1;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);

	if (!bind(inda, 0, domain))
		goto error;
	if (!bind(inda, 1, bbsname))
		goto error;
	if (isc_dsql_execute(status, &tr, &stmt, 3, inda))
		goto error;

	while ((fetch = isc_dsql_fetch(status, &stmt, 3, outda)) == 0) {
		get_columns(outda, rec);
		out.push_back(rec);
	}
	if (fetch != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (true);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (false);
}




bool
O2DatDB::
select(O2DatRecList &out, time_t publish_tt, size_t limit)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select lastpublish");
#endif
	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;
	O2DatRec rec;
	bool ret = false;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select first ? "
		COLUMNSA
		" from dat"
		" where lastpublish < ?"
		" order by lastpublish"
		"";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);

	if (!bind(inda, 0, limit))
		goto error;
	if (!bind(inda, 1, time(NULL)-publish_tt))
		goto error;

	if (isc_dsql_execute(status, &tr, &stmt, 3, inda))
		goto error;
	long st;
	while ((st = isc_dsql_fetch(status, &stmt, 3, outda)) == 0) {
		get_columns(outda, rec);
		out.push_back(rec);
	}
	if (st != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (true);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (false);
}




uint64
O2DatDB::
select_datcount(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount");
#endif

	isc_db_handle db = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	isc_tr_handle tr = NULL;
	XSQLDA *sqlda = NULL;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql = "select count(*) from dat;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	sqlda = prepare_xsqlda(stmt, false);

	if (isc_dsql_execute(status, &tr, &stmt, 1, NULL))
		goto error;
	if (isc_dsql_fetch(status, &stmt, 1, sqlda))
		goto error;
	uint64 count = *(long *)sqlda->sqlvar[0].sqldata;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);

	return (count);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);
	return (0);
}




uint64
O2DatDB::
select_datcount(wstrnummap &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount group by domain bbsname");
#endif

	isc_db_handle db = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	isc_tr_handle tr = NULL;
	wstring domain_bbsname, tmpstr;
	uint64 total = 0;
	long num;
	long fetch;
	XSQLDA *sqlda = NULL;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"select domainname, bbsname, count(*) from dat group by domainname, bbsname;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	sqlda = prepare_xsqlda(stmt, false);

	if (isc_dsql_execute(status, &tr, &stmt, 3, NULL))
		goto error;
	XSQLVAR *var;
	while ((fetch = isc_dsql_fetch(status, &stmt, 3, sqlda)) == 0) {
		var = sqlda->sqlvar;
		ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, tmpstr);
		domain_bbsname = tmpstr;
		domain_bbsname += L":";
		var++;
		ascii2unicode(var->sqldata+2, *(ISC_SHORT*)var->sqldata, tmpstr);
		domain_bbsname += tmpstr;
		var++;
		num = *(long*)var->sqldata;

		out.insert(wstrnummap::value_type(domain_bbsname, num));
		total += num;

	}
	if (fetch != 100L)
		goto error;

	if (isc_commit_transaction(status, &tr))
		goto error;
	free_xsqlda(sqlda);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	return (total);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);
	return (0);
}




uint64
O2DatDB::
select_totaldisksize(void)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select totakdisksize");
#endif
	isc_db_handle db = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	isc_tr_handle tr = NULL;
	XSQLDA *sqlda = NULL;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql = "select sum(disksize) from dat;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	sqlda = prepare_xsqlda(stmt, false);

	if (isc_dsql_execute(status, &tr, &stmt, 3, NULL))
		goto error;
	if (isc_dsql_fetch(status, &stmt, 1, sqlda))
		goto error;
	uint64 totalsize = *(long *)sqlda->sqlvar[0].sqldata;//over flow?

	if (isc_commit_transaction(status, &tr))
		goto error;
	free_xsqlda(sqlda);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	return (totalsize);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(sqlda);
	return (0);
}




uint64
O2DatDB::
select_publishcount(time_t publish_tt)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select datcount by lastpublish");
#endif
	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	XSQLDA *outda = NULL, *inda = NULL;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql = "select count(*) from dat where lastpublish > ?;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	outda = prepare_xsqlda(stmt, false);
	inda = prepare_xsqlda(stmt, true);
	if (!bind(inda, 0, time(NULL)-publish_tt))
		goto error;

	if (!isc_dsql_execute2(status, &tr, &stmt, 3, inda, outda))
		if (isc_sqlcode(status) == 100L)
			goto error;

	uint64 ret = *(long*)outda->sqlvar->sqldata;
	
	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (ret);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	free_xsqlda(outda);
	return (0);
}




#if 0
bool
O2DatDB::
update(O2DatRec &in, bool is_origurl)
{
}
#endif



void
O2DatDB::
update(O2DatRecList &in)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("update by reclist");
#endif
	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt_insert = NULL;
	isc_stmt_handle stmt_update = NULL;
	isc_stmt_handle stmt_updatepublish = NULL;
	XSQLDA *da_insert=NULL, *da_update=NULL, *da_updatepublish=NULL;
	O2DatRec org;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;

	char *sql_insert =
		"insert into dat ("
		COLUMNSA
		") values ("
		"?,?,?,?,?,?,?,?,?,?,?"
		");";
	if (isc_dsql_allocate_statement(status, &db, &stmt_insert))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt_insert, 0, sql_insert, 3, NULL))
		goto error;

	char *sql_update =
		"update  dat"
		"   set filesize        = ?"
		"     , disksize    = ?"
		"     , url         = ?"
		"     , res	     = ?"
		"     , lastupdate  = ?"
//		"     , lastpublish = 0"
		" where hash = ?;";
	if (isc_dsql_allocate_statement(status, &db, &stmt_update))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt_update, 0, sql_update, 3, NULL))
		goto error;

	char *sql_updatepublish =
		"update dat"
		"   set lastpublish = ?"
		" where hash = ?;";
	if (isc_dsql_allocate_statement(status, &db, &stmt_updatepublish))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt_updatepublish, 0, sql_updatepublish, 3, NULL))
		goto error;

	for (O2DatRecListIt it = in.begin(); it != in.end(); it++) {
		if (!select(org, it->hash)) {
			da_insert = prepare_xsqlda(stmt_insert, true);
			if (!bind(da_insert, 0, it->hash))
				goto error;
			if (!bind(da_insert, 1, it->domain))
				goto error;
			if (!bind(da_insert, 2, it->bbsname))
				goto error;
			if (!bind(da_insert, 3, it->datname))
				goto error;
			if (!bind(da_insert, 4, it->size))
				goto error;
			if (!bind(da_insert, 5, it->disksize))
				goto error;
			if (!bind(da_insert, 6, it->url))
				goto error;
			if (!bind(da_insert, 7, it->title))
				goto error;
			if (!bind(da_insert, 8, it->res))
				goto error;
			if (!bind(da_insert, 9, time(NULL)))
				goto error;
			if (!bind(da_insert, 10, (uint64)0))
				goto error;
			if (isc_dsql_execute(status, &tr, &stmt_insert, 3, da_insert))
				;//goto error;
			free_xsqlda(da_insert);
		} else if (it->userdata = 0) {
			da_update = prepare_xsqlda(stmt_update, true);
			if (!bind(da_update, 0, it->size))
				goto error;
			if (!bind(da_update, 1, it->disksize))
				goto error;
			if (!bind(da_update, 2, (wcsstr(org.url.c_str(), L"xxx") == 0 ? it->url : org.url)))
				goto error;
			if (!bind(da_update, 3, it->res))
				goto error;
			if (!bind(da_update, 4, time(NULL)))
				goto error;
			if (!bind(da_update, 5, it->hash))
				goto error;
//			if (!bind(da_update, 5, (uint64)0))
//				goto error;

			if (isc_dsql_execute(status, &tr, &stmt_update, 3, da_update))
				goto error;
			free_xsqlda(da_update);
		} else {
			da_updatepublish = prepare_xsqlda(stmt_updatepublish, true);
			if (!bind(da_updatepublish, 0, time(NULL)))
				goto error;
			if (!bind(da_updatepublish, 1, it->hash))
				goto error;
			if (isc_dsql_execute(status, &tr, &stmt_updatepublish, 3, da_updatepublish))
				goto error;
			free_xsqlda(da_updatepublish);
		}
	}

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt_insert) isc_dsql_free_statement(status, &stmt_insert, DSQL_drop);
	if (stmt_update) isc_dsql_free_statement(status, &stmt_update, DSQL_drop);
	if (stmt_updatepublish) isc_dsql_free_statement(status, &stmt_updatepublish, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	return ;

error:
 	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt_insert) isc_dsql_free_statement(status, &stmt_insert, DSQL_drop);
	if (stmt_update) isc_dsql_free_statement(status, &stmt_update, DSQL_drop);
	if (stmt_updatepublish) isc_dsql_free_statement(status, &stmt_updatepublish, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(da_insert);
	free_xsqlda(da_update);
	free_xsqlda(da_updatepublish);
	return;
}




#if 0
bool
O2DatDB::
update_lastpublish(const hashT &hash)
{

}
#endif




bool
O2DatDB::
remove(const hashT &hash)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("remove");
#endif

	isc_db_handle db = NULL;
	isc_tr_handle tr = NULL;
	ISC_STATUS_ARRAY status;
	isc_stmt_handle stmt = NULL;
	bool ret = false;

	if (isc_attach_database(status, 0, dbfilenameA.c_str(), &db, dpblen, dpb_buff))
		goto error;

	char *sql =
		"delete from dat where hash = ?;";

	if (isc_dsql_allocate_statement(status, &db, &stmt))
		goto error;
	if (isc_start_transaction(status, &tr, 1, &db, 0, NULL))
		goto error;
	if (isc_dsql_prepare(status, &tr, &stmt, 0, sql, 3, NULL))
		goto error;
	XSQLDA* inda = prepare_xsqlda(stmt, true);

	if (bind(inda, 0, hash))
		goto error;

	if (isc_dsql_execute(status, &tr, &stmt, 3, NULL)) {
		ret = true;
	} else
		ret = false;

	if (isc_commit_transaction(status, &tr))
		goto error;
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	return (ret);

error:
	log(status);
	if (tr) isc_rollback_transaction(status, &tr);
	if (stmt) isc_dsql_free_statement(status, &stmt, DSQL_drop);
	if (db) isc_detach_database(status, &db);
	free_xsqlda(inda);
	return (false);	
}
#else

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




size_t
O2DatDB::
select(const wchar_t *sql, SQLResultList &out)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("select");
#endif
	wstrarray cols;

	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	sqlite3_stmt *stmt = NULL;
	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_reset(stmt);

	err = sqlite3_step(stmt);
	if (err != SQLITE_ROW && err != SQLITE_DONE)
		goto error;

	if (out.empty()) {
		//àÍçsñ⁄
		cols.clear();
		get_column_names(stmt, cols);
		out.push_back(cols);
		cols.clear();
		get_columns(stmt, cols);
		out.push_back(cols);
	}
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		//2çsñ⁄à»ç~
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
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" order by random() limit 1;";

	sqlite3_stmt *stmt = NULL;
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
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"select"
		COLUMNS
		L" from dat"
		L" where hash = ?;";

	sqlite3_stmt *stmt = NULL;
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

	sqlite3_stmt *stmt = NULL;
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

	sqlite3_stmt *stmt = NULL;
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

	wchar_t *sql = L"select count(*) from dat;";

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

	wchar_t *sql = L"select sum(disksize) from dat;";

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




#if 0
bool
O2DatDB::
update(O2DatRec &in, bool is_origurl)
{
	sqlite3 *db = NULL;
	sqlite3_stmt* stmt;
	O2DatRec org;

	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	if (select(org, in.hash)) {
		wchar_t *sql =
			L"update or replace dat"
			L"   set size        = ?"
			L"     , disksize    = ?"
			L"     , url         = ?"
			L"     , res	     = ?"
			L"     , lastupdate  = ?"
			L"     , lastpublish = 0"
			L" where hash = ?;";

		err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
		if (err != SQLITE_OK)
			goto error;

		if (!is_origurl)
			in.url = org.url;
		if (!bind(db, stmt, 1, in.size))
			goto error;
		if (!bind(db, stmt, 2, in.disksize))
			goto error;
		if (!bind(db, stmt, 3, in.url))
			goto error;
		if (!bind(db, stmt, 4, in.res))
			goto error;
		if (!bind(db, stmt, 5, time(NULL)))
			goto error;
		if (!bind(db, stmt, 6, in.hash))
			goto error;

		err = sqlite3_step(stmt);
		if (err != SQLITE_ROW && err != SQLITE_DONE)
			goto error;
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	else {
		wchar_t *sql =
			L"insert or replace into dat ("
			COLUMNS
			L") values ("
			L"?,?,?,?,?,?,?,?,?,?,?"
			L");";

		err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
		if (err != SQLITE_OK)
			goto error;

		if (!bind(db, stmt, 1, in.hash))
			goto error;
		if (!bind(db, stmt, 2, in.domain))
			goto error;
		if (!bind(db, stmt, 3, in.bbsname))
			goto error;
		if (!bind(db, stmt, 4, in.datname))
			goto error;
		if (!bind(db, stmt, 5, in.size))
			goto error;
		if (!bind(db, stmt, 6, in.disksize))
			goto error;
		if (!bind(db, stmt, 7, in.url))
			goto error;
		if (!bind(db, stmt, 8, in.title))
			goto error;
		if (!bind(db, stmt, 9, in.res))
			goto error;
		if (!bind(db, stmt, 10, time(NULL)))
			goto error;
		if (!bind(db, stmt, 11, (uint64)0))
			goto error;

		err = sqlite3_step(stmt);
		if (err != SQLITE_ROW && err != SQLITE_DONE)
			goto error;
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

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
#endif



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
//			if (!bind(db, stmt_update, 6, (uint64)0))
//				goto error;

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




#if 0
bool
O2DatDB::
update_lastpublish(const hashT &hash)
{
	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"update or replace dat"
		L"   set lastpublish = ?"
		L" where hash = ?;";

	sqlite3_stmt *stmt = NULL;
	err = sqlite3_prepare16_v2(db, sql, wcslen(sql)*2, &stmt, NULL);
	if (err != SQLITE_OK)
		goto error;

	if (!bind(db, stmt, 1, time(NULL)))
		goto error;
	if (!bind(db, stmt, 2, hash))
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
#endif




bool
O2DatDB::
remove(const hashT &hash)
{
#if TRACE_SQL_EXEC_TIME
	stopwatch sw("remove");
#endif

	sqlite3 *db = NULL;
	int err = sqlite3_open16(dbfilename.c_str(), &db);
	if (err != SQLITE_OK)
		goto error;
	sqlite3_busy_timeout(db, 5000);

	wchar_t *sql =
		L"delete from dat where hash = ?;";

	sqlite3_stmt *stmt = NULL;
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
#endif




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
	if (UpdateThreadHandle)
		return;

	UpdateThreadLoop = true;
	UpdateThreadHandle = (HANDLE)_beginthreadex(
		NULL, 0, StaticUpdateThread, (void*)this, 0, NULL);
}
void
O2DatDB::
StopUpdateThread(void)
{
	if (!UpdateThreadHandle)
		return;
	UpdateThreadLoop = false;
	WaitForSingleObject(UpdateThreadHandle, INFINITE);
	CloseHandle(UpdateThreadHandle);

	UpdateQueueLock.Lock();
	if (!UpdateQueue.empty()) {
		update(UpdateQueue);
		UpdateQueue.clear();
	}
	UpdateQueueLock.Unlock();
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
		Sleep(3000);
	}
}
