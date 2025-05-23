/*
 * Copyright (c) 1999, 2000 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD: src/sys/nwfs/nwfs_vnops.c,v 1.6.2.3 2001/03/14 11:26:59 bp Exp $
 * $DragonFly: src/sys/vfs/nwfs/nwfs_vnops.c,v 1.21 2005/02/15 08:32:18 joerg Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

#include <netproto/ncp/ncp.h>
#include <netproto/ncp/ncp_conn.h>
#include <netproto/ncp/ncp_subr.h>
#include <netproto/ncp/nwerror.h>
#include <netproto/ncp/ncp_nls.h>

#include "nwfs.h"
#include "nwfs_node.h"
#include "nwfs_subr.h"

/*
 * Prototypes for NWFS vnode operations
 */
static int nwfs_create(struct vop_create_args *);
static int nwfs_mknod(struct vop_mknod_args *);
static int nwfs_open(struct vop_open_args *);
static int nwfs_close(struct vop_close_args *);
static int nwfs_access(struct vop_access_args *);
static int nwfs_getattr(struct vop_getattr_args *);
static int nwfs_setattr(struct vop_setattr_args *);
static int nwfs_read(struct vop_read_args *);
static int nwfs_write(struct vop_write_args *);
static int nwfs_fsync(struct vop_fsync_args *);
static int nwfs_remove(struct vop_remove_args *);
static int nwfs_link(struct vop_link_args *);
static int nwfs_lookup(struct vop_lookup_args *);
static int nwfs_rename(struct vop_rename_args *);
static int nwfs_mkdir(struct vop_mkdir_args *);
static int nwfs_rmdir(struct vop_rmdir_args *);
static int nwfs_symlink(struct vop_symlink_args *);
static int nwfs_readdir(struct vop_readdir_args *);
static int nwfs_bmap(struct vop_bmap_args *);
static int nwfs_strategy(struct vop_strategy_args *);
static int nwfs_print(struct vop_print_args *);
static int nwfs_pathconf(struct vop_pathconf_args *ap);

/* Global vfs data structures for nwfs */
struct vnodeopv_entry_desc nwfs_vnodeop_entries[] = {
	{ &vop_default_desc,		vop_defaultop },
	{ &vop_access_desc,		(vnodeopv_entry_t) nwfs_access },
	{ &vop_bmap_desc,		(vnodeopv_entry_t) nwfs_bmap },
	{ &vop_open_desc,		(vnodeopv_entry_t) nwfs_open },
	{ &vop_close_desc,		(vnodeopv_entry_t) nwfs_close },
	{ &vop_create_desc,		(vnodeopv_entry_t) nwfs_create },
	{ &vop_fsync_desc,		(vnodeopv_entry_t) nwfs_fsync },
	{ &vop_getattr_desc,		(vnodeopv_entry_t) nwfs_getattr },
	{ &vop_getpages_desc,		(vnodeopv_entry_t) nwfs_getpages },
	{ &vop_putpages_desc,		(vnodeopv_entry_t) nwfs_putpages },
	{ &vop_ioctl_desc,		(vnodeopv_entry_t) nwfs_ioctl },
	{ &vop_inactive_desc,		(vnodeopv_entry_t) nwfs_inactive },
	{ &vop_islocked_desc,		(vnodeopv_entry_t) vop_stdislocked },
	{ &vop_link_desc,		(vnodeopv_entry_t) nwfs_link },
	{ &vop_lock_desc,		(vnodeopv_entry_t) vop_stdlock },
	{ &vop_lookup_desc,		(vnodeopv_entry_t) nwfs_lookup },
	{ &vop_mkdir_desc,		(vnodeopv_entry_t) nwfs_mkdir },
	{ &vop_mknod_desc,		(vnodeopv_entry_t) nwfs_mknod },
	{ &vop_pathconf_desc,		(vnodeopv_entry_t) nwfs_pathconf },
	{ &vop_print_desc,		(vnodeopv_entry_t) nwfs_print },
	{ &vop_read_desc,		(vnodeopv_entry_t) nwfs_read },
	{ &vop_readdir_desc,		(vnodeopv_entry_t) nwfs_readdir },
	{ &vop_reclaim_desc,		(vnodeopv_entry_t) nwfs_reclaim },
	{ &vop_remove_desc,		(vnodeopv_entry_t) nwfs_remove },
	{ &vop_rename_desc,		(vnodeopv_entry_t) nwfs_rename },
	{ &vop_rmdir_desc,		(vnodeopv_entry_t) nwfs_rmdir },
	{ &vop_setattr_desc,		(vnodeopv_entry_t) nwfs_setattr },
	{ &vop_strategy_desc,		(vnodeopv_entry_t) nwfs_strategy },
	{ &vop_symlink_desc,		(vnodeopv_entry_t) nwfs_symlink },
	{ &vop_unlock_desc,		(vnodeopv_entry_t) vop_stdunlock },
	{ &vop_write_desc,		(vnodeopv_entry_t) nwfs_write },
	{ NULL, NULL }
};

