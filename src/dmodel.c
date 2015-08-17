/* data model for SQLite3 table */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dmodel.h"
#include "yasf.h"

struct dmodel_entry {
	struct wbt_node node;
	size_t key;	/* to keep entries ordered */
	sqlite3_value *values[1];
};

#define ENTRY(x) ((struct dmodel_entry *)((char *) x \
	- offsetof(struct dmodel_entry, node)))

struct dmodel *dmodel_new(const char *qualified_name)
{
	struct dmodel *d;

	d = malloc(sizeof *d);
	d->qualified_name = strdup(qualified_name);
	d->select_sql = d->select_pk_sql = d->insert_sql = d->delete_sql
		= d->update_sql = 0;
	d->ncol = d->npkcol = 0;
	d->pkcolname = 0;
	d->current_key = 0;
	d->entries->root = 0;
	return d;
}

void dmodel_add_col(struct dmodel *d, const char *pkname, int ispk)
{
	++d->ncol;
	if (ispk) {
		int npkcol = ++d->npkcol;
#if 0	/* O(N) algorithm */
		if ((npkcol & (npkcol - 1)) == 0)
			d->pkcolname = realloc(d->pkcolname,
				npkcol * 2 * sizeof (char *));
#else	/* O(N^2) algorithm */
		d->pkcolname = realloc(d->pkcolname, npkcol * sizeof (char *));
#endif
		d->pkcolname[npkcol-1] = strdup(pkname);
	}
}

void dmodel_init_sql(struct dmodel *d)
{
	char *where_cl = 0;
	size_t where_cl_len;
	char *buf, *p;
	int i;

	p = where_cl = bufnew(BUFSIZ);
	for (i = 0; i < d->npkcol; i++) {
		p = bufcat2(&where_cl, p,
			(i == 0) ? " where " : " and ",
			d->pkcolname[i], "=?", 0);
	}
	p = bufcat(&where_cl, p, ";");
	if (!p) goto fail;
	where_cl_len = (size_t) (p - where_cl);

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "select");
	for (i = 0; i < d->npkcol; i++) {
		p = bufcat2(&buf, p,
			(i == 0) ? " " : ", ",
			d->pkcolname[i], 0);
	}
	p = bufcat(&buf, p, " from ");
	p = bufcat(&buf, p, d->qualified_name);
	p = bufcat(&buf, p, ";");
	d->select_pk_sql = buf;
	if (!p) goto fail;

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "insert into ");
	p = bufcat(&buf, p, d->qualified_name);
	p = bufcat(&buf, p, " values(");
	for (i = 0; i + 1 < d->ncol; i++) {
		p = bufcat(&buf, p, "?, ");
	}
	p = bufcat(&buf, p, "?);");
	d->insert_sql = buf;
	if (!p) goto fail;

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "select \"%w\" from ");
	p = bufcat(&buf, p, d->qualified_name);
	p = bufncat(&buf, p, where_cl, where_cl_len);
	d->select_sql = buf;
	if (!p) goto fail;

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "delete from ");
	p = bufcat(&buf, p, d->qualified_name);
	p = bufncat(&buf, p, where_cl, where_cl_len);
	d->delete_sql = buf;
	if (!p) goto fail;

	p = buf = bufnew(BUFSIZ);
	p = bufcat(&buf, p, "update ");
	p = bufcat(&buf, p, d->qualified_name);
	p = bufcat(&buf, p, " set \"%w\"=?");
	p = bufncat(&buf, p, where_cl, where_cl_len);
	d->update_sql = buf;
	if (!p) goto fail;

	bufdel(where_cl);
	return;
fail:
	bufdel(where_cl);
	bufdel(d->select_sql);
	bufdel(d->select_pk_sql);
	bufdel(d->insert_sql);
	bufdel(d->delete_sql);
	bufdel(d->update_sql);
	d->select_sql = d->select_pk_sql = d->insert_sql = d->delete_sql
		= d->update_sql = 0;
}

void dmodel_entry_free(struct dmodel_entry *ent, int npkcol)
{
	int i;

	for (i = 0; i < npkcol; i++) {
		sqlite3_value_free(ent->values[i]);
	}
	free(ent);
}

void dmodel_free(struct dmodel *d)
{
	int i;
	struct wbt_node *root;

	if (!d) return;

	free(d->qualified_name);
	
	/* clear generated SQL's */
	bufdel(d->select_sql);
	bufdel(d->select_pk_sql);
	bufdel(d->insert_sql);
	bufdel(d->delete_sql);
	bufdel(d->update_sql);

	/* clear PK columns names */
	for (i = 0; i < d->npkcol; i++)
		free(d->pkcolname[i]);
	free(d->pkcolname);

	/* clear entries */
	while ((root = d->entries->root)) {
		wbt_erase(root, d->entries);
		dmodel_entry_free(ENTRY(root), d->npkcol);
	}
	free(d);
}

sqlite3_value **
dmodel_add_entry(struct dmodel *d)
{
	struct dmodel_entry *ent;
	struct wbt_node *parent = 0, **link;

	ent = malloc(sizeof (struct dmodel_entry) + d->npkcol);
	wbt_init(&ent->node);
	ent->key = d->current_key++;

	link = &d->entries->root;
	while ((*link)) {
		parent = *link;
		/* special: newer key is always greater,
		 * no need to make a standard binary tree comparison */
		assert(ent->key > ENTRY(parent)->key);
		link = &parent->right;
	}
	wbt_link_node(&ent->node, parent, link);
	wbt_adjust_size(&ent->node, d->entries);
	return ent->values;	/* let the user fill the values */
}

sqlite3_value **
dmodel_get_entry(struct dmodel *d, size_t ord)
{
	struct dmodel_entry *ent;

	ent = ENTRY(wbt_select(ord, d->entries->root));
	return ent->values;	/* let the user update the values */
}

void
dmodel_del_entry(struct dmodel *d, size_t ord)
{
	struct wbt_node *node;

	node = wbt_select(ord, d->entries->root);
	wbt_erase(node, d->entries);
	dmodel_entry_free(ENTRY(node), d->npkcol);
}
