/*-
 * Copyright (c) 2001, 2002 Scott Long <scottl@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/udf/udf_vfsops.c,v 1.16 2003/11/05 06:56:08 scottl Exp $
 * $DragonFly: src/sys/vfs/udf/udf_vfsops.c,v 1.12 2005/02/02 21:34:18 joerg Exp $
 */

/* udf_vfsops.c */
/* Implement the VFS side of things */

/*
 * Ok, here's how it goes.  The UDF specs are pretty clear on how each data
 * structure is made up, but not very clear on how they relate to each other.
 * Here is the skinny... This demostrates a filesystem with one file in the
 * root directory.  Subdirectories are treated just as normal files, but they
 * have File Id Descriptors of their children as their file data.  As for the
 * Anchor Volume Descriptor Pointer, it can exist in two of the following three
 * places: sector 256, sector n (the max sector of the disk), or sector
 * n - 256.  It's a pretty good bet that one will exist at sector 256 though.
 * One caveat is unclosed CD media.  For that, sector 256 cannot be written,
 * so the Anchor Volume Descriptor Pointer can exist at sector 512 until the
 * media is closed.
 *
 *  Sector:
 *     256:
 *       n: Anchor Volume Descriptor Pointer
 * n - 256:	|
 *		|
 *		|-->Main Volume Descriptor Sequence
 *			|	|
 *			|	|
 *			|	|-->Logical Volume Descriptor
 *			|			  |
 *			|-->Partition Descriptor  |
 *				|		  |
 *				|		  |
 *				|-->Fileset Descriptor
 *					|
 *					|
 *					|-->Root Dir File Entry
 *						|
 *						|
 *						|-->File data:
 *						    File Id Descriptor
 *							|
 *							|
 *							|-->File Entry
 *								|
 *								|
 *								|-->File data
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/nlookup.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/vnode.h>

#include <vfs/udf/ecma167-udf.h>
#include <vfs/udf/osta.h>
#include <vfs/udf/udf.h>
#include <vfs/udf/udf_mount.h>

extern struct vnodeopv_entry_desc udf_vnodeop_entries[];

MALLOC_DEFINE(M_UDFNODE, "UDF node", "UDF node structure");
MALLOC_DEFINE(M_UDFMOUNT, "UDF mount", "UDF mount structure");
MALLOC_DEFINE(M_UDFFENTRY, "UDF fentry", "UDF file entry structure");

static int udf_mount(struct mount *, char *, caddr_t, struct thread *);
static int udf_unmount(struct mount *, int, struct thread *);
static int udf_root(struct mount *, struct vnode **);
static int udf_statfs(struct mount *, struct statfs *, struct thread *);
static int udf_fhtovp(struct mount *, struct fid *, struct vnode **);
static int udf_vptofh(struct vnode *, struct fid *);

static int udf_find_partmaps(struct udf_mnt *, struct logvol_desc *);

static struct vfsops udf_vfsops = {
	udf_mount,
	vfs_stdstart,
	udf_unmount,
	udf_root,
	vfs_stdquotactl,
	udf_statfs,
	vfs_stdsync,
	udf_vget,
	udf_fhtovp,
	vfs_stdcheckexp,
	udf_vptofh,
	vfs_stdinit,
	vfs_stduninit,
	vfs_stdextattrctl,
};
VFS_SET(udf_vfsops, udf, VFCF_READONLY);

MODULE_VERSION(udf, 1);

static int udf_mountfs(struct vnode *, struct mount *, struct thread *);

static int
udf_mount(struct mount *mp, char *path, caddr_t data, struct thread *td)
{
	struct vnode *devvp;	/* vnode of the mount device */
	struct udf_args args;
	struct udf_mnt *imp = 0;
	size_t size;
	int error;
	struct nlookupdata nd;

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EROFS);

	/*
	 * No root filesystem support.  Probably not a big deal, since the
	 * bootloader doesn't understand UDF.
	 */
	if (mp->mnt_flag & MNT_ROOTFS)
		return (ENOTSUP);

	if ((error = copyin(data, (caddr_t)&args, sizeof(struct udf_args))))
		return(error);

	if (mp->mnt_flag & MNT_UPDATE) {
		imp = VFSTOUDFFS(mp);
		if (args.fspec == NULL)
			return(vfs_export(mp, &imp->im_export, &args.export));
	}

	/* Check that the mount device exists */
	devvp = NULL;
	error = nlookup_init(&nd, args.fspec, UIO_USERSPACE, NLC_FOLLOW);
	if (error == 0)
		error = nlookup(&nd);
	if (error == 0)
		error = cache_vref(nd.nl_ncp, nd.nl_cred, &devvp);
	nlookup_done(&nd);
	if (error)
		return (error);

	if (vn_isdisk(devvp, &error) == 0) {
		vrele(devvp);
		return(error);
	}

	/* Check the access rights on the mount device */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_ACCESS(devvp, VREAD, td->td_proc->p_ucred, td);
	if (error)
		error = suser(td);
	if (error) {
		vput(devvp);
		return(error);
	}
	VOP_UNLOCK(devvp, 0, td);

	if ((error = udf_mountfs(devvp, mp, td))) {
		vrele(devvp);
		return(error);
	}

	imp = VFSTOUDFFS(mp);

	imp->im_flags = args.flags;

	copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	udf_statfs(mp, &mp->mnt_stat, td);
	return(0);
}