/*
 * nwfs_access vnode op
 * for now just return ok
 *
 * nwfs_access(struct vnode *a_vp, int a_mode, struct ucred *a_cred,
 *		struct thread *a_td)
 */
static int
nwfs_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	u_int mode = ap->a_mode;
	struct nwmount *nmp = VTONWFS(vp);
	int error = 0;

	NCPVNDEBUG("\n");
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		    case VREG: case VDIR: case VLNK:
			return (EROFS);
		    default:
			break;
		}
	}
	if (cred->cr_uid == 0)
		return 0;
	if (cred->cr_uid != nmp->m.uid) {
		mode >>= 3;
		if (!groupmember(nmp->m.gid, cred))
			mode >>= 3;
	}
	error = (((vp->v_type == VREG) ? nmp->m.file_mode : nmp->m.dir_mode) & mode) == mode ? 0 : EACCES;
	return error;
}
/*
 * nwfs_open vnode op
 *
 * nwfs_open(struct vnode *a_vp, int a_mode, struct ucred *a_cred,
 *	     struct thread *a_td)
 */
/* ARGSUSED */
static int
nwfs_open(struct vop_open_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int mode = ap->a_mode;
	struct nwnode *np = VTONW(vp);
	struct ncp_open_info no;
	struct nwmount *nmp = VTONWFS(vp);
	struct vattr vattr;
	int error, nwm;

	NCPVNDEBUG("%s,%d\n",np->n_name, np->opened);
	if (vp->v_type != VREG && vp->v_type != VDIR) { 
		NCPFATAL("open vtype = %d\n", vp->v_type);
		return (EACCES);
	}
	if (vp->v_type == VDIR) return 0;	/* nothing to do now */
	if (np->n_flag & NMODIFIED) {
		if ((error = nwfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1)) == EINTR)
			return (error);
		np->n_atime = 0;
		error = VOP_GETATTR(vp, &vattr, ap->a_td);
		if (error) return (error);
		np->n_mtime = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_td);
		if (error) return (error);
		if (np->n_mtime != vattr.va_mtime.tv_sec) {
			if ((error = nwfs_vinvalbuf(vp, V_SAVE,	ap->a_td, 1)) == EINTR)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		}
	}
	if (np->opened) {
		np->opened++;
		return 0;
	}
	nwm = AR_READ;
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		nwm |= AR_WRITE;
	error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN,
					       0, nwm, &no, ap->a_td, ap->a_cred);
	if (error) {
		if (mode & FWRITE)
			return EACCES;
		nwm = AR_READ;
		error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN, 0,
						   nwm, &no, ap->a_td,ap->a_cred);
	}
	if (!error) {
		np->opened++;
		np->n_fh = no.fh;
		np->n_origfh = no.origfh;
	}
	np->n_atime = 0;
	return (error);
}

/*
 * nwfs_close(struct vnodeop_desc *a_desc, struct vnode *a_vp, int a_fflag,
 *	      struct ucred *a_cred, struct thread *a_td)
 */
static int
nwfs_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	int error;

	NCPVNDEBUG("name=%s,td=%p,c=%d\n",np->n_name,ap->a_td,np->opened);

	if (vp->v_type == VDIR) return 0;	/* nothing to do now */
	error = 0;
	if (np->opened == 0) {
		return 0;
	}
	error = nwfs_vinvalbuf(vp, V_SAVE, ap->a_td, 1);
	if (np->opened == 0) {
		return 0;
	}
	if (--np->opened == 0) {
		error = ncp_close_file(NWFSTOCONN(VTONWFS(vp)), &np->n_fh, 
		   ap->a_td, proc0.p_ucred);
	} 
	np->n_atime = 0;
	return (error);
}

