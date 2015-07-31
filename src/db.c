#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "yasf.h"

struct {
	sqlite3 *db; /* current db connection */
	char *dbname, *type, *name; /* active view info */
	/* if active view is a table, we have to know its primary key.
	 * pk == 0: PK is the implicit rowid;
	 * pk >  0: PK is the pk'th column (starting from 1);
	 * pk <  0: PK is not applicable (i.e. active view is not a table).
	 */
	int pk;
	/* whether the active view is editable */
	int editing;
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

static
int add_detail(Ihandle *tree, int id, const char *dbname)
{
	static const char * const objtypes[] = {
		"table", "index", "view", "trigger", 0
	};
	int i, parentid, rc;
	sqlite3_stmt *stmt;
	char *zSql;

	zSql = sqlite3_mprintf("select name from \"%w\".sqlite_master where type = ?;", dbname);
	rc = sqlite3_prepare_v2(glst->db, zSql, -1, &stmt, 0);
	report(rc, 1);
	sqlite3_free(zSql);

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
	IupSetAttribute(tree, "DELNODE0", "CHILDREN");

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

void db_disable_edit(void)
{
	free(glst->dbname);
	free(glst->type);
	free(glst->name);
	glst->dbname = glst->type = glst->name = 0;
	glst->pk = -1;
	glst->editing = 0;
}

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

void db_enable_edit(const char *dbname, const char *type, const char *name)
{
	if (strcmp(type, "table") == 0) {
		assert(name);
		glst->pk = get_pk_cid(dbname, name);
		/* TODO do not rely on global identifer here? */
		db_exec_args(sqlcb_mat, IupGetHandle("ctl_matrix"),
			"select %s from \"%w\".\"%w\";",
			(glst->pk) ? "*" : "rowid, *",
			dbname,
			name
		);
	} else {
		IupMessagef("Sorry...", "Editing %s %s is not implemented.",
			(type[0] == 'i') ? "an" : "a", type);
		return;
	}
	glst->dbname = strdup(dbname);
	glst->type = strdup(type);
	glst->name = strdup(name);
	glst->editing = 1;
}

int cb_matrix_edit(Ihandle *ih, int lin, int col, int mode, int update)
{
	/* TODO cannot edit yet, since the primary key is not tracked. */
#if 0
	if (mode == 1) { /* enter */
		return (glst->editing) ? IUP_CONTINUE : IUP_IGNORE;
	} else if (update) { /* leave */
		Ihandle *ih;
		const char *colname, *newvalue;
		const char *pkcol, *pkval;
		int rc;

		assert(glst->name && glst->type && strcmp(glst->type, "table") == 0);
		ih = IupGetHandle("ctl_matrix");
		colname = IupGetAttributeId2(ih, "", 0, col);
		newvalue = IupGetAttribute(ih, "VALUE");
		pkcol = (glst->pk) ? IupGetAttributeId2(ih, "", 0, glst->pk) : "rowid";
		pkval = IupGetAttributeId2(ih, "", lin, glst->pk);
		/* XXX pkval might be a huge blob or long string (if table is
		 * without rowid and use blob/string as the primary key).
		 * Normally nobody would do this. */
		/* XXX Values might be a huge blob or long string.
		 * This is very common, so it's better to use sqlite3_bind_*
		 * rather than string-based method. */
		rc = db_exec_args(ih, "update \"%w\".\"%w\" set \"%w\" = %Q where \"%w\" = %Q;",
			glst->dbname,
			glst->name,
			colname,
			newvalue,
			pkcol,
			pkval
		);
		if (rc == SQLITE_OK) {
			return IUP_DEFAULT;
		} else {
			return IUP_IGNORE;
		}
	}
	return IUP_CONTINUE;
#else
	return IUP_IGNORE;
#endif
}
