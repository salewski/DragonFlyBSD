/*
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 *  	@(#) src/sys/coda/coda_vnops.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD: src/sys/coda/coda_vnops.c,v 1.22.2.1 2001/06/29 16:26:22 shafeeq Exp $
 * $DragonFly: src/sys/vfs/coda/Attic/coda_vnops.c,v 1.26 2005/02/17 14:00:10 joerg Exp $
 * 
 */

/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/nlookup.h>
#include <sys/namei.h>
#include <sys/ioccom.h>
#include <sys/select.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

#include "coda.h"
#include "cnode.h"
#include "coda_vnops.h"
#include "coda_venus.h"
#include "coda_opstats.h"
#include "coda_subr.h"
#include "coda_namecache.h"
#include "coda_pioctl.h"

/* 
 * These flags select various performance enhancements.
 */
int coda_attr_cache  = 1;       /* Set to cache attributes in the kernel */
int coda_symlink_cache = 1;     /* Set to cache symbolic link information */
int coda_access_cache = 1;      /* Set to handle some access checks directly */

/* structure to keep track of vfs calls */

struct coda_op_stats coda_vnodeopstats[CODA_VNODEOPS_SIZE];

#define MARK_ENTRY(op) (coda_vnodeopstats[op].entries++)
#define MARK_INT_SAT(op) (coda_vnodeopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (coda_vnodeopstats[op].unsat_intrn++)
#define MARK_INT_GEN(op) (coda_vnodeopstats[op].gen_intrn++)

/* What we are delaying for in printf */
int coda_printf_delay = 0;  /* in microseconds */
int coda_vnop_print_entry = 0;
static int coda_lockdebug = 0;

/* Definition of the vfs operation vector */

/*
 * Some NetBSD details:
 * 
 *   coda_start is called at the end of the mount syscall.
 *   coda_init is called at boot time.
 */

#define ENTRY  if(coda_vnop_print_entry) myprintf(("Entered %s\n",__func__))

/* Definition of the vnode operation vector */

struct vnodeopv_entry_desc coda_vnodeop_entries[] = {
    { &vop_default_desc, (vnodeopv_entry_t)coda_vop_error },
    { &vop_lookup_desc, (vnodeopv_entry_t)coda_lookup },	/* lookup */
    { &vop_create_desc, (vnodeopv_entry_t)coda_create },	/* create */
    { &vop_mknod_desc, (vnodeopv_entry_t)coda_vop_error },	/* mknod */
    { &vop_open_desc, (vnodeopv_entry_t)coda_open },		/* open */
    { &vop_close_desc, (vnodeopv_entry_t)coda_close },		/* close */
    { &vop_access_desc, (vnodeopv_entry_t)coda_access },	/* access */
    { &vop_getattr_desc, (vnodeopv_entry_t)coda_getattr },	/* getattr */
    { &vop_setattr_desc, (vnodeopv_entry_t)coda_setattr },	/* setattr */
    { &vop_read_desc, (vnodeopv_entry_t)coda_read },		/* read */
    { &vop_write_desc, (vnodeopv_entry_t)coda_write },		/* write */
    { &vop_ioctl_desc, (vnodeopv_entry_t)coda_ioctl },		/* ioctl */
    { &vop_mmap_desc, (vnodeopv_entry_t)coda_vop_error },	/* mmap */
    { &vop_fsync_desc, (vnodeopv_entry_t)coda_fsync },		/* fsync */
    { &vop_remove_desc, (vnodeopv_entry_t)coda_remove },	/* remove */
    { &vop_link_desc, (vnodeopv_entry_t)coda_link },		/* link */
    { &vop_rename_desc, (vnodeopv_entry_t)coda_rename },	/* rename */
    { &vop_mkdir_desc, (vnodeopv_entry_t)coda_mkdir },		/* mkdir */
    { &vop_rmdir_desc, (vnodeopv_entry_t)coda_rmdir },		/* rmdir */
    { &vop_symlink_desc, (vnodeopv_entry_t)coda_symlink },	/* symlink */
    { &vop_readdir_desc, (vnodeopv_entry_t)coda_readdir },	/* readdir */
    { &vop_readlink_desc, (vnodeopv_entry_t)coda_readlink },	/* readlink */
    { &vop_inactive_desc, (vnodeopv_entry_t)coda_inactive },	/* inactive */
    { &vop_reclaim_desc, (vnodeopv_entry_t)coda_reclaim },	/* reclaim */
    { &vop_lock_desc, (vnodeopv_entry_t)coda_lock },		/* lock */
    { &vop_unlock_desc, (vnodeopv_entry_t)coda_unlock },	/* unlock */
    { &vop_bmap_desc, (vnodeopv_entry_t)coda_bmap },		/* bmap */
    { &vop_strategy_desc, (vnodeopv_entry_t)coda_strategy },	/* strategy */
    { &vop_print_desc, (vnodeopv_entry_t)coda_vop_error },	/* print */
    { &vop_islocked_desc, (vnodeopv_entry_t)coda_islocked },	/* islocked */
    { &vop_pathconf_desc, (vnodeopv_entry_t)coda_vop_error },	/* pathconf */
    { &vop_advlock_desc, (vnodeopv_entry_t)coda_vop_nop },	/* advlock */
    { &vop_bwrite_desc, (vnodeopv_entry_t)coda_vop_error },	/* bwrite */
    { &vop_lease_desc, (vnodeopv_entry_t)coda_vop_nop },	/* lease */
    { &vop_poll_desc, vop_stdpoll },		/* poll */
    { &vop_getpages_desc, (vnodeopv_entry_t)coda_fbsd_getpages }, /* pager intf.*/
    { &vop_putpages_desc, (vnodeopv_entry_t)coda_fbsd_putpages }, /* pager intf.*/
    { &vop_createvobject_desc,      vop_stdcreatevobject },
    { &vop_destroyvobject_desc,     vop_stddestroyvobject },
    { &vop_getvobject_desc,         vop_stdgetvobject },

#if	0

    we need to define these someday
#define UFS_BLKATOFF(aa, bb, cc, dd) VFSTOUFS((aa)->v_mount)->um_blkatoff(aa, bb, cc, dd)
#define UFS_VALLOC(aa, bb, cc, dd) VFSTOUFS((aa)->v_mount)->um_valloc(aa, bb, cc, dd)
#define UFS_VFREE(aa, bb, cc) VFSTOUFS((aa)->v_mount)->um_vfree(aa, bb, cc)
#define UFS_TRUNCATE(aa, bb, cc, dd, ee) VFSTOUFS((aa)->v_mount)->um_truncate(aa, bb, cc, dd, ee)
#define UFS_UPDATE(aa, bb) VFSTOUFS((aa)->v_mount)->um_update(aa, bb)

    missing
    { &vop_reallocblks_desc,	(vnodeopv_entry_t) ufs_missingop },
    { &vop_lookup_desc,		(vnodeopv_entry_t) ufs_lookup },
    { &vop_whiteout_desc,	(vnodeopv_entry_t) ufs_whiteout },
#endif
    { NULL, NULL }
};

/* A generic panic: we were called with something we didn't define yet */
int
coda_vop_error(void *anon)
{
    struct vnodeop_desc **desc = (struct vnodeop_desc **)anon;

    myprintf(("coda_vop_error: Vnode operation %s called, but not defined.\n",
	      (*desc)->vdesc_name));
    /*
    panic("coda_vop_error");
    */
    return EIO;
}

/* A generic do-nothing.  For lease_check, advlock */
int
coda_vop_nop(void *anon)
{
    struct vnodeop_desc **desc = (struct vnodeop_desc **)anon;

    if (codadebug) {
	myprintf(("Vnode operation %s called, but unsupported\n",
		  (*desc)->vdesc_name));
    } 
   return (0);
}

