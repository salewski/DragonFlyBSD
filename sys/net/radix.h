/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)radix.h	8.2 (Berkeley) 10/31/94
 * $FreeBSD: src/sys/net/radix.h,v 1.16.2.1 2000/05/03 19:17:11 wollman Exp $
 * $DragonFly: src/sys/net/radix.h,v 1.9 2005/02/28 11:31:20 hsu Exp $
 */

#ifndef _RADIX_H_
#define	_RADIX_H_

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_RTABLE);
#endif

/*
 * Radix search tree node layout.
 */

struct radix_node {
	struct	radix_mask *rn_mklist;	/* masks contained in subtree */
	struct	radix_node *rn_parent;	/* parent */
	short	rn_bit;			/* bit offset; -1-index(netmask) */
	char	rn_bmask;		/* node: mask for bit test*/
	u_char	rn_flags;		/* enumerated next */
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {			/* leaf only data: */
			char   *rn_Key;		/* object of search */
			char   *rn_Mask;	/* netmask, if present */
			struct	radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			/* node only data: */
			int	rn_Off;		/* where to start compare */
			struct	radix_node *rn_L;/* progeny */
			struct	radix_node *rn_R;/* progeny */
		} rn_node;
	}		rn_u;
#ifdef RN_DEBUG
	int rn_info;
	struct radix_node *rn_twin;
	struct radix_node *rn_ybro;
#endif
};

#define	rn_dupedkey	rn_u.rn_leaf.rn_Dupedkey
#define	rn_key		rn_u.rn_leaf.rn_Key
#define	rn_mask		rn_u.rn_leaf.rn_Mask
#define	rn_offset	rn_u.rn_node.rn_Off
#define	rn_left		rn_u.rn_node.rn_L
#define	rn_right	rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

struct radix_mask {
	short	rm_bit;			/* bit offset; -1-index(netmask) */
	char	rm_unused;		/* cf. rn_bmask */
	u_char	rm_flags;		/* cf. rn_flags */
	struct	radix_mask *rm_next;	/* list of more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}	rm_rmu;
	int	rm_refs;		/* # of references to this struct */
};

#define	rm_mask rm_rmu.rmu_mask
#define	rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */

typedef int walktree_f_t (struct radix_node *, void *);

struct radix_node_head {
	struct	radix_node *rnh_treetop;

	/* add based on sockaddr */
	struct	radix_node *(*rnh_addaddr)
		    (char *key, char *mask,
		     struct radix_node_head *head, struct radix_node nodes[]);

	/* remove based on sockaddr */
	struct	radix_node *(*rnh_deladdr)
		    (char *key, char *mask, struct radix_node_head *head);

	/* locate based on sockaddr */
	struct	radix_node *(*rnh_matchaddr)
		    (char *key, struct radix_node_head *head);

	/* locate based on sockaddr */
	struct	radix_node *(*rnh_lookup)
		    (char *key, char *mask, struct radix_node_head *head);

	/* traverse tree */
	int	(*rnh_walktree)
		    (struct radix_node_head *head, walktree_f_t *f, void *w);

	/* traverse tree below a */
	int	(*rnh_walktree_from)
		    (struct radix_node_head *head, char *a, char *m,
		     walktree_f_t *f, void *w);

	/*
	 * Do something when the last ref drops.
	 * A (*rnh_close)() routine
	 *	can clear RTF_UP
	 *	can remove a route from the radix tree
	 *	cannot change the reference count
	 *	cannot deallocate the route
	 */
	void	(*rnh_close)
		    (struct radix_node *rn, struct radix_node_head *head);

	struct	radix_node rnh_nodes[3];	/* empty tree for common case */

	/* unused entries */
	int	rnh_addrsize;		/* permit, but not require fixed keys */
	int	rnh_pktsize;		/* permit, but not require fixed keys */
	struct	radix_node *(*rnh_addpkt)	/* add based on packet hdr */
		    (void *v, char *mask,
		     struct radix_node_head *head, struct radix_node nodes[]);
	struct	radix_node *(*rnh_delpkt)	/* remove based on packet hdr */
		    (void *v, char *mask, struct radix_node_head *head);
	struct	radix_node *(*rnh_matchpkt)	/* locate based on packet hdr */
		    (void *v, struct radix_node_head *head);
};

#ifndef _KERNEL
#include <stdbool.h>
#define boolean_t bool
#define R_Malloc(p, t, n) (p = (t) malloc((n)))
#define Free(p) free(p);
#else
#define R_Malloc(p, t, n) (p = (t) malloc((n), M_RTABLE, M_INTWAIT | M_NULLOK))
#define Free(p) free(p, M_RTABLE);
#endif

void			 rn_init (void);
int			 rn_inithead (void **, int);
boolean_t		 rn_refines (char *, char *);
struct radix_node	*rn_addmask (char *, boolean_t, int),
			*rn_addroute (char *, char *, struct radix_node_head *,
				      struct radix_node [2]),
			*rn_delete (char *, char *, struct radix_node_head *),
			*rn_lookup (char *key, char *mask,
				    struct radix_node_head *head),
			*rn_match (char *, struct radix_node_head *);

#endif /* _RADIX_H_ */
