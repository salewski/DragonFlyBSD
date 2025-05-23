/*-
 * Copyright (c) 1982, 1986, 1989, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_node.c	8.2 (Berkeley) 1/23/94
 * $FreeBSD: src/sys/isofs/cd9660/cd9660_node.c,v 1.29.2.1 2000/07/08 14:35:56 bp Exp $
 * $DragonFly: src/sys/vfs/isofs/cd9660/cd9660_node.c,v 1.14 2005/02/14 16:11:42 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include "iso.h"
#include "cd9660_node.h"
#include "cd9660_mount.h"

#define CD9660_HASH_SIZE_LIMIT	8192

/*
 * Structures associated with iso_node caching.
 */
static struct iso_node **isohashtbl;
static u_long isohash;
#define	INOHASH(device, inum)	((minor(device) + ((inum)>>12)) & isohash)
#ifndef NULL_SIMPLELOCKS
static struct lwkt_token cd9660_ihash_token;
#endif

static void cd9660_ihashrem (struct iso_node *);
static unsigned	cd9660_chars2ui (unsigned char *begin, int len);

/*
 * Initialize hash links for inodes and dnodes.  CDs and DVDs are small
 * and slow compared to hard disks, there is no need to have a huge hash
 * table so the size is capped at CD9660_HASH_SIZE_LIMIT.
 */
int
cd9660_init(struct vfsconf *vfsp)
{
	int hlimit;

	if ((hlimit = desiredvnodes) < CD9660_HASH_SIZE_LIMIT)
		hlimit = CD9660_HASH_SIZE_LIMIT;

	isohash = 16;
	while (isohash < hlimit)
		isohash <<= 1;
	isohashtbl = malloc(sizeof(void *) * isohash,
			    M_ISOFSMNT, M_WAITOK|M_ZERO);
	--isohash;
	lwkt_token_init(&cd9660_ihash_token);
	return (0);
}

int
cd9660_uninit(struct vfsconf *vfsp)
{

	if (isohashtbl != NULL)
		free(isohashtbl, M_ISOFSMNT);
	return (0);
}


/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
struct vnode *
cd9660_ihashget(dev_t dev, ino_t inum)
{
	struct thread *td = curthread;		/* XXX */
	struct iso_node *ip;
	lwkt_tokref ilock;
	struct vnode *vp;

	lwkt_gettoken(&ilock, &cd9660_ihash_token);
loop:
	for (ip = isohashtbl[INOHASH(dev, inum)]; ip; ip = ip->i_next) {
		if (inum != ip->i_number || dev != ip->i_dev)
			continue;
		vp = ITOV(ip);
		if (vget(vp, LK_EXCLUSIVE, td))
			goto loop;
		/*
		 * We must check to see if the inode has been ripped
		 * out from under us after blocking.
		 */   
		for (ip = isohashtbl[INOHASH(dev, inum)]; ip; ip = ip->i_next) {
			if (inum == ip->i_number && dev == ip->i_dev)
				break;
		}
		if (ip == NULL || ITOV(ip) != vp) {
			goto loop;
		}
		lwkt_reltoken(&ilock);
		return (vp);
	}
	lwkt_reltoken(&ilock);
	return (NULL);
}

/*
 * Insert the inode into the hash table, return 0 on success, non-zero
 * if the inode has already been found to be in the hash table.
 */
int
cd9660_ihashins(struct iso_node *ip)
{
	struct iso_node **ipp, *iq;
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &cd9660_ihash_token);
	ipp = &isohashtbl[INOHASH(ip->i_dev, ip->i_number)];
	while ((iq = *ipp) != NULL) {
		if (iq->i_dev == ip->i_dev && iq->i_number == ip->i_number) {
			lwkt_reltoken(&ilock);
			return(EBUSY);
		}
		ipp = &iq->i_next;
	}
	ip->i_next = NULL;
	*ipp = ip;
	lwkt_reltoken(&ilock);
	return(0);
}

/*
 * Remove the inode from the hash table.
 */