/*
 * Check the descriptor tag for both the correct id and correct checksum.
 * Return zero if all is good, EINVAL if not.
 */
int
udf_checktag(struct desc_tag *tag, uint16_t id)
{
	uint8_t *itag;
	uint8_t i, cksum = 0;

	itag = (uint8_t *)tag;

	if (tag->id != id)
		return(EINVAL);

	for (i = 0; i < 15; i++)
		cksum = cksum + itag[i];
	cksum = cksum - itag[4];

	if (cksum == tag->cksum)
		return(0);

	return(EINVAL);
}

static int
udf_mountfs(struct vnode *devvp, struct mount *mp, struct thread *td) 
{
	struct buf *bp = NULL;
	struct anchor_vdp avdp;
	struct udf_mnt *udfmp = NULL;
	struct part_desc *pd;
	struct logvol_desc *lvd;
	struct fileset_desc *fsd;
	struct file_entry *root_fentry;
	dev_t dev;
	uint32_t sector, size, mvds_start, mvds_end;
	uint32_t fsd_offset = 0;
	uint16_t part_num = 0, fsd_part = 0;
	int error = EINVAL, needclose = 0;
	int logvol_found = 0, part_found = 0, fsd_found = 0;
	int bsize;

	/*
	 * Disallow multiple mounts of the same device. Flush the buffer
	 * cache for the device.
	 */
	if ((error = vfs_mountedon(devvp)))
		return(error);
	if (count_udev(devvp->v_udev) > 0)
		return(EBUSY);
	if ((error = vinvalbuf(devvp, V_SAVE, td, 0, 0)))
		return(error);

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(devvp, FREAD, FSCRED, NULL, td);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return(error);
	needclose = 1;
	dev = devvp->v_rdev;

	udfmp = malloc(sizeof(*udfmp), M_UDFMOUNT, M_WAITOK | M_ZERO);

	mp->mnt_data = (qaddr_t)udfmp;
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	udfmp->im_mountp = mp;
	udfmp->im_dev = dev;
	udfmp->im_devvp = devvp;

	bsize = 2048;	/* XXX Should probe the media for it's size */

	/* 
	 * Get the Anchor Volume Descriptor Pointer from sector 256.
	 * XXX Should also check sector n - 256, n, and 512.
	 */
	sector = 256;
	if ((error = bread(devvp, sector * btodb(bsize), bsize, &bp)) != 0)
		goto bail;
	if ((error = udf_checktag((struct desc_tag *)bp->b_data, TAGID_ANCHOR)))
		goto bail;

	bcopy(bp->b_data, &avdp, sizeof(struct anchor_vdp));
	brelse(bp);
	bp = NULL;

	/*
	 * Extract the Partition Descriptor and Logical Volume Descriptor
	 * from the Volume Descriptor Sequence.
	 * XXX Should we care about the partition type right now?
	 * XXX What about multiple partitions?
	 */
	mvds_start = avdp.main_vds_ex.loc;
	mvds_end = mvds_start + (avdp.main_vds_ex.len - 1) / bsize;
	for (sector = mvds_start; sector < mvds_end; sector++) {
		if ((error = bread(devvp, sector * btodb(bsize), bsize,
				   &bp)) != 0) {
			printf("Can't read sector %d of VDS\n", sector);
			goto bail;
		}
		lvd = (struct logvol_desc *)bp->b_data;
		if (!udf_checktag(&lvd->tag, TAGID_LOGVOL)) {
			udfmp->bsize = lvd->lb_size;
			udfmp->bmask = udfmp->bsize - 1;
			udfmp->bshift = ffs(udfmp->bsize) - 1;
			fsd_part = lvd->_lvd_use.fsd_loc.loc.part_num;
			fsd_offset = lvd->_lvd_use.fsd_loc.loc.lb_num;
			if (udf_find_partmaps(udfmp, lvd))
				break;
			logvol_found = 1;
		}
		pd = (struct part_desc *)bp->b_data;
		if (!udf_checktag(&pd->tag, TAGID_PARTITION)) {
			part_found = 1;
			part_num = pd->part_num;
			udfmp->part_len = pd->part_len;
			udfmp->part_start = pd->start_loc;
		}

		brelse(bp); 
		bp = NULL;
		if ((part_found) && (logvol_found))
			break;
	}

	if (!part_found || !logvol_found) {
		error = EINVAL;
		goto bail;
	}

	if (fsd_part != part_num) {
		printf("FSD does not lie within the partition!\n");
		error = EINVAL;
		goto bail;
	}


	/*
	 * Grab the Fileset Descriptor
	 * Thanks to Chuck McCrobie <mccrobie@cablespeed.com> for pointing
	 * me in the right direction here.
	 */
	sector = udfmp->part_start + fsd_offset;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d of FSD\n", sector);
		goto bail;
	}
	fsd = (struct fileset_desc *)bp->b_data;
	if (!udf_checktag(&fsd->tag, TAGID_FSD)) {
		fsd_found = 1;
		bcopy(&fsd->rootdir_icb, &udfmp->root_icb,
		      sizeof(struct long_ad));
	}

	brelse(bp);
	bp = NULL;

	if (!fsd_found) {
		printf("Couldn't find the fsd\n");
		error = EINVAL;
		goto bail;
	} 

	vfs_add_vnodeops(mp, &mp->mnt_vn_norm_ops, udf_vnodeop_entries);

	/*
	 * Find the file entry for the root directory.
	 */
	sector = udfmp->root_icb.loc.lb_num + udfmp->part_start;
	size = udfmp->root_icb.len;
	if ((error = udf_readlblks(udfmp, sector, size, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		goto bail;
	}

	root_fentry = (struct file_entry *)bp->b_data;
	if ((error = udf_checktag(&root_fentry->tag, TAGID_FENTRY))) {
		printf("Invalid root file entry!\n");
		goto bail;
	}

	brelse(bp);
	bp = NULL;

	lwkt_token_init(&udfmp->hash_token);
	udfmp->hashtbl = phashinit(UDF_HASHTBLSIZE, M_UDFMOUNT, &udfmp->hashsz);

	return(0);

bail:
	if (udfmp != NULL)
		free(udfmp, M_UDFMOUNT);
	if (bp != NULL)
		brelse(bp);
	if (needclose)
		VOP_CLOSE(devvp, FREAD, td);
	return(error);
}

static int
udf_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct udf_mnt *udfmp;
	int error, flags = 0;

	udfmp = VFSTOUDFFS(mp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags)))
		return (error);

	udfmp->im_devvp->v_rdev->si_mountpoint = NULL;
	error = VOP_CLOSE(udfmp->im_devvp, FREAD, td);
	vrele(udfmp->im_devvp);

	if (udfmp->s_table)
		free(udfmp->s_table, M_UDFMOUNT);
	if (udfmp->hashtbl)
		free(udfmp->hashtbl, M_UDFMOUNT);
	free(udfmp, M_UDFMOUNT);

	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;

	return (error);
}

