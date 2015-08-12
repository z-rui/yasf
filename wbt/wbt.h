#ifndef WBT_H
#define WBT_H

#include <stddef.h>

struct wbt_root {
	struct wbt_node *root;
};

struct wbt_node {
	size_t size;
	struct wbt_node *parent, *left, *right;
};

extern struct wbt_node *wbt_first(const struct wbt_root *);
extern struct wbt_node *wbt_last(const struct wbt_root *);
extern struct wbt_node *wbt_prev(struct wbt_node *);
extern struct wbt_node *wbt_next(struct wbt_node *);

extern struct wbt_node *wbt_select(size_t, struct wbt_node *);
extern size_t wbt_rank(const struct wbt_node *);

extern void wbt_adjust_size(struct wbt_node *, struct wbt_root *);
extern void wbt_replace_node(
	struct wbt_node *, struct wbt_node *, struct wbt_root *);
extern void wbt_erase(struct wbt_node *, struct wbt_root *);

#define WBT_SIZE(x) ((x) ? (x)->size : 0)
#define WBT_ROOT {0}

static inline void
wbt_init(struct wbt_node *root)
{
	root->size = 1;
	root->left = root->right = 0;
}

static inline void
wbt_link_node(
	struct wbt_node *node, struct wbt_node *parent, struct wbt_node **link)
{
	if ((*link = node))
		node->parent = parent;
}

#endif
