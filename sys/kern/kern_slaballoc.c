/*
 * KERN_SLABALLOC.C	- Kernel SLAB memory allocator
 * 
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/kern/kern_slaballoc.c,v 1.28 2005/04/02 15:53:56 joerg Exp $
 *
 * This module implements a slab allocator drop-in replacement for the
 * kernel malloc().
 *
 * A slab allocator reserves a ZONE for each chunk size, then lays the
 * chunks out in an array within the zone.  Allocation and deallocation
 * is nearly instantanious, and fragmentation/overhead losses are limited
 * to a fixed worst-case amount.
 *
 * The downside of this slab implementation is in the chunk size
 * multiplied by the number of zones.  ~80 zones * 128K = 10MB of VM per cpu.
 * In a kernel implementation all this memory will be physical so
 * the zone size is adjusted downward on machines with less physical
 * memory.  The upside is that overhead is bounded... this is the *worst*
 * case overhead.
 *
 * Slab management is done on a per-cpu basis and no locking or mutexes
 * are required, only a critical section.  When one cpu frees memory
 * belonging to another cpu's slab manager an asynchronous IPI message
 * will be queued to execute the operation.   In addition, both the
 * high level slab allocator and the low level zone allocator optimize
 * M_ZERO requests, and the slab allocator does not have to pre initialize
 * the linked list of chunks.
 *
 * XXX Balancing is needed between cpus.  Balance will be handled through
 * asynchronous IPIs primarily by reassigning the z_Cpu ownership of chunks.
 *
 * XXX If we have to allocate a new zone and M_USE_RESERVE is set, use of
 * the new zone should be restricted to M_USE_RESERVE requests only.
 *
 *	Alloc Size	Chunking        Number of zones
 *	0-127		8		16
 *	128-255		16		8
 *	256-511		32		8
 *	512-1023	64		8
 *	1024-2047	128		8
 *	2048-4095	256		8
 *	4096-8191	512		8
 *	8192-16383	1024		8
 *	16384-32767	2048		8
 *	(if PAGE_SIZE is 4K the maximum zone allocation is 16383)
 *
 *	Allocations >= ZoneLimit go directly to kmem.
 *
 *			API REQUIREMENTS AND SIDE EFFECTS
 *
 *    To operate as a drop-in replacement to the FreeBSD-4.x malloc() we
 *    have remained compatible with the following API requirements:
 *
 *    + small power-of-2 sized allocations are power-of-2 aligned (kern_tty)
 *    + all power-of-2 sized allocations are power-of-2 aligned (twe)
 *    + malloc(0) is allowed and returns non-NULL (ahc driver)
 *    + ability to allocate arbitrarily large chunks of memory
 */

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/slaballoc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/lock.h>
#include <sys/thread.h>
#include <sys/globaldata.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#include <machine/cpu.h>

#include <sys/thread2.h>

#define arysize(ary)	(sizeof(ary)/sizeof((ary)[0]))

/*
 * Fixed globals (not per-cpu)
 */
static int ZoneSize;
static int ZoneLimit;
static int ZonePageCount;
static int ZoneMask;
static struct malloc_type *kmemstatistics;
static struct kmemusage *kmemusage;
static int32_t weirdary[16];

static void *kmem_slab_alloc(vm_size_t bytes, vm_offset_t align, int flags);
static void kmem_slab_free(void *ptr, vm_size_t bytes);

/*
 * Misc constants.  Note that allocations that are exact multiples of 
 * PAGE_SIZE, or exceed the zone limit, fall through to the kmem module.
 * IN_SAME_PAGE_MASK is used to sanity-check the per-page free lists.
 */
#define MIN_CHUNK_SIZE		8		/* in bytes */
#define MIN_CHUNK_MASK		(MIN_CHUNK_SIZE - 1)
#define ZONE_RELS_THRESH	2		/* threshold number of zones */
#define IN_SAME_PAGE_MASK	(~(intptr_t)PAGE_MASK | MIN_CHUNK_MASK)

/*
 * The WEIRD_ADDR is used as known text to copy into free objects to
 * try to create deterministic failure cases if the data is accessed after
 * free.
 */    
#define WEIRD_ADDR      0xdeadc0de
#define MAX_COPY        sizeof(weirdary)
#define ZERO_LENGTH_PTR	((void *)-8)

/*
 * Misc global malloc buckets
 */

