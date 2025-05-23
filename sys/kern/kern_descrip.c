/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_descrip.c	8.6 (Berkeley) 4/19/94
 * $FreeBSD: src/sys/kern/kern_descrip.c,v 1.81.2.19 2004/02/28 00:43:31 tegge Exp $
 * $DragonFly: src/sys/kern/kern_descrip.c,v 1.39.2.1 2005/06/21 18:12:16 dillon Exp $
 */

#include "opt_compat.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/conf.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/nlookup.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/event.h>
#include <sys/kern_syscall.h>
#include <sys/kcore.h>
#include <sys/kinfo.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <sys/file2.h>

static MALLOC_DEFINE(M_FILEDESC, "file desc", "Open file descriptor table");
static MALLOC_DEFINE(M_FILEDESC_TO_LEADER, "file desc to leader",
		     "file desc to leader structures");
MALLOC_DEFINE(M_FILE, "file", "Open file structure");
static MALLOC_DEFINE(M_SIGIO, "sigio", "sigio structures");

static	 d_open_t  fdopen;
#define NUMFDESC 64

#define CDEV_MAJOR 22
static struct cdevsw fildesc_cdevsw = {
	/* name */	"FD",
	/* maj */	CDEV_MAJOR,
	/* flags */	0,
	/* port */      NULL,
	/* clone */	NULL,

	/* open */	fdopen,
	/* close */	noclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};

static int badfo_readwrite (struct file *fp, struct uio *uio,
    struct ucred *cred, int flags, struct thread *td);
static int badfo_ioctl (struct file *fp, u_long com, caddr_t data,
    struct thread *td);
static int badfo_poll (struct file *fp, int events,
    struct ucred *cred, struct thread *td);
static int badfo_kqfilter (struct file *fp, struct knote *kn);
static int badfo_stat (struct file *fp, struct stat *sb, struct thread *td);
static int badfo_close (struct file *fp, struct thread *td);

/*
 * Descriptor management.
 */
struct filelist filehead;	/* head of list of open files */
int nfiles;			/* actual number of open files */
extern int cmask;	

/*
 * System calls on descriptors.
 */
/* ARGSUSED */
int
getdtablesize(struct getdtablesize_args *uap) 
{
	struct proc *p = curproc;

	uap->sysmsg_result = 
	    min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfilesperproc);
	return (0);
}

/*
 * Duplicate a file descriptor to a particular value.
 *
 * note: keep in mind that a potential race condition exists when closing
 * descriptors from a shared descriptor table (via rfork).
 */
/* ARGSUSED */
int
dup2(struct dup2_args *uap)
{
	int error;

	error = kern_dup(DUP_FIXED, uap->from, uap->to, uap->sysmsg_fds);

	return (error);
}

/*
 * Duplicate a file descriptor.
 */
/* ARGSUSED */
int
dup(struct dup_args *uap)
{
	int error;

	error = kern_dup(DUP_VARIABLE, uap->fd, 0, uap->sysmsg_fds);

	return (error);
}

int
kern_fcntl(int fd, int cmd, union fcntl_dat *dat)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	char *pop;
	struct vnode *vp;
	u_int newmin;
	int tmp, error, flg = F_POSIX;

	KKASSERT(p);

	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	pop = &fdp->fd_ofileflags[fd];

	switch (cmd) {
	case F_DUPFD:
		newmin = dat->fc_fd;
		if (newmin >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
		    newmin > maxfilesperproc)
			return (EINVAL);
		error = kern_dup(DUP_VARIABLE, fd, newmin, &dat->fc_fd);
		return (error);

	case F_GETFD:
		dat->fc_cloexec = (*pop & UF_EXCLOSE) ? FD_CLOEXEC : 0;
		return (0);

	case F_SETFD:
		*pop = (*pop &~ UF_EXCLOSE) |
		    (dat->fc_cloexec & FD_CLOEXEC ? UF_EXCLOSE : 0);
		return (0);

	case F_GETFL:
		dat->fc_flags = OFLAGS(fp->f_flag);
		return (0);

	case F_SETFL:
		fhold(fp);
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= FFLAGS(dat->fc_flags & ~O_ACCMODE) & FCNTLFLAGS;
		tmp = fp->f_flag & FNONBLOCK;
		error = fo_ioctl(fp, FIONBIO, (caddr_t)&tmp, td);
		if (error) {
			fdrop(fp, td);
			return (error);
		}
		tmp = fp->f_flag & FASYNC;
		error = fo_ioctl(fp, FIOASYNC, (caddr_t)&tmp, td);
		if (!error) {
			fdrop(fp, td);
			return (0);
		}
		fp->f_flag &= ~FNONBLOCK;
		tmp = 0;
		fo_ioctl(fp, FIONBIO, (caddr_t)&tmp, td);
		fdrop(fp, td);
		return (error);

	case F_GETOWN:
		fhold(fp);
		error = fo_ioctl(fp, FIOGETOWN, (caddr_t)&dat->fc_owner, td);
		fdrop(fp, td);
		return(error);

	case F_SETOWN:
		fhold(fp);
		error = fo_ioctl(fp, FIOSETOWN, (caddr_t)&dat->fc_owner, td);
		fdrop(fp, td);
		return(error);

	case F_SETLKW:
		flg |= F_WAIT;
		/* Fall into F_SETLK */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;

		/*
		 * copyin/lockop may block
		 */
		fhold(fp);
		if (dat->fc_flock.l_whence == SEEK_CUR)
			dat->fc_flock.l_start += fp->f_offset;

		switch (dat->fc_flock.l_type) {
		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0) {
				error = EBADF;
				break;
			}
			p->p_leader->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    &dat->fc_flock, flg);
			break;
		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0) {
				error = EBADF;
				break;
			}
			p->p_leader->p_flag |= P_ADVLOCK;
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    &dat->fc_flock, flg);
			break;
		case F_UNLCK:
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_UNLCK,
				&dat->fc_flock, F_POSIX);
			break;
		default:
			error = EINVAL;
			break;
		}
		/* Check for race with close */
		if ((unsigned) fd >= fdp->fd_nfiles ||
		    fp != fdp->fd_ofiles[fd]) {
			dat->fc_flock.l_whence = SEEK_SET;
			dat->fc_flock.l_start = 0;
			dat->fc_flock.l_len = 0;
			dat->fc_flock.l_type = F_UNLCK;
			(void) VOP_ADVLOCK(vp, (caddr_t)p->p_leader,
					   F_UNLCK, &dat->fc_flock, F_POSIX);
		}
		fdrop(fp, td);
		return(error);

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;
		/*
		 * copyin/lockop may block
		 */
		fhold(fp);
		if (dat->fc_flock.l_type != F_RDLCK &&
		    dat->fc_flock.l_type != F_WRLCK &&
		    dat->fc_flock.l_type != F_UNLCK) {
			fdrop(fp, td);
			return (EINVAL);
		}
		if (dat->fc_flock.l_whence == SEEK_CUR)
			dat->fc_flock.l_start += fp->f_offset;
		error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_GETLK,
			    &dat->fc_flock, F_POSIX);
		fdrop(fp, td);
		return(error);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * The file control system call.
 */
