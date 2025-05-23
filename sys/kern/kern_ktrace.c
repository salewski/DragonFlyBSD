/*
 * Copyright (c) 1989, 1993
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
 *	@(#)kern_ktrace.c	8.2 (Berkeley) 9/23/93
 * $FreeBSD: src/sys/kern/kern_ktrace.c,v 1.35.2.6 2002/07/05 22:36:38 darrenr Exp $
 * $DragonFly: src/sys/kern/kern_ktrace.c,v 1.18.2.1 2005/07/28 23:46:57 dillon Exp $
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/nlookup.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sysent.h>

#include <vm/vm_zone.h>
static MALLOC_DEFINE(M_KTRACE, "KTRACE", "KTRACE");

#ifdef KTRACE
static struct ktr_header *ktrgetheader (int type);
static void ktrwrite (struct vnode *, struct ktr_header *, struct uio *);
static int ktrcanset (struct proc *,struct proc *);
static int ktrsetchildren (struct proc *,struct proc *,int,int,struct vnode *);
static int ktrops (struct proc *,struct proc *,int,int,struct vnode *);


static struct ktr_header *
ktrgetheader(int type)
{
	struct ktr_header *kth;
	struct proc *p = curproc;	/* XXX */

	MALLOC(kth, struct ktr_header *, sizeof (struct ktr_header),
		M_KTRACE, M_WAITOK);
	kth->ktr_type = type;
	microtime(&kth->ktr_time);
	kth->ktr_pid = p->p_pid;
	bcopy(p->p_comm, kth->ktr_comm, MAXCOMLEN + 1);
	return (kth);
}

