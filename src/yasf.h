#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
extern void db_init(void);
extern void db_file(const char *);
extern int db_attach(const char *, const char *);
extern int db_detach(const char *);
extern void db_finalize(void);
extern int cb_edit_pragmas(Ihandle *);
#include <sqlite3.h> /* for sqlite3_stmt */
extern void exec_stmt(sqlite3_stmt *);
extern void exec_stmt_str(const char *);