int
fcntl(struct fcntl_args *uap)
{
	union fcntl_dat dat;
	int error;

	switch (uap->cmd) {
	case F_DUPFD:
		dat.fc_fd = uap->arg;
		break;
	case F_SETFD:
		dat.fc_cloexec = uap->arg;
		break;
	case F_SETFL:
		dat.fc_flags = uap->arg;
		break;
	case F_SETOWN:
		dat.fc_owner = uap->arg;
		break;
	case F_SETLKW:
	case F_SETLK:
	case F_GETLK:
		error = copyin((caddr_t)uap->arg, &dat.fc_flock,
		    sizeof(struct flock));
		if (error)
			return (error);
		break;
	}

	error = kern_fcntl(uap->fd, uap->cmd, &dat);

	if (error == 0) {
		switch (uap->cmd) {
		case F_DUPFD:
			uap->sysmsg_result = dat.fc_fd;
			break;
		case F_GETFD:
			uap->sysmsg_result = dat.fc_cloexec;
			break;
		case F_GETFL:
			uap->sysmsg_result = dat.fc_flags;
			break;
		case F_GETOWN:
			uap->sysmsg_result = dat.fc_owner;
		case F_GETLK:
			error = copyout(&dat.fc_flock, (caddr_t)uap->arg,
			    sizeof(struct flock));
			break;
		}
	}

	return (error);
}

/*
 * Common code for dup, dup2, and fcntl(F_DUPFD).
 *
 * The type flag can be either DUP_FIXED or DUP_VARIABLE.  DUP_FIXED tells
 * kern_dup() to destructively dup over an existing file descriptor if new
 * is already open.  DUP_VARIABLE tells kern_dup() to find the lowest
 * unused file descriptor that is greater than or equal to new.
 */
int
kern_dup(enum dup_type type, int old, int new, int *res)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct file *delfp;
	int holdleaders;
	int error, newfd;

	/*
	 * Verify that we have a valid descriptor to dup from and
	 * possibly to dup to.
	 */
	if (old < 0 || new < 0 || new > p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
	    new >= maxfilesperproc)
		return (EBADF);
	if (old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL)
		return (EBADF);
	if (type == DUP_FIXED && old == new) {
		*res = new;
		return (0);
	}
	fp = fdp->fd_ofiles[old];
	fhold(fp);

	/*
	 * Expand the table for the new descriptor if needed.  This may
	 * block and drop and reacquire the fidedesc lock.
	 */
	if (type == DUP_VARIABLE || new >= fdp->fd_nfiles) {
		error = fdalloc(p, new, &newfd);
		if (error) {
			fdrop(fp, td);
			return (error);
		}
	}
	if (type == DUP_VARIABLE)
		new = newfd;

	/*
	 * If the old file changed out from under us then treat it as a
	 * bad file descriptor.  Userland should do its own locking to
	 * avoid this case.
	 */
	if (fdp->fd_ofiles[old] != fp) {
		if (fdp->fd_ofiles[new] == NULL) {
			if (new < fdp->fd_freefile)
				fdp->fd_freefile = new;
			while (fdp->fd_lastfile > 0 &&
			    fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
				fdp->fd_lastfile--;
		}
		fdrop(fp, td);
		return (EBADF);
	}
	KASSERT(old != new, ("new fd is same as old"));

	/*
	 * Save info on the descriptor being overwritten.  We have
	 * to do the unmap now, but we cannot close it without
	 * introducing an ownership race for the slot.
	 */
	delfp = fdp->fd_ofiles[new];
	if (delfp != NULL && p->p_fdtol != NULL) {
		/*
		 * Ask fdfree() to sleep to ensure that all relevant
		 * process leaders can be traversed in closef().
		 */
		fdp->fd_holdleaderscount++;
		holdleaders = 1;
	} else
		holdleaders = 0;
	KASSERT(delfp == NULL || type == DUP_FIXED,
	    ("dup() picked an open file"));
#if 0
	if (delfp && (fdp->fd_ofileflags[new] & UF_MAPPED))
		(void) munmapfd(p, new);
#endif

	/*
	 * Duplicate the source descriptor, update lastfile
	 */
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	if (new > fdp->fd_lastfile)
		fdp->fd_lastfile = new;
	*res = new;

	/*
	 * If we dup'd over a valid file, we now own the reference to it
	 * and must dispose of it using closef() semantics (as if a
	 * close() were performed on it).
	 */
	if (delfp) {
		(void) closef(delfp, td);
		if (holdleaders) {
			fdp->fd_holdleaderscount--;
			if (fdp->fd_holdleaderscount == 0 &&
			    fdp->fd_holdleaderswakeup != 0) {
				fdp->fd_holdleaderswakeup = 0;
				wakeup(&fdp->fd_holdleaderscount);
			}
		}
	}
	return (0);
}

