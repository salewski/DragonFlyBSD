
/*
 * Copyright 1998 Marshall Kirk McKusick. All Rights Reserved.
 *
 * The soft updates code is derived from the appendix of a University
 * of Michigan technical report (Gregory R. Ganger and Yale N. Patt,
 * "Soft Updates: A Solution to the Metadata Update Problem in File
 * Systems", CSE-TR-254-95, August 1995).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. None of the names of McKusick, Ganger, or the University of Michigan
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)ffs_softdep_stub.c	9.1 (McKusick) 7/10/97
 * $FreeBSD: src/sys/ufs/ffs/ffs_softdep_stub.c,v 1.7.2.1 2000/12/28 11:01:45 ps Exp $
 * $DragonFly: src/sys/vfs/ufs/ffs_softdep_stub.c,v 1.6 2004/07/18 19:43:48 drhodus Exp $
 */

/* 
 * Use this file as ffs_softdep.c if you do not wish the real ffs_softdep.c
 * to be included in your system. (e.g for legal reasons )
 * The real files are in /usr/src/contrib/sys/softupdates.
 * You must copy them here before you can use soft updates.
 * Read the README for legal and technical information.
 */

#include "opt_ffs.h"
#if (SOFTUPDATES == 0 ) /* SOFTUPDATES not configured in, use these stubs. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include "quota.h"
#include "inode.h"
#include "ffs_extern.h"
#include "ufs_extern.h"

int
softdep_flushfiles(struct mount *oldmnt, int flags, thread_t td)
{
	panic("softdep_flushfiles called");
}

int
softdep_mount(struct vnode *devvp, struct mount *mp, struct fs *fs)
{
	return (0);
}

void 
softdep_initialize(void)
{
	/* empty */
}

void
softdep_setup_inomapdep(struct buf *bp, struct inode *ip, ino_t newinum)
{
	panic("softdep_setup_inomapdep called");
}

void
softdep_setup_blkmapdep(struct buf *bp, struct fs *fs, ufs_daddr_t newblkno)
{
	panic("softdep_setup_blkmapdep called");
}

void 
softdep_setup_allocdirect(struct inode *ip, ufs_lbn_t lbn,
			  ufs_daddr_t newblkno, ufs_daddr_t oldblkno,
			  long newsize, long oldsize, struct buf *bp)
{	
	panic("softdep_setup_allocdirect called");
}

void
softdep_setup_allocindir_page(struct inode *ip, ufs_lbn_t lbn, struct buf *bp,
			      int ptrno, ufs_daddr_t newblkno,
			      ufs_daddr_t oldblkno, struct buf *nbp)
{
	panic("softdep_setup_allocindir_page called");
}

void
softdep_setup_allocindir_meta(struct buf *nbp, struct inode *ip, struct buf *bp,
			      int ptrno, ufs_daddr_t newblkno)
{
	panic("softdep_setup_allocindir_meta called");
}

void
softdep_setup_freeblocks(struct inode *ip, off_t length)
{
	panic("softdep_setup_freeblocks called");
}

/* XXX needed to change this for FreeBSD.. hit poul */
void
softdep_freefile(struct vnode *pvp, ino_t ino, int mode)
{
	panic("softdep_freefile called");
}

void 
softdep_setup_directory_add(struct buf *bp, struct inode *dp, off_t diroffset,
			    ino_t newinum, struct buf *newdirbp)
{
	panic("softdep_setup_directory_add called");
}

void 
softdep_change_directoryentry_offset(struct inode *dp, caddr_t base,
				     caddr_t oldloc, caddr_t newloc,
				     int entrysize)
{
	panic("softdep_change_directoryentry_offset called");
}

void 
softdep_setup_remove(struct buf *bp, struct inode *dp, struct inode *ip,
		     int isrmdir)
{
	panic("softdep_setup_remove called");
}

void 
softdep_setup_directory_change(struct buf *bp, struct inode *dp,
			       struct inode *ip, ino_t newinum, int isrmdir)
{
	panic("softdep_setup_directory_change called");
}

void
softdep_change_linkcnt(struct inode *ip)
{
	panic("softdep_change_linkcnt called");
}

void 
softdep_load_inodeblock(struct inode *ip)
{
	panic("softdep_load_inodeblock called");
}

void 
softdep_update_inodeblock(struct inode *ip, struct buf *bp, int waitfor)
{
	panic("softdep_update_inodeblock called");
}

void
softdep_fsync_mountdev(struct vnode *vp)
{
	return;
}

/*
 * softdep_sync_metadata(struct vnode *a_vp, struct ucred *a_cred,
 *			 int a_waitfor, struct proc *a_p)
 */
int
softdep_sync_metadata(struct vop_fsync_args *ap)
{
	return (0);
}

int
softdep_slowdown(struct vnode *vp)
{
	panic("softdep_slowdown called");
}
#endif	/* SOFTUPDATES not configured in */
