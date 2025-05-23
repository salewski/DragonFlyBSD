/*
 * Copyright (c) 2000 Dag-Erling Co�dan Sm�rgrav
 * Copyright (c) 1999 Pierre Beyssac
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
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
 *	@(#)procfs_subr.c	8.6 (Berkeley) 5/14/95
 *
 * $FreeBSD: src/sys/i386/linux/linprocfs/linprocfs_subr.c,v 1.3.2.4 2001/06/25 19:46:47 pirzyk Exp $
 * $DragonFly: src/sys/emulation/linux/i386/linprocfs/linprocfs_subr.c,v 1.15 2004/12/17 00:18:05 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include "linprocfs.h"

#define PFSHSIZE	256
#define PFSHMASK	(PFSHSIZE - 1)

static struct pfsnode *pfshead[PFSHSIZE];
static int pfsvplock;

extern int procfs_domem (struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);

/*
 * allocate a pfsnode/vnode pair.  the vnode is
 * referenced, but not locked.
 *
 * the pid, pfs_type, and mount point uniquely
 * identify a pfsnode.  the mount point is needed
 * because someone might mount this filesystem
 * twice.
 *
 * all pfsnodes are maintained on a singly-linked
 * list.  new nodes are only allocated when they cannot
 * be found on this list.  entries on the list are
 * removed when the vfs reclaim entry is called.
 *
 * a single lock is kept for the entire list.  this is
 * needed because the getnewvnode() function can block
 * waiting for a vnode to become free, in which case there
 * may be more than one process trying to get the same
 * vnode.  this lock is only taken if we are going to
 * call getnewvnode, since the kernel itself is single-threaded.
 *
 * if an entry is found on the list, then call vget() to
 * take a reference.  this is done because there may be
 * zero references to it and so it needs to removed from
 * the vnode free list.
 */
int
linprocfs_allocvp(mp, vpp, pid, pfs_type)
	struct mount *mp;
	struct vnode **vpp;
	long pid;
	pfstype pfs_type;
{
	struct thread *td = curthread;	/* XXX */
	struct pfsnode *pfs;
	struct vnode *vp;
	struct pfsnode **pp;
	int error;

loop:
	for (pfs = pfshead[pid & PFSHMASK]; pfs; pfs = pfs->pfs_next) {
		vp = PFSTOV(pfs);
		if (pfs->pfs_pid == pid &&
		    pfs->pfs_type == pfs_type &&
		    vp->v_mount == mp) {
			if (vget(vp, LK_EXCLUSIVE|LK_SLEEPFAIL, td))
				goto loop;
			*vpp = vp;
			return (0);
		}
	}

	/*
	 * otherwise lock the vp list while we call getnewvnode
	 * since that can block.
	 */
	if (pfsvplock & PROCFS_LOCKED) {
		pfsvplock |= PROCFS_WANT;
		(void) tsleep((caddr_t) &pfsvplock, 0, "pfsavp", 0);
		goto loop;
	}
	pfsvplock |= PROCFS_LOCKED;

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(pfs, struct pfsnode *, sizeof(struct pfsnode), M_TEMP, M_WAITOK);

	error = getnewvnode(VT_PROCFS, mp, vpp, 0, 0);
	if (error) {
		FREE(pfs, M_TEMP);
		goto out;
	}
	vp = *vpp;

	vp->v_data = pfs;

	pfs->pfs_next = 0;
	pfs->pfs_pid = (pid_t) pid;
	pfs->pfs_type = pfs_type;
	pfs->pfs_vnode = vp;
	pfs->pfs_flags = 0;
	pfs->pfs_lockowner = NULL;
	pfs->pfs_fileno = PROCFS_FILENO(pid, pfs_type);

	switch (pfs_type) {
	case Proot:	/* /proc = dr-xr-xr-x */
		pfs->pfs_mode = (VREAD|VEXEC) |
				(VREAD|VEXEC) >> 3 |
				(VREAD|VEXEC) >> 6;
		vp->v_type = VDIR;
		vp->v_flag = VROOT;
		break;

	case Pself:	/* /proc/self = lr--r--r-- */
		pfs->pfs_mode = (VREAD) |
				(VREAD >> 3) |
				(VREAD >> 6);
		vp->v_type = VLNK;
		break;

	case Pproc:
		pfs->pfs_mode = (VREAD|VEXEC) |
				(VREAD|VEXEC) >> 3 |
				(VREAD|VEXEC) >> 6;
		vp->v_type = VDIR;
		break;

	case Pexe:
		pfs->pfs_mode = (VREAD|VEXEC) |
				(VREAD|VEXEC) >> 3 |
				(VREAD|VEXEC) >> 6;
		vp->v_type = VLNK;
		break;

	case Pmem:
		pfs->pfs_mode = (VREAD|VWRITE) |
				(VREAD) >> 3;;
		vp->v_type = VREG;
		break;

	case Pprocstat:
	case Pprocstatus:
		/* fallthrough */
		
	case Pmeminfo:
	case Pcpuinfo:
	case Pstat:
	case Puptime:
	case Pversion:
	case Ploadavg:
		pfs->pfs_mode = (VREAD) |
				(VREAD >> 3) |
				(VREAD >> 6);
		vp->v_type = VREG;
		break;

	default:
		panic("linprocfs_allocvp");
	}

	/* add to procfs vnode list */
	for (pp = &pfshead[pid & PFSHMASK]; *pp; pp = &(*pp)->pfs_next)
		continue;
	*pp = pfs;

	vx_unlock(vp);	/* vnode ready to roll! */

out:
	pfsvplock &= ~PROCFS_LOCKED;

	if (pfsvplock & PROCFS_WANT) {
		pfsvplock &= ~PROCFS_WANT;
		wakeup((caddr_t) &pfsvplock);
	}

	return (error);
}

