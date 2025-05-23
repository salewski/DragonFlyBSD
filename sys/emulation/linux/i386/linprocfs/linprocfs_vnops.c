/*
 * Copyright (c) 2000 Dag-Erling Co�dan Sm�rgrav
 * Copyright (c) 1999 Pierre Beyssac
 * Copyright (c) 1993, 1995 Jan-Simon Pendry
 * Copyright (c) 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 *
 * $FreeBSD: src/sys/i386/linux/linprocfs/linprocfs_vnops.c,v 1.3.2.5 2001/08/12 14:29:19 rwatson Exp $
 * $DragonFly: src/sys/emulation/linux/i386/linprocfs/linprocfs_vnops.c,v 1.18 2004/10/12 19:20:38 dillon Exp $
 */

/*
 * procfs vnode interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <machine/reg.h>
#include <vm/vm_zone.h>
#include "linprocfs.h"
#include <sys/pioctl.h>

extern struct vnode *procfs_findtextvp (struct proc *);

static int	linprocfs_access (struct vop_access_args *);
static int	linprocfs_badop (void);
static int	linprocfs_bmap (struct vop_bmap_args *);
static int	linprocfs_close (struct vop_close_args *);
static int	linprocfs_getattr (struct vop_getattr_args *);
static int	linprocfs_inactive (struct vop_inactive_args *);
static int	linprocfs_ioctl (struct vop_ioctl_args *);
static int	linprocfs_lookup (struct vop_lookup_args *);
static int	linprocfs_open (struct vop_open_args *);
static int	linprocfs_print (struct vop_print_args *);
static int	linprocfs_readdir (struct vop_readdir_args *);
static int	linprocfs_readlink (struct vop_readlink_args *);
static int	linprocfs_reclaim (struct vop_reclaim_args *);
static int	linprocfs_setattr (struct vop_setattr_args *);

/*
 * This is a list of the valid names in the
 * process-specific sub-directories.  It is
 * used in linprocfs_lookup and linprocfs_readdir
 */
static struct proc_target {
	u_char	pt_type;
	u_char	pt_namlen;
	char	*pt_name;
	pfstype	pt_pfstype;
	int	(*pt_valid) (struct proc *p);
} proc_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		type		validp */
	{ DT_DIR, N("."),	Pproc,		NULL },
	{ DT_DIR, N(".."),	Proot,		NULL },
	{ DT_REG, N("mem"),	Pmem,		NULL },
	{ DT_LNK, N("exe"),	Pexe,		NULL },
	{ DT_REG, N("stat"),	Pprocstat,	NULL },
	{ DT_REG, N("status"),	Pprocstatus,	NULL },
#undef N
};
static const int nproc_targets = sizeof(proc_targets) / sizeof(proc_targets[0]);

static pid_t atopid (const char *, u_int);

/*
 * set things up for doing i/o on
 * the pfsnode (vp).  (vp) is locked
 * on entry, and should be left locked
 * on exit.
 *
 * for procfs we don't need to do anything
 * in particular for i/o.  all that is done
 * is to support exclusive open on process
 * memory images.
 */
static int
linprocfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *p2;

	p2 = PFIND(pfs->pfs_pid);
	if (p2 == NULL)
		return (ENOENT);
	if (pfs->pfs_pid && !PRISON_CHECK(ap->a_cred, p2->p_ucred))
		return (ENOENT);

	switch (pfs->pfs_type) {
	case Pmem:
		if (((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL)) ||
		    ((pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE)))
			return (EBUSY);

		if (p_trespass(ap->a_cred, p2->p_ucred))
			return (EPERM);

		if (ap->a_mode & FWRITE)
			pfs->pfs_flags = ap->a_mode & (FWRITE|O_EXCL);

		return (0);

	default:
		break;
	}

	return (0);
}

/*
 * close the pfsnode (vp) after doing i/o.
 * (vp) is not locked on entry or exit.
 *
 * nothing to do for procfs other than undo
 * any exclusive open flag (see _open above).
 */
