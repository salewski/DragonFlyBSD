/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
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
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 * $FreeBSD: src/sys/ufs/ufs/ufs_ihash.c,v 1.20 1999/08/28 00:52:29 peter Exp $
 * $DragonFly: src/sys/vfs/ufs/ufs_ihash.c,v 1.15 2005/01/20 18:08:54 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include "quota.h"
#include "inode.h"
#include "ufs_extern.h"

static MALLOC_DEFINE(M_UFSIHASH, "UFS ihash", "UFS Inode hash tables");
/*
 * Structures associated with inode cacheing.
 */
static struct inode **ihashtbl;
static u_long	ihash;		/* size of hash table - 1 */
static struct lwkt_token ufs_ihash_token;

#define	INOHASH(device, inum)	(&ihashtbl[(minor(device) + (inum)) & ihash])

/*
 * Initialize inode hash table.
 */
void
ufs_ihashinit(void)
{
	ihash = 16;
	while (ihash < desiredvnodes)
		ihash <<= 1;
	ihashtbl = malloc(sizeof(void *) * ihash, M_UFSIHASH, M_WAITOK|M_ZERO);
	--ihash;
	lwkt_token_init(&ufs_ihash_token);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct vnode *
ufs_ihashlookup(dev_t dev, ino_t inum)
{
	struct inode *ip;
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &ufs_ihash_token);
	for (ip = *INOHASH(dev, inum); ip; ip = ip->i_next) {
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	}
	lwkt_reltoken(&ilock);
	if (ip)
		return (ITOV(ip));
	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 *
 * Note that the serializing tokens do not prevent other processes from
 * playing with the data structure being protected while we are blocked.
 * They do protect us from preemptive interrupts which might try to
 * play with the protected data structure.
 */
struct vnode *
ufs_ihashget(dev_t dev, ino_t inum)
{
	struct thread *td = curthread;	/* XXX */
	lwkt_tokref ilock;
	struct inode *ip;
	struct vnode *vp;

	lwkt_gettoken(&ilock, &ufs_ihash_token);
loop:
	for (ip = *INOHASH(dev, inum); ip; ip = ip->i_next) {
		if (inum != ip->i_number || dev != ip->i_dev)
			continue;
		vp = ITOV(ip);
		if (vget(vp, LK_EXCLUSIVE, td))
			goto loop;
		/*
		 * We must check to see if the inode has been ripped
		 * out from under us after blocking.
		 */
		for (ip = *INOHASH(dev, inum); ip; ip = ip->i_next) {
			if (inum == ip->i_number && dev == ip->i_dev)
				break;
		}
		if (ip == NULL || ITOV(ip) != vp) {
			vput(vp);
			goto loop;
		}
		lwkt_reltoken(&ilock);
		return (vp);
	}
	lwkt_reltoken(&ilock);
	return (NULL);
}

/*
 * Check to see if an inode is in the hash table.  This is used to interlock
 * file free operations to ensure that the vnode is not reused due to a
 * reallocate of its inode number before we have had a chance to recycle it.
 */
int
ufs_ihashcheck(dev_t dev, ino_t inum)
{
	lwkt_tokref ilock;
	struct inode *ip;

	lwkt_gettoken(&ilock, &ufs_ihash_token);
	for (ip = *INOHASH(dev, inum); ip; ip = ip->i_next) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			if (ip->i_vnode) {
			    printf("conflict with vnode %p", ip->i_vnode);
			    if (TAILQ_FIRST(&ip->i_vnode->v_namecache))
				printf(" ncp %s", TAILQ_FIRST(&ip->i_vnode->v_namecache)->nc_name);
			    printf("\n");
			}
			break;
		}
	}
	lwkt_reltoken(&ilock);
	return(ip ? 1 : 0);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
int
ufs_ihashins(struct inode *ip)
{
	struct inode **ipp;
	struct inode *iq;
	lwkt_tokref ilock;

	KKASSERT((ip->i_flag & IN_HASHED) == 0);
	lwkt_gettoken(&ilock, &ufs_ihash_token);
	ipp = INOHASH(ip->i_dev, ip->i_number);
	while ((iq = *ipp) != NULL) {
		if (ip->i_dev == iq->i_dev && ip->i_number == iq->i_number) {
			lwkt_reltoken(&ilock);
			return(EBUSY);
		}
		ipp = &iq->i_next;
	}
	ip->i_next = NULL;
	*ipp = ip;
	ip->i_flag |= IN_HASHED;
	lwkt_reltoken(&ilock);
	return(0);
}

/*
 * Remove the inode from the hash table.
 */
void
ufs_ihashrem(struct inode *ip)
{
	lwkt_tokref ilock;
	struct inode **ipp;
	struct inode *iq;

	lwkt_gettoken(&ilock, &ufs_ihash_token);
	if (ip->i_flag & IN_HASHED) {
		ipp = INOHASH(ip->i_dev, ip->i_number);
		while ((iq = *ipp) != NULL) {
			if (ip == iq)
				break;
			ipp = &iq->i_next;
		}
		KKASSERT(ip == iq);
		*ipp = ip->i_next;
		ip->i_next = NULL;
		ip->i_flag &= ~IN_HASHED;
	}
	lwkt_reltoken(&ilock);
}