static int
udf_root(struct mount *mp, struct vnode **vpp)
{
	struct udf_mnt *udfmp;
	struct vnode *vp;
	ino_t id;
	int error;

	udfmp = VFSTOUDFFS(mp);

	id = udf_getid(&udfmp->root_icb);

	error = udf_vget(mp, id, vpp);
	if (error)
		return(error);

	vp = *vpp;
	vp->v_flag |= VROOT;
	udfmp->root_vp = vp;

	return(0);
}

static int
udf_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct udf_mnt *udfmp;

	udfmp = VFSTOUDFFS(mp);

	sbp->f_bsize = udfmp->bsize;
	sbp->f_iosize = udfmp->bsize;
	sbp->f_blocks = udfmp->part_len;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}

	return(0);
}

int
udf_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct buf *bp;
	struct vnode *devvp;
	struct udf_mnt *udfmp;
	struct thread *td;
	struct vnode *vp;
	struct udf_node *unode;
	struct file_entry *fe;
	int error, sector, size;

	td = curthread;
	udfmp = VFSTOUDFFS(mp);

	/* See if we already have this in the cache */
	if ((error = udf_hashlookup(udfmp, ino, vpp)) != 0)
		return(error);
	if (*vpp != NULL) {
		return(0);
	}

	/*
	 * Allocate memory and check the tag id's before grabbing a new
	 * vnode, since it's hard to roll back if there is a problem.
	 */
	unode = malloc(sizeof(*unode), M_UDFNODE, M_WAITOK | M_ZERO);

	/*
	 * Copy in the file entry.  Per the spec, the size can only be 1 block.
	 */
	sector = ino + udfmp->part_start;
	devvp = udfmp->im_devvp;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		free(unode, M_UDFNODE);
		return(error);
	}

	fe = (struct file_entry *)bp->b_data;
	if (udf_checktag(&fe->tag, TAGID_FENTRY)) {
		printf("Invalid file entry!\n");
		free(unode, M_UDFNODE);
		brelse(bp);
		return(ENOMEM);
	}
	size = UDF_FENTRY_SIZE + fe->l_ea + fe->l_ad;
	unode->fentry = malloc(size, M_UDFFENTRY, M_WAITOK | M_ZERO);

	bcopy(bp->b_data, unode->fentry, size);
	
	brelse(bp);
	bp = NULL;

	if ((error = udf_allocv(mp, &vp))) {
		printf("Error from udf_allocv\n");
		free(unode, M_UDFNODE);
		return(error);
	}

	unode->i_vnode = vp;
	unode->hash_id = ino;
	unode->i_devvp = udfmp->im_devvp;
	unode->i_dev = udfmp->im_dev;
	unode->udfmp = udfmp;
	vp->v_data = unode;
	vref(udfmp->im_devvp);
	udf_hashins(unode);

	switch (unode->fentry->icbtag.file_type) {
	default:
		vp->v_type = VBAD;
		break;
	case 4:
		vp->v_type = VDIR;
		break;
	case 5:
		vp->v_type = VREG;
		break;
	case 6:
		vp->v_type = VBLK;
		break;
	case 7:
		vp->v_type = VCHR;
		break;
	case 9:
		vp->v_type = VFIFO;
		break;
	case 10:
		vp->v_type = VSOCK;
		break;
	case 12:
		vp->v_type = VLNK;
		break;
	}
	/*
	 * Locked and refd vnode returned
	 */
	*vpp = vp;

	return(0);
}