static int
linprocfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *p;

	switch (pfs->pfs_type) {
	case Pmem:
		if ((ap->a_fflag & FWRITE) && (pfs->pfs_flags & O_EXCL))
			pfs->pfs_flags &= ~(FWRITE|O_EXCL);
		/*
		 * If this is the last close, then it checks to see if
		 * the target process has PF_LINGER set in p_pfsflags,
		 * if this is *not* the case, then the process' stop flags
		 * are cleared, and the process is woken up.  This is
		 * to help prevent the case where a process has been
		 * told to stop on an event, but then the requesting process
		 * has gone away or forgotten about it.
		 */
		if ((ap->a_vp->v_usecount < 2)
		    && (p = pfind(pfs->pfs_pid))
		    && !(p->p_pfsflags & PF_LINGER)) {
			p->p_stops = 0;
			p->p_step = 0;
			wakeup(&p->p_step);
		}
		break;
	default:
		break;
	}

	return (0);
}

/*
 * do an ioctl operation on a pfsnode (vp).
 * (vp) is not locked on entry or exit.
 */
static int
linprocfs_ioctl(ap)
	struct vop_ioctl_args *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *procp;
	struct thread *td;
	int error;
	int signo;
	struct procfs_status *psp;
	unsigned char flags;

	td = ap->a_td;
	procp = pfind(pfs->pfs_pid);
	if (procp == NULL) {
		return ENOTTY;
	}

	if (p_trespass(ap->a_cred, procp->p_ucred))
		return EPERM;

	switch (ap->a_command) {
	case PIOCBIS:
	  procp->p_stops |= *(unsigned int*)ap->a_data;
	  break;
	case PIOCBIC:
	  procp->p_stops &= ~*(unsigned int*)ap->a_data;
	  break;
	case PIOCSFL:
	  /*
	   * NFLAGS is "non-suser_xxx flags" -- currently, only
	   * PFS_ISUGID ("ignore set u/g id");
	   */
#define NFLAGS	(PF_ISUGID)
	  flags = (unsigned char)*(unsigned int*)ap->a_data;
	  if (flags & NFLAGS && (error = suser_cred(ap->a_cred, 0)))
	    return error;
	  procp->p_pfsflags = flags;
	  break;
	case PIOCGFL:
	  *(unsigned int*)ap->a_data = (unsigned int)procp->p_pfsflags;
	case PIOCSTATUS:
	  psp = (struct procfs_status *)ap->a_data;
	  psp->state = (procp->p_step == 0);
	  psp->flags = procp->p_pfsflags;
	  psp->events = procp->p_stops;
	  if (procp->p_step) {
	    psp->why = procp->p_stype;
	    psp->val = procp->p_xstat;
	  } else {
	    psp->why = psp->val = 0;	/* Not defined values */
	  }
	  break;
	case PIOCWAIT:
	  psp = (struct procfs_status *)ap->a_data;
	  if (procp->p_step == 0) {
	    error = tsleep(&procp->p_stype, PCATCH, "piocwait", 0);
	    if (error)
	      return error;
	  }
	  psp->state = 1;	/* It stopped */
	  psp->flags = procp->p_pfsflags;
	  psp->events = procp->p_stops;
	  psp->why = procp->p_stype;	/* why it stopped */
	  psp->val = procp->p_xstat;	/* any extra info */
	  break;
	case PIOCCONT:	/* Restart a proc */
	  if (procp->p_step == 0)
	    return EINVAL;	/* Can only start a stopped process */
	  if ((signo = *(int*)ap->a_data) != 0) {
	    if (signo >= NSIG || signo <= 0)
	      return EINVAL;
	    psignal(procp, signo);
	  }
	  procp->p_step = 0;
	  wakeup(&procp->p_step);
	  break;
	default:
	  return (ENOTTY);
	}
	return 0;
}

/*
 * do block mapping for pfsnode (vp).
 * since we don't use the buffer cache
 * for procfs this function should never
 * be called.  in any case, it's not clear
 * what part of the kernel ever makes use
 * of this function.  for sanity, this is the
 * usual no-op bmap, although returning
 * (EIO) would be a reasonable alternative.
 */
static int
linprocfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

/*
 * linprocfs_inactive is called when the pfsnode
 * is vrele'd and the reference count is about
 * to go to zero.  (vp) will be on the vnode free
 * list, so to get it back vget() must be
 * used.
 *
 * (vp) is locked on entry and must remain locked
 *      on exit.
 */
static int
linprocfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	/*struct vnode *vp = ap->a_vp;*/

	return (0);
}

/*
 * _reclaim is called when getnewvnode()
 * wants to make use of an entry on the vnode
 * free list.  at this time the filesystem needs
 * to free any private data and remove the node
 * from any private lists.
 */
static int
linprocfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return (linprocfs_freevp(ap->a_vp));
}

