/*
 * Copyright (c) 1987, 1993
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
 *	@(#)malloc.h	8.5 (Berkeley) 5/3/95
 * $FreeBSD: src/sys/sys/malloc.h,v 1.48.2.2 2002/03/16 02:19:16 archie Exp $
 * $DragonFly: src/sys/sys/malloc.h,v 1.21 2005/03/28 18:49:25 joerg Exp $
 */

#ifndef _SYS_MALLOC_H_
#define	_SYS_MALLOC_H_

#ifndef _MACHINE_PARAM_H_
#include <machine/param.h>	/* for SMP_MAXCPU */
#endif

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

#ifndef _MACHINE_VMPARAM_H_
#include <machine/vmparam.h>	/* for VM_MIN_KERNEL_ADDRESS */
#endif

#define splmem splhigh

#ifndef _MACHINE_TYPES_H_
#include <machine/types.h>	/* vm_paddr_t */
#endif

#endif	/* _KERNEL */

/*
 * flags to malloc.
 */
#define	M_RNOWAIT     	0x0001	/* do not block */
#define	M_WAITOK     	0x0002	/* wait for resources / alloc from cache */
#define	M_ZERO       	0x0100	/* bzero() the allocation */
#define	M_USE_RESERVE	0x0200	/* can eat into free list reserve */
#define	M_NULLOK	0x0400	/* ok to return NULL */
#define M_PASSIVE_ZERO	0x0800	/* (internal to the slab code only) */
#define M_USE_INTERRUPT_RESERVE \
			0x1000	/* can exhaust free list entirely */

/*
 * M_NOWAIT has to be a set of flags for equivalence to prior use. 
 *
 * M_SYSALLOC should be used for any critical infrastructure allocations
 * made by the kernel proper.
 *
 * M_INTNOWAIT should be used for any critical infrastructure allocations
 * made by interrupts.  Such allocations can still fail but will not fail
 * as often as M_NOWAIT.
 *
 * NOTE ON DRAGONFLY USE OF M_NOWAIT.  In FreeBSD M_NOWAIT allocations
 * almost always succeed.  In DragonFly, however, there is a good chance
 * that an allocation will fail.  M_NOWAIT should only be used when 
 * allocations can fail without any serious detriment to the system.
 *
 * Note that allocations made from (preempted) interrupts will attempt to
 * use pages from the VM PAGE CACHE (PQ_CACHE) (i.e. those associated with
 * objects).  This is automatic.
 */

#define	M_INTNOWAIT	(M_RNOWAIT | M_NULLOK | 			\
			 M_USE_RESERVE | M_USE_INTERRUPT_RESERVE)
#define	M_SYSNOWAIT	(M_RNOWAIT | M_NULLOK | M_USE_RESERVE)
#define	M_INTWAIT	(M_WAITOK | M_USE_RESERVE | M_USE_INTERRUPT_RESERVE)
#define	M_SYSWAIT	(M_WAITOK | M_USE_RESERVE)

#define	M_NOWAIT	M_INTNOWAIT
#define	M_SYSALLOC	M_SYSWAIT

#define	M_MAGIC		877983977	/* time when first defined :-) */

/*
 * The malloc tracking structure.  Note that per-cpu entries must be
 * aggregated for accurate statistics, they do not actually break the
 * stats down by cpu (e.g. the cpu freeing memory will subtract from
 * its slot, not the originating cpu's slot).
 *
 * SMP_MAXCPU is used so modules which use malloc remain compatible
 * between UP and SMP.
 */