/*
 * nwfs_getattr call from vfs.
 *
 * nwfs_getattr(struct vnode *a_vp, struct vattr *a_vap, struct ucred *a_cred,
 *		struct thread *a_td)
 */
static int
nwfs_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	struct vattr *va=ap->a_vap;
	struct nwmount *nmp = VTONWFS(vp);
	struct nw_entry_info fattr;
	int error;
	u_int32_t oldsize;

	NCPVNDEBUG("%lx:%d: '%s' %d\n", (long)vp, nmp->n_volume, np->n_name, (vp->v_flag & VROOT) != 0);
	error = nwfs_attr_cachelookup(vp,va);
	if (!error) return 0;
	NCPVNDEBUG("not in cache\n");
	oldsize = np->n_size;
	if (np->n_flag & NVOLUME) {
		error = ncp_obtain_info(nmp, np->n_fid.f_id, 0, NULL, &fattr,
		    ap->a_td,proc0.p_ucred);
	} else {
		error = ncp_obtain_info(nmp, np->n_fid.f_parent, np->n_nmlen, 
		    np->n_name, &fattr, ap->a_td, proc0.p_ucred);
	}
	if (error) {
		NCPVNDEBUG("error %d\n", error);
		return error;
	}
	nwfs_attr_cacheenter(vp, &fattr);
	*va = np->n_vattr;
	if (np->opened)
		np->n_size = oldsize;
	return (0);
}
/*
 * nwfs_setattr call from vfs.
 *
 * nwfs_setattr(struct vnode *a_vp, struct vattr *a_vap, struct ucred *a_cred,
 *		struct thread *a_td)
 */
static int
nwfs_setattr(struct vop_setattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	struct vattr *vap = ap->a_vap;
	u_quad_t tsize=0;
	int error = 0;

	NCPVNDEBUG("\n");
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);
	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL || 
	     vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	     vap->va_mode != (mode_t)VNOVAL) &&(vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VREG:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			vnode_pager_setsize(vp, (u_long)vap->va_size);
 			tsize = np->n_size;
 			np->n_size = vap->va_size;
			break;
 		default:
			return EINVAL;
  		};
  	}
	error = ncp_setattr(vp, vap, ap->a_cred, ap->a_td);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = tsize;
		vnode_pager_setsize(vp, (u_long)tsize);
	}
	np->n_atime = 0;	/* invalidate cache */
	VOP_GETATTR(vp, vap, ap->a_td);
	np->n_mtime = vap->va_mtime.tv_sec;
	return (0);
}

/*
 * nwfs_read call.
 *
 * nwfs_read(struct vnode *a_vp, struct uio *a_uio, int a_ioflag,
 *	     struct ucred *a_cred)
 */
static int
nwfs_read(struct vop_read_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio=ap->a_uio;
	int error;
	NCPVNDEBUG("nwfs_read:\n");

	if (vp->v_type != VREG && vp->v_type != VDIR)
		return (EPERM);
	error = nwfs_readvnode(vp, uio, ap->a_cred);
	return error;
}

/*
 * nwfs_write(struct vnode *a_vp, struct uio *a_uio, int a_ioflag,
 *	      struct ucred *a_cred)
 */
static int
nwfs_write(struct vop_write_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;

	NCPVNDEBUG("%d,ofs=%d,sz=%d\n",vp->v_type, (int)uio->uio_offset, uio->uio_resid);

	if (vp->v_type != VREG)
		return (EPERM);
	error = nwfs_writevnode(vp, uio, ap->a_cred,ap->a_ioflag);
	return(error);
}
/*
 * nwfs_create call
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. 
 *
 * nwfs_create(struct vnode *a_dvp, struct vnode **a_vpp,
 *		struct componentname *a_cnpl, struct vattr *a_vap)
 */