int
coda_vnodeopstats_init(void)
{
	int i;
	
	for(i=0;i<CODA_VNODEOPS_SIZE;i++) {
		coda_vnodeopstats[i].opcode = i;
		coda_vnodeopstats[i].entries = 0;
		coda_vnodeopstats[i].sat_intrn = 0;
		coda_vnodeopstats[i].unsat_intrn = 0;
		coda_vnodeopstats[i].gen_intrn = 0;
	}
	return 0;
}
		
/* 
 * coda_open calls Venus to return the device, inode pair of the cache
 * file holding the data. Using iget, coda_open finds the vnode of the
 * cache file, and then opens it.
 */
int
coda_open(void *v)
{
    /* 
     * NetBSD can pass the O_EXCL flag in mode, even though the check
     * has already happened.  Venus defensively assumes that if open
     * is passed the EXCL, it must be a bug.  We strip the flag here.
     */
/* true args */
    struct vop_open_args *ap = v;
    struct vnode **vpp = &(ap->a_vp);
    struct cnode *cp = VTOC(*vpp);
    int flag = ap->a_mode & (~O_EXCL);
    struct ucred *cred = ap->a_cred;
    struct thread *td = ap->a_td;
/* locals */
    int error;
    struct vnode *vp;
    dev_t dev;
    ino_t inode;

    MARK_ENTRY(CODA_OPEN_STATS);

    /* Check for open of control file. */
    if (IS_CTL_VP(*vpp)) {
	/* XXX */
	/* if (WRITEABLE(flag)) */ 
	if (flag & (FWRITE | O_TRUNC | O_CREAT | O_EXCL)) {
	    MARK_INT_FAIL(CODA_OPEN_STATS);
	    return(EACCES);
	}
	MARK_INT_SAT(CODA_OPEN_STATS);
	return(0);
    }

    error = venus_open(vtomi((*vpp)), &cp->c_fid, flag, cred, td, &dev, &inode);
    if (error)
	return (error);
    if (!error) {
	CODADEBUG( CODA_OPEN,myprintf(("open: dev %#lx inode %lu result %d\n",
				       (u_long)dev2udev(dev), (u_long)inode,
				       error)); )
    }

    /* Translate the <device, inode> pair for the cache file into
       an inode pointer. */
    error = coda_grab_vnode(dev, inode, &vp);
    if (error)
	return (error);

    /* We get the vnode back locked.  Needs unlocked */
    VOP_UNLOCK(vp, 0, td);
    /* Keep a reference until the close comes in. */
    vref(*vpp);                

    /* Save the vnode pointer for the cache file. */
    if (cp->c_ovp == NULL) {
	cp->c_ovp = vp;
    } else {
	if (cp->c_ovp != vp)
	    panic("coda_open:  cp->c_ovp != ITOV(ip)");
    }
    cp->c_ocount++;

    /* Flush the attribute cached if writing the file. */
    if (flag & FWRITE) {
	cp->c_owrite++;
	cp->c_flags &= ~C_VATTR;
    }

    /* Save the <device, inode> pair for the cache file to speed
       up subsequent page_read's. */
    cp->c_device = dev;
    cp->c_inode = inode;

    /* Open the cache file. */
    error = VOP_OPEN(vp, flag, cred, NULL, td); 
    if (error) {
    	printf("coda_open: VOP_OPEN on container failed %d\n", error);
	return (error);
    }
/* grab (above) does this when it calls newvnode unless it's in the cache*/
    if (vp->v_type == VREG) {
    	error = vfs_object_create(vp, td);
	if (error != 0) {
	    printf("coda_open: vfs_object_create() returns %d\n", error);
	    vput(vp);
	}
    }

    return(error);
}

/*
 * Close the cache file used for I/O and notify Venus.
 */
int
coda_close(void *v)
{
/* true args */
    struct vop_close_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    int flag = ap->a_fflag;
    struct thread *td = ap->a_td;
/* locals */
    int error;

    MARK_ENTRY(CODA_CLOSE_STATS);

    /* Check for close of control file. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CODA_CLOSE_STATS);
	return(0);
    }

    if (IS_UNMOUNTING(cp)) {
	if (cp->c_ovp) {
#ifdef	CODA_VERBOSE
	    printf("coda_close: destroying container ref %d, ufs vp %p of vp %p/cp %p\n",
		    vp->v_usecount, cp->c_ovp, vp, cp);
#endif
#ifdef	hmm
	    vgone(cp->c_ovp);
#else
	    VOP_CLOSE(cp->c_ovp, flag, td); /* Do errors matter here? */
	    vrele(cp->c_ovp);
#endif
	} else {
#ifdef	CODA_VERBOSE
	    printf("coda_close: NO container vp %p/cp %p\n", vp, cp);
#endif
	}
	return ENODEV;
    } else {
	VOP_CLOSE(cp->c_ovp, flag, td); /* Do errors matter here? */
	vrele(cp->c_ovp);
    }

    if (--cp->c_ocount == 0)
	cp->c_ovp = NULL;

    if (flag & FWRITE)                    /* file was opened for write */
	--cp->c_owrite;

    error = venus_close(vtomi(vp), &cp->c_fid, flag, proc0.p_ucred, td);
    vrele(CTOV(cp));

    CODADEBUG(CODA_CLOSE, myprintf(("close: result %d\n",error)); )
    return(error);
}

int
coda_read(void *v)
{
    struct vop_read_args *ap = v;

    ENTRY;
    return(coda_rdwr(ap->a_vp, ap->a_uio, UIO_READ,
		    ap->a_ioflag, ap->a_cred, ap->a_uio->uio_td));
}

int
coda_write(void *v)
{
    struct vop_write_args *ap = v;

    ENTRY;
    return(coda_rdwr(ap->a_vp, ap->a_uio, UIO_WRITE,
		    ap->a_ioflag, ap->a_cred, ap->a_uio->uio_td));
}

int
coda_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw, int ioflag,
	  struct ucred *cred, struct thread *td)
{ 
/* upcall decl */
  /* NOTE: container file operation!!! */
/* locals */
    struct cnode *cp = VTOC(vp);
    struct vnode *cfvp = cp->c_ovp;
    int igot_internally = 0;
    int opened_internally = 0;
    int error = 0;

    MARK_ENTRY(CODA_RDWR_STATS);

    CODADEBUG(CODA_RDWR, myprintf(("coda_rdwr(%d, %p, %d, %lld, %d)\n", rw, 
			      (void *)uiop->uio_iov->iov_base, uiop->uio_resid, 
			      (long long)uiop->uio_offset, uiop->uio_segflg)); )
	
    /* Check for rdwr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_RDWR_STATS);
	return(EINVAL);
    }

    /* 
     * If file is not already open this must be a page
     * {read,write} request.  Iget the cache file's inode
     * pointer if we still have its <device, inode> pair.
     * Otherwise, we must do an internal open to derive the
     * pair. 
     */
    if (cfvp == NULL) {
	/* 
	 * If we're dumping core, do the internal open. Otherwise
	 * venus won't have the correct size of the core when
	 * it's completely written.
	 */
	if (cp->c_inode != 0 && (ioflag & IO_CORE) == 0) { 
	    igot_internally = 1;
	    error = coda_grab_vnode(cp->c_device, cp->c_inode, &cfvp);
	    if (error) {
		MARK_INT_FAIL(CODA_RDWR_STATS);
		return(error);
	    }
	    /* 
	     * We get the vnode back locked in both Mach and
	     * NetBSD.  Needs unlocked 
	     */
	    VOP_UNLOCK(cfvp, 0, td);
	}
	else {
	    opened_internally = 1;
	    MARK_INT_GEN(CODA_OPEN_STATS);
	    error = VOP_OPEN(vp, (rw == UIO_READ ? FREAD : FWRITE), 
			     cred, NULL, td);
printf("coda_rdwr: Internally Opening %p\n", vp);
	    if (error) {
		printf("coda_rdwr: VOP_OPEN on container failed %d\n", error);
		return (error);
	    }
	    if (vp->v_type == VREG) {
		error = vfs_object_create(vp, td);
		if (error != 0) {
		    printf("coda_rdwr: vfs_object_create() returns %d\n", error);
		    vput(vp);
		}
	    }
	    if (error) {
		MARK_INT_FAIL(CODA_RDWR_STATS);
		return(error);
	    }
	    cfvp = cp->c_ovp;
	}
    }