MALLOC_DEFINE(M_CACHE, "cache", "Various Dynamically allocated caches");
MALLOC_DEFINE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_DEFINE(M_TEMP, "temp", "misc temporary data buffers");
 
MALLOC_DEFINE(M_IP6OPT, "ip6opt", "IPv6 options");
MALLOC_DEFINE(M_IP6NDP, "ip6ndp", "IPv6 Neighbor Discovery");

/*
 * Initialize the slab memory allocator.  We have to choose a zone size based
 * on available physical memory.  We choose a zone side which is approximately
 * 1/1024th of our memory, so if we have 128MB of ram we have a zone size of
 * 128K.  The zone size is limited to the bounds set in slaballoc.h
 * (typically 32K min, 128K max). 
 */
static void kmeminit(void *dummy);

SYSINIT(kmem, SI_SUB_KMEM, SI_ORDER_FIRST, kmeminit, NULL)

static void
kmeminit(void *dummy)
{
    vm_poff_t limsize;
    int usesize;
    int i;
    vm_pindex_t npg;

    limsize = (vm_poff_t)vmstats.v_page_count * PAGE_SIZE;
    if (limsize > VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)
	limsize = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;

    usesize = (int)(limsize / 1024);	/* convert to KB */

    ZoneSize = ZALLOC_MIN_ZONE_SIZE;
    while (ZoneSize < ZALLOC_MAX_ZONE_SIZE && (ZoneSize << 1) < usesize)
	ZoneSize <<= 1;
    ZoneLimit = ZoneSize / 4;
    if (ZoneLimit > ZALLOC_ZONE_LIMIT)
	ZoneLimit = ZALLOC_ZONE_LIMIT;
    ZoneMask = ZoneSize - 1;
    ZonePageCount = ZoneSize / PAGE_SIZE;

    npg = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / PAGE_SIZE;
    kmemusage = kmem_slab_alloc(npg * sizeof(struct kmemusage), PAGE_SIZE, M_WAITOK|M_ZERO);

    for (i = 0; i < arysize(weirdary); ++i)
	weirdary[i] = WEIRD_ADDR;

    if (bootverbose)
	printf("Slab ZoneSize set to %dKB\n", ZoneSize / 1024);
}

/*
 * Initialize a malloc type tracking structure.
 */
void
malloc_init(void *data)
{
    struct malloc_type *type = data;
    vm_poff_t limsize;

    if (type->ks_magic != M_MAGIC)
	panic("malloc type lacks magic");
					   
    if (type->ks_limit != 0)
	return;

    if (vmstats.v_page_count == 0)
	panic("malloc_init not allowed before vm init");

    limsize = (vm_poff_t)vmstats.v_page_count * PAGE_SIZE;
    if (limsize > VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)
	limsize = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;
    type->ks_limit = limsize / 10;

    type->ks_next = kmemstatistics;
    kmemstatistics = type;
}

void
malloc_uninit(void *data)
{
    struct malloc_type *type = data;
    struct malloc_type *t;
#ifdef INVARIANTS
    int i;
    long ttl;
#endif

    if (type->ks_magic != M_MAGIC)
	panic("malloc type lacks magic");

    if (vmstats.v_page_count == 0)
	panic("malloc_uninit not allowed before vm init");

    if (type->ks_limit == 0)
	panic("malloc_uninit on uninitialized type");

#ifdef INVARIANTS
    /*
     * memuse is only correct in aggregation.  Due to memory being allocated
     * on one cpu and freed on another individual array entries may be 
     * negative or positive (canceling each other out).
     */
    for (i = ttl = 0; i < ncpus; ++i)
	ttl += type->ks_memuse[i];
    if (ttl) {
	printf("malloc_uninit: %ld bytes of '%s' still allocated on cpu %d\n",
	    ttl, type->ks_shortdesc, i);
    }
#endif
    if (type == kmemstatistics) {
	kmemstatistics = type->ks_next;
    } else {
	for (t = kmemstatistics; t->ks_next != NULL; t = t->ks_next) {
	    if (t->ks_next == type) {
		t->ks_next = type->ks_next;
		break;
	    }
	}
    }
    type->ks_next = NULL;
    type->ks_limit = 0;
}

/*
 * Calculate the zone index for the allocation request size and set the
 * allocation request size to that particular zone's chunk size.
 */