static int
nwfs_create(struct vop_create_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp=ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *vp = (struct vnode *)0;
	int error = 0, fmode;
	struct vattr vattr;
	struct nwnode *np;
	struct ncp_open_info no;
	struct nwmount *nmp=VTONWFS(dvp);
	ncpfid fid;
	

	NCPVNDEBUG("\n");
	*vpp = NULL;
	if (vap->va_type == VSOCK)
		return (EOPNOTSUPP);
	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_td))) {
		return (error);
	}
	fmode = AR_READ | AR_WRITE;
/*	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= AR_DENY_READ | AR_DENY_WRITE;*/
	
	error = ncp_open_create_file_or_subdir(nmp,dvp,cnp->cn_namelen,cnp->cn_nameptr, 
			   OC_MODE_CREATE | OC_MODE_OPEN | OC_MODE_REPLACE,
			   0, fmode, &no, cnp->cn_td, cnp->cn_cred);
	if (!error) {
		error = ncp_close_file(NWFSTOCONN(nmp), &no.fh, cnp->cn_td,cnp->cn_cred);
		fid.f_parent = VTONW(dvp)->n_fid.f_id;
		fid.f_id = no.fattr.dirEntNum;
		error = nwfs_nget(VTOVFS(dvp), fid, &no.fattr, dvp, &vp);
		if (!error) {
			np = VTONW(vp);
			np->opened = 0;
			*vpp = vp;
		}
	}
	return (error);
}

/*
 * nwfs_remove call. It isn't possible to emulate UFS behaivour because
 * NetWare doesn't allow delete/rename operations on an opened file.
 *
 * nwfs_remove(struct vnodeop_desc *a_desc, struct vnode *a_dvp,
 *		struct vnode *a_vp, struct componentname *a_cnp)
 */
static int
nwfs_remove(struct vop_remove_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	int error;

	if (vp->v_type == VDIR || np->opened || vp->v_usecount != 1) {
		error = EPERM;
	} else if (!ncp_conn_valid(NWFSTOCONN(nmp))) {
		error = EIO;
	} else {
		error = ncp_DeleteNSEntry(nmp, VTONW(dvp)->n_fid.f_id,
		    cnp->cn_namelen,cnp->cn_nameptr,cnp->cn_td,cnp->cn_cred);
		if (error == 0)
			np->n_flag |= NSHOULDFREE;
		else if (error == 0x899c)
			error = EACCES;
	}
	return (error);
}

/*
 * nwfs_file rename call
 *
 * nwfs_rename(struct vnode *a_fdvp, struct vnode *a_fvp,
 *		struct componentname *a_fcnp, struct vnode *a_tdvp,
 *		struct vnode *a_tvp, struct componentname *a_tcnp)
 */
