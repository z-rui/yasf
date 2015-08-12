#include <iup.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "yasf.h"
#include "dmodel.h"

struct {
	sqlite3 *db; /* current db connection */
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

struct setcol_context {
	Ihandle *matrix;
	struct dmodel *dmodel;
	int rowid_id[3];
};

static const char *const rowid_ids[3] = {"rowid", "oid", "_rowid_"};

static
void sqlcb_setcol(void *data, const char *colname, int ispk)
{
	struct setcol_context *ctx = (struct setcol_context *) data;
	int ncol, i;

	ncol = IupGetInt(ctx->matrix, "NUMCOL");
	IupSetInt(ctx->matrix, "NUMCOL", ++ncol);
	IupSetStrAttributeId2(ctx->matrix, "", 0, ncol, colname);
	if (ispk) {
		dmodel_add_pkcol(ctx->dmodel, colname);
	}
	for (i = 0; i < 3; i++) {
		/* this comparison should be case-insensitive */
		if (sqlite3_stricmp(colname, rowid_ids[i]) == 0) {
			ctx->rowid_id[i] = 1;
		}
	}
}

static
int add_pk(struct dmodel *dmodel)
{
	sqlite3_stmt *stmt;
	int rc;

	rc = db_prepare(dmodel->select_sql, &stmt);
	if (rc != SQLITE_OK) goto fail;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		sqlite3_value **row;
		int i;

		row = dmodel_add_entry(dmodel);
		for (i = 0; i < dmodel->npkcol; i++) {
			row[i] = sqlite3_value_dup(sqlite3_column_value(stmt, i));
		}
	}
	if (rc != SQLITE_DONE) goto fail;
	return 1;
fail:
	return 0;
}

void db_begin_edit(Ihandle *matrix, const char *dbname, const char *name)
{
	int rc, k;
	char *qualified_name = 0;
	struct setcol_context ctx = {0};
	struct dmodel *dmodel;

	ctx.matrix = matrix;
	qualified_name = sqlite3_mprintf("\"%w\".\"%w\"", dbname, name);
	if (!qualified_name) return;
	ctx.dmodel = dmodel = dmodel_new(qualified_name);
	sqlite3_free(qualified_name);
	qualified_name = dmodel->qualified_name;
	if (!dmodel) goto fail;

	db_column_names(sqlcb_setcol, (void *) &ctx, dbname, name);

	if (dmodel->npkcol == 0) {	/* the primary key is the implicit ROWID. */
		/* find an appropriate name to refer to the implicit ROWID */
		for (k = 0; k < 3 && ctx.rowid_id[k]; k++)
			;
		if (k == 3) {
			IupMessage("Error",
				"All of the column names ROWID, OID and _ROWID_"
				" has been defined by the user, probably on a "
				"bad purpose. \nThe program cannot manipulate "
				"this kind of table.");
			goto fail;
		}
		/* rowid_ids[k] is a static variable */
		IupSetAttribute(matrix, "rowid_id", rowid_ids[k]);
		dmodel_add_pkcol(dmodel, rowid_ids[k]);
	} else {	/* the primary key is explicitly defined. */
		IupSetAttribute(matrix, "rowid_id", 0);
	}
	dmodel_init_sql(dmodel);
	if (!add_pk(dmodel))
		goto fail;

	rc = db_exec_args(sqlcb_mat, matrix,
		"select * from %s order by rowid asc;", qualified_name);
	if (rc == SQLITE_OK) {
		/* do not make a copy of qualified_name, so remember to free
		 * the memory later */
		int nrow, i;

		IupSetAttribute(matrix, "dmodel", (char *) dmodel);

		/* An additional line is appended to the end of the matrix,
		 * so that the user can easily insert a new record into the
		 * table. */
		nrow = IupGetInt(matrix, "NUMLIN");
		IupSetInt(matrix, "ADDLIN", ++nrow);

		/* Set line number */
		for (i = 1; i < nrow; i++) {
			IupSetIntId2(matrix, "", i, 0, i);
		}
		IupSetAttributeId2(matrix, "", nrow, 0, "*");
		//IupSetAttribute(matrix, "FITTOTEXT", "C0");
	} else {
fail:
		dmodel_free(dmodel);
	}
}

void db_end_edit(Ihandle *matrix)
{
	struct dmodel *dmodel;

	IupSetInt(matrix, "NUMCOL", 0);
	IupSetInt(matrix, "NUMLIN", 0);

	dmodel = (struct dmodel *) IupGetAttribute(matrix, "dmodel");
	IupSetAttribute(matrix, "dmodel", 0);
	dmodel_free(dmodel);
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