/*
 * _print is used for debugging.
 * just print a readable description
 * of (vp).
 */
static int
linprocfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	printf("tag VT_PROCFS, type %d, pid %ld, mode %x, flags %lx\n",
	    pfs->pfs_type, (long)pfs->pfs_pid, pfs->pfs_mode, pfs->pfs_flags);
	return (0);
}

/*
 * generic entry point for unsupported operations
 */
static int
linprocfs_badop()
{

	return (EIO);
}

/*
 * Invent attributes for pfsnode (vp) and store
 * them in (vap).
 * Directories lengths are returned as zero since
 * any real length would require the genuine size
 * to be computed, and nothing cares anyway.
 *
 * this is relatively minimal for procfs.
 */
static int
linprocfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct vattr *vap = ap->a_vap;
	struct proc *procp;
	int error;

	/*
	 * First make sure that the process and its credentials 
	 * still exist.
	 */
	switch (pfs->pfs_type) {
	case Proot:
	case Pself:
		procp = 0;
		break;

	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == 0 || procp->p_ucred == NULL)
			return (ENOENT);
	}

	error = 0;

	/* start by zeroing out the attributes */
	VATTR_NULL(vap);

	/* next do all the common fields */
	vap->va_type = ap->a_vp->v_type;
	vap->va_mode = pfs->pfs_mode;
	vap->va_fileid = pfs->pfs_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];

	/*
	 * Make all times be current TOD.
	 * It would be possible to get the process start
	 * time from the p_stat structure, but there's
	 * no "file creation" time stamp anyway, and the
	 * p_stat structure is not addressible if u. gets
	 * swapped out for that process.
	 */
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	/*
	 * now do the object specific fields
	 *
	 * The size could be set from struct reg, but it's hardly
	 * worth the trouble, and it puts some (potentially) machine
	 * dependent data into this machine-independent code.  If it
	 * becomes important then this function should break out into
	 * a per-file stat function in the corresponding .c file.
	 */

	vap->va_nlink = 1;
	if (procp) {
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
	}

	switch (pfs->pfs_type) {
	case Proot:
		/*
		 * Set nlink to 1 to tell fts(3) we don't actually know.
		 */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_size = vap->va_bytes = DEV_BSIZE;
		break;

	case Pself: {
		char buf[16];		/* should be enough */
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_size = vap->va_bytes =
		    snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);
		break;
	}

	case Pproc:
		vap->va_nlink = nproc_targets;
		vap->va_size = vap->va_bytes = DEV_BSIZE;
		break;

	case Pexe: {
		char *fullpath, *freepath;
		error = vn_fullpath(procp, NULL, &fullpath, &freepath);
		if (error == 0) {
			vap->va_size = strlen(fullpath);
			free(freepath, M_TEMP);
		} else {
			vap->va_size = sizeof("unknown") - 1;
			error = 0;
		}
		vap->va_bytes = vap->va_size;
		break;
	}

	case Pmeminfo:
	case Pcpuinfo:
	case Pstat:
	case Puptime:
	case Pversion:
	case Ploadavg:
		vap->va_bytes = vap->va_size = 0;
		vap->va_uid = 0;
		vap->va_gid = 0;
		break;
		
	case Pmem:
		/*
		 * If we denied owner access earlier, then we have to
		 * change the owner to root - otherwise 'ps' and friends
		 * will break even though they are setgid kmem. *SIGH*
		 */
		if (procp->p_flag & P_SUGID)
			vap->va_uid = 0;
		else
			vap->va_uid = procp->p_ucred->cr_uid;
		break;

	case Pprocstat:
	case Pprocstatus:
		vap->va_bytes = vap->va_size = 0;
		/* uid, gid are already set */
		break;

	default:
		panic("linprocfs_getattr");
	}

	return (error);
}

static int
linprocfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * just fake out attribute setting
	 * it's not good to generate an error
	 * return, otherwise things like creat()
	 * will fail when they try to set the
	 * file length to 0.  worse, this means
	 * that echo $note > /proc/$pid/note will fail.
	 */

	return (0);
}

/*
 * implement access checking.
 *
 * something very similar to this code is duplicated
 * throughout the 4bsd kernel and should be moved
 * into kern/vfs_subr.c sometime.
 *
 * actually, the check for super-user is slightly
 * broken since it will allow read access to write-only
 * objects.  this doesn't cause any particular trouble
 * but does mean that the i/o entry points need to check
 * that the operation really does make sense.
 */