/*
 * If sigio is on the list associated with a process or process group,
 * disable signalling from the device, remove sigio from the list and
 * free sigio.
 */
void
funsetown(struct sigio *sigio)
{
	int s;

	if (sigio == NULL)
		return;
	s = splhigh();
	*(sigio->sio_myref) = NULL;
	splx(s);
	if (sigio->sio_pgid < 0) {
		SLIST_REMOVE(&sigio->sio_pgrp->pg_sigiolst, sigio,
			     sigio, sio_pgsigio);
	} else /* if ((*sigiop)->sio_pgid > 0) */ {
		SLIST_REMOVE(&sigio->sio_proc->p_sigiolst, sigio,
			     sigio, sio_pgsigio);
	}
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
}

/* Free a list of sigio structures. */
void
funsetownlst(struct sigiolst *sigiolst)
{
	struct sigio *sigio;

	while ((sigio = SLIST_FIRST(sigiolst)) != NULL)
		funsetown(sigio);
}

/*
 * This is common code for FIOSETOWN ioctl called by fcntl(fd, F_SETOWN, arg).
 *
 * After permission checking, add a sigio structure to the sigio list for
 * the process or process group.
 */
int
fsetown(pid_t pgid, struct sigio **sigiop)
{
	struct proc *proc;
	struct pgrp *pgrp;
	struct sigio *sigio;
	int s;

	if (pgid == 0) {
		funsetown(*sigiop);
		return (0);
	}
	if (pgid > 0) {
		proc = pfind(pgid);
		if (proc == NULL)
			return (ESRCH);

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		if (proc->p_session != curproc->p_session)
			return (EPERM);

		pgrp = NULL;
	} else /* if (pgid < 0) */ {
		pgrp = pgfind(-pgid);
		if (pgrp == NULL)
			return (ESRCH);

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		if (pgrp->pg_session != curproc->p_session)
			return (EPERM);

		proc = NULL;
	}
	funsetown(*sigiop);
	sigio = malloc(sizeof(struct sigio), M_SIGIO, M_WAITOK);
	if (pgid > 0) {
		SLIST_INSERT_HEAD(&proc->p_sigiolst, sigio, sio_pgsigio);
		sigio->sio_proc = proc;
	} else {
		SLIST_INSERT_HEAD(&pgrp->pg_sigiolst, sigio, sio_pgsigio);
		sigio->sio_pgrp = pgrp;
	}
	sigio->sio_pgid = pgid;
	sigio->sio_ucred = crhold(curproc->p_ucred);
	/* It would be convenient if p_ruid was in ucred. */
	sigio->sio_ruid = curproc->p_ucred->cr_ruid;
	sigio->sio_myref = sigiop;
	s = splhigh();
	*sigiop = sigio;
	splx(s);
	return (0);
}

/*
 * This is common code for FIOGETOWN ioctl called by fcntl(fd, F_GETOWN, arg).
 */
pid_t
fgetown(struct sigio *sigio)
{
	return (sigio != NULL ? sigio->sio_pgid : 0);
}

/*
 * Close a file descriptor.
 */
/* ARGSUSED */
int
close(struct close_args *uap)
{
	return(kern_close(uap->fd));
}

int
kern_close(int fd)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct filedesc *fdp;
	struct file *fp;
	int error;
	int holdleaders;

	KKASSERT(p);
	fdp = p->p_fd;

	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
#if 0
	if (fdp->fd_ofileflags[fd] & UF_MAPPED)
		(void) munmapfd(p, fd);
#endif
	fdp->fd_ofiles[fd] = NULL;
	fdp->fd_ofileflags[fd] = 0;
	holdleaders = 0;
	if (p->p_fdtol != NULL) {
		/*
		 * Ask fdfree() to sleep to ensure that all relevant
		 * process leaders can be traversed in closef().
		 */
		fdp->fd_holdleaderscount++;
		holdleaders = 1;
	}

	/*
	 * we now hold the fp reference that used to be owned by the descriptor
	 * array.
	 */
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
	if (fd < fdp->fd_knlistsize)
		knote_fdclose(p, fd);
	error = closef(fp, td);
	if (holdleaders) {
		fdp->fd_holdleaderscount--;
		if (fdp->fd_holdleaderscount == 0 &&
		    fdp->fd_holdleaderswakeup != 0) {
			fdp->fd_holdleaderswakeup = 0;
			wakeup(&fdp->fd_holdleaderscount);
		}
	}
	return (error);
}

int
kern_fstat(int fd, struct stat *ub)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct filedesc *fdp;
	struct file *fp;
	int error;

	KKASSERT(p);

	fdp = p->p_fd;
	if ((unsigned)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	fhold(fp);
	error = fo_stat(fp, ub, td);
	fdrop(fp, td);

	return (error);
}

/*
 * Return status information about a file descriptor.
 */
int
fstat(struct fstat_args *uap)
{
	struct stat st;
	int error;

	error = kern_fstat(uap->fd, &st);

	if (error == 0)
		error = copyout(&st, uap->sb, sizeof(st));
	return (error);
}

/*
 * XXX: This is for source compatibility with NetBSD.  Probably doesn't
 * belong here.
 */
int
nfstat(struct nfstat_args *uap)
{
	struct stat st;
	struct nstat nst;
	int error;

	error = kern_fstat(uap->fd, &st);

	if (error == 0) {
		cvtnstat(&st, &nst);
		error = copyout(&nst, uap->sb, sizeof (nst));
	}
	return (error);
}

/*
 * Return pathconf information about a file descriptor.
 */
/* ARGSUSED */
int
fpathconf(struct fpathconf_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct filedesc *fdp;
	struct file *fp;
	struct vnode *vp;
	int error = 0;

	KKASSERT(p);
	fdp = p->p_fd;
	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);

	fhold(fp);

	switch (fp->f_type) {
	case DTYPE_PIPE:
	case DTYPE_SOCKET:
		if (uap->name != _PC_PIPE_BUF) {
			error = EINVAL;
		} else {
			uap->sysmsg_result = PIPE_BUF;
			error = 0;
		}
		break;
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		error = VOP_PATHCONF(vp, uap->name, uap->sysmsg_fds);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	fdrop(fp, td);
	return(error);
}

