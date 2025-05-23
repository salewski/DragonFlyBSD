/*	$NetBSD: ntfs_ihash.c,v 1.5 1999/09/30 16:56:40 jdolecek Exp $	*/

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
 * $FreeBSD: src/sys/ntfs/ntfs_ihash.c,v 1.7 1999/12/03 20:37:39 semenu Exp $
 * $DragonFly: src/sys/vfs/ntfs/ntfs_ihash.c,v 1.9 2004/04/20 19:59:30 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mount.h>

#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_ihash.h"

MALLOC_DEFINE(M_NTFSNTHASH, "NTFS nthash", "NTFS ntnode hash tables");

/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(nthashhead, ntnode) *ntfs_nthashtbl;
static u_long	ntfs_nthash;		/* size of hash table - 1 */
#define	NTNOHASH(device, inum)	(&ntfs_nthashtbl[(minor(device) + (inum)) & ntfs_nthash])
static struct lwkt_token ntfs_nthash_slock;
struct lock ntfs_hashlock;

/*
 * Initialize inode hash table.
 */
void
ntfs_nthashinit(void)
{
	lockinit(&ntfs_hashlock, 0, "ntfs_nthashlock", 0, 0);
	ntfs_nthashtbl = HASHINIT(desiredvnodes, M_NTFSNTHASH, M_WAITOK,
	    &ntfs_nthash);
	lwkt_token_init(&ntfs_nthash_slock);
}

/*
 * Free the inode hash table.
 */
int
ntfs_nthash_uninit(struct vfsconf *vfc)
{
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &ntfs_nthash_slock);
	if (ntfs_nthashtbl)
		free(ntfs_nthashtbl, M_NTFSNTHASH);
	lwkt_reltoken(&ilock);

	return 0;
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
struct ntnode *
ntfs_nthashlookup(dev_t dev, ino_t inum)
{
	struct ntnode *ip;
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &ntfs_nthash_slock);
	for (ip = NTNOHASH(dev, inum)->lh_first; ip; ip = ip->i_hash.le_next) {
		if (inum == ip->i_number && dev == ip->i_dev)
			break;
	}
	lwkt_reltoken(&ilock);

	return (ip);
}

/*
 * Insert the ntnode into the hash table.
 */
void
ntfs_nthashins(struct ntnode *ip)
{
	struct nthashhead *ipp;
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &ntfs_nthash_slock);
	ipp = NTNOHASH(ip->i_dev, ip->i_number);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	lwkt_reltoken(&ilock);
}

/*
 * Remove the inode from the hash table.
 */
void
ntfs_nthashrem(struct ntnode *ip)
{
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &ntfs_nthash_slock);
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
#ifdef DIAGNOSTIC
		ip->i_hash.le_next = NULL;
		ip->i_hash.le_prev = NULL;
#endif
	}
	lwkt_reltoken(&ilock);
}