static int
nwfs_rename(struct vop_rename_args *ap)
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct nwmount *nmp=VTONWFS(fvp);
	u_int16_t oldtype = 6;
	int error=0;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (tvp && tvp->v_usecount > 1) {
		error = EBUSY;
		goto out;
	}
	if (tvp && tvp != fvp) {
		error = ncp_DeleteNSEntry(nmp, VTONW(tdvp)->n_fid.f_id,
		    tcnp->cn_namelen, tcnp->cn_nameptr, 
		    tcnp->cn_td, tcnp->cn_cred);
		if (error == 0x899c) error = EACCES;
		if (error)
			goto out;
	}
	if (fvp->v_type == VDIR) {
		oldtype |= NW_TYPE_SUBDIR;
	} else if (fvp->v_type == VREG) {
		oldtype |= NW_TYPE_FILE;
	} else
		return EINVAL;
	error = ncp_nsrename(NWFSTOCONN(nmp), nmp->n_volume, nmp->name_space, 
		oldtype, &nmp->m.nls,
		VTONW(fdvp)->n_fid.f_id, fcnp->cn_nameptr, fcnp->cn_namelen,
		VTONW(tdvp)->n_fid.f_id, tcnp->cn_nameptr, tcnp->cn_namelen,
		tcnp->cn_td,tcnp->cn_cred);

	if (error == 0x8992)
		error = EEXIST;
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	nwfs_attr_cacheremove(fdvp);
	nwfs_attr_cacheremove(tdvp);
	/*
	 * Need to get rid of old vnodes, because netware will change
	 * file id on rename
	 */
	vgone(fvp);
	if (tvp)
		vgone(tvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nwfs hard link create call
 * Netware filesystems don't know what links are.
 *
 * nwfs_link(struct vnode *a_tdvp, struct vnode *a_vp,
 *	     struct componentname *a_cnp)
 */
static int
nwfs_link(struct vop_link_args *ap)
{
	return EOPNOTSUPP;
}

/*
 * nwfs_symlink link create call
 * Netware filesystems don't know what symlinks are.
 *
 * nwfs_symlink(struct vnode *a_dvp, struct vnode **a_vpp,
 *		struct componentname *a_cnp, struct vattr *a_vap,
 *		char *a_target)
 */
static int
nwfs_symlink(struct vop_symlink_args *ap)
{
	return (EOPNOTSUPP);
}

static int nwfs_mknod(struct vop_mknod_args *ap)
{
	return (EOPNOTSUPP);
}

/*
 * nwfs_mkdir call
 *
 * nwfs_mkdir(struct vnode *a_dvp, struct vnode **a_vpp,
 *		struct componentname *a_cnp, struct vattr *a_vap)
 */
static int
nwfs_mkdir(struct vop_mkdir_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
/*	struct vattr *vap = ap->a_vap;*/
	struct componentname *cnp = ap->a_cnp;
	int len=cnp->cn_namelen;
	struct ncp_open_info no;
	struct nwnode *np;
	struct vnode *newvp = (struct vnode *)0;
	ncpfid fid;
	int error = 0;
	struct vattr vattr;
	char *name=cnp->cn_nameptr;

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_td))) {
		return (error);
	}	
	if ((name[0] == '.') && ((len == 1) || ((len == 2) && (name[1] == '.')))) {
		return EEXIST;
	}
	if (ncp_open_create_file_or_subdir(VTONWFS(dvp),dvp, cnp->cn_namelen,
			cnp->cn_nameptr,OC_MODE_CREATE, aDIR, 0xffff,
			&no, cnp->cn_td, cnp->cn_cred) != 0) {
		error = EACCES;
	} else {
		error = 0;
        }
	if (!error) {
		fid.f_parent = VTONW(dvp)->n_fid.f_id;
		fid.f_id = no.fattr.dirEntNum;
		error = nwfs_nget(VTOVFS(dvp), fid, &no.fattr, dvp, &newvp);
		if (!error) {
			np = VTONW(newvp);
			newvp->v_type = VDIR;
			*ap->a_vpp = newvp;
		}
	}
	return (error);
}

/*
 * nwfs_remove directory call
 *
 * nwfs_rmdir(struct vnode *a_dvp, struct vnode *a_vp,
 *	      struct componentname *a_cnp)
 */
static int
nwfs_rmdir(struct vop_rmdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	struct nwnode *dnp = VTONW(dvp);
	int error = EIO;

	if (dvp == vp)
		return EINVAL;

	error = ncp_DeleteNSEntry(nmp, dnp->n_fid.f_id, 
		cnp->cn_namelen, cnp->cn_nameptr,cnp->cn_td,cnp->cn_cred);
	if (error == 0)
		np->n_flag |= NSHOULDFREE;
	else if (error == NWE_DIR_NOT_EMPTY)
		error = ENOTEMPTY;
	dnp->n_flag |= NMODIFIED;
	nwfs_attr_cacheremove(dvp);
	return (error);
}

/*
 * nwfs_readdir call
 *
 * nwfs_readdir(struct vnode *a_vp, struct uio *a_uio, struct ucred *a_cred,
 *		int *a_eofflag, u_long *a_cookies, int a_ncookies)
 */
static int
nwfs_readdir(struct vop_readdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;

	if (vp->v_type != VDIR)
		return (EPERM);
	if (ap->a_ncookies) {
		printf("nwfs_readdir: no support for cookies now...");
		return (EOPNOTSUPP);
	}

	error = nwfs_readvnode(vp, uio, ap->a_cred);
	return error;
}

/*
 * nwfs_fsync(struct vnodeop_desc *a_desc, struct vnode *a_vp,
 *	      struct ucred *a_cred, int a_waitfor, struct thread *a_td)
 */
/* ARGSUSED */
static int
nwfs_fsync(struct vop_fsync_args *ap)
{
/*	return (nfs_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_td, 1));*/
    return (0);
}

/*
 * nwfs_print(struct vnode *a_vp)
 */
