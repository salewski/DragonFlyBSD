/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*	
 * Copyright (c) 1989, 1991, 1993, 1994	
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
 *	@(#)ffs_vfsops.c	8.8 (Berkeley) 4/18/94
 *	$FreeBSD: src/sys/gnu/ext2fs/ext2_vfsops.c,v 1.63.2.7 2002/07/01 00:18:51 iedowse Exp $
 *	$DragonFly: src/sys/vfs/gnu/ext2fs/ext2_vfsops.c,v 1.26 2005/02/02 21:34:18 joerg Exp $
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/nlookup.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/buf2.h>

#include <vfs/ufs/quota.h>
#include <vfs/ufs/ufsmount.h>
#include <vfs/ufs/inode.h>
#include <vfs/ufs/ufs_extern.h>

#include <vm/vm_zone.h>

#include "fs.h"
#include "ext2_extern.h"
#include "ext2_fs.h"
#include "ext2_fs_sb.h"

extern struct vnodeopv_entry_desc ext2_vnodeop_entries[];
extern struct vnodeopv_entry_desc ext2_specop_entries[];
extern struct vnodeopv_entry_desc ext2_fifoop_entries[];

static int ext2_fhtovp (struct mount *, struct fid *, struct vnode **);
static int ext2_flushfiles (struct mount *mp, int flags, struct thread *td);
static int ext2_mount (struct mount *, char *, caddr_t, struct thread *);
static int ext2_mountfs (struct vnode *, struct mount *, struct thread *);
static int ext2_reload (struct mount *mountp, struct ucred *cred,
			struct thread *p);
static int ext2_sbupdate (struct ufsmount *, int);
static int ext2_statfs (struct mount *, struct statfs *, struct thread *);
static int ext2_sync (struct mount *, int, struct thread *);
static int ext2_unmount (struct mount *, int, struct thread *);
static int ext2_vget (struct mount *, ino_t, struct vnode **);
static int ext2_vptofh (struct vnode *, struct fid *);

static MALLOC_DEFINE(M_EXT2NODE, "EXT2 node", "EXT2 vnode private part");

static struct vfsops ext2fs_vfsops = {
	ext2_mount,
	ufs_start,		/* empty function */
	ext2_unmount,
	ufs_root,		/* root inode via vget */
	ufs_quotactl,		/* does operations associated with quotas */
	ext2_statfs,
	ext2_sync,
	ext2_vget,
	ext2_fhtovp,
	ufs_check_export,
	ext2_vptofh,
	ext2_init,
	vfs_stduninit,
	vfs_stdextattrctl,
};

VFS_SET(ext2fs_vfsops, ext2fs, 0);
#define bsd_malloc malloc
#define bsd_free free

static int ext2fs_inode_hash_lock;

static int	ext2_check_sb_compat (struct ext2_super_block *es,
					  dev_t dev, int ronly);
static int	compute_sb_data (struct vnode * devvp,
				     struct ext2_super_block * es,
				     struct ext2_sb_info * fs);

/*
 * VFS Operations.
 *
 * mount system call
 */
static int
ext2_mount(struct mount *mp, char *path,
	   caddr_t data,	/* this is actually a (struct ufs_args *) */
	   struct thread *td)
{
	struct vnode *devvp;
	struct ufs_args args;
	struct ufsmount *ump = 0;
	struct ext2_sb_info *fs;
	size_t size;
	int error, flags;
	mode_t accessmode;
	struct ucred *cred;
	struct nlookupdata nd;

	if ((error = copyin(data, (caddr_t)&args, sizeof (struct ufs_args))) != 0)
		return (error);

	cred = td->td_proc->p_ucred;
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
		error = 0;
		if (fs->s_rd_only == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp, LK_NOWAIT, NULL, td))
				return (EBUSY);
			error = ext2_flushfiles(mp, flags, td);
			vfs_unbusy(mp, td);
			if (!error && fs->s_wasvalid) {
				fs->s_es->s_state |= EXT2_VALID_FS;
				ext2_sbupdate(ump, MNT_WAIT);
			}
			fs->s_rd_only = 1;
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			error = ext2_reload(mp, proc0.p_ucred, td);
		if (error)
			return (error);
		devvp = ump->um_devvp;
		if (ext2_check_sb_compat(fs->s_es, devvp->v_rdev,
		    (mp->mnt_kern_flag & MNTK_WANTRDWR) == 0) != 0)
			return (EPERM);
		if (fs->s_rd_only && (mp->mnt_kern_flag & MNTK_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (cred->cr_uid != 0) {
				vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
				if ((error = VOP_ACCESS(devvp, VREAD | VWRITE,
				    cred, td)) != 0) {
					VOP_UNLOCK(devvp, 0, td);
					return (error);
				}
				VOP_UNLOCK(devvp, 0, td);
			}

			if ((fs->s_es->s_state & EXT2_VALID_FS) == 0 ||
			    (fs->s_es->s_state & EXT2_ERROR_FS)) {
				if (mp->mnt_flag & MNT_FORCE) {
					printf(
"WARNING: %s was not properly dismounted\n",
					    fs->fs_fsmnt);
				} else {
					printf(
"WARNING: R/W mount of %s denied.  Filesystem is not clean - run fsck\n",
					    fs->fs_fsmnt);
					return (EPERM);
				}
			}
			fs->s_es->s_state &= ~EXT2_VALID_FS;
			ext2_sbupdate(ump, MNT_WAIT);
			fs->s_rd_only = 0;
		}
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &ump->um_export, &args.export));
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	devvp = NULL;
	error = nlookup_init(&nd, args.fspec, UIO_USERSPACE, NLC_FOLLOW);
	if (error == 0)
		error = nlookup(&nd);
	if (error == 0)
		error = cache_vref(nd.nl_ncp, nd.nl_cred, &devvp);
	nlookup_done(&nd);
	if (error)
		return (error);

	if (!vn_isdisk(devvp, &error)) {
		vrele(devvp);
		return (error);
	}

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (cred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
		if ((error = VOP_ACCESS(devvp, accessmode, cred, td)) != 0) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp, 0, td);
	}

	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = ext2_mountfs(devvp, mp, td);
	} else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	copyinstr(path, fs->fs_fsmnt, sizeof(fs->fs_fsmnt) - 1, &size);
	bzero(fs->fs_fsmnt + size, sizeof(fs->fs_fsmnt) - size);
	copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	ext2_statfs(mp, &mp->mnt_stat, td);
	return (0);
}

