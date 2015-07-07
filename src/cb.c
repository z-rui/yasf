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

