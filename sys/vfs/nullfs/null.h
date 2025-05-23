/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)null.h	8.3 (Berkeley) 8/20/94
 *
 * $FreeBSD: src/sys/miscfs/nullfs/null.h,v 1.11.2.3 2001/06/26 04:20:09 bp Exp $
 * $DragonFly: src/sys/vfs/nullfs/null.h,v 1.6 2004/08/28 19:02:23 dillon Exp $
 */

struct null_args {
	char		*target;	/* Target of loopback  */
};

struct null_mount {
	struct mount	*nullm_vfs;
	struct vnode	*nullm_rootvp;	/* Reference to root null_node */
};

#ifdef _KERNEL
/*
 * A cache of vnode references
 */
struct null_node {
	struct null_node	*null_next;	/* Hash list */
	struct vnode	        *null_lowervp;	/* vrefed once */
	struct vnode		*null_vnode;	/* Back pointer */
};

#define	MOUNTTONULLMOUNT(mp) ((struct null_mount *)((mp)->mnt_data))
#define	VTONULL(vp) ((struct null_node *)(vp)->v_data)
#define	NULLTOV(xp) ((xp)->null_vnode)

int nullfs_init(struct vfsconf *vfsp);
int nullfs_uninit(struct vfsconf *vfsp);
int null_node_add(struct null_node *np);
void null_node_rem(struct null_node *np);
int null_node_create(struct mount *mp, struct vnode *target, struct vnode **vpp);
int null_bypass(struct vop_generic_args *ap);

#ifdef DIAGNOSTIC
struct vnode *null_checkvp(struct vnode *vp, char *fil, int lno);
#define	NULLVPTOLOWERVP(vp) null_checkvp((vp), __FILE__, __LINE__)
#else
#define	NULLVPTOLOWERVP(vp) (VTONULL(vp)->null_lowervp)
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NULLFSNODE);
#endif

#ifdef NULLFS_DEBUG
#define NULLFSDEBUG(format, args...) printf(format ,## args)
#else
#define NULLFSDEBUG(format, args...)
#endif /* NULLFS_DEBUG */

#endif /* _KERNEL */
