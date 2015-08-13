#include <sqlite3.h>
#include <stddef.h>
#include "wbt.h"

struct dmodel {
	char *qualified_name;	/* OWNER: "database"."table" */
	int ncol;	/* number of columns */
	int npkcol;	/* number of PK columns */
	/* NOTE: npkcol should be reasonably small.
	 * SQLite3 itself uses some O(N^2) algorithms where N stands for npkcol here.
	 * The max number of columns in SQLite3 is by default 2000 and max 32767.
	 * It is hard to imagine that any application will make a table with
	 * more than a few dozen columns, much less PK columns.
	 * I believe that in most situations, npkcol == 1 and the PK column is ROWID.
	 */
	char **pkcolname;	/* OWNER: names of PK columns, each being quoted. */
	char *select_sql, *select_pk_sql, *insert_sql, *delete_sql, *update_sql;
	size_t current_key;
	struct wbt_root entries[1];
};

extern struct dmodel *dmodel_new(const char *);
extern void dmodel_add_col(struct dmodel *, const char *, int);
extern void dmodel_init_sql(struct dmodel *);
extern void dmodel_free(struct dmodel *);

extern sqlite3_value **dmodel_add_entry(struct dmodel *);
extern sqlite3_value **dmodel_get_entry(struct dmodel *, size_t);
extern void dmodel_del_entry(struct dmodel *, size_t);