/*
 * checks that the data in the descriptor blocks make sense
 * this is taken from ext2/super.c
 */
static int ext2_check_descriptors(struct ext2_sb_info *sb)
{
        int i;
        int desc_block = 0;
        unsigned long block = sb->s_es->s_first_data_block;
        struct ext2_group_desc * gdp = NULL;

        /* ext2_debug ("Checking group descriptors"); */

        for (i = 0; i < sb->s_groups_count; i++)
        {
		/* examine next descriptor block */
                if ((i % EXT2_DESC_PER_BLOCK(sb)) == 0)
                        gdp = (struct ext2_group_desc *) 
				sb->s_group_desc[desc_block++]->b_data;
                if (gdp->bg_block_bitmap < block ||
                    gdp->bg_block_bitmap >= block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Block bitmap for group %d"
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_block_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_bitmap < block ||
                    gdp->bg_inode_bitmap >= block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode bitmap for group %d"
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_inode_bitmap);
                        return 0;
                }
                if (gdp->bg_inode_table < block ||
                    gdp->bg_inode_table + sb->s_itb_per_group >=
                    block + EXT2_BLOCKS_PER_GROUP(sb))
                {
                        printf ("ext2_check_descriptors: "
                                    "Inode table for group %d"
                                    " not in group (block %lu)!\n",
                                    i, (unsigned long) gdp->bg_inode_table);
                        return 0;
                }
                block += EXT2_BLOCKS_PER_GROUP(sb);
                gdp++;
        }
        return 1;
}

