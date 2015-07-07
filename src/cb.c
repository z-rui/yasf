#include <iup.h>
#include "yasf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int cb_file_new(Ihandle *ih);

int cb_main_init(Ihandle *ih)
{
	Ihandle *cmdline;

	// initialization work
	cmdline = IupGetHandle("ctl_cmdline");
	fprintf(stderr, IupGetAttribute(cmdline, "KEYWORDSETS"));
	IupSetAttribute(cmdline, "KEYWORDS0",
"abort action add after all alter analyze and as asc attach autoincrement before begin between by cascade case cast check collate column commit conflict constraint create cross current_date current_time current_timestamp database default deferrable deferred delete desc detach distinct drop each else end escape except exclusive exists explain fail for foreign from full glob group having if ignore immediate in index indexed initially inner insert instead intersect into is isnull join key left like limit match natural no not notnull null of offset on or order outer plan pragma primary query raise recursive references regexp reindex release rename replace restrict right rollback row savepoint select set table temp temporary then to transaction trigger union unique update using vacuum values view virtual when where with without");
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 1, 0, 128, 0);    // 1-C comment�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 2, 0, 128, 0);    // 2-C++ comment line�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 4, 255, 0, 255);    // 4-Number�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 5, 0, 0, 255);    // 5-Keyword�
	IupSetAttributeId(cmdline, "STYLEBOLD", 5, "YES");
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 6, 160, 20, 20);  // 6-String�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 7, 128, 0, 0);    // 7-Character�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 9, 0, 0, 255);    // 9-Preprocessor block�
	IupSetRGBId(cmdline, "STYLEFGCOLOR", 10, 0, 0, 0); // 10-Operator�

	cb_file_new(ih);

	return IUP_DEFAULT;
}

int cb_execute(Ihandle *ih)
{
	Ihandle *cmdline;
	const char *s;

	cmdline = IupGetHandle("ctl_cmdline");
	s = IupGetAttribute(cmdline, "VALUE");
	exec_stmt_str(s);
	return IUP_DEFAULT;
}

int cb_help_about(Ihandle *ih)
{
	Ihandle *dlg;
	static const char *about_text = 
		"Version: " YASF_VERSION "\n"
		"SQLite version: " SQLITE_VERSION "\n"
		"IUP version: " IUP_VERSION "\n";

	dlg = IupGetHandle("dlg_about");
	IupSetAttribute(dlg, "VALUE", about_text);
	IupPopup(dlg, IUP_DEFAULT, IUP_DEFAULT);
	return IUP_DEFAULT;
}

int cb_default_close(Ihandle *ih)
{
	return IUP_CLOSE;
}

static
char *get_file(void)
{
	int rc;
	Ihandle *dlg;

	dlg = IupGetHandle("dlg_open");
	IupSetAttribute(dlg, "VALUE", 0);
	IupPopup(dlg, IUP_DEFAULT, IUP_DEFAULT);
	rc = IupGetInt(dlg, "STATUS");
	if (rc == 0 || rc == 1) {
		return IupGetAttribute(dlg, "VALUE");
	}
	return 0;
}

int cb_file_open(Ihandle *ih)
{
	const char *filename;

	filename = get_file();
	if (filename)
		db_file(filename);
	return IUP_DEFAULT;
}

int cb_file_new(Ihandle *ih)
{
	db_file(":memory:");
	return IUP_DEFAULT;
}

int cb_file_attach(Ihandle *ih)
{
	int rc;
	const char *filename;
	static char dbname[512];

	filename = get_file();
	dbname[0] = '\0';
	if (filename) {
		rc = IupGetParam("Attach", 0, 0,
			"Attach as: %s\n",
			dbname
		);
		if (rc) db_attach(filename, dbname);
	}
	return IUP_DEFAULT;
}

static
int get_detachable_dbnames(const char ***plist)
{
	int countdb, countid;
	Ihandle *tree;
	int id, i;

	tree = IupGetHandle("ctl_tree");
	countdb = IupGetIntId(tree, "CHILDCOUNT", 0) - 2;

	if (countdb == 0) return 0;
	assert(countdb > 0);

	*plist = malloc(sizeof (char *) * countdb);
	if (!*plist) return -1;

	countid = IupGetInt(tree, "COUNT");
	i = 0;
	for (id = 1; id <= countid; id++) {
		if (IupGetIntId(tree, "DEPTH", id) == 1) {
			char *dbname;

			dbname = IupGetAttributeId(tree, "TITLE", id);
			if (strcmp(dbname, "main") != 0 && strcmp(dbname, "temp") != 0) {
				assert(i < countdb);
				(*plist)[i++] = dbname;
			}
		}
	}
	assert(i == countdb);
	return countdb;
}

int cb_file_detach(Ihandle *ih)
{
	int rc;
	const char **dbnames = 0;
	int count;

	count = get_detachable_dbnames(&dbnames);
	if (!count) {
		IupMessage("Detach", "No database is able to be detached.");
		return IUP_DEFAULT;
	}
	if (!dbnames) return IUP_DEFAULT;
	rc = IupListDialog(1, "Detach", count, dbnames, 1, 1, 10, 0);
	if (rc >= 0) {
		assert(rc < count);
		db_detach(dbnames[rc]);
	}
	free(dbnames);
	return IUP_DEFAULT;
}

#define REGISTER(x) IupSetFunction(#x, (Icallback) &x)

void reg_cb(void)
{
	REGISTER(cb_main_init);
	REGISTER(cb_file_new);
	REGISTER(cb_file_open);
	REGISTER(cb_file_attach);
	REGISTER(cb_file_detach);
	REGISTER(cb_edit_pragmas);
	REGISTER(cb_execute);
	REGISTER(cb_help_about);
	REGISTER(cb_default_close);
}

