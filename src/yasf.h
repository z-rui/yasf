#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
extern void db_init(void);
extern void db_file(const char *);
extern void db_finalize(void);
extern int cb_edit_pragmas(Ihandle *);
extern int cb_matrix_edit(Ihandle *, int, int, int, int);
extern void update_treeview(Ihandle *);
extern int db_exec_str(const char *, int (*)(void *, int, char **, char **), void *);
extern int db_exec_args(int (*)(void *, int, char **, char **), void *, const char *, ...);
extern void db_disable_edit(void);
extern void db_enable_edit(const char *, const char *, const char *);

/* in cb.c */
extern int sqlcb_mat(void *, int, char **, char **);

/* in util.c */
#include <stddef.h>	/* for size_t */
extern char *bufnew(size_t);
extern char *bufext(char *, size_t);
extern char *bufcat(char *, const char *);
extern void buffree(char *);
extern size_t buflen(char *);
extern size_t escquote(char *, const char *, int);