    /* Have UFS handle the call. */
    CODADEBUG(CODA_RDWR, myprintf(("indirect rdwr: fid = (%lx.%lx.%lx), refcnt = %d\n",
			      cp->c_fid.Volume, cp->c_fid.Vnode, 
			      cp->c_fid.Unique, CTOV(cp)->v_usecount)); )


    if (rw == UIO_READ) {
	error = VOP_READ(cfvp, uiop, ioflag, cred);
    } else {
	error = VOP_WRITE(cfvp, uiop, ioflag, cred);
	/* ufs_write updates the vnode_pager_setsize for the vnode/object */

	{   struct vattr attr;

	    if (VOP_GETATTR(cfvp, &attr, td) == 0) {
		vnode_pager_setsize(vp, attr.va_size);
	    }
	}
    }

    if (error)
	MARK_INT_FAIL(CODA_RDWR_STATS);
    else
	MARK_INT_SAT(CODA_RDWR_STATS);

    /* Do an internal close if necessary. */
    if (opened_internally) {
	MARK_INT_GEN(CODA_CLOSE_STATS);
	(void)VOP_CLOSE(vp, (rw == UIO_READ ? FREAD : FWRITE), td);
    }

    /* Invalidate cached attributes if writing. */
    if (rw == UIO_WRITE)
	cp->c_flags &= ~C_VATTR;
    return(error);
}

int
coda_ioctl(void *v)
{
/* true args */
    struct vop_ioctl_args *ap = v;
    struct vnode *vp = ap->a_vp;
    int com = ap->a_command;
    caddr_t data = ap->a_data;
    int flag = ap->a_fflag;
    struct ucred *cred = ap->a_cred;
    struct thread *td = ap->a_td;
/* locals */
    int error;
    struct vnode *tvp;
    struct nlookupdata nd;
    struct PioctlData *iap = (struct PioctlData *)data;

    MARK_ENTRY(CODA_IOCTL_STATS);

    CODADEBUG(CODA_IOCTL, myprintf(("in coda_ioctl on %s\n", iap->path));)
	
    /* Don't check for operation on a dying object, for ctlvp it
       shouldn't matter */
	
    /* Must be control object to succeed. */
    if (!IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_IOCTL_STATS);
	CODADEBUG(CODA_IOCTL, myprintf(("coda_ioctl error: vp != ctlvp"));)
	    return (EOPNOTSUPP);
    }
    /* Look up the pathname. */

    /* Should we use the name cache here? It would get it from
       lookupname sooner or later anyway, right? */

    tvp = NULL;
    error = nlookup_init(&nd, iap->path, UIO_USERSPACE,
			(iap->follow ? NLC_FOLLOW : 0));
    if (error == 0)
	error = nlookup(&nd);
    if (error == 0)
	error = cache_vref(nd.nl_ncp, nd.nl_cred, &tvp);
    nlookup_done(&nd);

    if (error) {
	MARK_INT_FAIL(CODA_IOCTL_STATS);
	CODADEBUG(CODA_IOCTL, myprintf(("coda_ioctl error: lookup returns %d\n",
				   error));)
	return(error);
    }

    /* 
     * Make sure this is a coda style cnode, but it may be a
     * different vfsp 
     */
    if (tvp->v_tag != VT_CODA) {
	vrele(tvp);
	MARK_INT_FAIL(CODA_IOCTL_STATS);
	CODADEBUG(CODA_IOCTL, 
		 myprintf(("coda_ioctl error: %s not a coda object\n", 
			iap->path));)
	return(EINVAL);
    }

    if (iap->vi.in_size > VC_MAXDATASIZE) {
	vrele(tvp);
	return(EINVAL);
    }
    error = venus_ioctl(vtomi(tvp), &((VTOC(tvp))->c_fid), com, flag, data, cred, td);

    if (error)
	MARK_INT_FAIL(CODA_IOCTL_STATS);
    else
	CODADEBUG(CODA_IOCTL, myprintf(("Ioctl returns %d \n", error)); )

    vrele(tvp);
    return(error);
}

/*
 * To reduce the cost of a user-level venus;we cache attributes in
 * the kernel.  Each cnode has storage allocated for an attribute. If
 * c_vattr is valid, return a reference to it. Otherwise, get the
 * attributes from venus and store them in the cnode.  There is some
 * question if this method is a security leak. But I think that in
 * order to make this call, the user must have done a lookup and
 * opened the file, and therefore should already have access.  
 */
int
coda_getattr(void *v)
{
/* true args */
    struct vop_getattr_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct vattr *vap = ap->a_vap;
    struct thread *td = ap->a_td;
/* locals */
    int error;

    MARK_ENTRY(CODA_GETATTR_STATS);

    if (IS_UNMOUNTING(cp))
	return ENODEV;

    /* Check for getattr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_GETATTR_STATS);
	return(ENOENT);
    }

    /* Check to see if the attributes have already been cached */
    if (VALID_VATTR(cp)) { 
	CODADEBUG(CODA_GETATTR, { myprintf(("attr cache hit: (%lx.%lx.%lx)\n",
				       cp->c_fid.Volume,
				       cp->c_fid.Vnode,
				       cp->c_fid.Unique));});
	CODADEBUG(CODA_GETATTR, if (!(codadebug & ~CODA_GETATTR))
		 print_vattr(&cp->c_vattr); );
	
	*vap = cp->c_vattr;
	MARK_INT_SAT(CODA_GETATTR_STATS);
	return(0);
    }

    error = venus_getattr(vtomi(vp), &cp->c_fid, proc0.p_ucred, td, vap);

    if (!error) {
	CODADEBUG(CODA_GETATTR, myprintf(("getattr miss (%lx.%lx.%lx): result %d\n",
				     cp->c_fid.Volume,
				     cp->c_fid.Vnode,
				     cp->c_fid.Unique,
				     error)); )
	    
	CODADEBUG(CODA_GETATTR, if (!(codadebug & ~CODA_GETATTR))
		 print_vattr(vap);	);
	
    {	int size = vap->va_size;
    	struct vnode *convp = cp->c_ovp;
	if (convp != (struct vnode *)0) {
	    vnode_pager_setsize(convp, size);
	}
    }
	/* If not open for write, store attributes in cnode */   
	if ((cp->c_owrite == 0) && (coda_attr_cache)) {  
	    cp->c_vattr = *vap;
	    cp->c_flags |= C_VATTR; 
	}
	
    }
    return(error);
}

