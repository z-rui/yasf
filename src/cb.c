#include <iup.h>
#include <sqlite3.h>
#include "yasf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int cb_file_new(Ihandle *ih);

static Ihandle *ctl_tree, *ctl_matrix;

int cb_main_init(Ihandle *ih)
{
	// initialization work
	ctl_tree = IupGetHandle("ctl_tree");
	ctl_matrix = IupGetHandle("ctl_matrix");

	cb_file_new(ih);

	return IUP_DEFAULT;
}

int cb_execute(Ihandle *ih)
{
	Ihandle *cmdline;
	const char *s;

	cmdline = IupGetHandle("ctl_cmdline");
	s = IupGetAttribute(cmdline, "VALUE");
	exec_stmt_str(ctl_matrix, s);
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
	if (filename) {
		db_file(filename);
		update_treeview(ctl_tree);
	}
	return IUP_DEFAULT;
}

int cb_file_new(Ihandle *ih)
{
	db_file(":memory:");
	update_treeview(ctl_tree);
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
		if (rc) {
			rc = exec_stmt_args(ctl_matrix, "attach %Q as \"%w\";", filename, dbname);
			if (rc == SQLITE_OK)
				update_treeview(ctl_tree);
		}
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
		rc = exec_stmt_args(ctl_matrix, "detach \"%w\";", dbnames[rc]);
		if (rc == SQLITE_OK)
			update_treeview(ctl_tree);
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
			menu = IupGetHandle("mnu_index_leaf");
			IupPopup(menu, IUP_MOUSEPOS, IUP_MOUSEPOS);
		} else if (strcmp(type, "view") == 0) {
			// TODO
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
	db_enable_edit(dbname, type, tablename);
	return IUP_DEFAULT;
}

int cb_viewschema(Ihandle *ih)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	if (strcmp(type, "table") == 0) {
		db_disable_edit();
		exec_stmt_args(ctl_matrix, "pragma \"%w\".table_info(\"%w\");", dbname, tablename);
	} else if (strcmp(type, "index") == 0) {
		db_disable_edit();
		exec_stmt_args(ctl_matrix, "pragma \"%w\".index_xinfo(\"%w\");", dbname, tablename);
	} else {
		assert(0 && "unknown type");
	}
	return IUP_DEFAULT;
}

int cb_table_rename(Ihandle *ih)
{
	Ihandle *tree;

	tree = IupGetHandle("ctl_tree");
	IupSetAttribute(tree, "RENAME", "YES");
	return IUP_DEFAULT;
}

int cb_drop(Ihandle *ih)
{
	int rc;
	static char buf[512];
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);

	if (strlen(tablename) > 400)
		return IUP_DEFAULT; /* too long table name may overflow buffer */
	sprintf(buf, "Drop %s '%s'?", type, tablename);
	rc = IupAlarm("Drop", buf, "Yes", "No", 0);
	assert(rc == 0 || rc == 1 || rc == 2);
	if (rc == 1) {
		rc = exec_stmt_args(ctl_matrix, "drop %s \"%w\".\"%w\"", type, dbname, tablename);
		update_treeview(ctl_tree);
	}
	return IUP_DEFAULT;
}

int cb_tree_showrename(Ihandle *ih, int id)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	if (tablename == 0 || strcmp(type, "table") != 0) {
		/* only tables can be renamed */
		return IUP_IGNORE;
	}
	return IUP_DEFAULT;
}

int cb_tree_rename(Ihandle *ih, int id, char *title)
{
	const char *dbname, *type, *tablename;
	int rc;

	get_node_info(&dbname, &type, &tablename);
	rc = exec_stmt_args(ctl_matrix, "alter table \"%w\".\"%w\" rename to \"%w\"", dbname, tablename, title);
	return (rc == SQLITE_OK) ? IUP_DEFAULT : IUP_IGNORE;
}

int cb_edit_create_table(Ihandle *ih)
{
	Ihandle *dlg;

	dlg = IupGetHandle("dlg_create_table");
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
	return IUP_DEFAULT;
}

int cb_edit_create_index(Ihandle *ih)
{
	Ihandle *dlg;

	dlg = IupGetHandle("dlg_create_index");
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
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
	REGISTER(cb_edit_create_table);
	REGISTER(cb_edit_create_index);
	REGISTER(cb_edit_pragmas);
	REGISTER(cb_execute);
	REGISTER(cb_help_about);
	REGISTER(cb_default_close);
	REGISTER(cb_tree_rightclick);
	REGISTER(cb_table_viewdata);
	REGISTER(cb_viewschema);
	REGISTER(cb_table_rename);
	REGISTER(cb_drop);
	REGISTER(cb_tree_showrename);
	REGISTER(cb_tree_rename);
	REGISTER(cb_matrix_edit);
}

