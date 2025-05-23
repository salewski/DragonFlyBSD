/*
 * Copyright (c) 1990, 1993
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
 *	@(#)filedesc.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/sys/sys/filedesc.h,v 1.19.2.5 2003/06/06 20:21:32 tegge Exp $
 * $DragonFly: src/sys/sys/filedesc.h,v 1.8 2004/11/12 00:09:27 dillon Exp $
 */

#ifndef _SYS_FILEDESC_H_
#define _SYS_FILEDESC_H_

#include <sys/queue.h>

/*
 * This structure is used for the management of descriptors.  It may be
 * shared by multiple processes.
 *
 * A process is initially started out with NDFILE descriptors stored within
 * this structure, selected to be enough for typical applications based on
 * the historical limit of 20 open files (and the usage of descriptors by
 * shells).  If these descriptors are exhausted, a larger descriptor table
 * may be allocated, up to a process' resource limit; the internal arrays
 * are then unused.  The initial expansion is set to NDEXTENT; each time
 * it runs out, it is doubled until the resource limit is reached. NDEXTENT
 * should be selected to be the biggest multiple of OFILESIZE (see below)
 * that will fit in a power-of-two sized piece of memory.
 */
#define NDFILE		20
#define NDEXTENT	50		/* 250 bytes in 256-byte alloc. */

struct klist;
struct namecache;

struct filedesc {
	struct	file **fd_ofiles;	/* file structures for open files */
	char	*fd_ofileflags;		/* per-process open file flags */
	struct	vnode *fd_cdir;		/* current directory (phaseout) */
	struct	vnode *fd_rdir;		/* root directory (phaseout) */
	struct	vnode *fd_jdir;		/* jail root directory (phaseout) */
	struct  namecache *fd_ncdir;	/* current directory */
	struct  namecache *fd_nrdir;	/* root directory */
	struct  namecache *fd_njdir;	/* jail directory */
	int	fd_nfiles;		/* number of open files allocated */
	u_short	fd_lastfile;		/* high-water mark of fd_ofiles */
	u_short	fd_freefile;		/* approx. next free file */
	u_short	fd_cmask;		/* mask for file creation */
	u_short	fd_refcnt;		/* reference count */

	int	fd_knlistsize;		/* size of knlist */
	struct	klist *fd_knlist;	/* list of attached knotes */
	u_long	fd_knhashmask;		/* size of knhash */
	struct	klist *fd_knhash;	/* hash table for attached knotes */
	int	fd_holdleaderscount;	/* block fdfree() for shared close() */
	int	fd_holdleaderswakeup;	/* fdfree() needs wakeup */
};

/*
 * Basic allocation of descriptors:
 * one of the above, plus arrays for NDFILE descriptors.
 */
struct filedesc0 {
	struct	filedesc fd_fd;
	/*
	 * These arrays are used when the number of open files is
	 * <= NDFILE, and are then pointed to by the pointers above.
	 */
	struct	file *fd_dfiles[NDFILE];
	char	fd_dfileflags[NDFILE];
};



/*
 * Structure to keep track of (process leader, struct fildedesc) tuples.
 * Each process has a pointer to such a structure when detailed tracking
 * is needed. e.g. when rfork(RFPROC | RFMEM) causes a file descriptor
 * table to be shared by processes having different "p_leader" pointers
 * and thus distinct POSIX style locks.
 */
struct filedesc_to_leader {
	int		fdl_refcount;	/* references from struct proc */
	int		fdl_holdcount;	/* temporary hold during closef */
	int		fdl_wakeup;	/* fdfree() waits on closef() */
	struct proc	*fdl_leader;	/* owner of POSIX locks */
	/* Circular list */
	struct filedesc_to_leader *fdl_prev;
	struct filedesc_to_leader *fdl_next;
};

/*
 * Per-process open flags.
 */
#define	UF_EXCLOSE 	0x01		/* auto-close on exec */
#if 0
#define	UF_MAPPED 	0x02		/* mapped from device */
#endif

/*
 * Storage required per open file descriptor.
 */
#define OFILESIZE (sizeof(struct file *) + sizeof(char))

/*
 * This structure that holds the information needed to send a SIGIO or
 * a SIGURG signal to a process or process group when new data arrives
 * on a device or socket.  The structure is placed on an SLIST belonging
 * to the proc or pgrp so that the entire list may be revoked when the
 * process exits or the process group disappears.
 */
struct	sigio {
	union {
		struct	proc *siu_proc; /* process to receive SIGIO/SIGURG */
		struct	pgrp *siu_pgrp; /* process group to receive ... */
	} sio_u;
	SLIST_ENTRY(sigio) sio_pgsigio;	/* sigio's for process or group */
	struct	sigio **sio_myref;	/* location of the pointer that holds
					 * the reference to this structure */
	struct	ucred *sio_ucred;	/* current credentials */
	uid_t	sio_ruid;		/* real user id */
	pid_t	sio_pgid;		/* pgid for signals */
};
#define	sio_proc	sio_u.siu_proc
#define	sio_pgrp	sio_u.siu_pgrp

SLIST_HEAD(sigiolst, sigio);

#ifdef _KERNEL

struct thread;
struct proc;

/*
 * Kernel global variables and routines.
 */
int	dupfdopen (struct filedesc *, int, int, int, int);
int	fdalloc (struct proc *p, int want, int *result);
int	fdavail (struct proc *p, int n);
int	falloc (struct proc *p, struct file **resultfp, int *resultfd);
int	fsetfd (struct proc *p, struct file *fp, int *resultfd);
void	fsetcred (struct file *fp, struct ucred *cr);
void	ffree (struct file *);
struct	filedesc *fdinit (struct proc *p);
struct	filedesc *fdshare (struct proc *p);
struct	filedesc *fdcopy (struct proc *p);
void	fdfree (struct proc *p);
int	closef (struct file *fp, struct thread *td);
void	fdcloseexec (struct proc *p);
int	fdcheckstd (struct proc *p);
struct	file *holdfp (struct filedesc *fdp, int fd, int flag);
int	getvnode (struct filedesc *fdp, int fd, struct file **fpp);
int	fdissequential (struct file *);
void	fdsequential (struct file *, int);
pid_t	fgetown (struct sigio *);
int	fsetown (pid_t, struct sigio **);
void	funsetown (struct sigio *);
void	funsetownlst (struct sigiolst *);
void	setugidsafety (struct proc *p);

struct filedesc_to_leader *
filedesc_to_leader_alloc(struct filedesc_to_leader *old,
			 struct proc *leader);

#endif

#endif