static __inline int
zoneindex(unsigned long *bytes)
{
    unsigned int n = (unsigned int)*bytes;	/* unsigned for shift opt */
    if (n < 128) {
	*bytes = n = (n + 7) & ~7;
	return(n / 8 - 1);		/* 8 byte chunks, 16 zones */
    }
    if (n < 256) {
	*bytes = n = (n + 15) & ~15;
	return(n / 16 + 7);
    }
    if (n < 8192) {
	if (n < 512) {
	    *bytes = n = (n + 31) & ~31;
	    return(n / 32 + 15);
	}
	if (n < 1024) {
	    *bytes = n = (n + 63) & ~63;
	    return(n / 64 + 23);
	} 
	if (n < 2048) {
	    *bytes = n = (n + 127) & ~127;
	    return(n / 128 + 31);
	}
	if (n < 4096) {
	    *bytes = n = (n + 255) & ~255;
	    return(n / 256 + 39);
	}
	*bytes = n = (n + 511) & ~511;
	return(n / 512 + 47);
    }
#if ZALLOC_ZONE_LIMIT > 8192
    if (n < 16384) {
	*bytes = n = (n + 1023) & ~1023;
	return(n / 1024 + 55);
    }
#endif
#if ZALLOC_ZONE_LIMIT > 16384
    if (n < 32768) {
	*bytes = n = (n + 2047) & ~2047;
	return(n / 2048 + 63);
    }
#endif
    panic("Unexpected byte count %d", n);
    return(0);
}

/*
 * malloc()	(SLAB ALLOCATOR)
 *
 *	Allocate memory via the slab allocator.  If the request is too large,
 *	or if it page-aligned beyond a certain size, we fall back to the
 *	KMEM subsystem.  A SLAB tracking descriptor must be specified, use
 *	&SlabMisc if you don't care.
 *
 *	M_RNOWAIT	- don't block.
 *	M_NULLOK	- return NULL instead of blocking.
 *	M_ZERO		- zero the returned memory.
 *	M_USE_RESERVE	- allow greater drawdown of the free list
 *	M_USE_INTERRUPT_RESERVE - allow the freelist to be exhausted
 */
