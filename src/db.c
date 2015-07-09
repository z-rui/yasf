#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "yasf.h"

struct {
	sqlite3 *db; /* current db connection */
	const char *dbname, *type, *name; /* active view info */
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

	zSql = sqlite3_mprintf("select name from %Q.sqlite_master where type = ?;", dbname);
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

void update_treeview(void)
{
	int rc, id, parentid;
	Ihandle *tree;
	sqlite3_stmt *stmt;

	tree = IupGetHandle("ctl_tree");

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
	update_treeview();
}

void db_finalize(void)
{
	int rc;
	
	rc = sqlite3_close(glst->db);
	report(rc, 1);
}

#include "pragmas.c"

int exec_stmt(sqlite3_stmt *stmt)
{
	int rc;
	Ihandle *matrix;
	int rows = 0, cols = 0;
	int i;
	int implicit_rowid;

	implicit_rowid = !glst->pk;
	matrix = IupGetHandle("ctl_matrix");
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *s;

		if (rows == 0) {
			IupSetAttribute(matrix, "CLEARVALUE", "ALL");
			IupSetInt(matrix, "NUMLIN", 0);
			IupSetInt(matrix, "NUMCOL", 0);

			/* cols is the actual number of columns, excluding
			 * the implicit rowid */
			cols = sqlite3_column_count(stmt) - implicit_rowid;
			assert(cols >= 1);
			IupSetInt(matrix, "NUMCOL", cols);
			for (i = 0; i < cols; i++) {
				s = sqlite3_column_name(stmt, i + implicit_rowid);
				IupSetStrAttributeId2(matrix, "", 0, i+1, s);
			}
		}
		IupSetInt(matrix, "NUMLIN", ++rows);
		for (i = -implicit_rowid; i < cols; i++) {
			if (sqlite3_column_type(stmt, i + implicit_rowid) == SQLITE_NULL) {
				s = "NULL";
				IupSetRGBId2(matrix, "FGCOLOR", rows, i+1, 192, 192, 192);
			} else {
				s = (const char *) sqlite3_column_text(stmt, i + implicit_rowid);
			}
			IupSetStrAttributeId2(matrix, "", rows, i+1, s);
			fprintf(stderr, "COLUMN! (%d)\n",
				sqlite3_column_type(stmt, i + implicit_rowid));
		}
		fprintf(stderr, "ROW!\n");
	}
	for (i = 0; i <= cols; i++) {
		IupSetStrf(matrix, "FITTOTEXT", "C%d", i);
	}
	//IupSetAttribute(matrix, "REDRAW", "ALL");
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "ERROR!\n"); // XXX
		report(rc, 0);
	}
	return rc;
}

int exec_stmt_str(const char *s)
{
	int rc = SQLITE_OK;
	sqlite3_stmt *stmt;
	const char *tail;

	assert(s);
	while (rc == SQLITE_OK && s[0]) {
		rc = sqlite3_prepare_v2(
			glst->db,	/* db */
			s,		/* zSql */
			-1,		/* nByte */
			&stmt,		/* ppStmt */
			&tail		/* pzTail XXX */
		);
		if (report(rc, 0)) continue;
		if (stmt) {
			exec_stmt(stmt);
			sqlite3_finalize(stmt);
		}
		s = tail;
	}
	return rc;
}

#include <stdarg.h>

int exec_stmt_args(const char *stmt, ...)
{
	char *zSql;
	va_list va;
	int rc;

	va_start(va, stmt);
	zSql = sqlite3_vmprintf(stmt, va);
	va_end(va);
	if (!zSql) return SQLITE_NOMEM;
	rc = exec_stmt_str(zSql);
	sqlite3_free(zSql);
	return rc;
}

void db_disable_edit(void)
{
	glst->dbname = glst->type = glst->name = 0;
	glst->pk = -1;
	glst->editing = 0;
}

static
int get_pk_cid(const char *dbname, const char *tablename)
{
	char *zSql;
	sqlite3_stmt *stmt;
	int rc, cid = 0;

	zSql = sqlite3_mprintf("pragma %Q.table_info(%Q);", dbname, tablename);
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
		exec_stmt_args((glst->pk)
				? "select * from %Q.%Q;"
				: "select rowid, * from %Q.%Q",
			dbname,
			name
		);
	} else {
		IupMessagef("Sorry...", "Editing %s %s is not implemented.",
			(type[0] == 'i') ? "an" : "a", type);
		return;
	}
	glst->dbname = dbname;
	glst->type = type;
	glst->name = name;
	glst->editing = 1;
}

int cb_matrix_edit(Ihandle *ih, int lin, int col, int mode, int update)
{
	if (mode == 1) { /* enter */
		return (glst->editing) ? IUP_CONTINUE : IUP_IGNORE;
	} else if (update) { /* leave */
		IupMessage("Sorry...", "Modifying data is not implemented.");
		return IUP_IGNORE;
	}
	return IUP_CONTINUE;
}
