#include <iup.h>
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yasf.h"

static
void sqlcb_columnlist(void *data, const char *col, int ispk)
{
	Ihandle *tablelist = (Ihandle *) data;

	IupSetStrAttribute(tablelist, "APPENDITEM", col);
}

static
void clear_columns(Ihandle *ih)
{
	Ihandle *llist, *rlist;

	llist = IupGetDialogChild(ih, "llist");
	rlist = IupGetDialogChild(ih, "rlist");
	IupSetAttribute(llist, "REMOVEITEM", "ALL");
	IupSetAttribute(rlist, "REMOVEITEM", "ALL");
}

int cb_update_columns(Ihandle *ih, char *text, int item, int state)
{
	Ihandle *dblist, *llist;
	const char *dbname;

	dblist = IupGetDialogChild(ih, "dblist");
	dbname = IupGetAttribute(dblist, "VALUESTRING");
	llist = IupGetDialogChild(ih, "llist");
	if (state == 1) {
		clear_columns(ih);
		db_column_names(sqlcb_columnlist, (void *) llist, dbname, text);
	}
	return IUP_DEFAULT;
}

static
int sqlcb_tablelist(void *data, int cols, char **val, char **title)
{
	Ihandle *tablelist = (Ihandle *) data;

	assert(cols == 1);
	IupSetStrAttribute(tablelist, "APPENDITEM", val[0]);
	return 0;
}

int cb_update_tablelist(Ihandle *ih, char *text, int item, int state)
{
	Ihandle *tablelist;

	tablelist = IupGetDialogChild(ih, "tablelist");
	if (state == 1) {
		IupSetAttribute(tablelist, "REMOVEITEM", "ALL");
		/* "text" is database name */
		if (strcmp(text, "temp") == 0) {
			db_exec_str("select name from sqlite_temp_master where type='table';",
				sqlcb_tablelist, (void *) tablelist);
		} else {
			db_exec_args(sqlcb_tablelist, (void *) tablelist,
				"select name from \"%w\".sqlite_master where type='table';",
				text
			);
		}
		if (IupGetInt(tablelist, "COUNT") > 0) {
			IupSetInt(tablelist, "VALUE", 1);
			cb_update_columns(tablelist, IupGetAttribute(tablelist, "VALUESTRING"), 1, 1);
		} else {
			clear_columns(ih);
		}
	}
	return IUP_DEFAULT;
}

static
int sqlcb_dblist(void *data, int cols, char **val, char **title)
{
	Ihandle *tablelist = (Ihandle *) data;

	assert(cols == 3);
	if (strcmp(val[1], "temp") != 0)
		IupSetStrAttribute(tablelist, "APPENDITEM", val[1]);
	return 0;
}

static
void dlg_fitsize(Ihandle *dlg)
{
	IupSetAttribute(dlg, "SIZE", 0);
	IupRefresh(dlg);
	fprintf(stderr, "SIZE = %s, NATURALSIZE = %s\n",
		IupGetAttribute(dlg, "SIZE"),
		IupGetAttribute(dlg, "NATURALSIZE")
	);
}

static
void update_dblist(Ihandle *dblist)
{
	IupSetAttribute(dblist, "REMOVEITEM", "ALL");
	db_exec_str("pragma database_list;",
		sqlcb_dblist, (void *) dblist
	);
	IupSetAttribute(dblist, "APPENDITEM", "temp");
	IupSetInt(dblist, "VALUE", 1);
}

int cb_createindex_map(Ihandle *ih)
{
	Ihandle *dblist;

	dblist = IupGetDialogChild(ih, "dblist");
	update_dblist(dblist);
	cb_update_tablelist(dblist, IupGetAttribute(dblist, "VALUESTRING"), 1, 1);
	dlg_fitsize(ih);
	return IUP_DEFAULT;
}

int cb_createtable_map(Ihandle *ih)
{
	Ihandle *dblist;
	dblist = IupGetDialogChild(ih, "dblist");
	update_dblist(dblist);
	dlg_fitsize(ih);
	return IUP_DEFAULT;
}

static
const char *DialogChildAttribute(Ihandle *ih, const char *name, const char *attr)
{
	return IupGetAttribute(IupGetDialogChild(ih, name), attr);
}

static
int exec_and_free(char *buf, char *p)
{
	int rc = IUP_DEFAULT;
	int schema_version = db_schema_version();

	if (!p) {
		IupMessage("Error", "Out of memory");
	} else if (db_exec_str(buf, 0, 0) == SQLITE_OK) {
		rc = IUP_CLOSE;
	}
	bufdel(buf);
	/* update the tree view only when schema has changed */
	if (schema_version != db_schema_version())
		ui_update_tree(IupGetHandle("ctl_tree"));
	return rc;
}