void *
malloc(unsigned long size, struct malloc_type *type, int flags)
{
    SLZone *z;
    SLChunk *chunk;
    SLGlobalData *slgd;
    struct globaldata *gd;
    int zi;

    gd = mycpu;
    slgd = &gd->gd_slab;

    /*
     * XXX silly to have this in the critical path.
     */
    if (type->ks_limit == 0) {
	crit_enter();
	if (type->ks_limit == 0)
	    malloc_init(type);
	crit_exit();
    }
    ++type->ks_calls;

    /*
     * Handle the case where the limit is reached.  Panic if can't return
     * NULL.  XXX the original malloc code looped, but this tended to
     * simply deadlock the computer.
     */
    while (type->ks_loosememuse >= type->ks_limit) {
	int i;
	long ttl;

	for (i = ttl = 0; i < ncpus; ++i)
	    ttl += type->ks_memuse[i];
	type->ks_loosememuse = ttl;
	if (ttl >= type->ks_limit) {
	    if (flags & M_NULLOK)
		return(NULL);
	    panic("%s: malloc limit exceeded", type->ks_shortdesc);
	}
    }

    /*
     * Handle the degenerate size == 0 case.  Yes, this does happen.
     * Return a special pointer.  This is to maintain compatibility with
     * the original malloc implementation.  Certain devices, such as the
     * adaptec driver, not only allocate 0 bytes, they check for NULL and
     * also realloc() later on.  Joy.
     */
    if (size == 0)
	return(ZERO_LENGTH_PTR);

    /*
     * Handle hysteresis from prior frees here in malloc().  We cannot
     * safely manipulate the kernel_map in free() due to free() possibly
     * being called via an IPI message or from sensitive interrupt code.
     */
    while (slgd->NFreeZones > ZONE_RELS_THRESH && (flags & M_RNOWAIT) == 0) {
	crit_enter();
	if (slgd->NFreeZones > ZONE_RELS_THRESH) {	/* crit sect race */
	    z = slgd->FreeZones;
	    slgd->FreeZones = z->z_Next;
	    --slgd->NFreeZones;
	    kmem_slab_free(z, ZoneSize);	/* may block */
	}
	crit_exit();
    }
    /*
     * XXX handle oversized frees that were queued from free().
     */
    while (slgd->FreeOvZones && (flags & M_RNOWAIT) == 0) {
	crit_enter();
	if ((z = slgd->FreeOvZones) != NULL) {
	    KKASSERT(z->z_Magic == ZALLOC_OVSZ_MAGIC);
	    slgd->FreeOvZones = z->z_Next;
	    kmem_slab_free(z, z->z_ChunkSize);	/* may block */
	}
	crit_exit();
    }

    /*
     * Handle large allocations directly.  There should not be very many of
     * these so performance is not a big issue.
     *
     * Guarentee page alignment for allocations in multiples of PAGE_SIZE
     */
    if (size >= ZoneLimit || (size & PAGE_MASK) == 0) {
	struct kmemusage *kup;

	size = round_page(size);
	chunk = kmem_slab_alloc(size, PAGE_SIZE, flags);
	if (chunk == NULL)
	    return(NULL);
	flags &= ~M_ZERO;	/* result already zero'd if M_ZERO was set */
	flags |= M_PASSIVE_ZERO;
	kup = btokup(chunk);
	kup->ku_pagecnt = size / PAGE_SIZE;
	kup->ku_cpu = gd->gd_cpuid;
	crit_enter();
	goto done;
    }

    /*
     * Attempt to allocate out of an existing zone.  First try the free list,
     * then allocate out of unallocated space.  If we find a good zone move
     * it to the head of the list so later allocations find it quickly
     * (we might have thousands of zones in the list).
     *
     * Note: zoneindex() will panic of size is too large.
     */
    zi = zoneindex(&size);
    KKASSERT(zi < NZONES);
    crit_enter();
    if ((z = slgd->ZoneAry[zi]) != NULL) {
	KKASSERT(z->z_NFree > 0);

	/*
	 * Remove us from the ZoneAry[] when we become empty
	 */
	if (--z->z_NFree == 0) {
	    slgd->ZoneAry[zi] = z->z_Next;
	    z->z_Next = NULL;
	}

	/*
	 * Locate a chunk in a free page.  This attempts to localize
	 * reallocations into earlier pages without us having to sort
	 * the chunk list.  A chunk may still overlap a page boundary.
	 */
	while (z->z_FirstFreePg < ZonePageCount) {
	    if ((chunk = z->z_PageAry[z->z_FirstFreePg]) != NULL) {
#ifdef DIAGNOSTIC
		/*
		 * Diagnostic: c_Next is not total garbage.
		 */
		KKASSERT(chunk->c_Next == NULL ||
			((intptr_t)chunk->c_Next & IN_SAME_PAGE_MASK) ==
			((intptr_t)chunk & IN_SAME_PAGE_MASK));
#endif
#ifdef INVARIANTS
		if ((uintptr_t)chunk < VM_MIN_KERNEL_ADDRESS)
			panic("chunk %p FFPG %d/%d", chunk, z->z_FirstFreePg, ZonePageCount);
		if (chunk->c_Next && (uintptr_t)chunk->c_Next < VM_MIN_KERNEL_ADDRESS)
			panic("chunkNEXT %p %p FFPG %d/%d", chunk, chunk->c_Next, z->z_FirstFreePg, ZonePageCount);
#endif
		z->z_PageAry[z->z_FirstFreePg] = chunk->c_Next;
		goto done;
	    }
	    ++z->z_FirstFreePg;
	}

	/*
	 * No chunks are available but NFree said we had some memory, so
	 * it must be available in the never-before-used-memory area
	 * governed by UIndex.  The consequences are very serious if our zone
	 * got corrupted so we use an explicit panic rather then a KASSERT.
	 */
	if (z->z_UIndex + 1 != z->z_NMax)
	    z->z_UIndex = z->z_UIndex + 1;
	else
	    z->z_UIndex = 0;
	if (z->z_UIndex == z->z_UEndIndex)
	    panic("slaballoc: corrupted zone");
	chunk = (SLChunk *)(z->z_BasePtr + z->z_UIndex * size);
	if ((z->z_Flags & SLZF_UNOTZEROD) == 0) {
	    flags &= ~M_ZERO;
	    flags |= M_PASSIVE_ZERO;
	}
	goto done;
    }

    /*
     * If all zones are exhausted we need to allocate a new zone for this
     * index.  Use M_ZERO to take advantage of pre-zerod pages.  Also see
     * UAlloc use above in regards to M_ZERO.  Note that when we are reusing
     * a zone from the FreeZones list UAlloc'd data will not be zero'd, and
     * we do not pre-zero it because we do not want to mess up the L1 cache.
     *
     * At least one subsystem, the tty code (see CROUND) expects power-of-2
     * allocations to be power-of-2 aligned.  We maintain compatibility by
     * adjusting the base offset below.
     */
    {
	int off;

	if ((z = slgd->FreeZones) != NULL) {
	    slgd->FreeZones = z->z_Next;
	    --slgd->NFreeZones;
	    bzero(z, sizeof(SLZone));
	    z->z_Flags |= SLZF_UNOTZEROD;
	} else {
	    z = kmem_slab_alloc(ZoneSize, ZoneSize, flags|M_ZERO);
	    if (z == NULL)
		goto fail;
	}

	/*
	 * Guarentee power-of-2 alignment for power-of-2-sized chunks.
	 * Otherwise just 8-byte align the data.
	 */
	if ((size | (size - 1)) + 1 == (size << 1))
	    off = (sizeof(SLZone) + size - 1) & ~(size - 1);
	else
	    off = (sizeof(SLZone) + MIN_CHUNK_MASK) & ~MIN_CHUNK_MASK;
	z->z_Magic = ZALLOC_SLAB_MAGIC;
	z->z_ZoneIndex = zi;
	z->z_NMax = (ZoneSize - off) / size;
	z->z_NFree = z->z_NMax - 1;
	z->z_BasePtr = (char *)z + off;
	z->z_UIndex = z->z_UEndIndex = slgd->JunkIndex % z->z_NMax;
	z->z_ChunkSize = size;
	z->z_FirstFreePg = ZonePageCount;
	z->z_CpuGd = gd;
	z->z_Cpu = gd->gd_cpuid;
	chunk = (SLChunk *)(z->z_BasePtr + z->z_UIndex * size);
	z->z_Next = slgd->ZoneAry[zi];
	slgd->ZoneAry[zi] = z;
	if ((z->z_Flags & SLZF_UNOTZEROD) == 0) {
	    flags &= ~M_ZERO;	/* already zero'd */
	    flags |= M_PASSIVE_ZERO;
	}

	/*
	 * Slide the base index for initial allocations out of the next
	 * zone we create so we do not over-weight the lower part of the
	 * cpu memory caches.
	 */
	slgd->JunkIndex = (slgd->JunkIndex + ZALLOC_SLAB_SLIDE)
				& (ZALLOC_MAX_ZONE_SIZE - 1);
    }
done:
    ++type->ks_inuse[gd->gd_cpuid];
    type->ks_memuse[gd->gd_cpuid] += size;
    type->ks_loosememuse += size;
    crit_exit();
    if (flags & M_ZERO)
	bzero(chunk, size);
#ifdef INVARIANTS
    else if ((flags & (M_ZERO|M_PASSIVE_ZERO)) == 0)
	chunk->c_Next = (void *)-1; /* avoid accidental double-free check */
#endif
    return(chunk);
fail:
    crit_exit();
    return(NULL);
}