struct ifid {
	u_short	ifid_len;
	u_short	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

static int
udf_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid *ifhp;
	struct vnode *nvp;
	int error;

	ifhp = (struct ifid *)fhp;

	if ((error = VFS_VGET(mp, ifhp->ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return(error);
	}

	*vpp = nvp;
	return(0);
}

static int
udf_vptofh (struct vnode *vp, struct fid *fhp)
{
	struct udf_node *node;
	struct ifid *ifhp;

	node = VTON(vp);
	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);
	ifhp->ifid_ino = node->hash_id;

	return(0);
}

static int
udf_find_partmaps(struct udf_mnt *udfmp, struct logvol_desc *lvd)
{
	union udf_pmap *pmap;
	struct part_map_spare *pms;
	struct regid *pmap_id;
	struct buf *bp;
	unsigned char regid_id[UDF_REGID_ID_SIZE + 1];
	int ptype, psize, error;
	unsigned int i;

	for (i = 0; i < lvd->n_pm; i++) {
		pmap = (union udf_pmap *)&lvd->maps[i * UDF_PMAP_SIZE];
		ptype = pmap->data[0];
		psize = pmap->data[1];
		if (((ptype != 1) && (ptype != 2)) ||
		    ((psize != UDF_PMAP_SIZE) && (psize != 6))) {
			printf("Invalid partition map found\n");
			return(1);
		}

		if (ptype == 1) {
			/* Type 1 map.  We don't care */
			continue;
		}

		/* Type 2 map.  Gotta find out the details */
		pmap_id = (struct regid *)&pmap->data[4];
		bzero(&regid_id[0], UDF_REGID_ID_SIZE);
		bcopy(&pmap_id->id[0], &regid_id[0], UDF_REGID_ID_SIZE);

		if (bcmp(&regid_id[0], "*UDF Sparable Partition",
		    UDF_REGID_ID_SIZE)) {
			printf("Unsupported partition map: %s\n", &regid_id[0]);
			return(1);
		}

		pms = &pmap->pms;
		udfmp->s_table = malloc(pms->st_size, M_UDFMOUNT,
					M_WAITOK | M_ZERO);
		if (udfmp->s_table == NULL)
			return(ENOMEM);

		/* Calculate the number of sectors per packet. */
		/* XXX Logical or physical? */
		udfmp->p_sectors = pms->packet_len / udfmp->bsize;

		/*
		 * XXX If reading the first Sparing Table fails, should look
		 * for another table.
		 */
		if ((error = udf_readlblks(udfmp, pms->st_loc[0], pms->st_size,
		    &bp)) != 0) {
			if (bp)
				brelse(bp);
			printf("Failed to read Sparing Table at sector %d\n",
			    pms->st_loc[0]);
			return(error);
		}
		bcopy(bp->b_data, udfmp->s_table, pms->st_size);
		brelse(bp);

		if (udf_checktag(&udfmp->s_table->tag, 0)) {
			printf("Invalid sparing table found\n");
			return(EINVAL);
		}

		/* See how many valid entries there are here.  The list is
		 * supposed to be sorted. 0xfffffff0 and higher are not valid
		 */
		for (i = 0; i < udfmp->s_table->rt_l; i++) {
			udfmp->s_table_entries = i;
			if (udfmp->s_table->entries[i].org >= 0xfffffff0)
				break;
		}
	}

	return(0);
}
