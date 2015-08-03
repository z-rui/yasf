#define YASF_VERSION "0.0-dev ("__DATE__")"

/* in db.c */
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

/* in cb.c */
extern int sqlcb_mat(void *, int, char **, char **);

/* in dialogs.c */
extern int cb_update_columns(Ihandle *, char *, int, int);
extern int cb_update_tablelist(Ihandle *, char *, int, int);
extern int cb_createindex_map(Ihandle *);
extern int cb_createtable_map(Ihandle *);
extern int cb_createindex_ok(Ihandle *);
extern int cb_createtable_ok(Ihandle *);
extern int cb_createview_ok(Ihandle *);
extern int cb_addall(Ihandle *);
extern int cb_addone(Ihandle *);
extern int cb_delone(Ihandle *);
extern int cb_delall(Ihandle *);
extern int cb_createtrigger_map(Ihandle *);
extern int cb_createtrigger_triggeraction(Ihandle *, char *, int, int);
extern int cb_update_tableviewlist(Ihandle *, char *, int, int);
extern int cb_createtrigger_ok(Ihandle *);

/* in util.c */
#include <stddef.h>	/* for size_t */
extern char *bufnew(size_t);
extern char *bufext(char **, char *, size_t);
extern char *bufcat(char **, char *, const char *);
extern void buffree(char *);
extern size_t escquote(char *, const char *, int);