void *
realloc(void *ptr, unsigned long size, struct malloc_type *type, int flags)
{
    SLZone *z;
    void *nptr;
    unsigned long osize;

    KKASSERT((flags & M_ZERO) == 0);	/* not supported */

    if (ptr == NULL || ptr == ZERO_LENGTH_PTR)
	return(malloc(size, type, flags));
    if (size == 0) {
	free(ptr, type);
	return(NULL);
    }

    /*
     * Handle oversized allocations.  XXX we really should require that a
     * size be passed to free() instead of this nonsense.
     */
    {
	struct kmemusage *kup;

	kup = btokup(ptr);
	if (kup->ku_pagecnt) {
	    osize = kup->ku_pagecnt << PAGE_SHIFT;
	    if (osize == round_page(size))
		return(ptr);
	    if ((nptr = malloc(size, type, flags)) == NULL)
		return(NULL);
	    bcopy(ptr, nptr, min(size, osize));
	    free(ptr, type);
	    return(nptr);
	}
    }

    /*
     * Get the original allocation's zone.  If the new request winds up
     * using the same chunk size we do not have to do anything.
     */
    z = (SLZone *)((uintptr_t)ptr & ~(uintptr_t)ZoneMask);
    KKASSERT(z->z_Magic == ZALLOC_SLAB_MAGIC);

    zoneindex(&size);
    if (z->z_ChunkSize == size)
	return(ptr);

    /*
     * Allocate memory for the new request size.  Note that zoneindex has
     * already adjusted the request size to the appropriate chunk size, which
     * should optimize our bcopy().  Then copy and return the new pointer.
     */
    if ((nptr = malloc(size, type, flags)) == NULL)
	return(NULL);
    bcopy(ptr, nptr, min(size, z->z_ChunkSize));
    free(ptr, type);
    return(nptr);
}

