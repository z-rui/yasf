#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
extern void db_init(void);
extern void db_file(const char *);
extern int db_attach(const char *, const char *);
extern int db_detach(const char *);
extern void db_finalize(void);
extern int cb_edit_pragmas(Ihandle *);
extern int cb_matrix_edit(Ihandle *, int, int, int, int);
extern void update_treeview(void);
#include <sqlite3.h> /* for sqlite3_stmt */
extern int exec_stmt(sqlite3_stmt *);
extern int exec_stmt_str(const char *);
extern int exec_stmt_args(const char *stmt, ...);
extern void db_disable_edit(void);
extern void db_enable_edit(const char *, const char *, const char *);