void
ktrsyscall(struct vnode *vp, int code, int narg, register_t args[])
{
	struct	ktr_header *kth;
	struct	ktr_syscall *ktp;
	int len = offsetof(struct ktr_syscall, ktr_args) +
	    (narg * sizeof(register_t));
	struct proc *p = curproc;	/* XXX */
	register_t *argp;
	int i;

	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_SYSCALL);
	MALLOC(ktp, struct ktr_syscall *, len, M_KTRACE, M_WAITOK);
	ktp->ktr_code = code;
	ktp->ktr_narg = narg;
	argp = &ktp->ktr_args[0];
	for (i = 0; i < narg; i++)
		*argp++ = args[i];
	kth->ktr_buf = (caddr_t)ktp;
	kth->ktr_len = len;
	ktrwrite(vp, kth, NULL);
	FREE(ktp, M_KTRACE);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrsysret(struct vnode *vp, int code, int error, register_t retval)
{
	struct ktr_header *kth;
	struct ktr_sysret ktp;
	struct proc *p = curproc;	/* XXX */

	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_SYSRET);
	ktp.ktr_code = code;
	ktp.ktr_error = error;
	ktp.ktr_retval = retval;		/* what about val2 ? */

	kth->ktr_buf = (caddr_t)&ktp;
	kth->ktr_len = sizeof(struct ktr_sysret);

	ktrwrite(vp, kth, NULL);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrnamei(struct vnode *vp, char *path)
{
	struct ktr_header *kth;
	struct proc *p = curproc;	/* XXX */

	/*
	 * don't let vp get ripped out from under us
	 */
	if (vp)
		vref(vp);
	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_NAMEI);
	kth->ktr_len = strlen(path);
	kth->ktr_buf = path;

	ktrwrite(vp, kth, NULL);
	if (vp)
		vrele(vp);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrgenio(struct vnode *vp, int fd, enum uio_rw rw, struct uio *uio, int error)
{
	struct ktr_header *kth;
	struct ktr_genio ktg;
	struct proc *p = curproc;	/* XXX */

	if (error)
		return;
	/*
	 * don't let p_tracep get ripped out from under us
	 */
	if (vp)
		vref(vp);
	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_GENIO);
	ktg.ktr_fd = fd;
	ktg.ktr_rw = rw;
	kth->ktr_buf = (caddr_t)&ktg;
	kth->ktr_len = sizeof(struct ktr_genio);
	uio->uio_offset = 0;
	uio->uio_rw = UIO_WRITE;

	ktrwrite(vp, kth, uio);
	if (vp)
		vrele(vp);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrpsig(struct vnode *vp, int sig, sig_t action, sigset_t *mask, int code)
{
	struct ktr_header *kth;
	struct ktr_psig	kp;
	struct proc *p = curproc;	/* XXX */

	/*
	 * don't let vp get ripped out from under us
	 */
	if (vp)
		vref(vp);
	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_PSIG);
	kp.signo = (char)sig;
	kp.action = action;
	kp.mask = *mask;
	kp.code = code;
	kth->ktr_buf = (caddr_t)&kp;
	kth->ktr_len = sizeof (struct ktr_psig);

	ktrwrite(vp, kth, NULL);
	if (vp)
		vrele(vp);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrcsw(struct vnode *vp, int out, int user)
{
	struct ktr_header *kth;
	struct	ktr_csw kc;
	struct proc *p = curproc;	/* XXX */

	/*
	 * don't let vp get ripped out from under us
	 */
	if (vp)
		vref(vp);
	p->p_traceflag |= KTRFAC_ACTIVE;
	kth = ktrgetheader(KTR_CSW);
	kc.out = out;
	kc.user = user;
	kth->ktr_buf = (caddr_t)&kc;
	kth->ktr_len = sizeof (struct ktr_csw);

	ktrwrite(vp, kth, NULL);
	if (vp)
		vrele(vp);
	FREE(kth, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}
#endif

/* Interface and common routines */

/*
 * ktrace system call
 */
/* ARGSUSED */
int
ktrace(struct ktrace_args *uap)
{
#ifdef KTRACE
	struct thread *td = curthread;
	struct proc *curp = td->td_proc;
	struct vnode *vp = NULL;
	struct proc *p;
	struct pgrp *pg;
	int facs = uap->facs & ~KTRFAC_ROOT;
	int ops = KTROP(uap->ops);
	int descend = uap->ops & KTRFLAG_DESCEND;
	int ret = 0;
	int error = 0;
	struct nlookupdata nd;

	curp->p_traceflag |= KTRFAC_ACTIVE;
	if (ops != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		error = nlookup_init(&nd, uap->fname, 
					UIO_USERSPACE, NLC_LOCKVP);
		if (error == 0)
			error = vn_open(&nd, NULL, FREAD|FWRITE|O_NOFOLLOW, 0);
		if (error == 0 && nd.nl_open_vp->v_type != VREG)
			error = EACCES;
		if (error) {
			curp->p_traceflag &= ~KTRFAC_ACTIVE;
			nlookup_done(&nd);
			return (error);
		}
		vp = nd.nl_open_vp;
		nd.nl_open_vp = NULL;
		nlookup_done(&nd);
		VOP_UNLOCK(vp, 0, td);
	}
	/*
	 * Clear all uses of the tracefile.  XXX umm, what happens to the
	 * loop if vn_close() blocks?
	 */
	if (ops == KTROP_CLEARFILE) {
		FOREACH_PROC_IN_SYSTEM(p) {
			if (p->p_tracep == vp) {
				if (ktrcanset(curp, p) && p->p_tracep == vp) {
					p->p_tracep = NULL;
					p->p_traceflag = 0;
					vn_close(vp, FREAD|FWRITE, td);
				} else {
					error = EPERM;
				}
			}
		}
		goto done;
	}
	/*
	 * need something to (un)trace (XXX - why is this here?)
	 */
	if (!facs) {
		error = EINVAL;
		goto done;
	}
	/*
	 * do it
	 */
	if (uap->pid < 0) {
		/*
		 * by process group
		 */
		pg = pgfind(-uap->pid);
		if (pg == NULL) {
			error = ESRCH;
			goto done;
		}
		LIST_FOREACH(p, &pg->pg_members, p_pglist) {
			if (descend)
				ret |= ktrsetchildren(curp, p, ops, facs, vp);
			else
				ret |= ktrops(curp, p, ops, facs, vp);
		}
	} else {
		/*
		 * by pid
		 */
		p = pfind(uap->pid);
		if (p == NULL) {
			error = ESRCH;
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(curp, p, ops, facs, vp);
		else
			ret |= ktrops(curp, p, ops, facs, vp);
	}
	if (!ret)
		error = EPERM;
done:
	if (vp != NULL)
		vn_close(vp, FWRITE, td);
	curp->p_traceflag &= ~KTRFAC_ACTIVE;
	return (error);
#else
	return ENOSYS;
#endif
}

/*
 * utrace system call
 */
/* ARGSUSED */
int
utrace(struct utrace_args *uap)
{
#ifdef KTRACE
	struct ktr_header *kth;
	struct thread *td = curthread;	/* XXX */
	struct proc *p = td->td_proc;
	struct vnode *vp;
	caddr_t cp;

	if (!KTRPOINT(td, KTR_USER))
		return (0);
	if (uap->len > KTR_USER_MAXLEN)
		return (EINVAL);
	p->p_traceflag |= KTRFAC_ACTIVE;
	/*
	 * don't let p_tracep get ripped out from under us while we are
	 * writing.
	 */
	if ((vp = p->p_tracep) != NULL)
		vref(vp);
	kth = ktrgetheader(KTR_USER);
	MALLOC(cp, caddr_t, uap->len, M_KTRACE, M_WAITOK);
	if (!copyin(uap->addr, cp, uap->len)) {
		kth->ktr_buf = cp;
		kth->ktr_len = uap->len;
		ktrwrite(vp, kth, NULL);
	}
	if (vp)
		vrele(vp);
	FREE(kth, M_KTRACE);
	FREE(cp, M_KTRACE);
	p->p_traceflag &= ~KTRFAC_ACTIVE;

	return (0);
#else
	return (ENOSYS);
#endif
}

#ifdef KTRACE
static int
ktrops(struct proc *curp, struct proc *p, int ops, int facs, struct vnode *vp)
{

	if (!ktrcanset(curp, p))
		return (0);
	if (ops == KTROP_SET) {
		if (p->p_tracep != vp) {
			struct vnode *vtmp;

			/*
			 * if trace file already in use, relinquish
			 */
			vref(vp);
			while ((vtmp = p->p_tracep) != NULL) {
				p->p_tracep = NULL;
				vrele(vtmp);
			}
			p->p_tracep = vp;
		}
		p->p_traceflag |= facs;
		if (curp->p_ucred->cr_uid == 0)
			p->p_traceflag |= KTRFAC_ROOT;
	} else {
		/* KTROP_CLEAR */
		if (((p->p_traceflag &= ~facs) & KTRFAC_MASK) == 0) {
			struct vnode *vtmp;

			/* no more tracing */
			p->p_traceflag = 0;
			if ((vtmp = p->p_tracep) != NULL) {
				p->p_tracep = NULL;
				vrele(vtmp);
			}
		}
	}

	return (1);
}

static int
ktrsetchildren(struct proc *curp, struct proc *top, int ops, int facs,
               struct vnode *vp)
{
	struct proc *p;
	int ret = 0;

	p = top;
	for (;;) {
		ret |= ktrops(curp, p, ops, facs, vp);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				return (ret);
			if (LIST_NEXT(p, p_sibling)) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}
	/*NOTREACHED*/
}

static void
ktrwrite(struct vnode *vp, struct ktr_header *kth, struct uio *uio)
{
	struct uio auio;
	struct iovec aiov[2];
	struct thread *td = curthread;
	struct proc *p = td->td_proc;	/* XXX */
	int error;

	/*
	 * Since multiple processes may be trying to ktrace, we must vref
	 * the vnode to prevent it from being ripped out from under us in
	 * case another process gets a write error while ktracing and removes
	 * the reference from the proc.
	 */
	if (vp == NULL)
		return;
	vref(vp);
	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_td = curthread;
	if (kth->ktr_len > 0) {
		auio.uio_iovcnt++;
		aiov[1].iov_base = kth->ktr_buf;
		aiov[1].iov_len = kth->ktr_len;
		auio.uio_resid += kth->ktr_len;
		if (uio != NULL)
			kth->ktr_len += uio->uio_resid;
	}
	VOP_LEASE(vp, td, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_WRITE(vp, &auio, IO_UNIT | IO_APPEND, p->p_ucred);
	if (error == 0 && uio != NULL) {
		VOP_LEASE(vp, td, p->p_ucred, LEASE_WRITE);
		error = VOP_WRITE(vp, uio, IO_UNIT | IO_APPEND, p->p_ucred);
	}
	VOP_UNLOCK(vp, 0, td);
	vrele(vp);
	if (!error)
		return;
	/*
	 * If error encountered, give up tracing on this vnode.  XXX what
	 * happens to the loop if vrele() blocks?
	 */
	log(LOG_NOTICE, "ktrace write failed, errno %d, tracing stopped\n",
	    error);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_tracep == vp) {
			p->p_tracep = NULL;
			p->p_traceflag = 0;
			vrele(vp);
		}
	}
}

/*
 * Return true if caller has permission to set the ktracing state
 * of target.  Essentially, the target can't possess any
 * more permissions than the caller.  KTRFAC_ROOT signifies that
 * root previously set the tracing status on the target process, and
 * so, only root may further change it.
 *
 * TODO: check groups.  use caller effective gid.
 */
static int
ktrcanset(struct proc *callp, struct proc *targetp)
{
	struct ucred *caller = callp->p_ucred;
	struct ucred *target = targetp->p_ucred;

	if (!PRISON_CHECK(caller, target))
		return (0);
	if ((caller->cr_uid == target->cr_ruid &&
	     target->cr_ruid == target->cr_svuid &&
	     caller->cr_rgid == target->cr_rgid &&	/* XXX */
	     target->cr_rgid == target->cr_svgid &&
	     (targetp->p_traceflag & KTRFAC_ROOT) == 0 &&
	     (targetp->p_flag & P_SUGID) == 0) ||
	     caller->cr_uid == 0)
		return (1);

	return (0);
}

#endif /* KTRACE */