/*
 * Allocate a file descriptor for the process.
 */
static int fdexpand;
SYSCTL_INT(_debug, OID_AUTO, fdexpand, CTLFLAG_RD, &fdexpand, 0, "");

int
fdalloc(struct proc *p, int want, int *result)
{
	struct filedesc *fdp = p->p_fd;
	int i;
	int lim, last, nfiles;
	struct file **newofile;
	char *newofileflags;

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 *
	 * In 1.2-REL u_short fields in struct filedesc limit us to 65535 
	 * descriptors per process.
	 */
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfilesperproc);
	if (lim > 65535)
		lim = 65535;
	for (;;) {
		last = min(fdp->fd_nfiles, lim);
		if ((i = want) < fdp->fd_freefile)
			i = fdp->fd_freefile;
		for (; i < last; i++) {
			if (fdp->fd_ofiles[i] == NULL) {
				fdp->fd_ofileflags[i] = 0;
				if (i > fdp->fd_lastfile)
					fdp->fd_lastfile = i;
				if (want <= fdp->fd_freefile)
					fdp->fd_freefile = i;
				*result = i;
				return (0);
			}
		}

		/*
		 * No space in current array.  Expand?
		 */
		if (fdp->fd_nfiles >= lim)
			return (EMFILE);
		if (fdp->fd_nfiles < NDEXTENT)
			nfiles = NDEXTENT;
		else
			nfiles = 2 * fdp->fd_nfiles;
		newofile = malloc(nfiles * OFILESIZE, M_FILEDESC, M_WAITOK);

		/*
		 * deal with file-table extend race that might have occured
		 * when malloc was blocked.
		 */
		if (fdp->fd_nfiles >= nfiles) {
			free(newofile, M_FILEDESC);
			continue;
		}
		newofileflags = (char *) &newofile[nfiles];
		/*
		 * Copy the existing ofile and ofileflags arrays
		 * and zero the new portion of each array.
		 */
		bcopy(fdp->fd_ofiles, newofile,
			(i = sizeof(struct file *) * fdp->fd_nfiles));
		bzero((char *)newofile + i, nfiles * sizeof(struct file *) - i);
		bcopy(fdp->fd_ofileflags, newofileflags,
			(i = sizeof(char) * fdp->fd_nfiles));
		bzero(newofileflags + i, nfiles * sizeof(char) - i);
		if (fdp->fd_nfiles > NDFILE)
			free(fdp->fd_ofiles, M_FILEDESC);
		fdp->fd_ofiles = newofile;
		fdp->fd_ofileflags = newofileflags;
		fdp->fd_nfiles = nfiles;
		fdexpand++;
	}
	return (0);
}

/*
 * Check to see whether n user file descriptors
 * are available to the process p.
 */
int
fdavail(struct proc *p, int n)
{
	struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	int i, lim, last;

	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfilesperproc);
	if ((i = lim - fdp->fd_nfiles) > 0 && (n -= i) <= 0)
		return (1);

	last = min(fdp->fd_nfiles, lim);
	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	for (i = last - fdp->fd_freefile; --i >= 0; fpp++) {
		if (*fpp == NULL && --n <= 0)
			return (1);
	}
	return (0);
}

/*
 * falloc:
 *	Create a new open file structure and allocate a file decriptor
 *	for the process that refers to it.  If p is NULL, no descriptor
 *	is allocated and the file pointer is returned unassociated with
 *	any process.  resultfd is only used if p is not NULL and may
 *	separately be NULL indicating that you don't need the returned fd.
 *
 *	A held file pointer is returned.  If a descriptor has been allocated
 *	an additional hold on the fp will be made due to the fd_ofiles[]
 *	reference.
 */
int
falloc(struct proc *p, struct file **resultfp, int *resultfd)
{
	static struct timeval lastfail;
	static int curfail;
	struct file *fp;
	int error;

	fp = NULL;

	/*
	 * Handle filetable full issues and root overfill.
	 */
	if (nfiles >= maxfiles - maxfilesrootres &&
	    ((p && p->p_ucred->cr_ruid != 0) || nfiles >= maxfiles)) {
		if (ppsratecheck(&lastfail, &curfail, 1)) {
			printf("kern.maxfiles limit exceeded by uid %d, please see tuning(7).\n",
				(p ? p->p_ucred->cr_ruid : -1));
		}
		error = ENFILE;
		goto done;
	}

	/*
	 * Allocate a new file descriptor.
	 */
	nfiles++;
	fp = malloc(sizeof(struct file), M_FILE, M_WAITOK | M_ZERO);
	fp->f_count = 1;
	fp->f_ops = &badfileops;
	fp->f_seqcount = 1;
	if (p)
		fp->f_cred = crhold(p->p_ucred);
	else
		fp->f_cred = crhold(proc0.p_ucred);
	LIST_INSERT_HEAD(&filehead, fp, f_list);
	if (resultfd) {
		if ((error = fsetfd(p, fp, resultfd)) != 0) {
			fdrop(fp, p->p_thread);
			fp = NULL;
		}
	} else {
		error = 0;
	}
done:
	*resultfp = fp;
	return (error);
}

/*
 * Associate a file pointer with a file descriptor.  On success the fp
 * will have an additional ref representing the fd_ofiles[] association.
 */
int
fsetfd(struct proc *p, struct file *fp, int *resultfd)
{
	int i;
	int error;

	KKASSERT(p);

	i = -1;
	if ((error = fdalloc(p, 0, &i)) == 0) {
		fhold(fp);
		p->p_fd->fd_ofiles[i] = fp;
	}
	*resultfd = i;
	return (0);
}

void
fsetcred(struct file *fp, struct ucred *cr)
{
	crhold(cr);
	crfree(fp->f_cred);
	fp->f_cred = cr;
}

