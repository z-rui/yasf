#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "yasf.h"

struct {
	sqlite3 *db; /* current db connection */
	char *dbname, *tablename; /* info of the table being edited */
	/* To handle WITHOUT ROWID tables, the name of the primary key
	 * needs to be stored. 
	 * TODO Support of WITHOUT ROWID tables is not implemented yet. */
	/* char *pkname; */
} glst[1]; /* global state */

static int report(int rc, int fatal)
{
	if (rc != SQLITE_OK) {
		IupMessagef("SQLite Error", "Error code: %d\nError: %s.\nMessage: %s.\n",
			rc, sqlite3_errstr(rc), sqlite3_errmsg(glst->db));
		if (fatal) exit(1);
		return 1;
	}
	return 0;
}

void db_init(void)
{
	/* XXX do nothing now */
}

int db_prepare(const char *zSql, sqlite3_stmt **ppStmt)
{
	int rc;

	rc = sqlite3_prepare_v2(glst->db, zSql, -1, ppStmt, 0);
	report(rc, 0);
	return rc;
}

static
int add_detail(Ihandle *tree, int id, const char *dbname)
{
	static const char * const objtypes[] = {
		"table", "index", "view", "trigger", 0
	};
	int i, parentid, rc;
	sqlite3_stmt *stmt;

	if (strcmp(dbname, "temp") != 0) {
		char *zSql;

		zSql = sqlite3_mprintf("select name from \"%w\".sqlite_master where type = ?;", dbname);
		db_prepare(zSql, &stmt);
		sqlite3_free(zSql);
	} else {
		db_prepare("select name from sqlite_temp_master where type = ?;", &stmt);
	}

	parentid = id;
	for (i = 0; objtypes[i]; i++) {
		/* Note: objtypes[i] is static variable, so we can use
		 * IupSetAttribute here. */
		IupSetAttributeId(tree, i ? "INSERTBRANCH" : "ADDBRANCH", parentid, objtypes[i]);
		parentid = ++id;
		sqlite3_bind_text(stmt, 1, objtypes[i], -1, SQLITE_STATIC);
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			const char *name;

			name = (const char *) sqlite3_column_text(stmt, 0);
			IupSetStrAttributeId(tree, "ADDLEAF", id++, name);
		}
		sqlite3_reset(stmt);
	}
	sqlite3_finalize(stmt);
	return id;
}

void update_treeview(Ihandle *tree)
{
	int rc, id, parentid;
	sqlite3_stmt *stmt;

	/* first, clear all nodes in the tree ... */
	IupSetAttributeId(tree, "DELNODE", 0, "CHILDREN");

	/* then, get all database info */
	rc = sqlite3_prepare_v2(glst->db, "pragma database_list;", -1, &stmt, 0);
	report(rc, 1);
	parentid = id = 0;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *dbname;
		
		dbname = (const char *) sqlite3_column_text(stmt, 1);
		if (strcmp(dbname, "temp") == 0)
			continue; /* don't handle 'temp' now */
		IupSetStrAttributeId(tree, id ? "INSERTBRANCH" : "ADDBRANCH", parentid, dbname);
		parentid = ++id;
		id = add_detail(tree, id, dbname);
	}
	if (rc != SQLITE_DONE)
		report(rc, 0);
	sqlite3_finalize(stmt);
	/* Sometimes, the pragma does not report there is a 'temp' database
	 * (maybe because there are not any temp tables).
	 * We'd better add it manually. */
	IupSetAttributeId(tree, "INSERTBRANCH", parentid, "temp");
	add_detail(tree, ++id, "temp");
}

void db_file(const char *filename)
{
	int rc;
	sqlite3 *db;

	rc = sqlite3_open(filename, &db);

	if (report(rc, 0)) return;
	
	if (glst->db) {
		rc = sqlite3_close(glst->db);
		report(rc, 1);
	}
	glst->db = db;
}

void db_finalize(void)
{
	int rc;
	
	rc = sqlite3_close(glst->db);
	report(rc, 1);
}

#include "pragmas.c"

#include <stdarg.h>

int db_exec_str(const char *sql, int (*callback)(void *, int, char **, char **), void *data)
{
	char *errmsg = 0;
	int rc;

	rc = sqlite3_exec(glst->db, sql, callback, data, &errmsg);
	if (rc != SQLITE_OK) {
		report(rc, 0); /* XXX cannot make use of errmsg for compatibility with report() */
		sqlite3_free(errmsg);
	}
	return rc;
}

int db_exec_args(int (*callback)(void *, int, char **, char **), void *data, const char *sql, ...)
{
	char *zSql;
	va_list va;
	int rc;

	va_start(va, sql);
	zSql = sqlite3_vmprintf(sql, va);
	va_end(va);
	if (!zSql)
		return SQLITE_NOMEM;
	rc = db_exec_str(zSql, callback, data);
	sqlite3_free(zSql);
	return rc;
}