static int
ext2_check_sb_compat(struct ext2_super_block *es, dev_t dev, int ronly)
{
	if (es->s_magic != EXT2_SUPER_MAGIC) {
		printf("ext2fs: %s: wrong magic number %#x (expected %#x)\n",
		    devtoname(dev), es->s_magic, EXT2_SUPER_MAGIC);
		return (1);
	}
	if (es->s_rev_level > EXT2_GOOD_OLD_REV) {
		if (es->s_feature_incompat & ~EXT2_FEATURE_INCOMPAT_SUPP) {
			printf(
"WARNING: mount of %s denied due to unsupported optional features\n",
			    devtoname(dev));
			return (1);
		}
		if (!ronly &&
		    (es->s_feature_ro_compat & ~EXT2_FEATURE_RO_COMPAT_SUPP)) {
			printf(
"WARNING: R/W mount of %s denied due to unsupported optional features\n",
			    devtoname(dev));
			return (1);
		}
	}
	return (0);
}

/*
 * this computes the fields of the  ext2_sb_info structure from the
 * data in the ext2_super_block structure read in
 */
static int
compute_sb_data(struct vnode *devvp, struct ext2_super_block *es,
		struct ext2_sb_info *fs)
{
    int db_count, error;
    int i, j;
    int logic_sb_block = 1;	/* XXX for now */

#if 1
#define V(v)  
#else
#define V(v)  printf(#v"= %d\n", fs->v);
#endif

    fs->s_blocksize = EXT2_MIN_BLOCK_SIZE << es->s_log_block_size; 
    V(s_blocksize)
    fs->s_bshift = EXT2_MIN_BLOCK_LOG_SIZE + es->s_log_block_size;
    V(s_bshift)
    fs->s_fsbtodb = es->s_log_block_size + 1;
    V(s_fsbtodb)
    fs->s_qbmask = fs->s_blocksize - 1;
    V(s_bmask)
    fs->s_blocksize_bits = EXT2_BLOCK_SIZE_BITS(es);
    V(s_blocksize_bits)
    fs->s_frag_size = EXT2_MIN_FRAG_SIZE << es->s_log_frag_size;
    V(s_frag_size)
    if (fs->s_frag_size)
	fs->s_frags_per_block = fs->s_blocksize / fs->s_frag_size;
    V(s_frags_per_block)
    fs->s_blocks_per_group = es->s_blocks_per_group;
    V(s_blocks_per_group)
    fs->s_frags_per_group = es->s_frags_per_group;
    V(s_frags_per_group)
    fs->s_inodes_per_group = es->s_inodes_per_group;
    V(s_inodes_per_group)
    fs->s_inodes_per_block = fs->s_blocksize / EXT2_INODE_SIZE;
    V(s_inodes_per_block)
    fs->s_itb_per_group = fs->s_inodes_per_group /fs->s_inodes_per_block;
    V(s_itb_per_group)
    fs->s_desc_per_block = fs->s_blocksize / sizeof (struct ext2_group_desc);
    V(s_desc_per_block)
    /* s_resuid / s_resgid ? */
    fs->s_groups_count = (es->s_blocks_count -
			  es->s_first_data_block +
			  EXT2_BLOCKS_PER_GROUP(fs) - 1) /
			 EXT2_BLOCKS_PER_GROUP(fs);
    V(s_groups_count)
    db_count = (fs->s_groups_count + EXT2_DESC_PER_BLOCK(fs) - 1) /
	EXT2_DESC_PER_BLOCK(fs);
    fs->s_db_per_group = db_count;
    V(s_db_per_group)

    fs->s_group_desc = bsd_malloc(db_count * sizeof (struct buf *),
		M_UFSMNT, M_WAITOK);