static void
cd9660_ihashrem(struct iso_node *ip)
{
	struct iso_node **ipp, *iq;
	lwkt_tokref ilock;

	lwkt_gettoken(&ilock, &cd9660_ihash_token);
	ipp = &isohashtbl[INOHASH(ip->i_dev, ip->i_number)];
	while ((iq = *ipp) != NULL) {
		if (ip == iq)
			break;
		ipp = &iq->i_next;
	}
	KKASSERT(ip == iq);
	*ipp = ip->i_next;
	ip->i_next = NULL;
	lwkt_reltoken(&ilock);
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 *
 * cd9660_inactive(struct vnode *a_vp, struct thread *a_td)
 */
int
cd9660_inactive(struct vop_inactive_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct thread *td = ap->a_td;
	struct iso_node *ip = VTOI(vp);
	int error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("cd9660_inactive: pushing active", vp);

	if (ip)
		ip->i_flag = 0;
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (ip == NULL || ip->inode.iso_mode == 0)
		vrecycle(vp, td);
	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 *
 * cd9660_reclaim(struct vnode *a_vp, struct proc *a_p)
 */
int
cd9660_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct iso_node *ip = VTOI(vp);

	if (prtactive && vp->v_usecount != 0)
		vprint("cd9660_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	vp->v_data = NULL;
	if (ip) {
		cd9660_ihashrem(ip);
		if (ip->i_devvp) {
			vrele(ip->i_devvp);
			ip->i_devvp = 0;
		}
		free(ip, M_ISOFSNODE);
	}
	return (0);
}

/*
 * File attributes
 */
void
cd9660_defattr(struct iso_directory_record *isodir, struct iso_node *inop,
	       struct buf *bp, enum ISO_FTYPE ftype)
{
	struct buf *bp2 = NULL;
	struct iso_mnt *imp;
	struct iso_extended_attributes *ap = NULL;
	int off;

	/* high sierra does not have timezone data, flag is one byte ahead */
	if (isonum_711(ftype == ISO_FTYPE_HIGH_SIERRA?
		       &isodir->date[6]: isodir->flags)&2) {
		inop->inode.iso_mode = S_IFDIR;
		/*
		 * If we return 2, fts() will assume there are no subdirectories
		 * (just links for the path and .), so instead we return 1.
		 */
		inop->inode.iso_links = 1;
	} else {
		inop->inode.iso_mode = S_IFREG;
		inop->inode.iso_links = 1;
	}
	if (!bp
	    && ((imp = inop->i_mnt)->im_flags & ISOFSMNT_EXTATT)
	    && (off = isonum_711(isodir->ext_attr_length))) {
		cd9660_blkatoff(ITOV(inop), (off_t)-(off << imp->im_bshift), NULL,
			     &bp2);
		bp = bp2;
	}
	if (bp) {
		ap = (struct iso_extended_attributes *)bp->b_data;
		
		if (isonum_711(ap->version) == 1) {
			if (!(ap->perm[0]&0x40))
				inop->inode.iso_mode |= VEXEC >> 6;
			if (!(ap->perm[0]&0x10))
				inop->inode.iso_mode |= VREAD >> 6;
			if (!(ap->perm[0]&4))
				inop->inode.iso_mode |= VEXEC >> 3;
			if (!(ap->perm[0]&1))
				inop->inode.iso_mode |= VREAD >> 3;
			if (!(ap->perm[1]&0x40))
				inop->inode.iso_mode |= VEXEC;
			if (!(ap->perm[1]&0x10))
				inop->inode.iso_mode |= VREAD;
			inop->inode.iso_uid = isonum_723(ap->owner); /* what about 0? */
			inop->inode.iso_gid = isonum_723(ap->group); /* what about 0? */
		} else
			ap = NULL;
	}
	if (!ap) {
		inop->inode.iso_mode |= VREAD|VEXEC|(VREAD|VEXEC)>>3|(VREAD|VEXEC)>>6;
		inop->inode.iso_uid = (uid_t)0;
		inop->inode.iso_gid = (gid_t)0;
	}
	if (bp2)
		brelse(bp2);
}

/*
 * Time stamps
 */
void
cd9660_deftstamp(struct iso_directory_record *isodir, struct iso_node *inop,
		 struct buf *bp, enum ISO_FTYPE ftype)
{
	struct buf *bp2 = NULL;
	struct iso_mnt *imp;
	struct iso_extended_attributes *ap = NULL;
	int off;

	if (!bp
	    && ((imp = inop->i_mnt)->im_flags & ISOFSMNT_EXTATT)
	    && (off = isonum_711(isodir->ext_attr_length))) {
		cd9660_blkatoff(ITOV(inop), (off_t)-(off << imp->im_bshift), NULL,
			     &bp2);
		bp = bp2;
	}
	if (bp) {
		ap = (struct iso_extended_attributes *)bp->b_data;
		
		if (ftype != ISO_FTYPE_HIGH_SIERRA
		    && isonum_711(ap->version) == 1) {
			if (!cd9660_tstamp_conv17(ap->ftime,&inop->inode.iso_atime))
				cd9660_tstamp_conv17(ap->ctime,&inop->inode.iso_atime);
			if (!cd9660_tstamp_conv17(ap->ctime,&inop->inode.iso_ctime))
				inop->inode.iso_ctime = inop->inode.iso_atime;
			if (!cd9660_tstamp_conv17(ap->mtime,&inop->inode.iso_mtime))
				inop->inode.iso_mtime = inop->inode.iso_ctime;
		} else
			ap = NULL;
	}
	if (!ap) {
		cd9660_tstamp_conv7(isodir->date,&inop->inode.iso_ctime,ftype);
		inop->inode.iso_atime = inop->inode.iso_ctime;
		inop->inode.iso_mtime = inop->inode.iso_ctime;
	}
	if (bp2)
		brelse(bp2);
}

int
cd9660_tstamp_conv7(u_char *pi, struct timespec *pu, enum ISO_FTYPE ftype)
{
	int crtime, days;
	int y, m, d, hour, minute, second, tz;

	y = pi[0] + 1900;
	m = pi[1];
	d = pi[2];
	hour = pi[3];
	minute = pi[4];
	second = pi[5];
	if(ftype != ISO_FTYPE_HIGH_SIERRA)
		tz = pi[6];
	else
		/* original high sierra misses timezone data */
		tz = 0;

	if (y < 1970) {
		pu->tv_sec  = 0;
		pu->tv_nsec = 0;
		return 0;
	} else {
#ifdef	ORIGINAL
		/* computes day number relative to Sept. 19th,1989 */
		/* don't even *THINK* about changing formula. It works! */
		days = 367*(y-1980)-7*(y+(m+9)/12)/4-3*((y+(m-9)/7)/100+1)/4+275*m/9+d-100;
#else
		/*
		 * Changed :-) to make it relative to Jan. 1st, 1970
		 * and to disambiguate negative division
		 */
		days = 367*(y-1960)-7*(y+(m+9)/12)/4-3*((y+(m+9)/12-1)/100+1)/4+275*m/9+d-239;
#endif
		crtime = ((((days * 24) + hour) * 60 + minute) * 60) + second;

		/* timezone offset is unreliable on some disks */
		if (-48 <= tz && tz <= 52)
			crtime -= tz * 15 * 60;
	}
	pu->tv_sec  = crtime;
	pu->tv_nsec = 0;
	return 1;
}

static u_int
cd9660_chars2ui(u_char *begin, int len)
{
	u_int rc;
	
	for (rc = 0; --len >= 0;) {
		rc *= 10;
		rc += *begin++ - '0';
	}
	return rc;
}

int
cd9660_tstamp_conv17(u_char *pi, struct timespec *pu)
{
	u_char buf[7];
	
	/* year:"0001"-"9999" -> -1900  */
	buf[0] = cd9660_chars2ui(pi,4) - 1900;

	/* month: " 1"-"12"   -> 1 - 12 */
	buf[1] = cd9660_chars2ui(pi + 4,2);

	/* day:	  " 1"-"31"   -> 1 - 31 */
	buf[2] = cd9660_chars2ui(pi + 6,2);

	/* hour:  " 0"-"23"   -> 0 - 23 */
	buf[3] = cd9660_chars2ui(pi + 8,2);

	/* minute:" 0"-"59"   -> 0 - 59 */
	buf[4] = cd9660_chars2ui(pi + 10,2);

	/* second:" 0"-"59"   -> 0 - 59 */
	buf[5] = cd9660_chars2ui(pi + 12,2);

	/* difference of GMT */
	buf[6] = pi[16];

	return cd9660_tstamp_conv7(buf, pu, ISO_FTYPE_DEFAULT);
}

ino_t
isodirino(struct iso_directory_record *isodir, struct iso_mnt *imp)
{
	ino_t ino;

	ino = (isonum_733(isodir->extent) + isonum_711(isodir->ext_attr_length))
	      << imp->im_bshift;
	return (ino);
}