static int
linprocfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vattr *vap;
	struct vattr vattr;
	int error;

	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (ap->a_cred->cr_uid == 0)
		return (0);

	vap = &vattr;
	error = VOP_GETATTR(ap->a_vp, vap, ap->a_td);
	if (error)
		return (error);

	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (ap->a_cred->cr_uid != vap->va_uid) {
		gid_t *gp;
		int i;

		ap->a_mode >>= 3;
		gp = ap->a_cred->cr_groups;
		for (i = 0; i < ap->a_cred->cr_ngroups; i++, gp++)
			if (vap->va_gid == *gp)
				goto found;
		ap->a_mode >>= 3;
found:
		;
	}

	if ((vap->va_mode & ap->a_mode) == ap->a_mode)
		return (0);

	return (EACCES);
}

/*
 * lookup.  this is incredibly complicated in the general case, however
 * for most pseudo-filesystems very little needs to be done.
 */
static int
linprocfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc_target *pt;
	pid_t pid;
	struct pfsnode *pfs;
	struct proc *p;
	int i;
	int error;

	*vpp = NULL;

	if (cnp->cn_nameiop == NAMEI_DELETE || 
	    cnp->cn_nameiop == NAMEI_RENAME ||
	    cnp->cn_nameiop == NAMEI_CREATE) {
		return (EROFS);
	}

	error = 0;

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		vref(*vpp);
		goto out;
	}

	pfs = VTOPFS(dvp);
	switch (pfs->pfs_type) {
	case Proot:
		if (cnp->cn_flags & CNP_ISDOTDOT)
			return (EIO);

		if (CNEQ(cnp, "self", 4)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Pself);
			goto out;
		}
		if (CNEQ(cnp, "meminfo", 7)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Pmeminfo);
			goto out;
		}
		if (CNEQ(cnp, "cpuinfo", 7)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Pcpuinfo);
			goto out;
		}
		if (CNEQ(cnp, "stat", 4)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Pstat);
			goto out;
		}
		if (CNEQ(cnp, "uptime", 6)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Puptime);
			goto out;
		}
		if (CNEQ(cnp, "version", 7)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Pversion);
			goto out;
		}
		if (CNEQ(cnp, "loadavg", 7)) {
			error = linprocfs_allocvp(dvp->v_mount, vpp, 0, Ploadavg);
			goto out;
		}

		pid = atopid(pname, cnp->cn_namelen);
		if (pid == NO_PID)
			break;

		p = PFIND(pid);
		if (p == 0)
			break;

		error = linprocfs_allocvp(dvp->v_mount, vpp, pid, Pproc);
		goto out;

	case Pproc:
		if (cnp->cn_flags & CNP_ISDOTDOT) {
			error = linprocfs_root(dvp->v_mount, vpp);
			goto out;
		}

		p = PFIND(pfs->pfs_pid);
		if (p == 0)
			break;

		for (pt = proc_targets, i = 0; i < nproc_targets; pt++, i++) {
			if (cnp->cn_namelen == pt->pt_namlen &&
			    bcmp(pt->pt_name, pname, cnp->cn_namelen) == 0 &&
			    (pt->pt_valid == NULL || (*pt->pt_valid)(p)))
				goto found;
		}
		break;

	found:
		error = linprocfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
					pt->pt_pfstype);
		goto out;

	default:
		error = ENOTDIR;
		goto out;
	}

	if (cnp->cn_nameiop == NAMEI_LOOKUP)
		error = ENOENT;
	else
		error = EROFS;

	/*
	 * If no error occured *vpp will hold a referenced locked vnode.
	 * dvp was passed to us locked and *vpp must be returned locked
	 * so if dvp != *vpp and CNP_LOCKPARENT is not set, unlock dvp.
	 */
out:
	if (error == 0) {
		if (*vpp != dvp && (cnp->cn_flags & CNP_LOCKPARENT) == 0) {
			cnp->cn_flags |= CNP_PDIRUNLOCK;
			VOP_UNLOCK(dvp, 0, cnp->cn_td);
		}
	}
	return (error);
}

/*
 * Does this process have a text file?
 */
int
linprocfs_validfile(p)
	struct proc *p;
{

	return (procfs_findtextvp(p) != NULLVP);
}

/*
 * readdir() returns directory entries from pfsnode (vp).
 *
 * We generate just one directory entry at a time, as it would probably
 * not pay off to buffer several entries locally to save uiomove calls.
 */