    /* adjust logic_sb_block */
    if(fs->s_blocksize > SBSIZE) 
	/* Godmar thinks: if the blocksize is greater than 1024, then
	   the superblock is logically part of block zero. 
	 */
        logic_sb_block = 0;
    
    for (i = 0; i < db_count; i++) {
	error = bread(devvp , fsbtodb(fs, logic_sb_block + i + 1), 
		fs->s_blocksize, &fs->s_group_desc[i]);
	if(error) {
	    for (j = 0; j < i; j++)
		brelse(fs->s_group_desc[j]);
	    bsd_free(fs->s_group_desc, M_UFSMNT);
	    printf("EXT2-fs: unable to read group descriptors (%d)\n", error);
	    return EIO;
	}
	/* Set the B_LOCKED flag on the buffer, then brelse() it */
	LCK_BUF(fs->s_group_desc[i])
    }
    if(!ext2_check_descriptors(fs)) {
	    for (j = 0; j < db_count; j++)
		    ULCK_BUF(fs->s_group_desc[j])
	    bsd_free(fs->s_group_desc, M_UFSMNT);
	    printf("EXT2-fs: (ext2_check_descriptors failure) "
		   "unable to read group descriptors\n");
	    return EIO;
    }

    for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
	    fs->s_inode_bitmap_number[i] = 0;
	    fs->s_inode_bitmap[i] = NULL;
	    fs->s_block_bitmap_number[i] = 0;
	    fs->s_block_bitmap[i] = NULL;
    }
    fs->s_loaded_inode_bitmaps = 0;
    fs->s_loaded_block_bitmaps = 0;
    return 0;
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
static int ext2_reload_scan1(struct mount *mp, struct vnode *vp, void *rescan);
static int ext2_reload_scan2(struct mount *mp, struct vnode *vp, void *rescan);

struct scaninfo {
	int rescan;
	int allerror;
	int waitfor;
	thread_t td;
	struct vnode *devvp;
	struct ext2_sb_info *fs;
};

