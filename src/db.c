#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct {
	sqlite3 *db;
} glst[1];

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

int db_file(const char *filename)
{
	int rc;
	sqlite3 *db;

	rc = sqlite3_open(filename, &db);

	if (report(rc, 0)) return 0;
	
	if (glst->db) {
		rc = sqlite3_close(glst->db);
		report(rc, 1);
	}
	glst->db = db;
	update_treeview();
	return 1;
}

int db_attach(const char *filename, const char *dbname)
{
	int rc;
	char *zSql;

	zSql = sqlite3_mprintf("attach %Q as %Q;", filename, dbname);
	rc = sqlite3_exec(glst->db, zSql, 0, 0, 0);
	if (report(rc, 0)) return 0;
	update_treeview();
	return 1;
}

int db_detach(const char *dbname)
{
	int rc;
	char *zSql;

	zSql = sqlite3_mprintf("detach %Q;", dbname);
	rc = sqlite3_exec(glst->db, zSql, 0, 0, 0);
	if (report(rc, 0)) return 0;
	update_treeview();
	return 1;
}

void db_finalize(void)
{
	int rc;
	
	rc = sqlite3_close(glst->db);
	report(rc, 1);
}

#include "pragmas.c"

void exec_stmt(sqlite3_stmt *stmt)
{
	int rc;
	Ihandle *matrix;
	int rows = 0, cols = 0;
	int i;

	matrix = IupGetHandle("ctl_matrix");
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *s;

		if (rows == 0) {
			IupSetAttribute(matrix, "CLEARVALUE", "ALL");
			IupSetInt(matrix, "NUMLIN", 0);
			IupSetInt(matrix, "NUMCOL", 0);

			cols = sqlite3_column_count(stmt);
			IupSetInt(matrix, "NUMCOL", cols);
			for (i = 0; i < cols; i++) {
				s = sqlite3_column_name(stmt, i);
				IupSetStrAttributeId2(matrix, "", 0, i+1, s);
			}
		}
		IupSetInt(matrix, "NUMLIN", ++rows);
		for (i = 0; i < cols; i++) {
			if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
				s = "NULL";
				IupSetRGBId2(matrix, "FGCOLOR", rows, i+1, 192, 192, 192);
			} else {
				s = (const char *) sqlite3_column_text(stmt, i);
			}
			IupSetStrAttributeId2(matrix, "", rows, i+1, s);
			fprintf(stderr, "COLUMN! (%d)\n",
				sqlite3_column_type(stmt, i));
		}
		fprintf(stderr, "ROW!\n");
	}
	for (i = 0; i < cols; i++) {
		IupSetStrf(matrix, "FITTOTEXT", "C%d", i+1);
	}
	//IupSetAttribute(matrix, "REDRAW", "ALL");
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "ERROR!\n"); // XXX
		report(rc, 0);
	}
}

void exec_stmt_str(const char *s)
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
}

