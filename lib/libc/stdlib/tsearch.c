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
 * $NetBSD: tsearch.c,v 1.3 1999/09/16 11:45:37 lukem Exp $
 * $FreeBSD: src/lib/libc/stdlib/tsearch.c,v 1.1.2.1 2000/08/17 07:38:39 jhb Exp $
 * $DragonFly: src/lib/libc/stdlib/tsearch.c,v 1.3 2003/09/06 08:19:16 asmodai Exp $
 */

#include <sys/cdefs.h>

#include <assert.h>
#define _SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

/* find or insert datum into search tree */
void *
tsearch(vkey, vrootp, compar)
	const void *vkey;		/* key to be located */
	void **vrootp;			/* address of tree root */
	int (*compar) (const void *, const void *);
{
	node_t *q;
	node_t **rootp = (node_t **)vrootp;

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {	/* Knuth's T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* we found it! */

		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}

	q = malloc(sizeof(node_t));		/* T5: key not found */
	if (q != 0) {				/* make new node */
		*rootp = q;			/* link new node to old */
		/* LINTED const castaway ok */
		q->key = (void *)vkey;		/* initialize new node */
		q->llink = q->rlink = NULL;
	}
	return q;
}
