#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "yasf.h"

struct {
	sqlite3 *db; /* current db connection */
} glst[1]; /* global state */

static int report(int rc, int fatal)
{
	if (rc != SQLITE_OK) {
		Ihandle *dlg;
		char *buf, *p;

		dlg = IupMessageDlg();
		p = buf = bufnew(BUFSIZ);
		p = bufcat(&buf, p, sqlite3_errmsg(glst->db));
		p = bufcat(&buf, p, "\nError code: ");
		p = bufext(&buf, p, 11);
		p += sprintf(p, "%d", rc);
		p = bufcat(&buf, p, "\nError description: ");
		p = bufcat(&buf, p, sqlite3_errstr(rc));
		if (p) {
			IupSetAttribute(dlg, "DIALOGTYPE", "ERROR");
			IupSetAttribute(dlg, "TITLE", "SQLite3 Error");
			IupSetAttribute(dlg, "VALUE", buf);
			IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
		}
		bufdel(buf);
		IupDestroy(dlg);
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

int db_exec_stmt(int (*callback)(void *, sqlite3_stmt *), void *data, sqlite3_stmt *stmt)
{
	int rc;

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (callback && (*callback)(data, stmt)) {
			rc = SQLITE_ABORT;
			break;
		}
	}
	if (rc == SQLITE_DONE)
		return SQLITE_OK;
	report(rc, 0);
	return rc;
}

void db_column_names(void (*cb)(void *, const char *, int), void *data, const char *dbname, const char *name)
{
	sqlite3_stmt *stmt;
	int rc;
	char *zSql;

	zSql = sqlite3_mprintf("pragma \"%w\".table_info(\"%w\");", dbname, name);
	rc = db_prepare(zSql, &stmt);
	sqlite3_free(zSql);
	if (rc != SQLITE_OK) return;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		(*cb)(data, (const char *) sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 5));
	}
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) {
		report(rc, 0);
	}
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

sqlite3_int64 db_last_insert_rowid(void)
{
	return sqlite3_last_insert_rowid(glst->db);
}
