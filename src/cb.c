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

int sqlcb_mat(void *data, int cols, char **val, char **title)
{
	Ihandle *matrix;
	int rows;
	int i;

	/* do not rely on global variable here, for future convenience. */
	matrix = (Ihandle *) data;
	rows = IupGetInt(matrix, "NUMLIN");
	if (rows == 0) {
		/* set up the titles */
		/* don't bother with primary keys now */
		IupSetInt(matrix, "NUMCOL", cols);
		for (i = 0; i < cols; i++) {
			IupSetStrAttributeId2(matrix, "", 0, i+1, title[i]);
		}
	}
	IupSetInt(matrix, "NUMLIN", ++rows);
	for (i = 0; i < cols; i++) {
		IupSetStrAttributeId2(matrix, "", rows, i+1, val[i] ? val[i] : "(NULL)");
	}
	return 0;
}

static
void fit_cols(Ihandle *matrix)
{
#if 0
	int cols;

	cols = IupGetInt(matrix, "NUMCOL");
	while (cols) {
		IupSetStrf(matrix, "FITTOTEXT", "C%d", cols--);
	}
#else
	IupSetStrf(matrix, "REDRAW", "ALL");
#endif
}

static
void clear(Ihandle *matrix)
{
	IupSetInt(matrix, "NUMLIN", 0);
	IupSetInt(matrix, "NUMCOL", 0);
}

int cb_execute(Ihandle *ih)
{
	Ihandle *cmdline;
	const char *s;

	cmdline = IupGetHandle("ctl_cmdline");
	s = IupGetAttribute(cmdline, "VALUE");
	clear(ctl_matrix);
	db_exec_str(s, sqlcb_mat, (void *) ctl_matrix);
	fit_cols(ctl_matrix);
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
			rc = db_exec_args(0, 0, "attach %Q as \"%w\";", filename, dbname);
			if (rc == SQLITE_OK)
				update_treeview(ctl_tree);
		}
	}
	return IUP_DEFAULT;
}

int cb_dlg_button(Ihandle *ih)
{
	Ihandle *dlg;

	dlg = IupGetDialog(ih);
	IupSetInt(dlg, "BUTTONRESPONSE", IupGetInt(ih, "RESPONSE_"));
	return IUP_CLOSE;
}

static
int sqlcb_dblist(void *data, int cols, char **val, char **title)
{
	Ihandle *list;
	const char *dbname;

	list = (Ihandle *) data;
	assert(cols == 3);
	dbname = val[1];
	if (strcmp(dbname, "main") != 0 && strcmp(dbname, "temp") != 0)
		IupSetStrAttribute(list, "APPENDITEM", dbname);
	return 0;
}

static
const char *select_database(void)
{
	Ihandle *dlg, *list;
	const char *dbname = 0;
	int rc;

	dlg = IupGetHandle("dlg_detach");
	list = IupGetDialogChild(dlg, "dblist");

	IupMap(dlg);
	db_exec_str("pragma database_list;", sqlcb_dblist, (void *) list);

	if (IupGetInt(list, "COUNT") == 0) {
		IupMessage("Error", "No database can be detached.");
	} else {
		IupSetInt(list, "VALUE", 1);
		IupPopup(dlg, IUP_CENTER, IUP_CENTER);
		rc = IupGetInt(dlg, "BUTTONRESPONSE");
		if (rc == 1) {
			dbname = IupGetAttribute(list, "VALUESTRING");
		}
	}
	IupUnmap(dlg);
	return dbname;
}

int cb_file_detach(Ihandle *ih)
{
	const char *dbname;

	dbname = select_database();
	if (dbname) {
		int rc = db_exec_args(0, 0, "detach \"%w\";", dbname);
		if (rc == SQLITE_OK)
			update_treeview(ctl_tree);
	}
	return IUP_DEFAULT;
}

static
void get_node_info(const char **dbname, const char **type, const char **leafname)
{
	int id, depth;
	const char **p[] = {dbname, type, leafname};

	*dbname = *type = *leafname = 0;

	id = IupGetInt(ctl_tree, "VALUE");
	depth = IupGetIntId(ctl_tree, "DEPTH", id);
	assert(0 <= depth && depth <= 3);

	while (depth-- > 0) {
		if (p[depth])
			*p[depth] = IupGetAttributeId(ctl_tree, "TITLE", id);
		id = IupGetIntId(ctl_tree, "PARENT", id);
	}
}