/*
 * Free a file descriptor.
 */
void
ffree(struct file *fp)
{
	KASSERT((fp->f_count == 0), ("ffree: fp_fcount not 0!"));
	LIST_REMOVE(fp, f_list);
	crfree(fp->f_cred);
	if (fp->f_ncp) {
	    cache_drop(fp->f_ncp);
	    fp->f_ncp = NULL;
	}
	nfiles--;
	free(fp, M_FILE);
}

/*
 * Build a new filedesc structure.
 */
struct filedesc *
fdinit(struct proc *p)
{
	struct filedesc0 *newfdp;
	struct filedesc *fdp = p->p_fd;

	newfdp = malloc(sizeof(struct filedesc0), M_FILEDESC, M_WAITOK|M_ZERO);
	if (fdp->fd_cdir) {
		newfdp->fd_fd.fd_cdir = fdp->fd_cdir;
		vref(newfdp->fd_fd.fd_cdir);
		newfdp->fd_fd.fd_ncdir = cache_hold(fdp->fd_ncdir);
	}

	/*
	 * rdir may not be set in e.g. proc0 or anything vm_fork'd off of
	 * proc0, but should unconditionally exist in other processes.
	 */
	if (fdp->fd_rdir) {
		newfdp->fd_fd.fd_rdir = fdp->fd_rdir;
		vref(newfdp->fd_fd.fd_rdir);
		newfdp->fd_fd.fd_nrdir = cache_hold(fdp->fd_nrdir);
	}
	if (fdp->fd_jdir) {
		newfdp->fd_fd.fd_jdir = fdp->fd_jdir;
		vref(newfdp->fd_fd.fd_jdir);
		newfdp->fd_fd.fd_njdir = cache_hold(fdp->fd_njdir);
	}

	/* Create the file descriptor table. */
	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_cmask = cmask;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_knlistsize = -1;

	return (&newfdp->fd_fd);
}

/*
 * Share a filedesc structure.
 */
struct filedesc *
fdshare(struct proc *p)
{
	p->p_fd->fd_refcnt++;
	return (p->p_fd);
}

/*
 * Copy a filedesc structure.
 */
struct filedesc *
fdcopy(struct proc *p)
{
	struct filedesc *newfdp, *fdp = p->p_fd;
	struct file **fpp;
	int i;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return (NULL);

	newfdp = malloc(sizeof(struct filedesc0), M_FILEDESC, M_WAITOK);
	bcopy(fdp, newfdp, sizeof(struct filedesc));
	if (newfdp->fd_cdir) {
		vref(newfdp->fd_cdir);
		newfdp->fd_ncdir = cache_hold(newfdp->fd_ncdir);
	}
	/*
	 * We must check for fd_rdir here, at least for now because
	 * the init process is created before we have access to the
	 * rootvode to take a reference to it.
	 */
	if (newfdp->fd_rdir) {
		vref(newfdp->fd_rdir);
		newfdp->fd_nrdir = cache_hold(newfdp->fd_nrdir);
	}
	if (newfdp->fd_jdir) {
		vref(newfdp->fd_jdir);
		newfdp->fd_njdir = cache_hold(newfdp->fd_njdir);
	}
	newfdp->fd_refcnt = 1;

	/*
	 * If the number of open files fits in the internal arrays
	 * of the open file structure, use them, otherwise allocate
	 * additional memory for the number of descriptors currently
	 * in use.
	 */
	if (newfdp->fd_lastfile < NDFILE) {
		newfdp->fd_ofiles = ((struct filedesc0 *) newfdp)->fd_dfiles;
		newfdp->fd_ofileflags =
		    ((struct filedesc0 *) newfdp)->fd_dfileflags;
		i = NDFILE;
	} else {
		/*
		 * Compute the smallest multiple of NDEXTENT needed
		 * for the file descriptors currently in use,
		 * allowing the table to shrink.
		 */
		i = newfdp->fd_nfiles;
		while (i > 2 * NDEXTENT && i > newfdp->fd_lastfile * 2)
			i /= 2;
		newfdp->fd_ofiles = malloc(i * OFILESIZE, M_FILEDESC, M_WAITOK);
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}
	newfdp->fd_nfiles = i;
	bcopy(fdp->fd_ofiles, newfdp->fd_ofiles, i * sizeof(struct file **));
	bcopy(fdp->fd_ofileflags, newfdp->fd_ofileflags, i * sizeof(char));

	/*
	 * kq descriptors cannot be copied.
	 */
	if (newfdp->fd_knlistsize != -1) {
		fpp = &newfdp->fd_ofiles[newfdp->fd_lastfile];
		for (i = newfdp->fd_lastfile; i >= 0; i--, fpp--) {
			if (*fpp != NULL && (*fpp)->f_type == DTYPE_KQUEUE) {
				*fpp = NULL;
				if (i < newfdp->fd_freefile)
					newfdp->fd_freefile = i;
			}
			if (*fpp == NULL && i == newfdp->fd_lastfile && i > 0)
				newfdp->fd_lastfile--;
		}
		newfdp->fd_knlist = NULL;
		newfdp->fd_knlistsize = -1;
		newfdp->fd_knhash = NULL;
		newfdp->fd_knhashmask = 0;
	}

	fpp = newfdp->fd_ofiles;
	for (i = newfdp->fd_lastfile; i-- >= 0; fpp++) {
		if (*fpp != NULL)
			fhold(*fpp);
	}
	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(struct proc *p)
{
	struct thread *td = p->p_thread;
	struct filedesc *fdp = p->p_fd;
	struct file **fpp;
	int i;
	struct filedesc_to_leader *fdtol;
	struct file *fp;
	struct vnode *vp;
	struct flock lf;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return;

	/* Check for special need to clear POSIX style locks */
	fdtol = p->p_fdtol;
	if (fdtol != NULL) {
		KASSERT(fdtol->fdl_refcount > 0,
			("filedesc_to_refcount botch: fdl_refcount=%d",
			 fdtol->fdl_refcount));
		if (fdtol->fdl_refcount == 1 &&
		    (p->p_leader->p_flag & P_ADVLOCK) != 0) {
			i = 0;
			fpp = fdp->fd_ofiles;
			for (i = 0, fpp = fdp->fd_ofiles;
			     i <= fdp->fd_lastfile;
			     i++, fpp++) {
				if (*fpp == NULL ||
				    (*fpp)->f_type != DTYPE_VNODE)
					continue;
				fp = *fpp;
				fhold(fp);
				lf.l_whence = SEEK_SET;
				lf.l_start = 0;
				lf.l_len = 0;
				lf.l_type = F_UNLCK;
				vp = (struct vnode *)fp->f_data;
				(void) VOP_ADVLOCK(vp,
						   (caddr_t)p->p_leader,
						   F_UNLCK,
						   &lf,
						   F_POSIX);
				fdrop(fp, p->p_thread);
				fpp = fdp->fd_ofiles + i;
			}
		}
	retry:
		if (fdtol->fdl_refcount == 1) {
			if (fdp->fd_holdleaderscount > 0 &&
			    (p->p_leader->p_flag & P_ADVLOCK) != 0) {
				/*
				 * close() or do_dup() has cleared a reference
				 * in a shared file descriptor table.
				 */
				fdp->fd_holdleaderswakeup = 1;
				tsleep(&fdp->fd_holdleaderscount,
				       0, "fdlhold", 0);
				goto retry;
			}
			if (fdtol->fdl_holdcount > 0) {
				/* 
				 * Ensure that fdtol->fdl_leader
				 * remains valid in closef().
				 */
				fdtol->fdl_wakeup = 1;
				tsleep(fdtol, 0, "fdlhold", 0);
				goto retry;
			}
		}
		fdtol->fdl_refcount--;
		if (fdtol->fdl_refcount == 0 &&
		    fdtol->fdl_holdcount == 0) {
			fdtol->fdl_next->fdl_prev = fdtol->fdl_prev;
			fdtol->fdl_prev->fdl_next = fdtol->fdl_next;
		} else
			fdtol = NULL;
		p->p_fdtol = NULL;
		if (fdtol != NULL)
			free(fdtol, M_FILEDESC_TO_LEADER);
	}
	if (--fdp->fd_refcnt > 0)
		return;
	/*
	 * we are the last reference to the structure, we can
	 * safely assume it will not change out from under us.
	 */
	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i-- >= 0; fpp++) {
		if (*fpp)
			(void) closef(*fpp, td);
	}
	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);
	if (fdp->fd_cdir) {
		cache_drop(fdp->fd_ncdir);
		vrele(fdp->fd_cdir);
	}
	if (fdp->fd_rdir) {
		cache_drop(fdp->fd_nrdir);
		vrele(fdp->fd_rdir);
	}
	if (fdp->fd_jdir) {
		cache_drop(fdp->fd_njdir);
		vrele(fdp->fd_jdir);
	}
	if (fdp->fd_knlist)
		free(fdp->fd_knlist, M_KQUEUE);
	if (fdp->fd_knhash)
		free(fdp->fd_knhash, M_KQUEUE);
	free(fdp, M_FILEDESC);
}