int
coda_setattr(void *v)
{
/* true args */
    struct vop_setattr_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct vattr *vap = ap->a_vap;
    struct ucred *cred = ap->a_cred;
    struct thread *td = ap->a_td;
/* locals */
    int error;

    MARK_ENTRY(CODA_SETATTR_STATS);

    /* Check for setattr of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_SETATTR_STATS);
	return(ENOENT);
    }

    if (codadebug & CODADBGMSK(CODA_SETATTR)) {
	print_vattr(vap);
    }
    error = venus_setattr(vtomi(vp), &cp->c_fid, vap, cred, td);

    if (!error)
	cp->c_flags &= ~C_VATTR;

    {	int size = vap->va_size;
    	struct vnode *convp = cp->c_ovp;
	if (size != VNOVAL && convp != (struct vnode *)0) {
	    vnode_pager_setsize(convp, size);
	}
    }
    CODADEBUG(CODA_SETATTR,	myprintf(("setattr %d\n", error)); )
    return(error);
}

int
coda_access(void *v)
{
/* true args */
    struct vop_access_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    int mode = ap->a_mode;
    struct ucred *cred = ap->a_cred;
    struct thread *td = ap->a_td;
/* locals */
    int error;

    MARK_ENTRY(CODA_ACCESS_STATS);

    /* Check for access of control object.  Only read access is
       allowed on it. */
    if (IS_CTL_VP(vp)) {
	/* bogus hack - all will be marked as successes */
	MARK_INT_SAT(CODA_ACCESS_STATS);
	return(((mode & VREAD) && !(mode & (VWRITE | VEXEC))) 
	       ? 0 : EACCES);
    }

    /*
     * if the file is a directory, and we are checking exec (eg lookup) 
     * access, and the file is in the namecache, then the user must have 
     * lookup access to it.
     */
    if (coda_access_cache) {
	if ((vp->v_type == VDIR) && (mode & VEXEC)) {
	    if (coda_nc_lookup(cp, ".", 1, cred)) {
		MARK_INT_SAT(CODA_ACCESS_STATS);
		return(0);                     /* it was in the cache */
	    }
	}
    }

    error = venus_access(vtomi(vp), &cp->c_fid, mode, cred, td);

    return(error);
}

int
coda_readlink(void *v)
{
/* true args */
    struct vop_readlink_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct uio *uiop = ap->a_uio;
    struct ucred *cred = ap->a_cred;
    struct thread *td = ap->a_uio->uio_td;
/* locals */
    int error;
    char *str;
    int len;

    MARK_ENTRY(CODA_READLINK_STATS);

    /* Check for readlink of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_READLINK_STATS);
	return(ENOENT);
    }

    if ((coda_symlink_cache) && (VALID_SYMLINK(cp))) { /* symlink was cached */
	uiop->uio_rw = UIO_READ;
	error = uiomove(cp->c_symlink, (int)cp->c_symlen, uiop);
	if (error)
	    MARK_INT_FAIL(CODA_READLINK_STATS);
	else
	    MARK_INT_SAT(CODA_READLINK_STATS);
	return(error);
    }

    error = venus_readlink(vtomi(vp), &cp->c_fid, cred, td, &str, &len);

    if (!error) {
	uiop->uio_rw = UIO_READ;
	error = uiomove(str, len, uiop);

	if (coda_symlink_cache) {
	    cp->c_symlink = str;
	    cp->c_symlen = len;
	    cp->c_flags |= C_SYMLINK;
	} else
	    CODA_FREE(str, len);
    }

    CODADEBUG(CODA_READLINK, myprintf(("in readlink result %d\n",error));)
    return(error);
}

int
coda_fsync(void *v)
{
/* true args */
    struct vop_fsync_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct thread *td = ap->a_td;
/* locals */
    struct vnode *convp = cp->c_ovp;
    int error;
   
    MARK_ENTRY(CODA_FSYNC_STATS);

    /* Check for fsync on an unmounting object */
    /* The NetBSD kernel, in it's infinite wisdom, can try to fsync
     * after an unmount has been initiated.  This is a Bad Thing,
     * which we have to avoid.  Not a legitimate failure for stats.
     */
    if (IS_UNMOUNTING(cp)) {
	return(ENODEV);
    }

    /* Check for fsync of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CODA_FSYNC_STATS);
	return(0);
    }

    if (convp)
    	VOP_FSYNC(convp, MNT_WAIT, td);

    /*
     * We see fsyncs with usecount == 1 then usecount == 0.
     * For now we ignore them.
     */
    /*
    if (!vp->v_usecount) {
    	printf("coda_fsync on vnode %p with %d usecount.  c_flags = %x (%x)\n",
		vp, vp->v_usecount, cp->c_flags, cp->c_flags&C_PURGING);
    }
    */

    /*
     * We can expect fsync on any vnode at all if venus is pruging it.
     * Venus can't very well answer the fsync request, now can it?
     * Hopefully, it won't have to, because hopefully, venus preserves
     * the (possibly untrue) invariant that it never purges an open
     * vnode.  Hopefully.
     */
    if (cp->c_flags & C_PURGING) {
	return(0);
    }

    /* needs research */
    return 0;
    error = venus_fsync(vtomi(vp), &cp->c_fid, proc0.p_ucred, td);

    CODADEBUG(CODA_FSYNC, myprintf(("in fsync result %d\n",error)); );
    return(error);
}

int
coda_inactive(void *v)
{
    /* XXX - at the moment, inactive doesn't look at cred, and doesn't
       have a proc pointer.  Oops. */
/* true args */
    struct vop_inactive_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct ucred *cred __attribute__((unused)) = NULL;
    /*struct thread *td = curthread;*/
/* upcall decl */
/* locals */

    /* We don't need to send inactive to venus - DCS */
    MARK_ENTRY(CODA_INACTIVE_STATS);

    if (IS_CTL_VP(vp)) {
	MARK_INT_SAT(CODA_INACTIVE_STATS);
	return 0;
    }

    CODADEBUG(CODA_INACTIVE, myprintf(("in inactive, %lx.%lx.%lx. vfsp %p\n",
				  cp->c_fid.Volume, cp->c_fid.Vnode, 
				  cp->c_fid.Unique, vp->v_mount));)

    /* If an array has been allocated to hold the symlink, deallocate it */
    if ((coda_symlink_cache) && (VALID_SYMLINK(cp))) {
	if (cp->c_symlink == NULL)
	    panic("coda_inactive: null symlink pointer in cnode");
	
	CODA_FREE(cp->c_symlink, cp->c_symlen);
	cp->c_flags &= ~C_SYMLINK;
	cp->c_symlen = 0;
    }

    /* Remove it from the table so it can't be found. */
    coda_unsave(cp);
    if ((struct coda_mntinfo *)(vp->v_mount->mnt_data) == NULL) {
	myprintf(("Help! vfsp->vfs_data was NULL, but vnode %p wasn't dying\n", vp));
	panic("badness in coda_inactive\n");
    }

    if (IS_UNMOUNTING(cp)) {
#ifdef	DEBUG
	printf("coda_inactive: IS_UNMOUNTING use %d: vp %p, cp %p\n", vp->v_usecount, vp, cp);
	if (cp->c_ovp != NULL)
	    printf("coda_inactive: cp->ovp != NULL use %d: vp %p, cp %p\n",
	    	   vp->v_usecount, vp, cp);
#endif
    } else {
#ifdef OLD_DIAGNOSTIC
	if (CTOV(cp)->v_usecount) {
	    panic("coda_inactive: nonzero reference count");
	}
	if (cp->c_ovp != NULL) {
	    panic("coda_inactive:  cp->ovp != NULL");
	}
#endif
	vgone(vp);
    }

    MARK_INT_SAT(CODA_INACTIVE_STATS);
    return(0);
}

/*
 * Remote file system operations having to do with directory manipulation.
 */

/* 
 * It appears that in NetBSD, lookup is supposed to return the vnode locked
 */
