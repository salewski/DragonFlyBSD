/*
 * Copyright (c) 1980, 1986, 1989, 1993
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
 *	@(#)param.c	8.3 (Berkeley) 8/20/94
 * $FreeBSD: src/sys/kern/subr_param.c,v 1.42.2.10 2002/03/09 21:05:47 silby Exp $
 * $DragonFly: src/sys/kern/subr_param.c,v 1.5 2004/05/03 16:06:26 joerg Exp $
 */

#include "opt_param.h"
#include "opt_maxusers.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/vmparam.h>

/*
 * System parameter formulae.
 */

#ifndef HZ
#define	HZ 100
#endif
#define	NPROC (20 + 16 * maxusers)
#ifndef NBUF
#define NBUF 0
#endif
#ifndef MAXFILES
#define	MAXFILES (maxproc * 2)
#endif
#ifndef NSFBUFS
#define NSFBUFS (512 + maxusers * 16)
#endif
#ifndef MAXPOSIXLOCKSPERUID
#define MAXPOSIXLOCKSPERUID (maxusers * 64) /* Should be a safe value */
#endif

int	hz;
int	stathz;
int	profhz;
int	tick;
int	tickadj;			 /* can adjust 30ms in 60s */
int	maxusers;			/* base tunable */
int	maxproc;			/* maximum # of processes */
int	maxprocperuid;			/* max # of procs per user */
int	maxfiles;			/* system wide open files limit */
int	maxfilesrootres;		/* descriptors reserved for root use */
int	maxfilesperproc;		/* per-proc open files limit */
int	maxposixlocksperuid;		/* max # POSIX locks per uid */
int	ncallout;			/* maximum # of timer events */
int	mbuf_wait = 32;			/* mbuf sleep time in ticks */
int	nbuf;
int	nswbuf;
int	maxswzone;			/* max swmeta KVA storage */
int	maxbcache;			/* max buffer cache KVA storage */
u_quad_t	maxtsiz;			/* max text size */
u_quad_t	dfldsiz;			/* initial data size limit */
u_quad_t	maxdsiz;			/* max data size */
u_quad_t	dflssiz;			/* initial stack size limit */
u_quad_t	maxssiz;			/* max stack size */
u_quad_t	sgrowsiz;			/* amount to grow stack */

/* maximum # of sf_bufs (sendfile(2) zero-copy virtual buffers) */
int	nsfbufs;

/*
 * These have to be allocated somewhere; allocating
 * them here forces loader errors if this file is omitted
 * (if they've been externed everywhere else; hah!).
 */
struct	buf *swbuf;

/*
 * Boot time overrides that are not scaled against main memory
 */
void
init_param1(void)
{
	hz = HZ;
	TUNABLE_INT_FETCH("kern.hz", &hz);
	stathz = hz * 128 / 100;
	profhz = stathz;
	tick = 1000000 / hz;
	tickadj = howmany(30000, 60 * hz);	/* can adjust 30ms in 60s */

#ifdef VM_SWZONE_SIZE_MAX
	maxswzone = VM_SWZONE_SIZE_MAX;
#endif
	TUNABLE_INT_FETCH("kern.maxswzone", &maxswzone);
#ifdef VM_BCACHE_SIZE_MAX
	maxbcache = VM_BCACHE_SIZE_MAX;
#endif
	TUNABLE_INT_FETCH("kern.maxbcache", &maxbcache);
	maxtsiz = MAXTSIZ;
	TUNABLE_QUAD_FETCH("kern.maxtsiz", &maxtsiz);
	dfldsiz = DFLDSIZ;
	TUNABLE_QUAD_FETCH("kern.dfldsiz", &dfldsiz);
	maxdsiz = MAXDSIZ;
	TUNABLE_QUAD_FETCH("kern.maxdsiz", &maxdsiz);
	dflssiz = DFLSSIZ;
	TUNABLE_QUAD_FETCH("kern.dflssiz", &dflssiz);
	maxssiz = MAXSSIZ;
	TUNABLE_QUAD_FETCH("kern.maxssiz", &maxssiz);
	sgrowsiz = SGROWSIZ;
	TUNABLE_QUAD_FETCH("kern.sgrowsiz", &sgrowsiz);
}

/*
 * Boot time overrides that are scaled against main memory
 */
void
init_param2(int physpages)
{

	/* Base parameters */
	maxusers = MAXUSERS;
	TUNABLE_INT_FETCH("kern.maxusers", &maxusers);
	if (maxusers == 0) {
		maxusers = physpages / (2 * 1024 * 1024 / PAGE_SIZE);
		if (maxusers < 32)
		    maxusers = 32;
		if (maxusers > 384)
		    maxusers = 384;
	}

	/*
	 * The following can be overridden after boot via sysctl.  Note:
	 * unless overriden, these macros are ultimately based on maxusers.
	 */
	maxproc = NPROC;
	TUNABLE_INT_FETCH("kern.maxproc", &maxproc);
	/*
	 * Limit maxproc so that kmap entries cannot be exhausted by
	 * processes.
	 */
	if (maxproc > (physpages / 12))
		maxproc = physpages / 12;
	maxfiles = MAXFILES;
	TUNABLE_INT_FETCH("kern.maxfiles", &maxfiles);
	maxprocperuid = (maxproc * 9) / 10;
	maxfilesperproc = (maxfiles * 9) / 10;
	maxfilesrootres = maxfiles / 20;

	maxposixlocksperuid = MAXPOSIXLOCKSPERUID;
	TUNABLE_INT_FETCH("kern.maxposixlocksperuid", &maxposixlocksperuid);

	/*
	 * Cannot be changed after boot.  Unless overriden, NSFBUFS is based
	 * on maxusers and NBUF is typically 0 (auto-sized later).
	 */
	nsfbufs = NSFBUFS;
	TUNABLE_INT_FETCH("kern.ipc.nsfbufs", &nsfbufs);
	nbuf = NBUF;
	TUNABLE_INT_FETCH("kern.nbuf", &nbuf);

	ncallout = 16 + maxproc + maxfiles;
	TUNABLE_INT_FETCH("kern.ncallout", &ncallout);
}

