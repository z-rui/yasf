#include <iup.h>
#include "yasf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int cb_file_new(Ihandle *ih);

int cb_main_init(Ihandle *ih)
{
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
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
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
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
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

static
const char *get_leaf_type(Ihandle *tree, int id)
{
	int id1;

	id1 = IupGetIntId(tree, "PARENT", id);
	return IupGetAttributeId(tree, "TITLE", id1);
}

int cb_tree_rightclick(Ihandle *ih, int id)
{
	const char *type;
	Ihandle *menu;

	IupSetFocus(ih);
	type = IupGetAttributeId(ih, "KIND", id);
	fprintf(stderr, "RIGHT CLICK! (%s %d)\n", type, id);
	IupSetInt(ih, "VALUE", id);
	if (strcmp(type, "LEAF") == 0) {
		type = get_leaf_type(ih, id);
		if (strcmp(type, "table") == 0) {
			menu = IupGetHandle("mnu_table_leaf");
			IupPopup(menu, IUP_MOUSEPOS, IUP_MOUSEPOS);
		} else if (strcmp(type, "index") == 0) {
			// TODO
		} else if (strcmp(type, "view") == 0) {
		} else if (strcmp(type, "trigger") == 0) {
		} else {
			assert(0 && "unknown node type");
		}
	} else {
		// TODO handle branches
	}
	return IUP_DEFAULT;
}

static
const char *get_title_at_depth(Ihandle *tree, int id, int at_depth)
{
	int depth;

	depth = IupGetIntId(tree, "DEPTH", id);
	if (depth < at_depth)
		return 0;
	while (depth-- > at_depth) {
		id = IupGetIntId(tree, "PARENT", id);
	}
	return IupGetAttributeId(tree, "TITLE", id);
}

static
void get_node_info(const char **dbname, const char **type, const char **leafname)
{
	int id, depth;
	Ihandle *tree;

	tree = IupGetHandle("ctl_tree");
	id = IupGetInt(tree, "VALUE");
	depth = IupGetIntId(tree, "DEPTH", id);
	assert(strcmp(IupGetAttributeId(tree, "KIND", id), (depth == 3) ? "LEAF" : "BRANCH") == 0);

	if (leafname)
		*leafname = get_title_at_depth(tree, id, 3);
	if (type)
		*type = get_title_at_depth(tree, id, 2);
	if (dbname)
		*dbname = get_title_at_depth(tree, id, 1);
}

int cb_table_viewdata(Ihandle *ih)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	assert(strcmp(type, "table") == 0);
	exec_stmt_args("select * from %Q.%Q;", dbname, tablename);
	return IUP_DEFAULT;
}

int cb_table_viewschema(Ihandle *ih)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	assert(strcmp(type, "table") == 0);
	exec_stmt_args("pragma %Q.table_info(%Q);", dbname, tablename);
	return IUP_DEFAULT;
}

int cb_table_rename(Ihandle *ih)
{
	const char *dbname, *type, *tablename;
	static char newname[512];
	int rc;

	get_node_info(&dbname, &type, &tablename);
	if (strcmp(type, "table") != 0) {
		IupMessagef("Error", "Cannot rename %s %s", (type[0] == 'i') ? "an" : "a", type);
		return IUP_DEFAULT;
	}
	newname[0] = '\0';
	rc = IupGetParam("Rename", 0, 0,
		"New name: %s\n",
		newname);
	if (rc) {
		Ihandle *tree;
		int id;

		exec_stmt_args("alter table %Q.%Q rename to %Q", dbname, tablename, newname);
		/* position of the node does not alter after
		 * an 'alter table' statement, so we make a backup
		 * before updating treeview, and restore it later. */
		tree = IupGetHandle("ctl_tree");
		id = IupGetInt(tree, "VALUE");
		update_treeview();
		IupSetInt(tree, "VALUE", id);
	}
	return IUP_DEFAULT;
}

int cb_table_drop(Ihandle *ih)
{
	int rc;
	static char buf[512];
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	assert(strcmp(type, "table") == 0);

	if (strlen(tablename) > 400)
		return IUP_DEFAULT; /* too long table name may overflow buffer */
	sprintf(buf, "Drop table '%s'?", tablename);
	rc = IupAlarm("Drop", buf, "Yes", "No", 0);
	assert(rc == 1 || rc == 2);
	if (rc == 1) {
		exec_stmt_args("drop table %Q.%Q", dbname, tablename);
		update_treeview();
	}
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
	REGISTER(cb_tree_rightclick);
	REGISTER(cb_table_viewdata);
	REGISTER(cb_table_viewschema);
	REGISTER(cb_table_rename);
	REGISTER(cb_table_drop);
}