static int
ext2_reload(struct mount *mountp, struct ucred *cred, struct thread *td)
{
	struct vnode *devvp;
	struct buf *bp;
	struct ext2_super_block * es;
	struct ext2_sb_info *fs;
	int error;
	struct scaninfo scaninfo;

	if ((mountp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mountp)->um_devvp;
	if (vinvalbuf(devvp, 0, td, 0, 0))
		panic("ext2_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 * constants have been adjusted for ext2
	 */
	if ((error = bread(devvp, SBLOCK, SBSIZE, &bp)) != 0)
		return (error);
	es = (struct ext2_super_block *)bp->b_data;
	if (ext2_check_sb_compat(es, devvp->v_rdev, 0) != 0) {
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	fs = VFSTOUFS(mountp)->um_e2fs;
	bcopy(bp->b_data, fs->s_es, sizeof(struct ext2_super_block));

	if((error = compute_sb_data(devvp, es, fs)) != 0) {
		brelse(bp);
		return error;
	}
#ifdef UNKLAR
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
#endif
	brelse(bp);

	scaninfo.rescan = 1;
	scaninfo.td = td;
	scaninfo.devvp = devvp;
	scaninfo.fs = fs;
	while (error == 0 && scaninfo.rescan) {
	    scaninfo.rescan = 0;
	    error = vmntvnodescan(mountp, VMSC_GETVX, ext2_reload_scan1, 
				ext2_reload_scan2, &scaninfo);
	}
	return(error);
}

static int
ext2_reload_scan1(struct mount *mp, struct vnode *vp, void *data)
{
	/*struct scaninfo *info = data;*/

	return(0);
}

static int
ext2_reload_scan2(struct mount *mp, struct vnode *vp, void *data)
{
	struct scaninfo *info = data;
	struct inode *ip;
	struct buf *bp;
	int error;

	/*
	 * Try to recycle
	 */
	if (vrecycle(vp, curthread))
		return(0);

	/*
	 * Step 5: invalidate all cached file data.
	 */
	if (vinvalbuf(vp, 0, info->td, 0, 0))
		panic("ext2_reload: dirty2");
	/*
	 * Step 6: re-read inode data for all active vnodes.
	 */
	ip = VTOI(vp);
	error = bread(info->devvp, fsbtodb(info->fs, ino_to_fsba(info->fs, ip->i_number)),
		    (int)info->fs->s_blocksize, &bp);
	if (error)
		return (error);
	ext2_ei2di((struct ext2_inode *) ((char *)bp->b_data + 
	    EXT2_INODE_SIZE * ino_to_fsbo(info->fs, ip->i_number)), 
	    &ip->i_din);
	brelse(bp);
	return(0);
}

/*
 * Common code for mount and mountroot
 */
static int
ext2_mountfs(struct vnode *devvp, struct mount *mp, struct thread *td)
{
	struct ufsmount *ump;
	struct buf *bp;
	struct ext2_sb_info *fs;
	struct ext2_super_block * es;
	dev_t dev;
	struct partinfo dpart;
	int havepart = 0;
	int error, i, size;
	int ronly;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = vfs_mountedon(devvp)) != 0)
		return (error);
	if (count_udev(devvp->v_udev) > 0)
		return (EBUSY);
	if ((error = vinvalbuf(devvp, V_SAVE, td, 0, 0)) != 0)
		return (error);
#ifdef READONLY
/* turn on this to force it to be read-only */
	mp->mnt_flag |= MNT_RDONLY;
#endif

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, NULL, td);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);
	dev = devvp->v_rdev;
	if (dev->si_iosize_max != 0)
		mp->mnt_iosize_max = dev->si_iosize_max;
	if (mp->mnt_iosize_max > MAXPHYS)
		mp->mnt_iosize_max = MAXPHYS;
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, td) != 0)
		size = DEV_BSIZE;
	else {
		havepart = 1;
		size = dpart.disklab->d_secsize;
	}

	bp = NULL;
	ump = NULL;
	if ((error = bread(devvp, SBLOCK, SBSIZE, &bp)) != 0)
		goto out;
	es = (struct ext2_super_block *)bp->b_data;
	if (ext2_check_sb_compat(es, dev, ronly) != 0) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	if ((es->s_state & EXT2_VALID_FS) == 0 ||
	    (es->s_state & EXT2_ERROR_FS)) {
		if (ronly || (mp->mnt_flag & MNT_FORCE)) {
			printf(
"WARNING: Filesystem was not properly dismounted\n");
		} else {
			printf(
"WARNING: R/W mount denied.  Filesystem is not clean - run fsck\n");
			error = EPERM;
			goto out;
		}
	}
	ump = bsd_malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	ump->um_malloctype = M_EXT2NODE;
	ump->um_blkatoff = ext2_blkatoff;
	ump->um_truncate = ext2_truncate;
	ump->um_update = ext2_update;
	ump->um_valloc = ext2_valloc;
	ump->um_vfree = ext2_vfree;
	/* I don't know whether this is the right strategy. Note that
	   we dynamically allocate both a ext2_sb_info and a ext2_super_block
	   while Linux keeps the super block in a locked buffer
	 */
	ump->um_e2fs = bsd_malloc(sizeof(struct ext2_sb_info), 
		M_UFSMNT, M_WAITOK);
	ump->um_e2fs->s_es = bsd_malloc(sizeof(struct ext2_super_block), 
		M_UFSMNT, M_WAITOK);
	bcopy(es, ump->um_e2fs->s_es, (u_int)sizeof(struct ext2_super_block));
	if ((error = compute_sb_data(devvp, ump->um_e2fs->s_es, ump->um_e2fs)))
		goto out;
	/*
	 * We don't free the group descriptors allocated by compute_sb_data()
	 * until ext2_unmount().  This is OK since the mount will succeed.
	 */
	brelse(bp);
	bp = NULL;
	fs = ump->um_e2fs;
	fs->s_rd_only = ronly;	/* ronly is set according to mnt_flags */
	/* if the fs is not mounted read-only, make sure the super block is 
	   always written back on a sync()
	 */
	fs->s_wasvalid = fs->s_es->s_state & EXT2_VALID_FS ? 1 : 0;
	if (ronly == 0) {
		fs->s_dirt = 1;		/* mark it modified */
		fs->s_es->s_state &= ~EXT2_VALID_FS;	/* set fs invalid */
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	/* setting those two parameters allows us to use 
	   ufs_bmap w/o changse !
	*/
	ump->um_nindir = EXT2_ADDR_PER_BLOCK(fs);
	ump->um_bptrtodb = fs->s_es->s_log_block_size + 1;
	ump->um_seqinc = EXT2_FRAGS_PER_BLOCK(fs);
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP; 
	dev->si_mountpoint = mp;

	vfs_add_vnodeops(mp, &mp->mnt_vn_norm_ops, ext2_vnodeop_entries);
	vfs_add_vnodeops(mp, &mp->mnt_vn_spec_ops, ext2_specop_entries);
	vfs_add_vnodeops(mp, &mp->mnt_vn_fifo_ops, ext2_fifoop_entries);

	if (ronly == 0) 
		ext2_sbupdate(ump, MNT_WAIT);
	return (0);
out:
	if (bp)
		brelse(bp);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, td);
	if (ump) {
		bsd_free(ump->um_e2fs->s_es, M_UFSMNT);
		bsd_free(ump->um_e2fs, M_UFSMNT);
		bsd_free(ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * unmount system call
 */
static int
ext2_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct ufsmount *ump;
	struct ext2_sb_info *fs;
	int error, flags, ronly, i;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		if (mp->mnt_flag & MNT_ROOTFS)
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	if ((error = ext2_flushfiles(mp, flags, td)) != 0)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	ronly = fs->s_rd_only;
	if (ronly == 0) {
		if (fs->s_wasvalid)
			fs->s_es->s_state |= EXT2_VALID_FS;
		ext2_sbupdate(ump, MNT_WAIT);
	}

	/* release buffers containing group descriptors */
	for(i = 0; i < fs->s_db_per_group; i++) 
		ULCK_BUF(fs->s_group_desc[i])
	bsd_free(fs->s_group_desc, M_UFSMNT);

	/* release cached inode/block bitmaps */
        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_inode_bitmap[i])
			ULCK_BUF(fs->s_inode_bitmap[i])

        for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
                if (fs->s_block_bitmap[i])
			ULCK_BUF(fs->s_block_bitmap[i])

	ump->um_devvp->v_rdev->si_mountpoint = NULL;
	error = VOP_CLOSE(ump->um_devvp, ronly ? FREAD : FREAD|FWRITE, td);
	vrele(ump->um_devvp);
	bsd_free(fs->s_es, M_UFSMNT);
	bsd_free(fs, M_UFSMNT);
	bsd_free(ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
static int
ext2_flushfiles(struct mount *mp, int flags, struct thread *td)
{
	struct ufsmount *ump;
	int error;
#if QUOTA
	int i;
#endif

	ump = VFSTOUFS(mp);
#if QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		if ((error = vflush(mp, 0, SKIPSYSTEM|flags)) != 0)
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ump->um_quotas[i] == NULLVP)
				continue;
			quotaoff(td, mp, i);
		}
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}
#endif
	error = vflush(mp, 0, flags);
	return (error);
}