static int
linprocfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct dirent d;
	struct dirent *dp = &d;
	struct pfsnode *pfs;
	int count, error, i, off;
	static u_int delen;

	if (!delen) {

		d.d_namlen = PROCFS_NAMELEN;
		delen = GENERIC_DIRSIZ(&d);
	}

	pfs = VTOPFS(ap->a_vp);

	off = (int)uio->uio_offset;
	if (off != uio->uio_offset || off < 0 || 
	    off % delen != 0 || uio->uio_resid < delen)
		return (EINVAL);

	error = 0;
	count = 0;
	i = off / delen;

	switch (pfs->pfs_type) {
	/*
	 * this is for the process-specific sub-directories.
	 * all that is needed to is copy out all the entries
	 * from the procent[] table (top of this file).
	 */
	case Pproc: {
		struct proc *p;
		struct proc_target *pt;

		p = PFIND(pfs->pfs_pid);
		if (p == NULL)
			break;
		if (!PRISON_CHECK(ap->a_cred, p->p_ucred))
			break;

		for (pt = &proc_targets[i];
		     uio->uio_resid >= delen && i < nproc_targets; pt++, i++) {
			if (pt->pt_valid && (*pt->pt_valid)(p) == 0)
				continue;

			dp->d_reclen = delen;
			dp->d_fileno = PROCFS_FILENO(pfs->pfs_pid, pt->pt_pfstype);
			dp->d_namlen = pt->pt_namlen;
			bcopy(pt->pt_name, dp->d_name, pt->pt_namlen + 1);
			dp->d_type = pt->pt_type;

			if ((error = uiomove((caddr_t)dp, delen, uio)) != 0)
				break;
		}

	    	break;
	    }

	/*
	 * this is for the root of the procfs filesystem
	 * what is needed is a special entry for "self"
	 * followed by an entry for each process on allproc
#ifdef PROCFS_ZOMBIE
	 * and zombproc.
#endif
	 */

	case Proot: {
#ifdef PROCFS_ZOMBIE
		int doingzomb = 0;
#endif
		int pcnt = 0;
		struct proc *p = allproc.lh_first;

		for (; p && uio->uio_resid >= delen; i++, pcnt++) {
			bzero((char *) dp, delen);
			dp->d_reclen = delen;

			switch (i) {
			case 0:		/* `.' */
			case 1:		/* `..' */
				dp->d_fileno = PROCFS_FILENO(0, Proot);
				dp->d_namlen = i + 1;
				bcopy("..", dp->d_name, dp->d_namlen);
				dp->d_name[i + 1] = '\0';
				dp->d_type = DT_DIR;
				break;

			case 2:
				dp->d_fileno = PROCFS_FILENO(0, Pself);
				dp->d_namlen = 4;
				bcopy("self", dp->d_name, 5);
				dp->d_type = DT_LNK;
				break;

			case 3:
				dp->d_fileno = PROCFS_FILENO(0, Pmeminfo);
				dp->d_namlen = 7;
				bcopy("meminfo", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			case 4:
				dp->d_fileno = PROCFS_FILENO(0, Pcpuinfo);
				dp->d_namlen = 7;
				bcopy("cpuinfo", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			case 5:
				dp->d_fileno = PROCFS_FILENO(0, Pstat);
				dp->d_namlen = 4;
				bcopy("stat", dp->d_name, 5);
				dp->d_type = DT_REG;
				break;
			    
			case 6:
				dp->d_fileno = PROCFS_FILENO(0, Puptime);
				dp->d_namlen = 6;
				bcopy("uptime", dp->d_name, 7);
				dp->d_type = DT_REG;
				break;

			case 7:
				dp->d_fileno = PROCFS_FILENO(0, Pversion);
				dp->d_namlen = 7;
				bcopy("version", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			case 8:
				dp->d_fileno = PROCFS_FILENO(0, Ploadavg);
				dp->d_namlen = 7;
				bcopy("loadavg", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;


			default:
				while (pcnt < i) {
					p = p->p_list.le_next;
					if (!p)
						goto done;
					if (!PRISON_CHECK(ap->a_cred, p->p_ucred))
						continue;
					pcnt++;
				}
				while (!PRISON_CHECK(ap->a_cred, p->p_ucred)) {
					p = p->p_list.le_next;
					if (!p)
						goto done;
				}
				dp->d_fileno = PROCFS_FILENO(p->p_pid, Pproc);
				dp->d_namlen = sprintf(dp->d_name, "%ld",
				    (long)p->p_pid);
				dp->d_type = DT_DIR;
				p = p->p_list.le_next;
				break;
			}

			if ((error = uiomove((caddr_t)dp, delen, uio)) != 0)
				break;
		}
	done:

#ifdef PROCFS_ZOMBIE
		if (p == 0 && doingzomb == 0) {
			doingzomb = 1;
			p = zombproc.lh_first;
			goto again;
		}
#endif

		break;

	    }

	default:
		error = ENOTDIR;
		break;
	}

	uio->uio_offset = i * delen;

	return (error);
}

/*
 * readlink reads the link of `self' or `exe'
 */
static int
linprocfs_readlink(ap)
	struct vop_readlink_args *ap;
{
	char buf[16];		/* should be enough */
	struct proc *procp;
	struct vnode *vp = ap->a_vp;
	struct pfsnode *pfs = VTOPFS(vp);
	char *fullpath, *freepath;
	int error, len;

	switch (pfs->pfs_type) {
	case Pself:
		if (pfs->pfs_fileno != PROCFS_FILENO(0, Pself))
			return (EINVAL);

		len = snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);

		return (uiomove(buf, len, ap->a_uio));
	/*
	 * There _should_ be no way for an entire process to disappear
	 * from under us...
	 */
	case Pexe:
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL || procp->p_ucred == NULL) {
			printf("linprocfs_readlink: pid %d disappeared\n",
			    pfs->pfs_pid);
			return (uiomove("unknown", sizeof("unknown") - 1,
			    ap->a_uio));
		}
		error = vn_fullpath(procp, NULL, &fullpath, &freepath);
		if (error != 0)
			return (uiomove("unknown", sizeof("unknown") - 1,
			    ap->a_uio));
		error = uiomove(fullpath, strlen(fullpath), ap->a_uio);
		free(freepath, M_TEMP);
		return (error);
	default:
		return (EINVAL);
	}
}

/*
 * convert decimal ascii to pid_t
 */
static pid_t
atopid(b, len)
	const char *b;
	u_int len;
{
	pid_t p = 0;

	while (len--) {
		char c = *b++;
		if (c < '0' || c > '9')
			return (NO_PID);
		p = 10 * p + (c - '0');
		if (p > PID_MAX)
			return (NO_PID);
	}

	return (p);
}

/*
 * procfs vnode operations.
 */
struct vnodeopv_entry_desc linprocfs_vnodeop_entries[] = {
	{ &vop_default_desc,		vop_defaultop },
	{ &vop_access_desc,		(void *) linprocfs_access },
	{ &vop_advlock_desc,		(void *) linprocfs_badop },
	{ &vop_bmap_desc,		(void *) linprocfs_bmap },
	{ &vop_close_desc,		(void *) linprocfs_close },
	{ &vop_create_desc,		(void *) linprocfs_badop },
	{ &vop_getattr_desc,		(void *) linprocfs_getattr },
	{ &vop_inactive_desc,		(void *) linprocfs_inactive },
	{ &vop_link_desc,		(void *) linprocfs_badop },
	{ &vop_lookup_desc,		(void *) linprocfs_lookup },
	{ &vop_mkdir_desc,		(void *) linprocfs_badop },
	{ &vop_mknod_desc,		(void *) linprocfs_badop },
	{ &vop_open_desc,		(void *) linprocfs_open },
	{ &vop_pathconf_desc,		(void *) vop_stdpathconf },
	{ &vop_print_desc,		(void *) linprocfs_print },
	{ &vop_read_desc,		(void *) linprocfs_rw },
	{ &vop_readdir_desc,		(void *) linprocfs_readdir },
	{ &vop_readlink_desc,		(void *) linprocfs_readlink },
	{ &vop_reclaim_desc,		(void *) linprocfs_reclaim },
	{ &vop_remove_desc,		(void *) linprocfs_badop },
	{ &vop_rename_desc,		(void *) linprocfs_badop },
	{ &vop_rmdir_desc,		(void *) linprocfs_badop },
	{ &vop_setattr_desc,		(void *) linprocfs_setattr },
	{ &vop_symlink_desc,		(void *) linprocfs_badop },
	{ &vop_write_desc,		(void *) linprocfs_rw },
	{ &vop_ioctl_desc,		(void *) linprocfs_ioctl },
	{ NULL, NULL }
};

