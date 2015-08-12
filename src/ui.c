#include <iup.h>
#include <sqlite3.h>
#include <string.h>
#include <assert.h>
#include "yasf.h"
#include "dmodel.h"

struct ui_update_tree_ctx {
	Ihandle *tree;
	int parentid, id;
};

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

static
int sqlcb_ui_update_tree(void *data, sqlite3_stmt *stmt)
{
	struct ui_update_tree_ctx *ctx = (struct ui_update_tree_ctx *) data;
	const char *dbname;

	dbname = (const char *) sqlite3_column_text(stmt, 1);
	if (strcmp(dbname, "temp") == 0)
		return 0; /* don't handle 'temp' now */
	IupSetStrAttributeId(ctx->tree, ctx->id ? "INSERTBRANCH" : "ADDBRANCH",
		ctx->parentid, dbname);
	ctx->parentid = ++ctx->id;
	ctx->id = add_detail(ctx->tree, ctx->id, dbname);
	return 0;
}

void ui_update_tree(Ihandle *tree)
{
	struct ui_update_tree_ctx ctx;
	int rc;
	sqlite3_stmt *stmt;

	/* first, clear all nodes in the tree ... */
	IupSetAttributeId(tree, "DELNODE", 0, "CHILDREN");

	/* then, get all database info */
	rc = db_prepare("pragma database_list;", &stmt);
	if (rc != SQLITE_OK) return;

	ctx.tree = tree;
	ctx.parentid = ctx.id = 0;
	rc = db_exec_stmt(sqlcb_ui_update_tree, (void *) &ctx, stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) return;
	/* Sometimes, the pragma does not report there is a 'temp' database
	 * (maybe because there are not any temp tables).
	 * We'd better add it manually. */
	IupSetAttributeId(tree, "INSERTBRANCH", ctx.parentid, "temp");
	add_detail(tree, ++ctx.id, "temp");
}


struct setcol_ctx {
	Ihandle *matrix;
	struct dmodel *dmodel;
	int rowid_id[3];
};

static const char *const rowid_ids[3] = {"rowid", "oid", "_rowid_"};

static
void sqlcb_setcol(void *data, const char *colname, int ispk)
{
	struct setcol_ctx *ctx = (struct setcol_ctx *) data;
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
int sqlcb_addpk(void *data, sqlite3_stmt *stmt)
{
	struct dmodel *d = (struct dmodel *) data;
	sqlite3_value **row;
	int i;

	row = dmodel_add_entry(d);
	if (!row) return 1;
	assert(d->npkcol == sqlite3_column_count(stmt));
	for (i = 0; i < d->npkcol; i++) {
		row[i] = sqlite3_value_dup(sqlite3_column_value(stmt, i));
	}
	return 0;
}

static
void add_additional_line(Ihandle *matrix)
{
	int nrow;

	/* An additional line is appended to the end of the matrix,
	 * so that the user can easily insert a new record into the
	 * table. */
	nrow = IupGetInt(matrix, "NUMLIN");
	IupSetInt(matrix, "ADDLIN", ++nrow);
	IupSetAttributeId2(matrix, "", nrow, 0, "*");
	//IupSetAttribute(matrix, "FITTOTEXT", "C0");
}

void ui_begin_edit(Ihandle *matrix, const char *dbname, const char *name)
{
	int rc, k;
	char *qualified_name = 0;
	struct setcol_ctx ctx = {0};
	struct dmodel *dmodel;
	sqlite3_stmt *stmt;

	qualified_name = sqlite3_mprintf("\"%w\".\"%w\"", dbname, name);
	if (!qualified_name) return;

	ctx.matrix = matrix;
	ctx.dmodel = dmodel = dmodel_new(qualified_name);
	if (!dmodel) goto fail;
	sqlite3_free(qualified_name);
	qualified_name = dmodel->qualified_name;

	/* get all column names */
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

	/* get values in PK columns */
	rc = db_prepare(dmodel->select_sql, &stmt);
	if (rc != SQLITE_OK) goto fail;
	rc = db_exec_stmt(sqlcb_addpk, (void *) dmodel, stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_OK) goto fail;

	/* show all values in the matrix */
	rc = db_exec_args(sqlcb_mat, matrix,
		"select * from %s order by rowid asc;", qualified_name);
	if (rc != SQLITE_OK) goto fail;
	IupSetAttribute(matrix, "dmodel", (char *) dmodel);
	add_additional_line(matrix);
	return;
fail:
	dmodel_free(dmodel);
}

void ui_end_edit(Ihandle *matrix)
{
	struct dmodel *dmodel;

	IupSetInt(matrix, "NUMCOL", 0);
	IupSetInt(matrix, "NUMLIN", 0);

	dmodel = (struct dmodel *) IupGetAttribute(matrix, "dmodel");
	IupSetAttribute(matrix, "dmodel", 0);
	dmodel_free(dmodel);
}