char *
strdup(const char *str, struct malloc_type *type)
{
    int zlen;	/* length inclusive of terminating NUL */
    char *nstr;

    if (str == NULL)
	return(NULL);
    zlen = strlen(str) + 1;
    nstr = malloc(zlen, type, M_WAITOK);
    bcopy(str, nstr, zlen);
    return(nstr);
}

#ifdef SMP
/*
 * free()	(SLAB ALLOCATOR)
 *
 *	Free the specified chunk of memory.
 */
static
void
free_remote(void *ptr)
{
    free(ptr, *(struct malloc_type **)ptr);
}

#endif

void
free(void *ptr, struct malloc_type *type)
{
    SLZone *z;
    SLChunk *chunk;
    SLGlobalData *slgd;
    struct globaldata *gd;
    int pgno;

    gd = mycpu;
    slgd = &gd->gd_slab;

    if (ptr == NULL)
	panic("trying to free NULL pointer");

    /*
     * Handle special 0-byte allocations
     */
    if (ptr == ZERO_LENGTH_PTR)
	return;

    /*
     * Handle oversized allocations.  XXX we really should require that a
     * size be passed to free() instead of this nonsense.
     *
     * This code is never called via an ipi.
     */
    {
	struct kmemusage *kup;
	unsigned long size;

	kup = btokup(ptr);
	if (kup->ku_pagecnt) {
	    size = kup->ku_pagecnt << PAGE_SHIFT;
	    kup->ku_pagecnt = 0;
#ifdef INVARIANTS
	    KKASSERT(sizeof(weirdary) <= size);
	    bcopy(weirdary, ptr, sizeof(weirdary));
#endif
	    /*
	     * note: we always adjust our cpu's slot, not the originating
	     * cpu (kup->ku_cpuid).  The statistics are in aggregate.
	     *
	     * note: XXX we have still inherited the interrupts-can't-block
	     * assumption.  An interrupt thread does not bump
	     * gd_intr_nesting_level so check TDF_INTTHREAD.  This is
	     * primarily until we can fix softupdate's assumptions about free().
	     */
	    crit_enter();
	    --type->ks_inuse[gd->gd_cpuid];
	    type->ks_memuse[gd->gd_cpuid] -= size;
	    if (mycpu->gd_intr_nesting_level || (gd->gd_curthread->td_flags & TDF_INTTHREAD)) {
		z = (SLZone *)ptr;
		z->z_Magic = ZALLOC_OVSZ_MAGIC;
		z->z_Next = slgd->FreeOvZones;
		z->z_ChunkSize = size;
		slgd->FreeOvZones = z;
		crit_exit();
	    } else {
		crit_exit();
		kmem_slab_free(ptr, size);	/* may block */
	    }
	    return;
	}
    }

    /*
     * Zone case.  Figure out the zone based on the fact that it is
     * ZoneSize aligned. 
     */
    z = (SLZone *)((uintptr_t)ptr & ~(uintptr_t)ZoneMask);
    KKASSERT(z->z_Magic == ZALLOC_SLAB_MAGIC);

    /*
     * If we do not own the zone then forward the request to the
     * cpu that does.
     */
    if (z->z_CpuGd != gd) {
	*(struct malloc_type **)ptr = type;
#ifdef SMP
	lwkt_send_ipiq(z->z_CpuGd, free_remote, ptr);
#else
	panic("Corrupt SLZone");
#endif
	return;
    }

    if (type->ks_magic != M_MAGIC)
	panic("free: malloc type lacks magic");

    crit_enter();
    pgno = ((char *)ptr - (char *)z) >> PAGE_SHIFT;
    chunk = ptr;

#ifdef INVARIANTS
    /*
     * Attempt to detect a double-free.  To reduce overhead we only check
     * if there appears to be link pointer at the base of the data.
     */
    if (((intptr_t)chunk->c_Next - (intptr_t)z) >> PAGE_SHIFT == pgno) {
	SLChunk *scan;
	for (scan = z->z_PageAry[pgno]; scan; scan = scan->c_Next) {
	    if (scan == chunk)
		panic("Double free at %p", chunk);
	}
    }
#endif

    /*
     * Put weird data into the memory to detect modifications after freeing,
     * illegal pointer use after freeing (we should fault on the odd address),
     * and so forth.  XXX needs more work, see the old malloc code.
     */
#ifdef INVARIANTS
    if (z->z_ChunkSize < sizeof(weirdary))
	bcopy(weirdary, chunk, z->z_ChunkSize);
    else
	bcopy(weirdary, chunk, sizeof(weirdary));
#endif

    /*
     * Add this free non-zero'd chunk to a linked list for reuse, adjust
     * z_FirstFreePg.
     */
#ifdef INVARIANTS
    if ((uintptr_t)chunk < VM_MIN_KERNEL_ADDRESS)
	panic("BADFREE %p", chunk);
#endif
    chunk->c_Next = z->z_PageAry[pgno];
    z->z_PageAry[pgno] = chunk;
#ifdef INVARIANTS
    if (chunk->c_Next && (uintptr_t)chunk->c_Next < VM_MIN_KERNEL_ADDRESS)
	panic("BADFREE2");
#endif
    if (z->z_FirstFreePg > pgno)
	z->z_FirstFreePg = pgno;

    /*
     * Bump the number of free chunks.  If it becomes non-zero the zone
     * must be added back onto the appropriate list.
     */
    if (z->z_NFree++ == 0) {
	z->z_Next = slgd->ZoneAry[z->z_ZoneIndex];
	slgd->ZoneAry[z->z_ZoneIndex] = z;
    }

    --type->ks_inuse[z->z_Cpu];
    type->ks_memuse[z->z_Cpu] -= z->z_ChunkSize;

    /*
     * If the zone becomes totally free, and there are other zones we
     * can allocate from, move this zone to the FreeZones list.  Since
     * this code can be called from an IPI callback, do *NOT* try to mess
     * with kernel_map here.  Hysteresis will be performed at malloc() time.
     */
    if (z->z_NFree == z->z_NMax && 
	(z->z_Next || slgd->ZoneAry[z->z_ZoneIndex] != z)
    ) {
	SLZone **pz;

	for (pz = &slgd->ZoneAry[z->z_ZoneIndex]; z != *pz; pz = &(*pz)->z_Next)
	    ;
	*pz = z->z_Next;
	z->z_Magic = -1;
	z->z_Next = slgd->FreeZones;
	slgd->FreeZones = z;
	++slgd->NFreeZones;
    }
    crit_exit();
}

