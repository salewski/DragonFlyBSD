/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 * $FreeBSD: src/sys/fs/hpfs/hpfs_vfsops.c,v 1.3.2.2 2001/12/25 01:44:45 dillon Exp $
 * $DragonFly: src/sys/vfs/hpfs/hpfs_vfsops.c,v 1.26 2005/02/02 21:34:18 joerg Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/nlookup.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#if defined(__NetBSD__)
#include <vm/vm_prot.h>
#endif
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <sys/buf2.h>

#if defined(__NetBSD__)
#include <miscfs/specfs/specdev.h>
#endif

#include "hpfs.h"
#include "hpfsmount.h"
#include "hpfs_subr.h"

extern struct vnodeopv_entry_desc hpfs_vnodeop_entries[];

#if defined(__DragonFly__)
MALLOC_DEFINE(M_HPFSMNT, "HPFS mount", "HPFS mount structure");
MALLOC_DEFINE(M_HPFSNO, "HPFS node", "HPFS node structure");
#endif

static int	hpfs_root (struct mount *, struct vnode **);
static int	hpfs_statfs (struct mount *, struct statfs *,
				 struct thread *);
static int	hpfs_unmount (struct mount *, int, struct thread *);
static int	hpfs_vget (struct mount *mp, ino_t ino,
			       struct vnode **vpp);
static int	hpfs_mountfs (struct vnode *, struct mount *, 
				  struct hpfs_args *, struct thread *);
static int	hpfs_vptofh (struct vnode *, struct fid *);
static int	hpfs_fhtovp (struct mount *, struct fid *,
				 struct vnode **);

#if !defined(__DragonFly__)
static int	hpfs_quotactl (struct mount *, int, uid_t, caddr_t,
				   struct proc *);
static int	hpfs_start (struct mount *, int, struct proc *);
static int	hpfs_sync (struct mount *, int, struct ucred *,
			       struct proc *);
#endif

#if defined(__DragonFly__)
struct sockaddr;
static int	hpfs_mount (struct mount *, char *, caddr_t, struct thread *);
static int	hpfs_init (struct vfsconf *);
static int	hpfs_checkexp (struct mount *, struct sockaddr *,
				   int *, struct ucred **);
#else /* defined(__NetBSD__) */
static int	hpfs_mount (struct mount *, const char *, void *,
				struct nlookupdata *, struct proc *);
static void	hpfs_init (void);
static int	hpfs_sysctl (int *, u_int, void *, size_t *, void *,
				 size_t, struct proc *);
static int	hpfs_checkexp (struct mount *, struct mbuf *,
				   int *, struct ucred **);
#endif

/*ARGSUSED*/
static int
hpfs_checkexp(struct mount *mp,
#if defined(__DragonFly__)
	      struct sockaddr *nam,
#else /* defined(__NetBSD__) */
	      struct mbuf *nam,
#endif
	      int *exflagsp, struct ucred **credanonp)
{
	struct netcred *np;
	struct hpfsmount *hpm = VFSTOHPFS(mp);

	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &hpm->hpm_export, nam);
	if (np == NULL)
		return (EACCES);

	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

#if defined(__DragonFly__)
static int
hpfs_init(struct vfsconf *vcp)
#else /* defined(__NetBSD__) */
static void
hpfs_init(void)
#endif
{
	dprintf(("hpfs_init():\n"));
	
	hpfs_hphashinit();
#if defined(__DragonFly__)
	return 0;
#endif
}

static int
hpfs_mount(struct mount *mp,
#if defined(__DragonFly__)
	   char *path, caddr_t data,
#else /* defined(__NetBSD__) */
	   const char *path, void *data,
#endif
	   struct thread *td)
{
	u_int		size;
	int		error;
	struct vnode	*devvp;
	struct hpfs_args args;
	struct hpfsmount *hpmp = 0;
	struct nlookupdata nd;

	dprintf(("hpfs_mount():\n"));
	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	error = copyin(data, (caddr_t)&args, sizeof (struct hpfs_args));
	if (error)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		dprintf(("hpfs_mount: MNT_UPDATE: "));

		hpmp = VFSTOHPFS(mp);

