#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
#include <sqlite3.h>	/* for sqlite3_stmt */
#include <iup.h>	/* for Ihandle */
extern void db_init(void);
extern void db_file(const char *);
extern void db_finalize(void);
extern int cb_edit_pragmas(Ihandle *);
extern int cb_matrix_edit(Ihandle *, int, int, int, int);
extern void update_treeview(Ihandle *);
extern int get_pk_cid(const char *, const char *);
extern int db_exec_str(const char *, int (*)(void *, int, char **, char **), void *);
extern int db_exec_args(int (*)(void *, int, char **, char **), void *, const char *, ...);
extern void db_end_edit(Ihandle *);
extern void db_begin_edit(Ihandle *, const char *, const char *);
extern int db_prepare(const char *, sqlite3_stmt **);
extern int db_schema_version(void);
extern sqlite3_int64 db_last_insert_rowid(void);

/* in cb.c */
extern int sqlcb_mat(void *, int, char **, char **);

/* in util.c */
#include <stddef.h>	/* for size_t */
#include "ulbuf.h"	/* for bufncat */
extern size_t escquote(char *, const char *, int);
#define bufcat0(buf, p, s) bufncat(buf, p, sizeof s - 1)
char *bufcat1(char **, char *, const char *);
char *bufcat2(char **, char *, ...);