/*
 * Get file system statistics.
 * taken from ext2/super.c ext2_statfs
 */
static int
ext2_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
        unsigned long overhead;
	struct ufsmount *ump;
	struct ext2_sb_info *fs;
	struct ext2_super_block *es;
	int i, nsb;

	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	es = fs->s_es;

	if (es->s_magic != EXT2_SUPER_MAGIC)
		panic("ext2_statfs - magic number spoiled");

	/*
	 * Compute the overhead (FS structures)
	 */
	if (es->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
		nsb = 0;
		for (i = 0 ; i < fs->s_groups_count; i++)
			if (ext2_group_sparse(i))
				nsb++;
	} else
		nsb = fs->s_groups_count;
	overhead = es->s_first_data_block + 
	    /* Superblocks and block group descriptors: */
	    nsb * (1 + fs->s_db_per_group) +
	    /* Inode bitmap, block bitmap, and inode table: */
	    fs->s_groups_count * (1 + 1 + fs->s_itb_per_group);

	sbp->f_bsize = EXT2_FRAG_SIZE(fs);	
	sbp->f_iosize = EXT2_BLOCK_SIZE(fs);
	sbp->f_blocks = es->s_blocks_count - overhead;
	sbp->f_bfree = es->s_free_blocks_count; 
	sbp->f_bavail = sbp->f_bfree - es->s_r_blocks_count; 
	sbp->f_files = es->s_inodes_count; 
	sbp->f_ffree = es->s_free_inodes_count; 
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */

static int ext2_sync_scan(struct mount *mp, struct vnode *vp, void *data);

static int
ext2_sync(struct mount *mp, int waitfor, struct thread *td)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct ext2_sb_info *fs;
	struct scaninfo scaninfo;
	int error;

	fs = ump->um_e2fs;
	if (fs->s_dirt != 0 && fs->s_rd_only != 0) {		/* XXX */
		printf("fs = %s\n", fs->fs_fsmnt);
		panic("ext2_sync: rofs mod");
	}

	/*
	 * Write back each (modified) inode.
	 */
	scaninfo.allerror = 0;
	scaninfo.rescan = 1;
	scaninfo.waitfor = waitfor;
	scaninfo.td = td;
	while (scaninfo.rescan) {
		scaninfo.rescan = 0;
		vmntvnodescan(mp, VMSC_GETVP|VMSC_NOWAIT, 
				NULL, ext2_sync_scan, &scaninfo);
	}

	/*
	 * Force stale file system control information to be flushed.
	 */
	if (waitfor != MNT_LAZY) {
		vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY, td);
		if ((error = VOP_FSYNC(ump->um_devvp, waitfor, td)) != 0)
			scaninfo.allerror = error;
		VOP_UNLOCK(ump->um_devvp, 0, td);
	}
#if QUOTA
	qsync(mp);
#endif
	/*
	 * Write back modified superblock.
	 */
	if (fs->s_dirt != 0) {
		fs->s_dirt = 0;
		fs->s_es->s_wtime = time_second;
		if ((error = ext2_sbupdate(ump, waitfor)) != 0)
			scaninfo.allerror = error;
	}
	return (scaninfo.allerror);
}

static int
ext2_sync_scan(struct mount *mp, struct vnode *vp, void *data)
{
	struct scaninfo *info = data;
	struct inode *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VNON ||
	    ((ip->i_flag &
	    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
	    (TAILQ_EMPTY(&vp->v_dirtyblkhd) || info->waitfor == MNT_LAZY))) {
		return(0);
	}
	if ((error = VOP_FSYNC(vp, info->waitfor, info->td)) != 0)
		info->allerror = error;
	return(0);
}

/*
 * Look up a EXT2FS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
static int
ext2_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct ext2_sb_info *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int i, error;
	int used_blocks;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
restart:
	if ((*vpp = ufs_ihashget(dev, ino)) != NULL)
		return (0);

	/*
	 * Lock out the creation of new entries in the FFS hash table in
	 * case getnewvnode() or MALLOC() blocks, otherwise a duplicate
	 * may occur!
	 */
	if (ext2fs_inode_hash_lock) {
		while (ext2fs_inode_hash_lock) {
			ext2fs_inode_hash_lock = -1;
			tsleep(&ext2fs_inode_hash_lock, 0, "e2vget", 0);
		}
		goto restart;
	}
	ext2fs_inode_hash_lock = 1;

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ext2_sync() if a sync happens to fire right then,
	 * which will cause a panic because ext2_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), M_EXT2NODE, M_WAITOK);

	/* Allocate a new vnode/inode. */
	if ((error = getnewvnode(VT_UFS, mp, &vp, 0, LK_CANRECURSE)) != 0) {
		if (ext2fs_inode_hash_lock < 0)
			wakeup(&ext2fs_inode_hash_lock);
		ext2fs_inode_hash_lock = 0;
		*vpp = NULL;
		FREE(ip, M_EXT2NODE);
		return (error);
	}
	bzero((caddr_t)ip, sizeof(struct inode));
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_dev = dev;
	ip->i_number = ino;