int
coda_lookup(void *v)
{
/* true args */
    struct vop_lookup_args *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct cnode *dcp = VTOC(dvp);
    struct vnode **vpp = ap->a_vpp;
    /* 
     * It looks as though ap->a_cnp->ni_cnd->cn_nameptr holds the rest
     * of the string to xlate, and that we must try to get at least
     * ap->a_cnp->ni_cnd->cn_namelen of those characters to macth.  I
     * could be wrong. 
     */
    struct componentname  *cnp = ap->a_cnp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* locals */
    struct cnode *cp;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    ViceFid VFid;
    int	vtype;
    int error = 0;

    MARK_ENTRY(CODA_LOOKUP_STATS);

    CODADEBUG(CODA_LOOKUP, myprintf(("lookup: %s in %lx.%lx.%lx\n",
				   nm, dcp->c_fid.Volume,
				   dcp->c_fid.Vnode, dcp->c_fid.Unique)););

    /* Check for lookup of control object. */
    if (IS_CTL_NAME(dvp, nm, len)) {
	*vpp = coda_ctlvp;
	vref(*vpp);
	MARK_INT_SAT(CODA_LOOKUP_STATS);
	goto exit;
    }

    if (len+1 > CODA_MAXNAMLEN) {
	MARK_INT_FAIL(CODA_LOOKUP_STATS);
	CODADEBUG(CODA_LOOKUP, myprintf(("name too long: lookup, %lx.%lx.%lx(%s)\n",
				    dcp->c_fid.Volume, dcp->c_fid.Vnode,
				    dcp->c_fid.Unique, nm)););
	*vpp = (struct vnode *)0;
	error = EINVAL;
	goto exit;
    }
    /* First try to look the file up in the cfs name cache */
    /* lock the parent vnode? */
    cp = coda_nc_lookup(dcp, nm, len, cred);
    if (cp) {
	*vpp = CTOV(cp);
	vref(*vpp);
	CODADEBUG(CODA_LOOKUP, 
		 myprintf(("lookup result %d vpp %p\n",error,*vpp));)
    } else {
	
	/* The name wasn't cached, so we need to contact Venus */
	error = venus_lookup(vtomi(dvp), &dcp->c_fid, nm, len, cred, td, &VFid, &vtype);
	
	if (error) {
	    MARK_INT_FAIL(CODA_LOOKUP_STATS);
	    CODADEBUG(CODA_LOOKUP, myprintf(("lookup error on %lx.%lx.%lx(%s)%d\n",
					dcp->c_fid.Volume, dcp->c_fid.Vnode, dcp->c_fid.Unique, nm, error));)
	    *vpp = (struct vnode *)0;
	} else {
	    MARK_INT_SAT(CODA_LOOKUP_STATS);
	    CODADEBUG(CODA_LOOKUP, 
		     myprintf(("lookup: vol %lx vno %lx uni %lx type %o result %d\n",
			    VFid.Volume, VFid.Vnode, VFid.Unique, vtype,
			    error)); )
		
	    cp = make_coda_node(&VFid, dvp->v_mount, vtype);
	    *vpp = CTOV(cp);
	    
	    /* enter the new vnode in the Name Cache only if the top bit isn't set */
	    /* And don't enter a new vnode for an invalid one! */
	    if (!(vtype & CODA_NOCACHE))
		coda_nc_enter(VTOC(dvp), nm, len, cred, VTOC(*vpp));
	}
    }

 exit:
    /* 
     * If we are creating, and this was the last name to be looked up,
     * and the error was ENOENT, then there really shouldn't be an
     * error and we can make the leaf NULL and return success.  Since
     * this is supposed to work under Mach as well as NetBSD, we're
     * leaving this fn wrapped.  We also must tell lookup/namei that
     * we need to save the last component of the name.  (Create will
     * have to free the name buffer later...lucky us...)
     */
    if ((cnp->cn_nameiop == NAMEI_CREATE || cnp->cn_nameiop == NAMEI_RENAME)
	&& error == ENOENT)
    {
	error = EJUSTRETURN;
	*ap->a_vpp = NULL;
    }

    /* 
     * If the lookup went well, we need to (potentially?) unlock the
     * parent, and lock the child.  We are only responsible for
     * checking to see if the parent is supposed to be unlocked before
     * we return.  We must always lock the child (provided there is
     * one, and (the parent isn't locked or it isn't the same as the
     * parent.)  Simple, huh?  We can never leave the parent locked unless
     * we are ISLASTCN
     */
    if (!error || (error == EJUSTRETURN)) {
	if ((cnp->cn_flags & CNP_LOCKPARENT) == 0) {
	    if ((error = VOP_UNLOCK(dvp, 0, td))) {
		return error; 
	    }	    
	    /* 
	     * The parent is unlocked.  As long as there is a child,
	     * lock it without bothering to check anything else. 
	     */
	    if (*ap->a_vpp) {
		if ((error = VOP_LOCK(*ap->a_vpp, LK_EXCLUSIVE, td))) {
		    printf("coda_lookup: ");
		    panic("unlocked parent but couldn't lock child");
		}
	    }
	} else {
	    /* The parent is locked, and may be the same as the child */
	    if (*ap->a_vpp && (*ap->a_vpp != dvp)) {
		/* Different, go ahead and lock it. */
		if ((error = VOP_LOCK(*ap->a_vpp, LK_EXCLUSIVE, td))) {
		    printf("coda_lookup: ");
		    panic("unlocked parent but couldn't lock child");
		}
	    }
	}
    } else {
	/* If the lookup failed, we need to ensure that the leaf is NULL */
	/* Don't change any locking? */
	*ap->a_vpp = NULL;
    }
    return(error);
}

/*ARGSUSED*/
int
coda_create(void *v)
{
/* true args */
    struct vop_create_args *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct cnode *dcp = VTOC(dvp);
    struct vattr *va = ap->a_vap;
    int exclusive = 1;
    int mode = ap->a_vap->va_mode;
    struct vnode **vpp = ap->a_vpp;
    struct componentname  *cnp = ap->a_cnp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* locals */
    int error;
    struct cnode *cp;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    ViceFid VFid;
    struct vattr attr;

    MARK_ENTRY(CODA_CREATE_STATS);

    /* All creates are exclusive XXX */
    /* I'm assuming the 'mode' argument is the file mode bits XXX */

    /* Check for create of control object. */
    if (IS_CTL_NAME(dvp, nm, len)) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CODA_CREATE_STATS);
	return(EACCES);
    }

    error = venus_create(vtomi(dvp), &dcp->c_fid, nm, len, exclusive, mode, va, cred, td, &VFid, &attr);

    if (!error) {
	
	/* If this is an exclusive create, panic if the file already exists. */
	/* Venus should have detected the file and reported EEXIST. */

	if ((exclusive == 1) &&
	    (coda_find(&VFid) != NULL))
	    panic("cnode existed for newly created file!");
	
	cp = make_coda_node(&VFid, dvp->v_mount, attr.va_type);
	*vpp = CTOV(cp);
	
	/* Update va to reflect the new attributes. */
	(*va) = attr;
	
	/* Update the attribute cache and mark it as valid */
	if (coda_attr_cache) {
	    VTOC(*vpp)->c_vattr = attr;
	    VTOC(*vpp)->c_flags |= C_VATTR;       
	}

	/* Invalidate the parent's attr cache, the modification time has changed */
	VTOC(dvp)->c_flags &= ~C_VATTR;
	
	/* enter the new vnode in the Name Cache */
	coda_nc_enter(VTOC(dvp), nm, len, cred, VTOC(*vpp));
	
	CODADEBUG(CODA_CREATE, 
		 myprintf(("create: (%lx.%lx.%lx), result %d\n",
			VFid.Volume, VFid.Vnode, VFid.Unique, error)); )
    } else {
	*vpp = (struct vnode *)0;
	CODADEBUG(CODA_CREATE, myprintf(("create error %d\n", error));)
    }

    if (!error) {
	if ((error = VOP_LOCK(*ap->a_vpp, LK_EXCLUSIVE, td))) {
	    printf("coda_create: ");
	    panic("unlocked parent but couldn't lock child");
	}
    }
    return(error);
}