int
linprocfs_freevp(vp)
	struct vnode *vp;
{
	struct pfsnode **pfspp;
	struct pfsnode *pfs = VTOPFS(vp);

	pfspp = &pfshead[pfs->pfs_pid & PFSHMASK]; 
	while (*pfspp != pfs) {
		KKASSERT(*pfspp != NULL);
		pfspp = &(*pfspp)->pfs_next;
	}
	*pfspp = pfs->pfs_next;
	FREE(vp->v_data, M_TEMP);
	vp->v_data = NULL;
	return (0);
}

int
linprocfs_rw(ap)
	struct vop_read_args *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct thread *td = uio->uio_td;
	struct pfsnode *pfs = VTOPFS(vp);
	struct proc *p;
	struct proc *curp;
	int rtval;

	curp = td->td_proc;
	KKASSERT(curp);

	p = PFIND(pfs->pfs_pid);
	if (p == 0)
		return (EINVAL);
	if (p->p_pid == 1 && securelevel > 0 && uio->uio_rw == UIO_WRITE)
		return (EACCES);

	while (pfs->pfs_lockowner) {
		tsleep(&pfs->pfs_lockowner, 0, "pfslck", 0);
	}
	pfs->pfs_lockowner = curthread;

	switch (pfs->pfs_type) {
	case Pmem:
		rtval = procfs_domem(curp, p, pfs, uio);
		break;
	case Pprocstat:
		rtval = linprocfs_doprocstat(curp, p, pfs, uio);
		break;
	case Pprocstatus:
		rtval = linprocfs_doprocstatus(curp, p, pfs, uio);
		break;
	case Pmeminfo:
		rtval = linprocfs_domeminfo(curp, p, pfs, uio);
		break;
	case Pcpuinfo:
		rtval = linprocfs_docpuinfo(curp, p, pfs, uio);
		break;
	case Pstat:
		rtval = linprocfs_dostat(curp, p, pfs, uio);
		break;
	case Puptime:
		rtval = linprocfs_douptime(curp, p, pfs, uio);
		break;
	case Pversion:
		rtval = linprocfs_doversion(curp, p, pfs, uio);
		break;
	case Ploadavg:
		rtval = linprocfs_doloadavg(curp, p, pfs, uio);
		break;
	default:
		rtval = EOPNOTSUPP;
		break;
	}
	pfs->pfs_lockowner = NULL;
	wakeup(&pfs->pfs_lockowner);
	return rtval;
}

#if 0
/*
 * Get a string from userland into (buf).  Strip a trailing
 * nl character (to allow easy access from the shell).
 * The buffer should be *buflenp + 1 chars long.  vfs_getuserstr
 * will automatically add a nul char at the end.
 *
 * Returns 0 on success or the following errors
 *
 * EINVAL:    file offset is non-zero.
 * EMSGSIZE:  message is longer than kernel buffer
 * EFAULT:    user i/o buffer is not addressable
 */
int
vfs_getuserstr(uio, buf, buflenp)
	struct uio *uio;
	char *buf;
	int *buflenp;
{
	int xlen;
	int error;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = *buflenp;

	/* must be able to read the whole string in one go */
	if (xlen < uio->uio_resid)
		return (EMSGSIZE);
	xlen = uio->uio_resid;

	if ((error = uiomove(buf, xlen, uio)) != 0)
		return (error);

	/* allow multiple writes without seeks */
	uio->uio_offset = 0;

	/* cleanup string and remove trailing newline */
	buf[xlen] = '\0';
	xlen = strlen(buf);
	if (xlen > 0 && buf[xlen-1] == '\n')
		buf[--xlen] = '\0';
	*buflenp = xlen;

	return (0);
}

vfs_namemap_t *
vfs_findname(nm, buf, buflen)
	vfs_namemap_t *nm;
	char *buf;
	int buflen;
{

	for (; nm->nm_name; nm++)
		if (bcmp(buf, nm->nm_name, buflen+1) == 0)
			return (nm);

	return (0);
}
#endif

void
linprocfs_exit(struct thread *td)
{
	struct pfsnode *pfs;
	struct vnode *vp;
	pid_t pid;

	KKASSERT(td->td_proc);
	pid = td->td_proc->p_pid;

	/*
	 * Remove all the procfs vnodes associated with an exiting process.
	 */
restart:
	for (pfs = pfshead[pid & PFSHMASK]; pfs; pfs = pfs->pfs_next) {
		if (pfs->pfs_pid == pid) {
			vp = PFSTOV(pfs);
			if (vx_get(vp) == 0)
				vgone(vp);
			goto restart;
		}
	}
}