/*
 * kmem_slab_alloc()
 *
 *	Directly allocate and wire kernel memory in PAGE_SIZE chunks with the
 *	specified alignment.  M_* flags are expected in the flags field.
 *
 *	Alignment must be a multiple of PAGE_SIZE.
 *
 *	NOTE! XXX For the moment we use vm_map_entry_reserve/release(),
 *	but when we move zalloc() over to use this function as its backend
 *	we will have to switch to kreserve/krelease and call reserve(0)
 *	after the new space is made available.
 *
 *	Interrupt code which has preempted other code is not allowed to
 *	use PQ_CACHE pages.  However, if an interrupt thread is run
 *	non-preemptively or blocks and then runs non-preemptively, then
 *	it is free to use PQ_CACHE pages.
 */
static void *
kmem_slab_alloc(vm_size_t size, vm_offset_t align, int flags)
{
    vm_size_t i;
    vm_offset_t addr;
    vm_offset_t offset;
    int count, vmflags, base_vmflags;
    thread_t td;
    vm_map_t map = kernel_map;

    size = round_page(size);
    addr = vm_map_min(map);

    /*
     * Reserve properly aligned space from kernel_map
     */
    count = vm_map_entry_reserve(MAP_RESERVE_COUNT);
    crit_enter();
    vm_map_lock(map);
    if (vm_map_findspace(map, vm_map_min(map), size, align, &addr)) {
	vm_map_unlock(map);
	if ((flags & M_NULLOK) == 0)
	    panic("kmem_slab_alloc(): kernel_map ran out of space!");
	crit_exit();
	vm_map_entry_release(count);
	return(NULL);
    }
    offset = addr - VM_MIN_KERNEL_ADDRESS;
    vm_object_reference(kernel_object);
    vm_map_insert(map, &count, 
		    kernel_object, offset, addr, addr + size,
		    VM_PROT_ALL, VM_PROT_ALL, 0);

    td = curthread;

    base_vmflags = 0;
    if (flags & M_ZERO)
        base_vmflags |= VM_ALLOC_ZERO;
    if (flags & M_USE_RESERVE)
	base_vmflags |= VM_ALLOC_SYSTEM;
    if (flags & M_USE_INTERRUPT_RESERVE)
        base_vmflags |= VM_ALLOC_INTERRUPT;
    if ((flags & (M_RNOWAIT|M_WAITOK)) == 0)
    	panic("kmem_slab_alloc: bad flags %08x (%p)", flags, ((int **)&size)[-1]);


    /*
     * Allocate the pages.  Do not mess with the PG_ZERO flag yet.
     */
    for (i = 0; i < size; i += PAGE_SIZE) {
	vm_page_t m;
	vm_pindex_t idx = OFF_TO_IDX(offset + i);

	/*
	 * VM_ALLOC_NORMAL can only be set if we are not preempting.
	 *
	 * VM_ALLOC_SYSTEM is automatically set if we are preempting and
	 * M_WAITOK was specified as an alternative (i.e. M_USE_RESERVE is
	 * implied in this case), though I'm sure if we really need to do
	 * that.
	 */
	vmflags = base_vmflags;
	if (flags & M_WAITOK) {
	    if (td->td_preempted)
		vmflags |= VM_ALLOC_SYSTEM;
	    else
		vmflags |= VM_ALLOC_NORMAL;
	}

	m = vm_page_alloc(kernel_object, idx, vmflags);

	/*
	 * If the allocation failed we either return NULL or we retry.
	 *
	 * If M_WAITOK is specified we wait for more memory and retry.
	 * If M_WAITOK is specified from a preemption we yield instead of
	 * wait.  Livelock will not occur because the interrupt thread
	 * will not be preempting anyone the second time around after the
	 * yield.
	 */
	if (m == NULL) {
	    if (flags & M_WAITOK) {
		if (td->td_preempted) {
		    vm_map_unlock(map);
		    lwkt_yield();
		    vm_map_lock(map);
		} else {
		    vm_map_unlock(map);
		    vm_wait();
		    vm_map_lock(map);
		}
		i -= PAGE_SIZE;	/* retry */
		continue;
	    }

	    /*
	     * We were unable to recover, cleanup and return NULL
	     */
	    while (i != 0) {
		i -= PAGE_SIZE;
		m = vm_page_lookup(kernel_object, OFF_TO_IDX(offset + i));
		vm_page_free(m);
	    }
	    vm_map_delete(map, addr, addr + size, &count);
	    vm_map_unlock(map);
	    crit_exit();
	    vm_map_entry_release(count);
	    return(NULL);
	}
    }

    /*
     * Success!
     *
     * Mark the map entry as non-pageable using a routine that allows us to
     * populate the underlying pages.
     */
    vm_map_set_wired_quick(map, addr, size, &count);
    crit_exit();

    /*
     * Enter the pages into the pmap and deal with PG_ZERO and M_ZERO.
     */
    for (i = 0; i < size; i += PAGE_SIZE) {
	vm_page_t m;

	m = vm_page_lookup(kernel_object, OFF_TO_IDX(offset + i));
	m->valid = VM_PAGE_BITS_ALL;
	vm_page_wire(m);
	vm_page_wakeup(m);
	pmap_enter(kernel_pmap, addr + i, m, VM_PROT_ALL, 1);
	if ((m->flags & PG_ZERO) == 0 && (flags & M_ZERO))
	    bzero((char *)addr + i, PAGE_SIZE);
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE | PG_REFERENCED);
    }
    vm_map_unlock(map);
    vm_map_entry_release(count);
    return((void *)addr);
}

static void
kmem_slab_free(void *ptr, vm_size_t size)
{
    crit_enter();
    vm_map_remove(kernel_map, (vm_offset_t)ptr, (vm_offset_t)ptr + size);
    crit_exit();
}