/* ARGSUSED */
static int
nwfs_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);

	printf("nwfs node: name = '%s', fid = %d, pfid = %d\n",
	    np->n_name, np->n_fid.f_id, np->n_fid.f_parent);
	return (0);
}

/*
 * nwfs_pathconf(struct vnode *vp, int name, register_t *retval)
 */
static int
nwfs_pathconf(struct vop_pathconf_args *ap)
{
	int name=ap->a_name, error=0;
	register_t *retval=ap->a_retval;
	
	switch(name){
		case _PC_LINK_MAX:
		        *retval=0;
			break;
		case _PC_NAME_MAX:
			*retval=NCP_MAX_FILENAME; /* XXX from nwfsnode */
			break;
		case _PC_PATH_MAX:
			*retval=NCP_MAXPATHLEN; /* XXX from nwfsnode */
			break;
		default:
			error=EINVAL;
	}
	return(error);
}

/*
 * nwfs_strategy(struct buf *a_bp)
 */
static int
nwfs_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp=ap->a_bp;
	int error = 0;
	struct thread *td = NULL;

	NCPVNDEBUG("\n");
	if (bp->b_flags & B_PHYS)
		panic("nwfs physio");
	if ((bp->b_flags & B_ASYNC) == 0)
		td = curthread;		/* YYY dunno if this is legal */
	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 )
		error = nwfs_doio(bp, proc0.p_ucred, td);
	return (error);
}

/*
 * nwfs_bmap(struct vnode *a_vp, daddr_t a_bn, struct vnode **a_vpp,
 *	     daddr_t *a_bnp, int *a_runp, int *a_runb)
 */
static int
nwfs_bmap(struct vop_bmap_args *ap)
{
	struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

int
nwfs_nget(struct mount *mp, ncpfid fid, struct nw_entry_info *fap,
	  struct vnode *dvp, struct vnode **vpp)
{
	int error;
	struct nwnode *newnp;
	struct vnode *vp;

	*vpp = NULL;
	error = nwfs_allocvp(mp, fid, &vp);
	if (error)
		return error;
	newnp = VTONW(vp);
	if (fap) {
		newnp->n_attr = fap->attributes;
		vp->v_type = newnp->n_attr & aDIR ? VDIR : VREG;
		nwfs_attr_cacheenter(vp, fap);
	}
	if (dvp) {
		newnp->n_parent = VTONW(dvp)->n_fid;
		if ((newnp->n_flag & NNEW) && vp->v_type == VDIR) {
			if ((dvp->v_flag & VROOT) == 0) {
				newnp->n_refparent = 1;
				vref(dvp);	/* vhold */
			}
		}
	} else {
		if ((newnp->n_flag & NNEW) && vp->v_type == VREG)
			printf("new vnode '%s' borned without parent ?\n",newnp->n_name);
	}
	newnp->n_flag &= ~NNEW;
	*vpp = vp;
	return 0;
}

/*
 * How to keep the brain busy ...
 * Currently lookup routine can make two lookup for vnode. This can be
 * avoided by reorg the code.
 *
 * nwfs_lookup(struct vnodeop_desc *a_desc, struct vnode *a_dvp,
 *		struct vnode **a_vpp, struct componentname *a_cnp)
 */
int
nwfs_lookup(struct vop_lookup_args *ap)
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	int flags = cnp->cn_flags;
	struct vnode *vp;
	struct nwmount *nmp;
	struct mount *mp = dvp->v_mount;
	struct nwnode *dnp, *npp;
	struct nw_entry_info fattr, *fap;
	ncpfid fid;
	int nameiop=cnp->cn_nameiop;
	int lockparent, wantparent, error = 0, notfound;
	struct thread *td = cnp->cn_td;
	char _name[cnp->cn_namelen+1];
	bcopy(cnp->cn_nameptr,_name,cnp->cn_namelen);
	_name[cnp->cn_namelen]=0;
	
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	if ((flags & CNP_ISDOTDOT) && (dvp->v_flag & VROOT)) {
		printf("nwfs_lookup: invalid '..'\n");
		return EIO;
	}

	NCPVNDEBUG("%d '%s' in '%s' id=d\n", nameiop, _name, 
		VTONW(dvp)->n_name/*, VTONW(dvp)->n_name*/);

	if ((mp->mnt_flag & MNT_RDONLY) && nameiop != NAMEI_LOOKUP)
		return (EROFS);
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, td)))
		return (error);
	lockparent = flags & CNP_LOCKPARENT;
	wantparent = flags & (CNP_LOCKPARENT | CNP_WANTPARENT);
	nmp = VFSTONWFS(mp);
	dnp = VTONW(dvp);
