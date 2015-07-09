#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
extern void db_init(void);
extern void db_file(const char *);
extern void db_finalize(void);
extern int cb_edit_pragmas(Ihandle *);
extern int cb_matrix_edit(Ihandle *, int, int, int, int);
extern void update_treeview(Ihandle *);
#include <sqlite3.h> /* for sqlite3_stmt */
extern int exec_stmt(Ihandle *, sqlite3_stmt *);
extern int exec_stmt_str(Ihandle *, const char *);
extern int exec_stmt_args(Ihandle *, const char *, ...);
extern void db_disable_edit(void);
extern void db_enable_edit(const char *, const char *, const char *);