/*
 * For setugid programs, we don't want to people to use that setugidness
 * to generate error messages which write to a file which otherwise would
 * otherwise be off-limits to the process.
 *
 * This is a gross hack to plug the hole.  A better solution would involve
 * a special vop or other form of generalized access control mechanism.  We
 * go ahead and just reject all procfs file systems accesses as dangerous.
 *
 * Since setugidsafety calls this only for fd 0, 1 and 2, this check is
 * sufficient.  We also don't for check setugidness since we know we are.
 */
static int
is_unsafe(struct file *fp)
{
	if (fp->f_type == DTYPE_VNODE && 
	    ((struct vnode *)(fp->f_data))->v_tag == VT_PROCFS)
		return (1);
	return (0);
}

/*
 * Make this setguid thing safe, if at all possible.
 */
void
setugidsafety(struct proc *p)
{
	struct thread *td = p->p_thread;
	struct filedesc *fdp = p->p_fd;
	int i;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return;

	/*
	 * note: fdp->fd_ofiles may be reallocated out from under us while
	 * we are blocked in a close.  Be careful!
	 */
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (i > 2)
			break;
		if (fdp->fd_ofiles[i] && is_unsafe(fdp->fd_ofiles[i])) {
			struct file *fp;

#if 0
			if ((fdp->fd_ofileflags[i] & UF_MAPPED) != 0)
				(void) munmapfd(p, i);
#endif
			if (i < fdp->fd_knlistsize)
				knote_fdclose(p, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			if (i < fdp->fd_freefile)
				fdp->fd_freefile = i;
			(void) closef(fp, td);
		}
	}
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(struct proc *p)
{
	struct thread *td = p->p_thread;
	struct filedesc *fdp = p->p_fd;
	int i;

	/* Certain daemons might not have file descriptors. */
	if (fdp == NULL)
		return;

	/*
	 * We cannot cache fd_ofiles or fd_ofileflags since operations
	 * may block and rip them out from under us.
	 */
	for (i = 0; i <= fdp->fd_lastfile; i++) {
		if (fdp->fd_ofiles[i] != NULL &&
		    (fdp->fd_ofileflags[i] & UF_EXCLOSE)) {
			struct file *fp;

#if 0
			if (fdp->fd_ofileflags[i] & UF_MAPPED)
				(void) munmapfd(p, i);
#endif
			if (i < fdp->fd_knlistsize)
				knote_fdclose(p, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fp = fdp->fd_ofiles[i];
			fdp->fd_ofiles[i] = NULL;
			fdp->fd_ofileflags[i] = 0;
			if (i < fdp->fd_freefile)
				fdp->fd_freefile = i;
			(void) closef(fp, td);
		}
	}
	while (fdp->fd_lastfile > 0 && fdp->fd_ofiles[fdp->fd_lastfile] == NULL)
		fdp->fd_lastfile--;
}

/*
 * It is unsafe for set[ug]id processes to be started with file
 * descriptors 0..2 closed, as these descriptors are given implicit
 * significance in the Standard C library.  fdcheckstd() will create a
 * descriptor referencing /dev/null for each of stdin, stdout, and
 * stderr that is not already open.
 */
int
fdcheckstd(struct proc *p)
{
	struct thread *td = p->p_thread;
	struct nlookupdata nd;
	struct filedesc *fdp;
	struct file *fp;
	register_t retval;
	int fd, i, error, flags, devnull;

       fdp = p->p_fd;
       if (fdp == NULL)
               return (0);
       devnull = -1;
       error = 0;
       for (i = 0; i < 3; i++) {
		if (fdp->fd_ofiles[i] != NULL)
			continue;
		if (devnull < 0) {
			if ((error = falloc(p, &fp, NULL)) != 0)
				break;

			error = nlookup_init(&nd, "/dev/null", UIO_SYSSPACE,
						NLC_FOLLOW|NLC_LOCKVP);
			flags = FREAD | FWRITE;
			if (error == 0)
				error = vn_open(&nd, fp, flags, 0);
			if (error == 0)
				error = fsetfd(p, fp, &fd);
			fdrop(fp, td);
			nlookup_done(&nd);
			if (error)
				break;
			KKASSERT(i == fd);
			devnull = fd;
		} else {
			error = kern_dup(DUP_FIXED, devnull, i, &retval);
			if (error != 0)
				break;
		}
       }
       return (error);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 * Note: td and/or p may be NULL when closing a file
 * that was being passed in a message.
 */
int
closef(struct file *fp, struct thread *td)
{
	struct vnode *vp;
	struct flock lf;
	struct filedesc_to_leader *fdtol;
	struct proc *p;

	if (fp == NULL)
		return (0);
	if (td == NULL) {
		td = curthread;
		p = NULL;		/* allow no proc association */
	} else {
		p = td->td_proc;	/* can also be NULL */
	}
	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor.
	 */
	if (p != NULL && 
	    fp->f_type == DTYPE_VNODE) {
		if ((p->p_leader->p_flag & P_ADVLOCK) != 0) {
			lf.l_whence = SEEK_SET;
			lf.l_start = 0;
			lf.l_len = 0;
			lf.l_type = F_UNLCK;
			vp = (struct vnode *)fp->f_data;
			(void) VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_UNLCK,
					   &lf, F_POSIX);
		}
		fdtol = p->p_fdtol;
		if (fdtol != NULL) {
			/*
			 * Handle special case where file descriptor table
			 * is shared between multiple process leaders.
			 */
			for (fdtol = fdtol->fdl_next;
			     fdtol != p->p_fdtol;
			     fdtol = fdtol->fdl_next) {
				if ((fdtol->fdl_leader->p_flag &
				     P_ADVLOCK) == 0)
					continue;
				fdtol->fdl_holdcount++;
				lf.l_whence = SEEK_SET;
				lf.l_start = 0;
				lf.l_len = 0;
				lf.l_type = F_UNLCK;
				vp = (struct vnode *)fp->f_data;
				(void) VOP_ADVLOCK(vp,
						   (caddr_t)p->p_leader,
						   F_UNLCK, &lf, F_POSIX);
				fdtol->fdl_holdcount--;
				if (fdtol->fdl_holdcount == 0 &&
				    fdtol->fdl_wakeup != 0) {
					fdtol->fdl_wakeup = 0;
					wakeup(fdtol);
				}
			}
		}
	}
	return (fdrop(fp, td));
}

int
fdrop(struct file *fp, struct thread *td)
{
	struct flock lf;
	struct vnode *vp;
	int error;

	if (--fp->f_count > 0)
		return (0);
	if (fp->f_count < 0)
		panic("fdrop: count < 0");
	if ((fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
	}
	if (fp->f_ops != &badfileops)
		error = fo_close(fp, td);
	else
		error = 0;
	ffree(fp);
	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on
 * the entire file (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
/* ARGSUSED */
int
flock(struct flock_args *uap)
{
	struct proc *p = curproc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	struct flock lf;

	if ((unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EOPNOTSUPP);
	vp = (struct vnode *)fp->f_data;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (uap->how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		fp->f_flag &= ~FHASLOCK;
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK));
	}
	if (uap->how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (uap->how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else
		return (EBADF);
	fp->f_flag |= FHASLOCK;
	if (uap->how & LOCK_NB)
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK));
	return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK|F_WAIT));
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 */
/* ARGSUSED */
static int
fdopen(dev_t dev, int mode, int type, struct thread *td)
{
	KKASSERT(td->td_proc != NULL);

	/*
	 * XXX Kludge: set curproc->p_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	td->td_proc->p_dupfd = minor(dev);
	return (ENODEV);
}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(struct filedesc *fdp, int indx, int dfd, int mode, int error)
{
	struct file *wfp;
	struct file *fp;

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, then reject.
	 */
	if ((u_int)dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL) {
		return (EBADF);
	}

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor
	 * (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and
	 * store it in (indx).  (dfd) is effectively closed by
	 * this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag)
			return (EACCES);
		fp = fdp->fd_ofiles[indx];
#if 0
		if (fp && fdp->fd_ofileflags[indx] & UF_MAPPED)
			(void) munmapfd(p, indx);
#endif
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fhold(wfp);
		if (indx > fdp->fd_lastfile)
			fdp->fd_lastfile = indx;
		/*
		 * we now own the reference to fp that the ofiles[] array
		 * used to own.  Release it.
		 */
		if (fp)
			fdrop(fp, curthread);
		return (0);

	case ENXIO:
		/*
		 * Steal away the file pointer from dfd, and stuff it into indx.
		 */
		fp = fdp->fd_ofiles[indx];
#if 0
		if (fp && fdp->fd_ofileflags[indx] & UF_MAPPED)
			(void) munmapfd(p, indx);
#endif
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofileflags[dfd] = 0;

		/*
		 * we now own the reference to fp that the ofiles[] array
		 * used to own.  Release it.
		 */
		if (fp)
			fdrop(fp, curthread);
		/*
		 * Complete the clean up of the filedesc structure by
		 * recomputing the various hints.
		 */
		if (indx > fdp->fd_lastfile) {
			fdp->fd_lastfile = indx;
		} else {
			while (fdp->fd_lastfile > 0 &&
			   fdp->fd_ofiles[fdp->fd_lastfile] == NULL) {
				fdp->fd_lastfile--;
			}
			if (dfd < fdp->fd_freefile)
				fdp->fd_freefile = dfd;
		}
		return (0);

	default:
		return (error);
	}
	/* NOTREACHED */
}


struct filedesc_to_leader *
filedesc_to_leader_alloc(struct filedesc_to_leader *old,
			 struct proc *leader)
{
	struct filedesc_to_leader *fdtol;
	
	fdtol = malloc(sizeof(struct filedesc_to_leader), 
			M_FILEDESC_TO_LEADER, M_WAITOK);
	fdtol->fdl_refcount = 1;
	fdtol->fdl_holdcount = 0;
	fdtol->fdl_wakeup = 0;
	fdtol->fdl_leader = leader;
	if (old != NULL) {
		fdtol->fdl_next = old->fdl_next;
		fdtol->fdl_prev = old;
		old->fdl_next = fdtol;
		fdtol->fdl_next->fdl_prev = fdtol;
	} else {
		fdtol->fdl_next = fdtol;
		fdtol->fdl_prev = fdtol;
	}
	return fdtol;
}

/*
 * Get file structures.
 */
static int
sysctl_kern_file(SYSCTL_HANDLER_ARGS)
{
	struct kinfo_file kf;
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	int count;
	int error;
	int n;

	/*
	 * Note: because the number of file descriptors is calculated
	 * in different ways for sizing vs returning the data,
	 * there is information leakage from the first loop.  However,
	 * it is of a similar order of magnitude to the leakage from
	 * global system statistics such as kern.openfiles.
	 *
	 * When just doing a count, note that we cannot just count
	 * the elements and add f_count via the filehead list because 
	 * threaded processes share their descriptor table and f_count might
	 * still be '1' in that case.
	 */
	count = 0;
	error = 0;
	LIST_FOREACH(p, &allproc, p_list) {
		if (p->p_stat == SIDL)
			continue;
		if (!PRISON_CHECK(req->td->td_proc->p_ucred, p->p_ucred) != 0)
			continue;
		if ((fdp = p->p_fd) == NULL)
			continue;
		for (n = 0; n < fdp->fd_nfiles; ++n) {
			if ((fp = fdp->fd_ofiles[n]) == NULL)
				continue;
			if (req->oldptr == NULL) {
				++count;
			} else {
				kcore_make_file(&kf, fp, p->p_pid,
						p->p_ucred->cr_uid, n);
				error = SYSCTL_OUT(req, &kf, sizeof(kf));
				if (error)
					break;
			}
		}
		if (error)
			break;
	}

	/*
	 * When just calculating the size, overestimate a bit to try to
	 * prevent system activity from causing the buffer-fill call 
	 * to fail later on.
	 */
	if (req->oldptr == NULL) {
		count = (count + 16) + (count / 10);
		error = SYSCTL_OUT(req, NULL, count * sizeof(kf));
	}
	return (error);
}

SYSCTL_PROC(_kern, KERN_FILE, file, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 0, sysctl_kern_file, "S,file", "Entire file table");

SYSCTL_INT(_kern, KERN_MAXFILESPERPROC, maxfilesperproc, CTLFLAG_RW, 
    &maxfilesperproc, 0, "Maximum files allowed open per process");

SYSCTL_INT(_kern, KERN_MAXFILES, maxfiles, CTLFLAG_RW, 
    &maxfiles, 0, "Maximum number of files");

SYSCTL_INT(_kern, OID_AUTO, maxfilesrootres, CTLFLAG_RW, 
    &maxfilesrootres, 0, "Descriptors reserved for root use");

SYSCTL_INT(_kern, OID_AUTO, openfiles, CTLFLAG_RD, 
	&nfiles, 0, "System-wide number of open files");

static void
fildesc_drvinit(void *unused)
{
	int fd;

	cdevsw_add(&fildesc_cdevsw, 0, 0);
	for (fd = 0; fd < NUMFDESC; fd++) {
		make_dev(&fildesc_cdevsw, fd,
		    UID_BIN, GID_BIN, 0666, "fd/%d", fd);
	}
	make_dev(&fildesc_cdevsw, 0, UID_ROOT, GID_WHEEL, 0666, "stdin");
	make_dev(&fildesc_cdevsw, 1, UID_ROOT, GID_WHEEL, 0666, "stdout");
	make_dev(&fildesc_cdevsw, 2, UID_ROOT, GID_WHEEL, 0666, "stderr");
}

struct fileops badfileops = {
	NULL,	/* port */
	NULL,	/* clone */
	badfo_readwrite,
	badfo_readwrite,
	badfo_ioctl,
	badfo_poll,
	badfo_kqfilter,
	badfo_stat,
	badfo_close
};

static int
badfo_readwrite(
	struct file *fp,
	struct uio *uio,
	struct ucred *cred,
	int flags,
	struct thread *td
) {
	return (EBADF);
}

static int
badfo_ioctl(struct file *fp, u_long com, caddr_t data, struct thread *td)
{
	return (EBADF);
}

static int
badfo_poll(struct file *fp, int events, struct ucred *cred, struct thread *td)
{
	return (0);
}

static int
badfo_kqfilter(struct file *fp, struct knote *kn)
{
	return (0);
}

static int
badfo_stat(struct file *fp, struct stat *sb, struct thread *td)
{
	return (EBADF);
}

static int
badfo_close(struct file *fp, struct thread *td)
{
	return (EBADF);
}

SYSINIT(fildescdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,
					fildesc_drvinit,NULL)