int cb_tree_rightclick(Ihandle *ih, int id)
{
	Ihandle *menu = 0;
	const char *dbname, *type, *leafname;

	IupSetFocus(ih);
	IupSetInt(ih, "VALUE", id);
	get_node_info(&dbname, &type, &leafname);
	if (leafname) { /* LEAF */
		if (strcmp(type, "table") == 0) {
			menu = IupGetHandle("mnu_table_leaf");
		} else if (strcmp(type, "index") == 0) {
			menu = IupGetHandle("mnu_index_leaf");
		} else if (strcmp(type, "view") == 0) {
			menu = IupGetHandle("mnu_view_leaf");
		} else if (strcmp(type, "trigger") == 0) {
			menu = IupGetHandle("mnu_trigger_leaf");
		} else {
			assert(0 && "unknown node type");
		}
	} else if (type) {
		// TODO handle branches
	} else if (dbname) {
		menu = IupGetHandle("mnu_database_branch");
	} else {
		// TODO handle root
	}
	if (menu)
		IupPopup(menu, IUP_MOUSEPOS, IUP_MOUSEPOS);
	return IUP_DEFAULT;
}

int cb_viewdata(Ihandle *ih)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	if (strcmp(type, "table") == 0 || strcmp(type, "view") == 0) {
		clear(ctl_matrix);
		db_enable_edit(dbname, type, tablename);
		fit_cols(ctl_matrix);
	} else {
		assert(0 && "unknown type");
	}
	return IUP_DEFAULT;
}

int cb_viewschema(Ihandle *ih)
{
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);
	if (strcmp(type, "table") == 0) {
		clear(ctl_matrix);
		db_disable_edit();
		db_exec_args(sqlcb_mat, ctl_matrix, "pragma \"%w\".table_info(\"%w\");", dbname, tablename);
		fit_cols(ctl_matrix);
	} else if (strcmp(type, "index") == 0) {
		clear(ctl_matrix);
		db_disable_edit();
		db_exec_args(sqlcb_mat, ctl_matrix, "pragma \"%w\".index_xinfo(\"%w\");", dbname, tablename);
		fit_cols(ctl_matrix);
	} else if (strcmp(type, "view") == 0) {
		// TODO
	} else if (strcmp(type, "trigger") == 0) {
		// TODO
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

int IupAlarm2(const char *title, const char *msg, const char *buttons)
{
	Ihandle *dlg;
	int rc;

	dlg = IupMessageDlg();
	IupSetAttribute(dlg, "DIALOGTYPE", "QUESTION");
	IupSetAttribute(dlg, "TITLE", "Drop");
	IupSetAttribute(dlg, "BUTTONS", "YESNO");
	IupSetAttribute(dlg, "VALUE", msg);
	IupPopup(dlg, IUP_CURRENT, IUP_CURRENT);
	rc = IupGetInt(dlg, "BUTTONRESPONSE");
	IupDestroy(dlg);

	return rc;
}

int cb_drop(Ihandle *ih)
{
	int rc;
	char *buf, *p;
	const char *dbname, *type, *tablename;

	get_node_info(&dbname, &type, &tablename);

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "Drop ");
	p = bufcat(&buf, p, type);
	p = bufcat(&buf, p, " ");
	p = bufcat(&buf, p, tablename);
	p = bufcat(&buf, p, "?");

	if (!p) {	/* OOM, but have a try anyway... */
		rc = IupAlarm2("Drop", "Drop the table?", "YESNO");
	} else {
		rc = IupAlarm2("Drop", buf, "YESNO");
	}
	assert(rc == 1 || rc == 2);
	buffree(buf);
	if (rc == 1) {
		rc = db_exec_args(0, 0, "drop %s \"%w\".\"%w\"", type, dbname, tablename);
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
	rc = db_exec_args(0, 0, "alter table \"%w\".\"%w\" rename to \"%w\"", dbname, tablename, title);
	return (rc == SQLITE_OK) ? IUP_DEFAULT : IUP_IGNORE;
}

int cb_dialog(Ihandle *ih)
{
	Ihandle *dlg;

	dlg = IupGetAttributeHandle(ih, "dialog");
	if (!dlg) {
		IupMessage("Not implemented", "Not implemented");
	}
	IupMap(dlg);
	IupPopup(dlg, IUP_CENTER, IUP_CENTER);
	IupUnmap(dlg);
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
	REGISTER(cb_dialog);
	REGISTER(cb_edit_pragmas);
	REGISTER(cb_execute);
	REGISTER(cb_help_about);
	REGISTER(cb_default_close);
	REGISTER(cb_tree_rightclick);
	REGISTER(cb_viewdata);
	REGISTER(cb_viewschema);
	REGISTER(cb_table_rename);
	REGISTER(cb_drop);
	REGISTER(cb_tree_showrename);
	REGISTER(cb_tree_rename);
	REGISTER(cb_matrix_edit);
	REGISTER(cb_dlg_button);
	REGISTER(cb_update_columns);
	REGISTER(cb_update_tablelist);
	REGISTER(cb_createindex_map);
	REGISTER(cb_createindex_ok);
	REGISTER(cb_createtable_map);
	REGISTER(cb_createtable_ok);
	REGISTER(cb_createview_ok);
	REGISTER(cb_addall);
	REGISTER(cb_addone);
	REGISTER(cb_delone);
	REGISTER(cb_delall);
}