int
coda_remove(void *v)
{
/* true args */
    struct vop_remove_args *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct cnode *cp = VTOC(dvp);
    struct componentname  *cnp = ap->a_cnp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* locals */
    int error;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    struct cnode *tp;

    MARK_ENTRY(CODA_REMOVE_STATS);

    CODADEBUG(CODA_REMOVE, myprintf(("remove: %s in %lx.%lx.%lx\n",
				   nm, cp->c_fid.Volume, cp->c_fid.Vnode,
				   cp->c_fid.Unique)););

    /* Remove the file's entry from the CODA Name Cache */
    /* We're being conservative here, it might be that this person
     * doesn't really have sufficient access to delete the file
     * but we feel zapping the entry won't really hurt anyone -- dcs
     */
    /* I'm gonna go out on a limb here. If a file and a hardlink to it
     * exist, and one is removed, the link count on the other will be
     * off by 1. We could either invalidate the attrs if cached, or
     * fix them. I'll try to fix them. DCS 11/8/94
     */
    tp = coda_nc_lookup(VTOC(dvp), nm, len, cred);
    if (tp) {
	if (VALID_VATTR(tp)) {	/* If attrs are cached */
	    if (tp->c_vattr.va_nlink > 1) {	/* If it's a hard link */
		tp->c_vattr.va_nlink--;
	    }
	}
	
	coda_nc_zapfile(VTOC(dvp), nm, len); 
	/* No need to flush it if it doesn't exist! */
    }
    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(dvp)->c_flags &= ~C_VATTR;

    /* Check for remove of control object. */
    if (IS_CTL_NAME(dvp, nm, len)) {
	MARK_INT_FAIL(CODA_REMOVE_STATS);
	return(ENOENT);
    }

    error = venus_remove(vtomi(dvp), &cp->c_fid, nm, len, cred, td);

    CODADEBUG(CODA_REMOVE, myprintf(("in remove result %d\n",error)); )

    return(error);
}

