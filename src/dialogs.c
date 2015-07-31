#include <iup.h>
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yasf.h"

static
int sqlcb_columnlist(void *data, int cols, char **val, char **title)
{
	Ihandle *tablelist = (Ihandle *) data;

	assert(cols == 6);
	IupSetStrAttribute(tablelist, "APPENDITEM", val[1]);
	return 0;
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
		db_exec_args(sqlcb_columnlist, (void *) llist,
			"pragma \"%w\".table_info(\"%w\");",
			dbname, text
		);
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
	if (strcmp(val[0], "temp") != 0)
		IupSetStrAttribute(tablelist, "APPENDITEM", val[1]);
	return 0;
}

int cb_ci_map(Ihandle *ih)
{
	Ihandle *dblist;

	dblist = IupGetDialogChild(ih, "dblist");
	IupSetAttribute(dblist, "REMOVEITEM", "ALL");
	db_exec_str("pragma database_list;",
		sqlcb_dblist, (void *) dblist
	);
	IupSetAttribute(dblist, "APPENDITEM", "temp");
	IupSetInt(dblist, "VALUE", 1);
	cb_update_tablelist(dblist, IupGetAttribute(dblist, "VALUESTRING"), 1, 1);
	IupSetAttribute(ih, "SIZE", 0);
	IupRefresh(ih);
	fprintf(stderr, "SIZE = %s, NATURALSIZE = %s\n",
		IupGetAttribute(ih, "SIZE"),
		IupGetAttribute(ih, "NATURALSIZE")
	);
	return IUP_DEFAULT;
}

static
char *bufcatQ(char *buf, const char *s)
{
	char *p;

	buf = bufext(buf, escquote(0, s, '"') + 2, &p);
	if (buf) {
		*p++ = '"';
		p += escquote(p, s, '"');
		*p = '"';
	}
	return buf;
}

int cb_ct_ok(Ihandle *ih)
{
	Ihandle *rlist;
	const char *dbname, *indexname, *tablename, *column;
	char *buf;
	int ncol, i, rc = IUP_DEFAULT;

	dbname = IupGetAttribute(IupGetDialogChild(ih, "dblist"), "VALUESTRING");
	indexname = IupGetAttribute(IupGetDialogChild(ih, "name"), "VALUE");
	tablename = IupGetAttribute(IupGetDialogChild(ih, "tablelist"), "VALUESTRING");

	assert(dbname);
	if (!indexname || !tablename) {
		IupMessage("Error", "You have to specify the table/index name.");
		return IUP_DEFAULT;
	}

	buf = bufnew(BUFSIZ);
	buf = bufcat(buf, "create ");
	if (IupGetInt(IupGetDialogChild(ih, "unique"), "VALUE") == 1) {
		buf = bufcat(buf, "unique ");
	}
	buf = bufcat(buf, "index ");
	buf = bufcatQ(buf, dbname);
	buf = bufcat(buf, ".");
	buf = bufcatQ(buf, indexname);
	buf = bufcat(buf, " on ");
	buf = bufcatQ(buf, tablename);
	buf = bufcat(buf, " (");

	rlist = IupGetDialogChild(ih, "rlist");
	ncol = IupGetInt(rlist, "COUNT");
	for (i = 1; i <= ncol; i++) {
		column = IupGetAttributeId(rlist, "", i);
		buf = bufcatQ(buf, column);
		if (i < ncol)
			buf = bufcat(buf, ", ");
	}
	buf = bufcat(buf, ");");
	if (!buf) {
		IupMessage("Error", "Out of memory");
	} else if (db_exec_str(buf, 0, 0) == SQLITE_OK) {
		update_treeview(IupGetHandle("ctl_tree"));
		rc = IUP_CLOSE;
	}
	buffree(buf);
	return rc;
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