struct malloc_type {
	struct malloc_type *ks_next;	/* next in list */
	long 	ks_memuse[SMP_MAXCPU];	/* total memory held in bytes */
	long	ks_loosememuse;		/* (inaccurate) aggregate memuse */
	long	ks_limit;	/* most that are allowed to exist */
	long	ks_size;	/* sizes of this thing that are allocated */
	long	ks_inuse[SMP_MAXCPU]; /* # of allocs currently in use */
	__int64_t ks_calls;	/* total packets of this type ever allocated */
	long	ks_maxused;	/* maximum number ever used */
	__uint32_t ks_magic;	/* if it's not magic, don't touch it */
	const char *ks_shortdesc;	/* short description */
	__uint16_t ks_limblocks; /* number of times blocked for hitting limit */
	__uint16_t ks_mapblocks; /* number of times blocked for kernel map */
	long	ks_reserved[4];	/* future use (module compatibility) */
};

typedef struct malloc_type	*malloc_type_t;

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

#define	MALLOC_DEFINE(type, shortdesc, longdesc)	\
	struct malloc_type type[1] = { 			\
	    { NULL, { 0 }, 0, 0, 0, { 0 }, 0, 0, M_MAGIC, shortdesc, 0, 0, { 0 } } \
	}; 								    \
	SYSINIT(type##_init, SI_SUB_KMEM, SI_ORDER_ANY, malloc_init, type); \
	SYSUNINIT(type##_uninit, SI_SUB_KMEM, SI_ORDER_ANY, malloc_uninit, type)

#else

#define	MALLOC_DEFINE(type, shortdesc, longdesc)	\
	struct malloc_type type[1] = { 			\
	    { NULL, { 0 }, 0, 0, 0, { 0 }, 0, 0, M_MAGIC, shortdesc, 0, 0 } \
	};

#endif

#define	MALLOC_DECLARE(type) \
	extern struct malloc_type type[1]

#ifdef _KERNEL

MALLOC_DECLARE(M_CACHE);
MALLOC_DECLARE(M_DEVBUF);
MALLOC_DECLARE(M_TEMP);

MALLOC_DECLARE(M_IP6OPT); /* for INET6 */
MALLOC_DECLARE(M_IP6NDP); /* for INET6 */

#endif /* _KERNEL */

/*
 * Array of descriptors that describe the contents of each page
 */
struct kmemusage {
	short ku_cpu;		/* cpu index */
	union {
		__int16_t freecnt;/* for small allocations, free pieces in page */
		__int16_t pagecnt;/* for large allocations, pages alloced */
	} ku_un;
};
#define ku_freecnt ku_un.freecnt
#define ku_pagecnt ku_un.pagecnt

#ifdef _KERNEL

#define	MINALLOCSIZE	sizeof(void *)

/*
 * Turn virtual addresses into kmem map indices
 */
#define btokup(addr)	(&kmemusage[((caddr_t)(addr) - (caddr_t)VM_MIN_KERNEL_ADDRESS) >> PAGE_SHIFT])

/*
 * Deprecated macro versions of not-quite-malloc() and free().
 */
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), (type), (flags))
#define	FREE(addr, type) free((addr), (type))

/*
 * XXX this should be declared in <sys/uio.h>, but that tends to fail
 * because <sys/uio.h> is included in a header before the source file
 * has a chance to include <sys/malloc.h> to get MALLOC_DECLARE() defined.
 */
MALLOC_DECLARE(M_IOV);

/* XXX struct malloc_type is unused for contig*(). */
void	contigfree (void *addr, unsigned long size,
			struct malloc_type *type);
void	*contigmalloc (unsigned long size, struct malloc_type *type,
			   int flags, vm_paddr_t low, vm_paddr_t high,
			   unsigned long alignment, unsigned long boundary);
void	free (void *addr, struct malloc_type *type);
void	*malloc (unsigned long size, struct malloc_type *type, int flags);
void	malloc_init (void *);
void	malloc_uninit (void *);
void	*realloc (void *addr, unsigned long size,
		      struct malloc_type *type, int flags);
void	*reallocf (void *addr, unsigned long size,
		      struct malloc_type *type, int flags);
char	*strdup (const char *, struct malloc_type *);

#endif /* _KERNEL */

#endif /* !_SYS_MALLOC_H_ */
