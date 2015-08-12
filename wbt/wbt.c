#include "wbt.h"
#include <assert.h>

struct wbt_node *
wbt_first(const struct wbt_root *tree)
{
	struct wbt_node *node;

	if ((node = tree->root))
		while (node->left)
			node = node->left;
	return node;
}

struct wbt_node *
wbt_last(const struct wbt_root *tree)
{
	struct wbt_node *node;

	if ((node = tree->root))
		while (node->right)
			node = node->right;
	return node;
}

struct wbt_node *
wbt_prev(struct wbt_node *node)
{
	struct wbt_node *parent;

	if ((parent = node->left)) {
		while ((node = parent->right))
			parent = node;
	} else {
		while ((parent = node->parent) && node == parent->left)
			node = parent;
	}
	return parent;
}

struct wbt_node *
wbt_next(struct wbt_node *node)
{
	struct wbt_node *parent;

	if ((parent = node->right)) {
		while ((node = parent->left))
			parent = node;
	} else {
		while ((parent = node->parent) && node == parent->right)
			node = parent;
	}
	return parent;
}

static void
wbt_rrot(struct wbt_node **link)
{
	struct wbt_node *x, *y = *link;
	size_t size;

	assert(WBT_SIZE(y) == WBT_SIZE(y->left) + WBT_SIZE(y->right) + 1);

	/* rotation */
	x = y->left;
	wbt_link_node(x->right, y, &y->left);
	wbt_link_node(x, y->parent, link);
	wbt_link_node(y, x, &x->right);

	/* update size info */
	size = y->size;
	y->size -= WBT_SIZE(x->left) + 1;
	x->size = size;
}

static void
wbt_lrot(struct wbt_node **link)
{
	struct wbt_node *x = *link, *y;
	size_t size;

	assert(WBT_SIZE(x) == WBT_SIZE(x->left) + WBT_SIZE(x->right) + 1);

	y = x->right;
	wbt_link_node(y->left, x, &x->right);
	wbt_link_node(y, x->parent, link);
	wbt_link_node(x, y, &y->left);

	size = x->size;
	x->size -= WBT_SIZE(y->right) + 1;
	y->size = size;
}

static void wbt_lbal(struct wbt_node **);
static void wbt_rbal(struct wbt_node **);

static void
wbt_rbal(struct wbt_node **link)
{
	struct wbt_node *x, *left;
	size_t size;

	x = *link;
	while ((left = x->left)) {
		if ((size = WBT_SIZE(x->right)) < WBT_SIZE(left->right)) {
			wbt_lrot(&x->left);
			wbt_rrot(&x);
		} else if (size < WBT_SIZE(left->left)) {
			wbt_rrot(&x);
		} else {
			break;
		}
		/* wbt_lbal(&x->right); */
	}
	*link = x;
}

static void
wbt_lbal(struct wbt_node **link)
{
	struct wbt_node *x, *right;
	size_t size;

	x = *link;
	while ((right = x->right)) {
		if ((size = WBT_SIZE(x->left)) < WBT_SIZE(right->left)) {
			wbt_rrot(&x->right);
			wbt_lrot(&x);
		} else if (size < WBT_SIZE(right->right)) {
			wbt_lrot(&x);
		} else {
			break;
		}
		/* This is necessary. See specialcase1.txt for an example. */
		/* wbt_rbal(&x->left); */
		/* XXX Since I did not observe any performance boost with this
		 * recursion call enabled, I commented it out.
		 * I cannot prove the time bound, but it seems sufficient. */
	}
	*link = x;
}

void
wbt_adjust_size(struct wbt_node *node, struct wbt_root *tree)
{
	struct wbt_node *parent;

	while ((parent = node->parent)) {
		struct wbt_node *grand = parent->parent;
		struct wbt_node **link;

		++parent->size;
		if (grand) {
			if (parent == grand->left) {
				link = &grand->left;
			} else {
				link = &grand->right;
			}
		} else {
			link = &tree->root;
		}
		(node == parent->left) ? wbt_rbal(link) : wbt_lbal(link);
		node = *link;
	}
}

static void
wbt_replace_node_x(
	struct wbt_node *old,
	struct wbt_node *parent,
	struct wbt_node **link,
	struct wbt_node *node)
{
	wbt_link_node(old->left, node, &node->left);
	wbt_link_node(old->right, node, &node->right);
	wbt_link_node(node, parent, link);
}

void
wbt_replace_node(
	struct wbt_node *old, struct wbt_node *node, struct wbt_root *tree)
{
	struct wbt_node **link, *parent;

	if ((parent = old->parent))
		link = (node == parent->left) ? &parent->left : &parent->right;
	else
		link = &tree->root;
	wbt_replace_node_x(old, parent, link, node);
}

void
wbt_erase(struct wbt_node *node, struct wbt_root *tree)
{
	struct wbt_node **link, *parent;

	if ((parent = node->parent))
		link = (node == parent->left) ? &parent->left : &parent->right;
	else
		link = &tree->root;

	if (!node->left) {
		wbt_link_node(node->right, parent, link);
	} else if (!node->right) {
		wbt_link_node(node->left, parent, link);
	} else {
		struct wbt_node *succ, *parent1, **link1;

		parent1 = node;
		link1 = &node->right;
		while ((succ = *link1)->left) {
			parent1 = succ;
			link1 = &succ->left;
		}
		wbt_link_node(succ->right, parent1, link1);
		wbt_replace_node_x(node, parent, link, succ);
		link = link1;
	}
}

struct wbt_node *
wbt_select(size_t rank, struct wbt_node *root)
{
	assert(rank < root->size);
	for (;;) {
		size_t lsize = WBT_SIZE(root->left);

		if (rank < lsize) {
			root = root->left;
		} else if (rank > lsize) {
			root = root->right;
			rank -= lsize + 1;
		} else {
			break;
		}
	}
	return root;
}

size_t
wbt_rank(const struct wbt_node *node)
{
	size_t rank = WBT_SIZE(node->left);
	const struct wbt_node *parent;

	while ((parent = node->parent)) {
		if (node == parent->right)
			rank += WBT_SIZE(parent->left) + 1;
		node = parent;
	}
	return rank;
}
