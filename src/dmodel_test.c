#include <stdio.h>
#include "dmodel.h"

int main()
{
	struct dmodel *d;

	d = dmodel_new("\"main\".\"tbl1\"");
	dmodel_add_pkcol(d, "rowid");
	dmodel_init_sql(d);

	printf("%s\n%s\n%s\n%s\n%s\n",
		d->select_sql,
		d->select_pk_sql,
		d->insert_sql,
		d->delete_sql,
		d->update_sql);

	dmodel_free(d);

	return 0;
}