		if (args.fspec == 0) {
			dprintf(("export 0x%x\n",args.export.ex_flags));
			error = vfs_export(mp, &hpmp->hpm_export, &args.export);
			if (error) {
				printf("hpfs_mount: vfs_export failed %d\n",
					error);
			}
			goto success;
		} else {
			dprintf(("name [FAILED]\n"));
			error = EINVAL;
			goto success;
		}
		dprintf(("\n"));
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
		goto error_1;

#if defined(__DragonFly__)
	if (!vn_isdisk(devvp, &error)) 
		goto error_2;
#else /* defined(__NetBSD__) */
	if (devvp->v_type != VBLK) {
		error = ENOTBLK;
		goto error_2;
	}
	if (umajor(devvp->v_udev) >= nblkdev) {
		error = ENXIO;
		goto error_2;
	}
#endif

	/*
	 ********************
	 * NEW MOUNT
	 ********************
	 */

	/* Save "mounted from" info for mount point (NULL pad)*/
	copyinstr(	args.fspec,			/* device name*/
			mp->mnt_stat.f_mntfromname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

	error = hpfs_mountfs(devvp, mp, &args, td);
	if (error)
		goto error_2;

	/*
	 * Initialize FS stat information in mount struct; uses
	 * mp->mnt_stat.f_mntfromname.
	 *
	 * This code is common to root and non-root mounts
	 */
	VFS_STATFS(mp, &mp->mnt_stat, td);
	return (error);

error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return (error);
}

/*
 * Common code for mount and mountroot
 */
int
hpfs_mountfs(struct vnode *devvp, struct mount *mp, struct hpfs_args *argsp,
	     struct thread *td)
{
	int error, ncount, ronly;
	struct sublock *sup;
	struct spblock *spp;
	struct hpfsmount *hpmp;
	struct buf *bp = NULL;
	struct vnode *vp;
	dev_t dev;

	dprintf(("hpfs_mountfs():\n"));
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	ncount = count_udev(devvp->v_udev);
#if defined(__DragonFly__)
	if (devvp->v_object)
		ncount -= 1;
#endif
	if (ncount > 0)
		return (EBUSY);

#if defined(__DragonFly__)
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = vinvalbuf(devvp, V_SAVE, td, 0, 0);
	VOP__UNLOCK(devvp, 0, td);
#else
	error = vinvalbuf(devvp, V_SAVE, td, 0, 0);
#endif
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	VN_LOCK(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, NULL, td);
	VOP__UNLOCK(devvp, 0, td);
	if (error)
		return (error);
	dev = devvp->v_rdev;

	/*
	 * Do actual mount
	 */
	hpmp = malloc(sizeof(struct hpfsmount), M_HPFSMNT, M_WAITOK);
	bzero(hpmp, sizeof(struct hpfsmount));

	/* Read in SuperBlock */
	error = bread(devvp, SUBLOCK, SUSIZE, &bp);
	if (error)
		goto failed;
	bcopy(bp->b_data, &hpmp->hpm_su, sizeof(struct sublock));
	brelse(bp); bp = NULL;

	/* Read in SpareBlock */
	error = bread(devvp, SPBLOCK, SPSIZE, &bp);
	if (error)
		goto failed;
	bcopy(bp->b_data, &hpmp->hpm_sp, sizeof(struct spblock));
	brelse(bp); bp = NULL;

	sup = &hpmp->hpm_su;
	spp = &hpmp->hpm_sp;

	/* Check magic */
	if (sup->su_magic != SU_MAGIC) {
		printf("hpfs_mountfs: SuperBlock MAGIC DOESN'T MATCH\n");
		error = EINVAL;
		goto failed;
	}
	if (spp->sp_magic != SP_MAGIC) {
		printf("hpfs_mountfs: SpareBlock MAGIC DOESN'T MATCH\n");
		error = EINVAL;
		goto failed;
	}

	mp->mnt_data = (qaddr_t)hpmp;
	hpmp->hpm_devvp = devvp;
	hpmp->hpm_dev = dev;
	hpmp->hpm_mp = mp;
	hpmp->hpm_uid = argsp->uid;
	hpmp->hpm_gid = argsp->gid;
	hpmp->hpm_mode = argsp->mode;

