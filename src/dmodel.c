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
	d->npkcol = 0;
	d->pkcolname = 0;
	d->current_key = 0;
	d->entries->root = 0;
	return d;
}

void dmodel_add_pkcol(struct dmodel *d, const char *pkname)
{
	int npkcol;

	npkcol = ++d->npkcol;
#if 0	/* O(N) algorithm */
	if ((npkcol & (npkcol - 1)) == 0)
		d->pkcolname = realloc(d->pkcolname,
			npkcol * 2 * sizeof (char *));
#else	/* O(N^2) algorithm */
	d->pkcolname = realloc(d->pkcolname, npkcol * sizeof (char *));
#endif
	d->pkcolname[npkcol-1] = strdup(pkname);
}

void dmodel_init_sql(struct dmodel *d)
{
	char *where_cl;
	char *select_sql, *select_pk_sql, *insert_sql, *delete_sql, *update_sql;
	char *p;
	int i;
	size_t where_cl_len;

	where_cl = 0;
	select_sql = select_pk_sql = insert_sql = delete_sql = update_sql = 0;

	p = select_pk_sql = bufnew(BUFSIZ);
	p = bufcat(&select_pk_sql, p, "select");
	for (i = 0; i < d->npkcol; i++) {
		p = bufcat2(&select_pk_sql, p,
			(i == 0) ? " " : ", ",
			d->pkcolname[i],
			" from ", 0);
		p = bufcat(&select_pk_sql, p, d->qualified_name);
	}
	p = bufcat(&select_pk_sql, p, ";");
	if (!p) goto fail;

	p = insert_sql = bufnew(BUFSIZ);
	p = bufcat(&insert_sql, p, "insert into ");
	p = bufcat(&insert_sql, p, d->qualified_name);
	for (i = 0; i < d->npkcol; i++) {
		p = bufcat2(&insert_sql, p,
			(i == 0) ? "(" : ", ",
			d->pkcolname[i], 0);
	}
	p = bufcat(&insert_sql, p, ") values(");
	for (i = 0; i + 1 < d->npkcol; i++) {
		p = bufcat(&insert_sql, p, "?, ");
	}
	p = bufcat(&insert_sql, p, "?);");
	if (!p) goto fail;

	p = where_cl = bufnew(BUFSIZ);
	for (i = 0; i < d->npkcol; i++) {
		p = bufcat2(&where_cl, p,
			(i == 0) ? " where " : " and ",
			d->pkcolname[i], "=?", 0);
	}
	p = bufcat(&where_cl, p, ";");
	if (!p) goto fail;
	where_cl_len = (size_t) (p - where_cl);

	p = select_sql = bufnew(BUFSIZ);
	p = bufcat(&select_sql, p, "select \"%w\" from ");
	p = bufcat(&select_sql, p, d->qualified_name);
	p = bufncat(&select_sql, p, where_cl, where_cl_len);
	if (!p) goto fail;

	p = delete_sql = bufnew(BUFSIZ);
	p = bufcat(&delete_sql, p, "delete from ");
	p = bufcat(&delete_sql, p, d->qualified_name);
	p = bufncat(&delete_sql, p, where_cl, where_cl_len);
	if (!p) goto fail;

	p = update_sql = bufnew(BUFSIZ);
	p = bufcat(&update_sql, p, "update ");
	p = bufcat(&update_sql, p, d->qualified_name);
	p = bufcat(&update_sql, p, " set \"%w\"=?");
	p = bufncat(&update_sql, p, where_cl, where_cl_len);
	if (!p) goto fail;

	bufdel(where_cl);
	d->select_sql = select_sql;
	d->select_pk_sql = select_pk_sql;
	d->insert_sql = insert_sql;
	d->delete_sql = delete_sql;
	d->update_sql = update_sql;
	return;
fail:
	bufdel(where_cl);
	bufdel(select_sql);
	bufdel(select_pk_sql);
	bufdel(insert_sql);
	bufdel(delete_sql);
	bufdel(update_sql);
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