/*
printf("dvp %d:%d:%d\n", (int)mp, (int)dvp->v_flag & VROOT, (int)flags & CNP_ISDOTDOT);
*/
	error = ncp_pathcheck(cnp->cn_nameptr, cnp->cn_namelen, &nmp->m.nls, 
	    (nameiop == NAMEI_CREATE || nameiop == NAMEI_RENAME) && (nmp->m.nls.opt & NWHP_NOSTRICT) == 0);
	if (error) 
	    return ENOENT;

	error = 0;
	*vpp = NULLVP;
	fap = NULL;
	if (flags & CNP_ISDOTDOT) {
		if (NWCMPF(&dnp->n_parent, &nmp->n_rootent)) {
			fid = nmp->n_rootent;
			fap = NULL;
			notfound = 0;
		} else {
			error = nwfs_lookupnp(nmp, dnp->n_parent, td, &npp);
			if (error) {
				return error;
			}
			fid = dnp->n_parent;
			fap = &fattr;
			/*np = *npp;*/
			notfound = ncp_obtain_info(nmp, npp->n_dosfid,
			    0, NULL, fap, td, cnp->cn_cred);
		}
	} else {
		fap = &fattr;
		notfound = ncp_lookup(dvp, cnp->cn_namelen, cnp->cn_nameptr,
			fap, td, cnp->cn_cred);
		fid.f_id = fap->dirEntNum;
		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
			fid.f_parent = dnp->n_fid.f_parent;
		} else
			fid.f_parent = dnp->n_fid.f_id;
		NCPVNDEBUG("call to ncp_lookup returned=%d\n",notfound);
	}
	if (notfound && notfound < 0x80 )
		return (notfound);	/* hard error */
	if (notfound) { /* entry not found */
		/* Handle RENAME or CREATE case... */
		if ((nameiop == NAMEI_CREATE || nameiop == NAMEI_RENAME) && wantparent) {
			if (!lockparent)
				VOP_UNLOCK(dvp, 0, td);
			return (EJUSTRETURN);
		}
		return ENOENT;
	}/* else {
		NCPVNDEBUG("Found entry %s with id=%d\n", fap->entryName, fap->dirEntNum);
	}*/
	/* handle DELETE case ... */
	if (nameiop == NAMEI_DELETE) { 	/* delete last component */
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, cnp->cn_td);
		if (error) return (error);
		if (NWCMPF(&dnp->n_fid, &fid)) {	/* we found ourselfs */
			vref(dvp);
			*vpp = dvp;
			return 0;
		}
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		if (!lockparent) VOP_UNLOCK(dvp, 0, td);
		return (0);
	}
	if (nameiop == NAMEI_RENAME && wantparent) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, cnp->cn_td);
		if (error) return (error);
		if (NWCMPF(&dnp->n_fid, &fid)) return EISDIR;
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		if (!lockparent)
			VOP_UNLOCK(dvp, 0, td);
		return (0);
	}
	if (flags & CNP_ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, td);	/* race to get the inode */
		error = nwfs_nget(mp, fid, NULL, NULL, &vp);
		if (error) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
			return (error);
		}
		if (lockparent && (error = vn_lock(dvp, LK_EXCLUSIVE, td))) {
		    	vput(vp);
			return (error);
		}
		*vpp = vp;
	} else if (NWCMPF(&dnp->n_fid, &fid)) {
		vref(dvp);
		*vpp = dvp;
	} else {
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		NCPVNDEBUG("lookup: getnewvp!\n");
		if (!lockparent)
			VOP_UNLOCK(dvp, 0, td);
	}
#if 0
	/* XXX MOVE TO NREMOVE */
	if ((cnp->cn_flags & CNP_MAKEENTRY)) {
		VTONW(*vpp)->n_ctime = VTONW(*vpp)->n_vattr.va_ctime.tv_sec;
		/* XXX */
	}
#endif
	return (0);
}