	error = hpfs_bminit(hpmp);
	if (error)
		goto failed;

	error = hpfs_cpinit(hpmp, argsp);
	if (error) {
		hpfs_bmdeinit(hpmp);
		goto failed;
	}
	vfs_add_vnodeops(mp, &mp->mnt_vn_norm_ops, hpfs_vnodeop_entries);

	error = hpfs_root(mp, &vp);
	if (error) {
		hpfs_cpdeinit(hpmp);
		hpfs_bmdeinit(hpmp);
		goto failed;
	}

	vput(vp);

#if defined(__DragonFly__)
	mp->mnt_stat.f_fsid.val[0] = (long)dev2udev(dev);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
#else
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_HPFS);
#endif
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
	dev->si_mountpoint = mp;
	return (0);

failed:
	if (bp)
		brelse (bp);
	mp->mnt_data = (qaddr_t)NULL;
#if defined(__DragonFly__)
	dev->si_mountpoint = NULL;
#else
	devvp->v_specflags &= ~SI_MOUNTEDON;
#endif
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, td);
	return (error);
}

#if !defined(__DragonFly__)
static int
hpfs_start(struct mount *mp, int flags, struct thread *td)
{
	return (0);
}
#endif

static int
hpfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	int error, flags, ronly;
	struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_unmount():\n"));

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("hpfs_unmount: vflushing...\n"));
	
	error = vflush(mp, 0, flags);
	if (error) {
		printf("hpfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

#if defined(__DragonFly__)
	hpmp->hpm_devvp->v_rdev->si_mountpoint = NULL;
#else
	hpmp->hpm_devvp->v_specflags &= ~SI_MOUNTEDON;
#endif

	vinvalbuf(hpmp->hpm_devvp, V_SAVE, td, 0, 0);
	error = VOP_CLOSE(hpmp->hpm_devvp, ronly ? FREAD : FREAD|FWRITE, td);

	vrele(hpmp->hpm_devvp);

	dprintf(("hpfs_umount: freeing memory...\n"));
	hpfs_cpdeinit(hpmp);
	hpfs_bmdeinit(hpmp);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	FREE(hpmp, M_HPFSMNT);

	return (0);
}

static int
hpfs_root(struct mount *mp, struct vnode **vpp)
{
	int error = 0;
	struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_root():\n"));
	error = VFS_VGET(mp, (ino_t)hpmp->hpm_su.su_rootfno, vpp);
	if(error) {
		printf("hpfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	return (error);
}

static int
hpfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct hpfsmount *hpmp = VFSTOHPFS(mp);

	dprintf(("hpfs_statfs(): HPFS%d.%d\n",
		hpmp->hpm_su.su_hpfsver, hpmp->hpm_su.su_fnctver));

#if defined(__DragonFly__)
	sbp->f_type = mp->mnt_vfc->vfc_typenum;
#else /* defined(__NetBSD__) */
	sbp->f_type = 0;
#endif
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = hpmp->hpm_su.su_btotal;
	sbp->f_bfree = sbp->f_bavail = hpmp->hpm_bavail;
	sbp->f_ffree = 0;
	sbp->f_files = 0;
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	sbp->f_flags = mp->mnt_flag;
	
	return (0);
}

#if !defined(__DragonFly__)
static int
hpfs_sync(struct mount *mp, int waitfor, struct ucred *cred,
	  struct thread *td)
{
	return (0);
}

static int
hpfs_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg,
	      struct thread *td)
{
	printf("hpfs_quotactl():\n");
	return (EOPNOTSUPP);
}
#endif

/*ARGSUSED*/
static int
hpfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct vnode *nvp;
	struct hpfid *hpfhp = (struct hpfid *)fhp;
	int error;

	if ((error = VFS_VGET(mp, hpfhp->hpfid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with HPFS, we don't need to check anything else for now */
	*vpp = nvp;

	return (0);
}

static int
hpfs_vptofh(struct vnode *vp, struct fid *fhp)
{
	struct hpfsnode *hpp;
	struct hpfid *hpfhp;

	hpp = VTOHP(vp);
	hpfhp = (struct hpfid *)fhp;
	hpfhp->hpfid_len = sizeof(struct hpfid);
	hpfhp->hpfid_ino = hpp->h_no;
	/* hpfhp->hpfid_gen = hpp->h_gen; */
	return (0);
}

static int
hpfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp) 
{
	struct hpfsmount *hpmp = VFSTOHPFS(mp);
	struct vnode *vp;
	struct hpfsnode *hp;
	struct buf *bp;
	struct thread *td = curthread;	/* XXX */
	int error;

	dprintf(("hpfs_vget(0x%x): ",ino));

	*vpp = NULL;
	hp = NULL;
	vp = NULL;

	if ((*vpp = hpfs_hphashvget(hpmp->hpm_dev, ino, td)) != NULL) {
		dprintf(("hashed\n"));
		return (0);
	}

	/*
	 * We have to lock node creation for a while,
	 * but then we have to call getnewvnode(), 
	 * this may cause hpfs_reclaim() to be called,
	 * this may need to VOP_VGET() parent dir for
	 * update reasons, and if parent is not in
	 * hash, we have to lock node creation...
	 * To solve this, we MALLOC, getnewvnode and init while
	 * not locked (probability of node appearence
	 * at that time is little, and anyway - we'll
	 * check for it).
	 */
	MALLOC(hp, struct hpfsnode *, sizeof(struct hpfsnode), 
		M_HPFSNO, M_WAITOK);

	error = getnewvnode(VT_HPFS, hpmp->hpm_mp, &vp, VLKTIMEOUT, 0);
	if (error) {
		printf("hpfs_vget: can't get new vnode\n");
		FREE(hp, M_HPFSNO);
		return (error);
	}

	dprintf(("prenew "));

	vp->v_data = hp;

	if (ino == (ino_t)hpmp->hpm_su.su_rootfno) 
		vp->v_flag |= VROOT;

	lwkt_token_init(&hp->h_interlock);

	hp->h_flag = H_INVAL;
	hp->h_vp = vp;
	hp->h_hpmp = hpmp;
	hp->h_no = ino;
	hp->h_dev = hpmp->hpm_dev;
	hp->h_uid = hpmp->hpm_uid;
	hp->h_gid = hpmp->hpm_uid;
	hp->h_mode = hpmp->hpm_mode;
	hp->h_devvp = hpmp->hpm_devvp;
	vref(hp->h_devvp);

	do {
		if ((*vpp = hpfs_hphashvget(hpmp->hpm_dev, ino, td)) != NULL) {
			dprintf(("hashed2\n"));
			vx_put(vp);
			return (0);
		}
	} while(LOCKMGR(&hpfs_hphash_lock,LK_EXCLUSIVE|LK_SLEEPFAIL,NULL,NULL));

	hpfs_hphashins(hp);

	LOCKMGR(&hpfs_hphash_lock, LK_RELEASE, NULL, NULL);

	error = bread(hpmp->hpm_devvp, ino, FNODESIZE, &bp);
	if (error) {
		printf("hpfs_vget: can't read ino %d\n",ino);
		vx_put(vp);
		return (error);
	}
	bcopy(bp->b_data, &hp->h_fn, sizeof(struct fnode));
	brelse(bp);

	if (hp->h_fn.fn_magic != FN_MAGIC) {
		printf("hpfs_vget: MAGIC DOESN'T MATCH\n");
		vx_put(vp);
		return (EINVAL);
	}

	vp->v_type = hp->h_fn.fn_flag ? VDIR:VREG;
	hp->h_flag &= ~H_INVAL;

	/* Return the locked and refd vnode */
	*vpp = vp;

	return (0);
}

static struct vfsops hpfs_vfsops = {
	hpfs_mount,
	vfs_stdstart,
	hpfs_unmount,
	hpfs_root,
	vfs_stdquotactl,
	hpfs_statfs,
	vfs_stdsync,
	hpfs_vget,
	hpfs_fhtovp,
	hpfs_checkexp,
	hpfs_vptofh,
	hpfs_init,
	hpfs_hphash_uninit,
	vfs_stdextattrctl,
};

VFS_SET(hpfs_vfsops, hpfs, 0);