#if QUOTA
	for (i = 0; i < MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
#endif
	/*
	 * Put it onto its hash chain.  Since our vnode is locked, other
	 * requests for this inode will block if they arrive while we are
	 * sleeping waiting for old data structures to be purged or for the
	 * contents of the disk portion of this inode to be read.
	 */
	ufs_ihashins(ip);

	if (ext2fs_inode_hash_lock < 0)
		wakeup(&ext2fs_inode_hash_lock);
	ext2fs_inode_hash_lock = 0;

	/* Read in the disk contents for the inode, copy into the inode. */
#if 0
printf("ext2_vget(%d) dbn= %d ", ino, fsbtodb(fs, ino_to_fsba(fs, ino)));
#endif
	if ((error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->s_blocksize, &bp)) != 0) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		vx_put(vp);
		brelse(bp);
		*vpp = NULL;
		return (error);
	}
	/* convert ext2 inode to dinode */
	ext2_ei2di((struct ext2_inode *) ((char *)bp->b_data + EXT2_INODE_SIZE *
			ino_to_fsbo(fs, ino)), &ip->i_din);
	ip->i_block_group = ino_to_cg(fs, ino);
	ip->i_next_alloc_block = 0;
	ip->i_next_alloc_goal = 0;
	ip->i_prealloc_count = 0;
	ip->i_prealloc_block = 0;
        /* now we want to make sure that block pointers for unused
           blocks are zeroed out - ext2_balloc depends on this 
	   although for regular files and directories only
	*/
	if(S_ISDIR(ip->i_mode) || S_ISREG(ip->i_mode)) {
		used_blocks = (ip->i_size+fs->s_blocksize-1) / fs->s_blocksize;
		for(i = used_blocks; i < EXT2_NDIR_BLOCKS; i++)
			ip->i_db[i] = 0;
	}
/*
	ext2_print_inode(ip);
*/
	brelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	if ((error = ufs_vinit(mp, &vp)) != 0) {
		vx_put(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	vref(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		ip->i_gen = random() / 2 + 1;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	/*
	 * Return the locked and refd vnode.
	 */
	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
static int
ext2_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ufid *ufhp;
	struct ext2_sb_info *fs;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOUFS(mp)->um_e2fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino > fs->s_groups_count * fs->s_es->s_inodes_per_group)
		return (ESTALE);
	return (ufs_fhtovp(mp, ufhp, vpp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
static int
ext2_vptofh(struct vnode *vp, struct fid *fhp)
{
	struct inode *ip;
	struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

/*
 * Write a superblock and associated information back to disk.
 */
static int
ext2_sbupdate(struct ufsmount *mp, int waitfor)
{
	struct ext2_sb_info *fs = mp->um_e2fs;
	struct ext2_super_block *es = fs->s_es;
	struct buf *bp;
	int error = 0;
/*
printf("\nupdating superblock, waitfor=%s\n", waitfor == MNT_WAIT ? "yes":"no");
*/
	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0);
	bcopy((caddr_t)es, bp->b_data, (u_int)sizeof(struct ext2_super_block));
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);

	/*
	 * The buffers for group descriptors, inode bitmaps and block bitmaps
	 * are not busy at this point and are (hopefully) written by the
	 * usual sync mechanism. No need to write them here
	 */

	return (error);
}