int cb_createindex_ok(Ihandle *ih)
{
	Ihandle *rlist;
	const char *dbname, *indexname, *tablename, *column;
	char *buf, *p;
	int ncol, unique;
	int i;

	dbname = DialogChildAttribute(ih, "dblist", "VALUESTRING");
	indexname = DialogChildAttribute(ih, "name", "VALUE");
	tablename = DialogChildAttribute(ih, "tablelist", "VALUESTRING");

	assert(dbname && indexname);
	if (!tablename) {
		IupMessage("Error", "You have to specify the table name.");
		return IUP_DEFAULT;
	}

	unique = IupGetInt(IupGetDialogChild(ih, "unique"), "VALUE");

	p = buf = bufnew(BUFSIZ);
	p = bufcat2(&buf, p,
		(unique) ? "create unique index " : "create index ",
		dbname, ".", indexname, " on ", tablename, " (",
	0);

	rlist = IupGetDialogChild(ih, "rlist");
	ncol = IupGetInt(rlist, "COUNT");
	for (i = 1; i <= ncol; i++) {
		column = IupGetAttributeId(rlist, "", i);
		p = bufcat2(&buf, p, (i > 1) ? ", " : "", column, 0);
	}
	p = bufcat(&buf, p, ");");
	return exec_and_free(buf, p);
}

int cb_createtable_ok(Ihandle *ih)
{
	const char *dbname, *tablename, *schema;
	char *buf, *p;

	dbname = DialogChildAttribute(ih, "dblist", "VALUESTRING");
	tablename = DialogChildAttribute(ih, "name", "VALUE");
	schema = DialogChildAttribute(ih, "schema", "VALUE");

	p = buf = bufnew(BUFSIZ);
	p = bufcat2(&buf, p, "create table ", dbname, ".", tablename, schema, 0);
	p = bufcat (&buf, p, ";");
	return exec_and_free(buf, p);
}

int cb_createview_ok(Ihandle *ih)
{
	const char *dbname, *viewname, *schema;
	char *buf, *p;

	dbname = DialogChildAttribute(ih, "dblist", "VALUESTRING");
	viewname = DialogChildAttribute(ih, "name", "VALUE");
	schema = DialogChildAttribute(ih, "schema", "VALUE");

	p = buf = bufnew(BUFSIZ);
	p = bufcat2(&buf, p, "create view ", dbname, ".", viewname, " as ", 0);
	p = bufcat (&buf, p, schema);
	p = bufcat (&buf, p, ";");
	return exec_and_free(buf, p);
}

static
void move_all(Ihandle *llist, Ihandle *rlist)
{
	int id;

	for (id = 1; ; id++) {
		char *itemtext = IupGetAttributeId(llist, "", id);
		if (!itemtext) break;
		IupSetStrAttribute(rlist, "APPENDITEM", itemtext);
	}
	IupSetAttribute(llist, "REMOVEITEM", "ALL");
}

static
void move_one(Ihandle *llist, Ihandle *rlist)
{
	int id;
	char *itemtext;

	id = IupGetInt(llist, "VALUE");
	if (!id) return;
	itemtext = IupGetAttributeId(llist, "", id);
	IupSetStrAttribute(rlist, "APPENDITEM", itemtext);
	IupSetInt(llist, "REMOVEITEM", id);
}

int cb_addall(Ihandle *ih)
{
	Ihandle *llist, *rlist;

	llist = IupGetDialogChild(ih, "llist");
	rlist = IupGetDialogChild(ih, "rlist");

	move_all(llist, rlist);
	return IUP_DEFAULT;
}

int cb_addone(Ihandle *ih)
{
	Ihandle *llist, *rlist;

	llist = IupGetDialogChild(ih, "llist");
	rlist = IupGetDialogChild(ih, "rlist");

	move_one(llist, rlist);
	return IUP_DEFAULT;
}

int cb_delone(Ihandle *ih)
{
	Ihandle *llist, *rlist;

	llist = IupGetDialogChild(ih, "llist");
	rlist = IupGetDialogChild(ih, "rlist");

	move_one(rlist, llist);
	return IUP_DEFAULT;
}

int cb_delall(Ihandle *ih)
{
	Ihandle *llist, *rlist;

	llist = IupGetDialogChild(ih, "llist");
	rlist = IupGetDialogChild(ih, "rlist");

	move_all(rlist, llist);
	return IUP_DEFAULT;
}

