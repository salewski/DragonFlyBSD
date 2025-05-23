/* 
 * Copyright (c) 2000 Peter Edwards
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by Peter Edwards
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
 * $FreeBSD: src/usr.bin/fstat/msdosfs.c,v 1.1.2.2 2001/11/21 10:49:37 dwmalone Exp $
 * $DragonFly: src/usr.bin/fstat/msdosfs.c,v 1.5 2003/10/04 20:36:44 hmp Exp $
 */

#define	_KERNEL_STRUCTURES

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include <sys/mount.h>
#include <vfs/msdosfs/bpb.h>
#include <vfs/msdosfs/msdosfsmount.h>

#include <vfs/msdosfs/denode.h>
#include <vfs/msdosfs/direntry.h>
#include <vfs/msdosfs/fat.h>

#include <err.h>
#include <kvm.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * XXX -
 * VTODE is defined in denode.h only if _KERNEL is defined, but that leads to
 * header explosion
 */
#define VTODE(vp) ((struct denode *)(vp)->v_data)

#include "fstat.h"

struct dosmount {
	struct dosmount *next;
	struct msdosfsmount *kptr;	/* Pointer in kernel space */
	struct msdosfsmount data;	/* User space copy of structure */
};

int
msdosfs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct denode denode;
	static struct dosmount *mounts;
	struct dosmount *mnt;
	u_long dirsperblk;
	int fileid;

	if (!KVM_READ(VTODE(vp), &denode, sizeof (denode))) {
		dprintf(stderr, "can't read denode at %p for pid %d\n",
		    (void *)VTODE(vp), Pid);
		return 0;
	}

	/*
	 * Find msdosfsmount structure for the vnode's filesystem. Needed
	 * for some filesystem parameters
	 */
	for (mnt = mounts; mnt; mnt = mnt->next)
		if (mnt->kptr == denode.de_pmp)
			break;

	if (!mnt) {
		if ((mnt = malloc(sizeof(struct dosmount))) == NULL)
			err(1, NULL);
		mnt->next = mounts;
		mounts = mnt;
		mnt->kptr = denode.de_pmp;
		if (!KVM_READ(denode.de_pmp, &mnt->data, sizeof mnt->data)) {
			dprintf(stderr,
			    "can't read mount info at %p for pid %d\n",
			    (void *)denode.de_pmp, Pid);
			return 0;
		}
	}

	fsp->fsid = dev2udev(denode.de_dev);
	fsp->mode = 0555;
	fsp->mode |= denode.de_Attributes & ATTR_READONLY ? 0 : 0222;
	fsp->mode &= mnt->data.pm_mask;

	/* Distinguish directories and files. No "special" files in FAT. */
	fsp->mode |= denode.de_Attributes & ATTR_DIRECTORY ? S_IFDIR : S_IFREG;

	fsp->size = denode.de_FileSize;
	fsp->rdev = denode.de_dev;

	/*
	 * XXX -
	 * Culled from msdosfs_vnops.c. There appears to be a problem
	 * here, in that a directory has the same inode number as the first
	 * file in the directory. stat(2) suffers from this problem also, so
	 * I won't try to fix it here.
	 * 
	 * The following computation of the fileid must be the same as that
	 * used in msdosfs_readdir() to compute d_fileno. If not, pwd
	 * doesn't work.
	 */
	dirsperblk = mnt->data.pm_BytesPerSec / sizeof(struct direntry);
	if (denode.de_Attributes & ATTR_DIRECTORY) {
		fileid = cntobn(&mnt->data, denode.de_StartCluster)
		    * dirsperblk;
		if (denode.de_StartCluster == MSDOSFSROOT)
			fileid = 1;
	} else {
		fileid = cntobn(&mnt->data, denode.de_dirclust) * dirsperblk;
		if (denode.de_dirclust == MSDOSFSROOT)
			fileid = roottobn(&mnt->data, 0) * dirsperblk;
		fileid += denode.de_diroffset / sizeof(struct direntry);
	}

	fsp->fileid = fileid;
	return 1;
}