#if 0
int get_pk_cid(const char *dbname, const char *tablename)
{
	char *zSql;
	sqlite3_stmt *stmt;
	int rc, cid = 0;

	zSql = sqlite3_mprintf("pragma \"%w\".table_info(\"%w\");", dbname, tablename);
	rc = sqlite3_prepare_v2(glst->db, zSql, -1, &stmt, 0);
	report(rc, 1);
	sqlite3_free(zSql);
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		int pk;

		pk = sqlite3_column_int(stmt, 5);
		if (pk) {
			cid = sqlite3_column_int(stmt, 0) + 1; /* explicit PK */
			break;
		}
	}
	if (rc != SQLITE_DONE && rc != SQLITE_ROW) report(rc, 1);
	rc = sqlite3_finalize(stmt);
	report(rc, 1);
	return cid;	/* implicit PK (rowid) */
}
#endif

static
int rowcount(const char *dbname, const char *name)
{
	char *zSql;
	sqlite3_stmt *stmt;
	int nrow = 0;
	int rc;

	zSql = sqlite3_mprintf("select count(*) from \"%w\".\"%w\";",
		dbname, name);
	db_prepare(zSql, &stmt);
	sqlite3_free(zSql);
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		nrow = sqlite3_column_int(stmt, 0);
	} else {
		report(rc, 0);
	}
	sqlite3_finalize(stmt);
	return nrow;
}

static
void fillpkslot(sqlite3_int64 *pkslot, int nrow, const char *dbname, const char *name)
{
	char *zSql;
	sqlite3_stmt *stmt;
	int n = 0;
	int rc;

	zSql = sqlite3_mprintf("select rowid from \"%w\".\"%w\" order by rowid asc;",
		dbname, name);
	db_prepare(zSql, &stmt);
	sqlite3_free(zSql);
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		assert(n < nrow);
		pkslot[n++] = sqlite3_column_int64(stmt, 0);
	}
	if (rc != SQLITE_DONE) {
		report(rc, 0);
	}
	sqlite3_finalize(stmt);
	assert(n == nrow);
	pkslot[n] = 0;
}

void db_begin_edit(Ihandle *matrix, const char *dbname, const char *name)
{
	int nrow;
	int rc;
	sqlite3_int64 *pkslot;

	/* Only tables can be editted. */
	/* TODO WITHOUT ROWID tables cannot be editted */
	nrow = rowcount(dbname, name);
	pkslot = (sqlite3_int64 *) malloc(sizeof (pkslot[0]) * (nrow + 1));
	if (!pkslot) {
		IupMessage("Error", "Out of memory");
		return;
	}
	fillpkslot(pkslot, nrow, dbname, name);
	rc = db_exec_args(sqlcb_mat, matrix, "select * from \"%w\".\"%w\" order by rowid asc;", dbname, name);
	IupSetStrAttribute(matrix, "dbname", dbname);
	IupSetStrAttribute(matrix, "name", name);
	IupSetAttribute(matrix, "pkslot", (char *) pkslot);
}

void db_end_edit(Ihandle *matrix)
{
	sqlite3_int64 *pkslot;

	IupSetAttribute(matrix, "dbname", 0);
	IupSetAttribute(matrix, "name", 0);
	IupSetInt(matrix, "NUMCOL", 0);
	IupSetInt(matrix, "NUMLIN", 0);
	pkslot = (sqlite3_int64 *) IupGetAttribute(matrix, "pkslot");
	IupSetAttribute(matrix, "pkslot", 0);
	free(pkslot);
}

int cb_matrix_edit(Ihandle *ih, int lin, int col, int mode, int update)
{
	sqlite_int64 *pkslot;

	pkslot = (sqlite3_int64 *) IupGetAttribute(ih, "pkslot");
#if 1
	if (mode == 1) { /* enter */
		/* if pkslot is set, then it is in editing mode. */
		return (pkslot) ? IUP_CONTINUE : IUP_IGNORE;
	} else if (update) { /* leave */
		const char *dbname, *name, *colname, *newvalue;
		int rc;

		dbname = IupGetAttribute(ih, "dbname");
		name = IupGetAttribute(ih, "name");
		colname = IupGetAttributeId2(ih, "", 0, col);
		newvalue = IupGetAttribute(ih, "VALUE");
		/* TODO WITHOUT ROWID tables are not supported yet. */
		rc = db_exec_args(0, 0, "update \"%w\".\"%w\" set \"%w\" = %Q where rowid = %d;",
			dbname, name, colname, newvalue, pkslot[lin-1]
		);
		return (rc == SQLITE_OK) ? IUP_DEFAULT : IUP_IGNORE;
	}
	return IUP_CONTINUE;
#else
	return IUP_IGNORE;
#endif
}

int db_schema_version(void)
{
	int ver = 0; /* XXX should be an 32-bit integer */
	sqlite3_stmt *stmt;
	int rc;

	sqlite3_prepare_v2(glst->db, "pragma schema_version;", -1, &stmt, 0);
	if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		ver = sqlite3_column_int(stmt, 0);
	} else {
		report(rc, 0);
	}
	sqlite3_finalize(stmt);
	return ver;
}