int cb_update_tableviewlist(Ihandle *ih, char *text, int item, int state)
{
	if (state == 1) {
		Ihandle *tableviewlist;
		const char *dbname, *trigger_type;

		dbname = DialogChildAttribute(ih, "dblist", "VALUESTRING");
		trigger_type = DialogChildAttribute(ih, "trigger_type", "VALUESTRING");
		tableviewlist = IupGetDialogChild(ih, "tableviewlist");
		IupSetAttribute(tableviewlist, "REMOVEITEM", "ALL");
		db_exec_args(sqlcb_tablelist, (void *) tableviewlist,
			(strcmp(dbname, "temp") == 0)
				? "select name from sqlite_%s_master where type=%Q;"
				: "select name from \"%w\".sqlite_master where type=%Q;",
			dbname,
			(strcmp(trigger_type, "instead of")) ? "table" : "view"
		);
		if (IupGetInt(tableviewlist, "COUNT") > 0) {
			IupSetInt(tableviewlist, "VALUE", 1);
		}
		IupRefresh(tableviewlist);
	}
	return IUP_DEFAULT;
}

int cb_createtrigger_map(Ihandle *ih)
{
	Ihandle *dblist, *trigger_type, *trigger_action, *updateofbox;

	trigger_type = IupGetDialogChild(ih, "trigger_type");
	IupSetAttributeId(trigger_type, "", 1, "before");
	IupSetAttributeId(trigger_type, "", 2, "after");
	IupSetAttributeId(trigger_type, "", 3, "instead of");
	IupSetInt(trigger_type, "VALUE", 1);

	trigger_action = IupGetDialogChild(ih, "trigger_action");
	IupSetAttributeId(trigger_action, "", 1, "delete");
	IupSetAttributeId(trigger_action, "", 2, "insert");
	IupSetAttributeId(trigger_action, "", 3, "update");
	IupSetInt(trigger_action, "VALUE", 1);

	updateofbox = IupGetDialogChild(ih, "updateofbox");
	IupSetAttribute(updateofbox, "VISIBLE", "NO");
	IupSetAttribute(updateofbox, "FLOATING", "YES");

	dblist = IupGetDialogChild(ih, "dblist");
	update_dblist(dblist);
	cb_update_tableviewlist(dblist, IupGetAttribute(dblist, "VALUESTRING"), 1, 1);

	IupSetAttribute(ih, "SIZE", 0);
	return IUP_DEFAULT;
}

int cb_createtrigger_triggeraction(Ihandle *ih, char *text, int item, int state)
{
	if (item == 3) {
		Ihandle *box;

		assert(strcmp(text, "update") == 0);
		box = IupGetDialogChild(ih, "updateofbox");
		IupSetInt(box, "VISIBLE", state);
		IupSetInt(box, "FLOATING", !state);
		IupRefresh(box);
		IupRefresh(IupGetDialog(ih));
	}
	return IUP_DEFAULT;
}

int cb_createtrigger_ok(Ihandle *ih)
{
	char *buf, *p;
	const char *dbname, *triggername, *tablename, *trigger_type, *trigger_action;

	dbname = DialogChildAttribute(ih, "dblist", "VALUESTRING");
	triggername = DialogChildAttribute(ih, "name", "VALUE");
	tablename = DialogChildAttribute(ih, "tableviewlist", "VALUESTRING");

	assert(dbname && triggername);
	if (!tablename) {
		IupMessage("Error", "You have to specify the table name.");
		return IUP_DEFAULT;
	}

	p = buf = bufnew(BUFSIZ);
	p = bufcat2(&buf, p, "create trigger ", dbname, ".", triggername, " ", 0);
	trigger_type = DialogChildAttribute(ih, "trigger_type", "VALUESTRING");
	assert(trigger_type);
	p = bufcat (&buf, p, trigger_type);
	p = bufcat (&buf, p, " ");
	trigger_action = DialogChildAttribute(ih, "trigger_action", "VALUESTRING");
	assert(trigger_action);
	p = bufcat (&buf, p, trigger_action);
	if (strcmp(trigger_action, "update") == 0 && IupGetInt(IupGetDialogChild(ih, "updateof"), "ACTIVE")) {
		p = bufcat (&buf, p, " of ");
		p = bufcat (&buf, p, DialogChildAttribute(ih, "updateof", "VALUE"));
	}
	p = bufcat2(&buf, p, " on ", tablename, 0);
	if (IupGetInt(IupGetDialogChild(ih, "foreachrow"), "VALUE")) {
		p = bufcat (&buf, p, " for each row");
	}
	if (IupGetInt(IupGetDialogChild(ih, "when"), "ACTIVE")) {
		p = bufcat (&buf, p, " when ");
		p = bufcat (&buf, p, DialogChildAttribute(ih, "when", "VALUE"));
	}
	p = bufcat (&buf, p, " begin\n");
	p = bufcat (&buf, p, DialogChildAttribute(ih, "code", "VALUE"));
	p = bufcat (&buf, p, "\nend;");
	return exec_and_free(buf, p);
}