int
coda_link(void *v)
{
/* true args */
    struct vop_link_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct vnode *tdvp = ap->a_tdvp;
    struct cnode *tdcp = VTOC(tdvp);
    struct componentname *cnp = ap->a_cnp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* locals */
    int error;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;

    MARK_ENTRY(CODA_LINK_STATS);

    if (codadebug & CODADBGMSK(CODA_LINK)) {

	myprintf(("nb_link:   vp fid: (%lx.%lx.%lx)\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
	myprintf(("nb_link: tdvp fid: (%lx.%lx.%lx)\n",
		  tdcp->c_fid.Volume, tdcp->c_fid.Vnode, tdcp->c_fid.Unique));
	
    }
    if (codadebug & CODADBGMSK(CODA_LINK)) {
	myprintf(("link:   vp fid: (%lx.%lx.%lx)\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
	myprintf(("link: tdvp fid: (%lx.%lx.%lx)\n",
		  tdcp->c_fid.Volume, tdcp->c_fid.Vnode, tdcp->c_fid.Unique));

    }

    /* Check for link to/from control object. */
    if (IS_CTL_NAME(tdvp, nm, len) || IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_LINK_STATS);
	return(EACCES);
    }

    error = venus_link(vtomi(vp), &cp->c_fid, &tdcp->c_fid, nm, len, cred, td);

    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(tdvp)->c_flags &= ~C_VATTR;
    VTOC(vp)->c_flags &= ~C_VATTR;

    CODADEBUG(CODA_LINK,	myprintf(("in link result %d\n",error)); )

    return(error);
}

int
coda_rename(void *v)
{
/* true args */
    struct vop_rename_args *ap = v;
    struct vnode *odvp = ap->a_fdvp;
    struct cnode *odcp = VTOC(odvp);
    struct componentname  *fcnp = ap->a_fcnp;
    struct vnode *ndvp = ap->a_tdvp;
    struct cnode *ndcp = VTOC(ndvp);
    struct componentname  *tcnp = ap->a_tcnp;
    struct ucred *cred = fcnp->cn_cred;
    struct thread *td = fcnp->cn_td;
/* true args */
    int error;
    const char *fnm = fcnp->cn_nameptr;
    int flen = fcnp->cn_namelen;
    const char *tnm = tcnp->cn_nameptr;
    int tlen = tcnp->cn_namelen;

    MARK_ENTRY(CODA_RENAME_STATS);

    /* Hmmm.  The vnodes are already looked up.  Perhaps they are locked?
       This could be Bad. XXX */
#ifdef OLD_DIAGNOSTIC
    if ((fcnp->cn_cred != tcnp->cn_cred) || (fcnp->cn_td != tcnp->cn_td))
    {
	panic("coda_rename: component names don't agree");
    }
#endif

    /* Check for rename involving control object. */ 
    if (IS_CTL_NAME(odvp, fnm, flen) || IS_CTL_NAME(ndvp, tnm, tlen)) {
	MARK_INT_FAIL(CODA_RENAME_STATS);
	return(EACCES);
    }

    /* Problem with moving directories -- need to flush entry for .. */
    if (odvp != ndvp) {
	struct cnode *ovcp = coda_nc_lookup(VTOC(odvp), fnm, flen, cred);
	if (ovcp) {
	    struct vnode *ovp = CTOV(ovcp);
	    if ((ovp) &&
		(ovp->v_type == VDIR)) /* If it's a directory */
		coda_nc_zapfile(VTOC(ovp),"..", 2);
	}
    }

    /* Remove the entries for both source and target files */
    coda_nc_zapfile(VTOC(odvp), fnm, flen);
    coda_nc_zapfile(VTOC(ndvp), tnm, tlen);

    /* Invalidate the parent's attr cache, the modification time has changed */
    VTOC(odvp)->c_flags &= ~C_VATTR;
    VTOC(ndvp)->c_flags &= ~C_VATTR;

    if (flen+1 > CODA_MAXNAMLEN) {
	MARK_INT_FAIL(CODA_RENAME_STATS);
	error = EINVAL;
	goto exit;
    }

    if (tlen+1 > CODA_MAXNAMLEN) {
	MARK_INT_FAIL(CODA_RENAME_STATS);
	error = EINVAL;
	goto exit;
    }

    error = venus_rename(vtomi(odvp), &odcp->c_fid, &ndcp->c_fid, fnm, flen, tnm, tlen, cred, td);

 exit:
    CODADEBUG(CODA_RENAME, myprintf(("in rename result %d\n",error));)

    /* It seems to be incumbent on us to drop locks on all four vnodes */
    /* From-vnodes are not locked, only ref'd.  To-vnodes are locked. */

    vrele(ap->a_fvp);
    vrele(odvp);

    if (ap->a_tvp) {
	if (ap->a_tvp == ndvp) {
	    vrele(ap->a_tvp);
	} else {
	    vput(ap->a_tvp);
	}
    }

    vput(ndvp);
    return(error);
}

int
coda_mkdir(void *v)
{
/* true args */
    struct vop_mkdir_args *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct cnode *dcp = VTOC(dvp);	
    struct componentname  *cnp = ap->a_cnp;
    struct vattr *va = ap->a_vap;
    struct vnode **vpp = ap->a_vpp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* locals */
    int error;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    struct cnode *cp;
    ViceFid VFid;
    struct vattr ova;

    MARK_ENTRY(CODA_MKDIR_STATS);

    /* Check for mkdir of target object. */
    if (IS_CTL_NAME(dvp, nm, len)) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CODA_MKDIR_STATS);
	return(EACCES);
    }

    if (len+1 > CODA_MAXNAMLEN) {
	*vpp = (struct vnode *)0;
	MARK_INT_FAIL(CODA_MKDIR_STATS);
	return(EACCES);
    }

    error = venus_mkdir(vtomi(dvp), &dcp->c_fid, nm, len, va, cred, td, &VFid, &ova);

    if (!error) {
	if (coda_find(&VFid) != NULL)
	    panic("cnode existed for newly created directory!");
	
	
	cp =  make_coda_node(&VFid, dvp->v_mount, va->va_type);
	*vpp = CTOV(cp);
	
	/* enter the new vnode in the Name Cache */
	coda_nc_enter(VTOC(dvp), nm, len, cred, VTOC(*vpp));

	/* as a side effect, enter "." and ".." for the directory */
	coda_nc_enter(VTOC(*vpp), ".", 1, cred, VTOC(*vpp));
	coda_nc_enter(VTOC(*vpp), "..", 2, cred, VTOC(dvp));

	if (coda_attr_cache) {
	    VTOC(*vpp)->c_vattr = ova;		/* update the attr cache */
	    VTOC(*vpp)->c_flags |= C_VATTR;	/* Valid attributes in cnode */
	}

	/* Invalidate the parent's attr cache, the modification time has changed */
	VTOC(dvp)->c_flags &= ~C_VATTR;
	
	CODADEBUG( CODA_MKDIR, myprintf(("mkdir: (%lx.%lx.%lx) result %d\n",
				    VFid.Volume, VFid.Vnode, VFid.Unique, error)); )
    } else {
	*vpp = (struct vnode *)0;
	CODADEBUG(CODA_MKDIR, myprintf(("mkdir error %d\n",error));)
    }

    return(error);
}

int
coda_rmdir(void *v)
{
/* true args */
    struct vop_rmdir_args *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct cnode *dcp = VTOC(dvp);
    struct componentname  *cnp = ap->a_cnp;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
/* true args */
    int error;
    const char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    struct cnode *cp;
   
    MARK_ENTRY(CODA_RMDIR_STATS);

    /* Check for rmdir of control object. */
    if (IS_CTL_NAME(dvp, nm, len)) {
	MARK_INT_FAIL(CODA_RMDIR_STATS);
	return(ENOENT);
    }

    /* We're being conservative here, it might be that this person
     * doesn't really have sufficient access to delete the file
     * but we feel zapping the entry won't really hurt anyone -- dcs
     */
    /*
     * As a side effect of the rmdir, remove any entries for children of
     * the directory, especially "." and "..".
     */
    cp = coda_nc_lookup(dcp, nm, len, cred);
    if (cp) coda_nc_zapParentfid(&(cp->c_fid), NOT_DOWNCALL);

    /* Remove the file's entry from the CODA Name Cache */
    coda_nc_zapfile(dcp, nm, len);

    /* Invalidate the parent's attr cache, the modification time has changed */
    dcp->c_flags &= ~C_VATTR;

    error = venus_rmdir(vtomi(dvp), &dcp->c_fid, nm, len, cred, td);

    CODADEBUG(CODA_RMDIR, myprintf(("in rmdir result %d\n", error)); )

    return(error);
}

int
coda_symlink(void *v)
{
/* true args */
    struct vop_symlink_args *ap = v;
    struct vnode *tdvp = ap->a_dvp;
    struct cnode *tdcp = VTOC(tdvp);	
    struct componentname *cnp = ap->a_cnp;
    struct vattr *tva = ap->a_vap;
    char *path = ap->a_target;
    struct ucred *cred = cnp->cn_cred;
    struct thread *td = cnp->cn_td;
    struct vnode **vpp = ap->a_vpp;
/* locals */
    int error;
    /* 
     * XXX I'm assuming the following things about coda_symlink's
     * arguments: 
     *       t(foo) is the new name/parent/etc being created.
     *       lname is the contents of the new symlink. 
     */
    char *nm = cnp->cn_nameptr;
    int len = cnp->cn_namelen;
    int plen = strlen(path);

    /* 
     * Here's the strategy for the moment: perform the symlink, then
     * do a lookup to grab the resulting vnode.  I know this requires
     * two communications with Venus for a new sybolic link, but
     * that's the way the ball bounces.  I don't yet want to change
     * the way the Mach symlink works.  When Mach support is
     * deprecated, we should change symlink so that the common case
     * returns the resultant vnode in a vpp argument.
     */

    MARK_ENTRY(CODA_SYMLINK_STATS);

    /* Check for symlink of control object. */
    if (IS_CTL_NAME(tdvp, nm, len)) {
	MARK_INT_FAIL(CODA_SYMLINK_STATS);
	return(EACCES);
    }

    if (plen+1 > CODA_MAXPATHLEN) {
	MARK_INT_FAIL(CODA_SYMLINK_STATS);
	return(EINVAL);
    }

    if (len+1 > CODA_MAXNAMLEN) {
	MARK_INT_FAIL(CODA_SYMLINK_STATS);
	error = EINVAL;
	goto exit;
    }

    error = venus_symlink(vtomi(tdvp), &tdcp->c_fid, path, plen, nm, len, tva, cred, td);

    /* Invalidate the parent's attr cache, the modification time has changed */
    tdcp->c_flags &= ~C_VATTR;

    if (error == 0)
	error = VOP_LOOKUP(tdvp, vpp, cnp);

 exit:    
    CODADEBUG(CODA_SYMLINK, myprintf(("in symlink result %d\n",error)); )
    return(error);
}

/*
 * Read directory entries.
 */
int
coda_readdir(void *v)
{
/* true args */
    struct vop_readdir_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct uio *uiop = ap->a_uio;
    struct ucred *cred = ap->a_cred;
    int *eofflag = ap->a_eofflag;
    u_long **cookies = ap->a_cookies;
    int *ncookies = ap->a_ncookies;
    struct thread *td = ap->a_uio->uio_td;
/* upcall decl */
/* locals */
    int error = 0;

    MARK_ENTRY(CODA_READDIR_STATS);

    CODADEBUG(CODA_READDIR, myprintf(("coda_readdir(%p, %d, %lld, %d)\n",
				      (void *)uiop->uio_iov->iov_base,
				      uiop->uio_resid,
				      (long long)uiop->uio_offset,
				      uiop->uio_segflg)); )
	
    /* Check for readdir of control object. */
    if (IS_CTL_VP(vp)) {
	MARK_INT_FAIL(CODA_READDIR_STATS);
	return(ENOENT);
    }

    {
	/* If directory is not already open do an "internal open" on it. */
	int opened_internally = 0;
	if (cp->c_ovp == NULL) {
	    opened_internally = 1;
	    MARK_INT_GEN(CODA_OPEN_STATS);
	    error = VOP_OPEN(vp, FREAD, cred, NULL, td);
printf("coda_readdir: Internally Opening %p\n", vp);
	    if (error) {
		printf("coda_readdir: VOP_OPEN on container failed %d\n", error);
		return (error);
	    }
	    if (vp->v_type == VREG) {
		error = vfs_object_create(vp, td);
		if (error != 0) {
		    printf("coda_readdir: vfs_object_create() returns %d\n", error);
		    vput(vp);
		}
	    }
	    if (error) return(error);
	}
	
	/* Have UFS handle the call. */
	CODADEBUG(CODA_READDIR, myprintf(("indirect readdir: fid = (%lx.%lx.%lx), refcnt = %d\n",cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique, vp->v_usecount)); )
	error = VOP_READDIR(cp->c_ovp, uiop, cred, eofflag, ncookies,
			       cookies);
	
	if (error)
	    MARK_INT_FAIL(CODA_READDIR_STATS);
	else
	    MARK_INT_SAT(CODA_READDIR_STATS);
	
	/* Do an "internal close" if necessary. */ 
	if (opened_internally) {
	    MARK_INT_GEN(CODA_CLOSE_STATS);
	    (void)VOP_CLOSE(vp, FREAD, td);
	}
    }

    return(error);
}

/*
 * Convert from file system blocks to device blocks
 */
int
coda_bmap(void *v)
{
    /* XXX on the global proc */
/* true args */
    struct vop_bmap_args *ap = v;
    struct vnode *vp __attribute__((unused)) = ap->a_vp;	/* file's vnode */
    daddr_t bn __attribute__((unused)) = ap->a_bn;	/* fs block number */
    struct vnode **vpp = ap->a_vpp;			/* RETURN vp of device */
    daddr_t *bnp __attribute__((unused)) = ap->a_bnp;	/* RETURN device block number */
/* upcall decl */
/* locals */

	int ret = 0;
	struct cnode *cp;

	cp = VTOC(vp);
	if (cp->c_ovp) {
		return EINVAL;
		ret =  VOP_BMAP(cp->c_ovp, bn, vpp, bnp, ap->a_runp, ap->a_runb);
#if	0
		printf("VOP_BMAP(cp->c_ovp %p, bn %p, vpp %p, bnp %p, ap->a_runp %p, ap->a_runb %p) = %d\n",
			cp->c_ovp, bn, vpp, bnp, ap->a_runp, ap->a_runb, ret);
#endif
		return ret;
	} else {
#if	0
		printf("coda_bmap: no container\n");
#endif
		return(EOPNOTSUPP);
	}
}

/*
 * I don't think the following two things are used anywhere, so I've
 * commented them out 
 * 
 * struct buf *async_bufhead; 
 * int async_daemon_count;
 */
int
coda_strategy(void *v)
{
/* true args */
    struct vop_strategy_args *ap = v;
    struct buf *bp __attribute__((unused)) = ap->a_bp;
/* upcall decl */
/* locals */

	printf("coda_strategy: called ???\n");
	return(EOPNOTSUPP);
}

int
coda_reclaim(void *v) 
{
/* true args */
    struct vop_reclaim_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
/* upcall decl */
/* locals */

/*
 * Forced unmount/flush will let vnodes with non zero use be destroyed!
 */
    ENTRY;

    if (IS_UNMOUNTING(cp)) {
#ifdef	DEBUG
	if (VTOC(vp)->c_ovp) {
	    if (IS_UNMOUNTING(cp))
		printf("coda_reclaim: c_ovp not void: vp %p, cp %p\n", vp, cp);
	}
#endif
    } else {
#ifdef OLD_DIAGNOSTIC
	if (vp->v_usecount != 0) 
	    print("coda_reclaim: pushing active %p\n", vp);
	if (VTOC(vp)->c_ovp) {
	    panic("coda_reclaim: c_ovp not void");
    }
#endif
    }	
    coda_free(VTOC(vp));
    vp->v_data = NULL;
    return (0);
}

int
coda_lock(void *v)
{
/* true args */
    struct vop_lock_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct thread *td = ap->a_td;
/* upcall decl */
/* locals */

    ENTRY;

    if (coda_lockdebug) {
	myprintf(("Attempting lock on %lx.%lx.%lx\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
    }

#ifndef	DEBUG_LOCKS
    return (lockmgr(&vp->v_lock, ap->a_flags, NULL, td));
#else
    return (debuglockmgr(&vp->v_lock, ap->a_flags, NULL, td,
			 "coda_lock", vp->filename, vp->line));
#endif
}

int
coda_unlock(void *v)
{
/* true args */
    struct vop_unlock_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct thread *td = ap->a_td;
/* upcall decl */
/* locals */

    ENTRY;
    if (coda_lockdebug) {
	myprintf(("Attempting unlock on %lx.%lx.%lx\n",
		  cp->c_fid.Volume, cp->c_fid.Vnode, cp->c_fid.Unique));
    }

    return (lockmgr(&vp->v_lock, ap->a_flags | LK_RELEASE, NULL, td));
}

int
coda_islocked(void *v)
{
/* true args */
    struct vop_islocked_args *ap = v;
    ENTRY;

    return (lockstatus(&ap->a_vp->v_lock, ap->a_td));
}

/* How one looks up a vnode given a device/inode pair: */
int
coda_grab_vnode(dev_t dev, ino_t ino, struct vnode **vpp)
{
    /* This is like VFS_VGET() or igetinode()! */
    int           error;
    struct mount *mp;

    if (!(mp = devtomp(dev))) {
	myprintf(("coda_grab_vnode: devtomp(%#lx) returns NULL\n",
		  (u_long)dev2udev(dev)));
	return(ENXIO);
    }

    /* XXX - ensure that nonzero-return means failure */
    error = VFS_VGET(mp,ino,vpp);
    if (error) {
	myprintf(("coda_grab_vnode: iget/vget(%lx, %lu) returns %p, err %d\n", 
		  (u_long)dev2udev(dev), (u_long)ino, (void *)*vpp, error));
	return(ENOENT);
    }
    return(0);
}

void
print_vattr(struct vattr *attr)
{
    char *typestr;

    switch (attr->va_type) {
    case VNON:
	typestr = "VNON";
	break;
    case VREG:
	typestr = "VREG";
	break;
    case VDIR:
	typestr = "VDIR";
	break;
    case VBLK:
	typestr = "VBLK";
	break;
    case VCHR:
	typestr = "VCHR";
	break;
    case VLNK:
	typestr = "VLNK";
	break;
    case VSOCK:
	typestr = "VSCK";
	break;
    case VFIFO:
	typestr = "VFFO";
	break;
    case VBAD:
	typestr = "VBAD";
	break;
    default:
	typestr = "????";
	break;
    }


    myprintf(("attr: type %s mode %d uid %d gid %d fsid %d rdev %d\n",
	      typestr, (int)attr->va_mode, (int)attr->va_uid,
	      (int)attr->va_gid, (int)attr->va_fsid, (int)attr->va_rdev));

    myprintf(("      fileid %d nlink %d size %d blocksize %d bytes %d\n",
	      (int)attr->va_fileid, (int)attr->va_nlink, 
	      (int)attr->va_size,
	      (int)attr->va_blocksize,(int)attr->va_bytes));
    myprintf(("      gen %ld flags %ld vaflags %d\n",
	      attr->va_gen, attr->va_flags, attr->va_vaflags));
    myprintf(("      atime sec %d nsec %d\n",
	      (int)attr->va_atime.tv_sec, (int)attr->va_atime.tv_nsec));
    myprintf(("      mtime sec %d nsec %d\n",
	      (int)attr->va_mtime.tv_sec, (int)attr->va_mtime.tv_nsec));
    myprintf(("      ctime sec %d nsec %d\n",
	      (int)attr->va_ctime.tv_sec, (int)attr->va_ctime.tv_nsec));
}

/* How to print a ucred */
void
print_cred(struct ucred *cred)
{

	int i;

	myprintf(("ref %d\tuid %d\n",cred->cr_ref,cred->cr_uid));

	for (i=0; i < cred->cr_ngroups; i++)
		myprintf(("\tgroup %d: (%d)\n",i,cred->cr_groups[i]));
	myprintf(("\n"));

}

/*
 * Return a vnode for the given fid.
 * If no cnode exists for this fid create one and put it
 * in a table hashed by fid.Volume and fid.Vnode.  If the cnode for
 * this fid is already in the table return it (ref count is
 * incremented by coda_find.  The cnode will be flushed from the
 * table when coda_inactive calls coda_unsave.
 */
struct cnode *
make_coda_node(ViceFid *fid, struct mount *vfsp, short type)
{
    struct cnode *cp;
    int          err;

    if ((cp = coda_find(fid)) == NULL) {
	struct vnode *vp;
	
	cp = coda_alloc();
	cp->c_fid = *fid;
	
	err = getnewvnode(VT_CODA, vfsp, &vp, 0, 0);
	if (err) {                                                
	    panic("coda: getnewvnode returned error %d\n", err);   
	}                                                         
	vp->v_data = cp;                                          
	vp->v_type = type;                                      
	cp->c_vnode = vp;                                         
	coda_save(cp);
	vx_unlock(vp);
    } else {
	vref(CTOV(cp));
    }

    return cp;
}
