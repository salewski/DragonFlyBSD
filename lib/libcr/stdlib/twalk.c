/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 *
 * $NetBSD: twalk.c,v 1.1 1999/02/22 10:33:16 christos Exp $
 * $FreeBSD: src/lib/libc/stdlib/twalk.c,v 1.1.2.1 2000/08/17 07:38:39 jhb Exp $
 * $DragonFly: src/lib/libcr/stdlib/Attic/twalk.c,v 1.3 2003/11/12 20:21:29 eirikn Exp $
 */

#include <sys/cdefs.h>

#include <assert.h>
#define _SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

static void trecurse (const node_t *,
    void  (*action)(const void *, VISIT, int), int level);

/* Walk the nodes of a tree */
static void
trecurse(root, action, level)
	const node_t *root;	/* Root of the tree to be walked */
	void (*action) (const void *, VISIT, int);
	int level;
{

	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			trecurse(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			trecurse(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

/* Walk the nodes of a tree */
void
twalk(vroot, action)
	const void *vroot;	/* Root of the tree to be walked */
	void (*action) (const void *, VISIT, int);
{
	if (vroot != NULL && action != NULL)
		trecurse(vroot, action, 0);
}
