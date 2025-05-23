/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1997-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: mem.h,v 1.54.2.1 2004/03/09 06:11:58 marka Exp $ */

#ifndef ISC_MEM_H
#define ISC_MEM_H 1

#include <stdio.h>

#include <isc/lang.h>
#include <isc/mutex.h>
#include <isc/platform.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

#define ISC_MEM_LOWATER 0
#define ISC_MEM_HIWATER 1
typedef void (*isc_mem_water_t)(void *, int);

typedef void * (*isc_memalloc_t)(void *, size_t);
typedef void (*isc_memfree_t)(void *, void *);

/*
 * ISC_MEM_DEBUG is enabled by default; set ISC_MEM_DEBUG=0 to disable it.
 */
#ifndef ISC_MEM_DEBUG
#define ISC_MEM_DEBUG 1
#endif

/*
 * Define ISC_MEM_TRACKLINES=1 to turn on detailed tracing of memory allocation
 * and freeing by file and line number.
 */
#ifndef ISC_MEM_TRACKLINES
#define ISC_MEM_TRACKLINES 0
#endif

/*
 * Define ISC_MEM_CHECKOVERRUN=1 to turn on checks for using memory outside
 * the requested space.  This will increase the size of each allocation.
 */
#ifndef ISC_MEM_CHECKOVERRUN
#define ISC_MEM_CHECKOVERRUN 0
#endif

/*
 * Define ISC_MEM_FILL to fill each block of memory returned to the system
 * with the byte string '0xbe'.  This helps track down uninitialized pointers
 * and the like.  On freeing memory, the space is filled with '0xde' for
 * the same reasons.
 */
#ifndef ISC_MEM_FILL
#define ISC_MEM_FILL 1
#endif

/*
 * Define this to turn on memory pool names.
 */
#ifndef ISC_MEMPOOL_NAMES
#define ISC_MEMPOOL_NAMES 1
#endif

/*
 * _DEBUGTRACE
 *	log (to isc_lctx) each allocation and free.
 *
 * _DEBUGRECORD
 *	remember each allocation, and match them up on free.  Crash if
 *	a free doesn't match an allocation
 * _DEBUGUSAGE
 *	if a hi_water mark is set print the maximium inuse memory every
 *	time it is raised once it exceeds the hi_water mark
 */
LIBISC_EXTERNAL_DATA extern unsigned int isc_mem_debugging;
#define ISC_MEM_DEBUGTRACE		0x00000001U
#define ISC_MEM_DEBUGRECORD		0x00000002U
#define ISC_MEM_DEBUGUSAGE		0x00000004U

#if ISC_MEM_TRACKLINES
#define _ISC_MEM_FILELINE	, __FILE__, __LINE__
#define _ISC_MEM_FLARG		, const char *, int
#else
#define _ISC_MEM_FILELINE
#define _ISC_MEM_FLARG
#endif

#define isc_mem_get(c, s)	isc__mem_get((c), (s) _ISC_MEM_FILELINE)
#define isc_mem_allocate(c, s)	isc__mem_allocate((c), (s) _ISC_MEM_FILELINE)
#define isc_mem_strdup(c, p)	isc__mem_strdup((c), (p) _ISC_MEM_FILELINE)
#define isc_mempool_get(c)	isc__mempool_get((c) _ISC_MEM_FILELINE)

/*
 * isc_mem_putanddetach() is a convienence function for use where you
 * have a structure with an attached memory context.
 *
 * Given:
 *
 * struct {
 *	...
 *	isc_mem_t *mctx;
 *	...
 * } *ptr;
 *
 * isc_mem_t *mctx;
 *
 * isc_mem_putanddetach(&ptr->mctx, ptr, sizeof(*ptr));
 *
 * is the equivalent of:
 *
 * mctx = NULL;
 * isc_mem_attach(ptr->mctx, &mctx);
 * isc_mem_detach(&ptr->mctx);
 * isc_mem_put(mctx, ptr, sizeof(*ptr));
 * isc_mem_detach(&mctx);
 */

#if ISC_MEM_DEBUG
#define isc_mem_put(c, p, s) \
	do { \
		isc__mem_put((c), (p), (s) _ISC_MEM_FILELINE); \
		(p) = NULL; \
	} while (0)
#define isc_mem_putanddetach(c, p, s) \
	do { \
		isc__mem_putanddetach((c), (p), (s) _ISC_MEM_FILELINE); \
		(p) = NULL; \
	} while (0)
#define isc_mem_free(c, p) \
	do { \
		isc__mem_free((c), (p) _ISC_MEM_FILELINE); \
		(p) = NULL; \
	} while (0)
#define isc_mempool_put(c, p) \
	do { \
		isc__mempool_put((c), (p) _ISC_MEM_FILELINE); \
		(p) = NULL; \
	} while (0)
#else
#define isc_mem_put(c, p, s)	isc__mem_put((c), (p), (s) _ISC_MEM_FILELINE)
#define isc_mem_putanddetach(c, p, s) \
	isc__mem_putanddetach((c), (p), (s) _ISC_MEM_FILELINE)
#define isc_mem_free(c, p)	isc__mem_free((c), (p) _ISC_MEM_FILELINE)
#define isc_mempool_put(c, p)	isc__mempool_put((c), (p) _ISC_MEM_FILELINE)
#endif

isc_result_t 
isc_mem_create(size_t max_size, size_t target_size,
			    isc_mem_t **mctxp);

isc_result_t 
isc_mem_createx(size_t max_size, size_t target_size,
			     isc_memalloc_t memalloc, isc_memfree_t memfree,
			     void *arg, isc_mem_t **mctxp);
/*
 * Create a memory context.
 *
 * 'max_size' and 'target_size' are tuning parameters.  When
 * ISC_MEM_USE_INTERNAL_MALLOC is true, allocations smaller than
 * 'max_size' will be satisfied by getting blocks of size
 * 'target_size' from the system allocator and breaking them up into
 * pieces; larger allocations will use the system allocator directly.
 * If 'max_size' and/or 'target_size' are zero, default values will be
 * used.  When ISC_MEM_USE_INTERNAL_MALLOC is false, 'max_size' and
 * 'target_size' are ignored.
 *
 * A memory context created using isc_mem_createx() will obtain
 * memory from the system by calling 'memalloc' and 'memfree',
 * passing them the argument 'arg'.  A memory context created
 * using isc_mem_create() will use the standard library malloc()
 * and free().
 *
 * Requires:
 * mctxp != NULL && *mctxp == NULL */

void 
isc_mem_attach(isc_mem_t *, isc_mem_t **);
void 
isc_mem_detach(isc_mem_t **);
/*
 * Attach to / detach from a memory context.
 *
 * This is intended for applications that use multiple memory contexts
 * in such a way that it is not obvious when the last allocations from
 * a given context has been freed and destroying the context is safe.
 * 
 * Most applications do not need to call these functions as they can
 * simply create a single memory context at the beginning of main()
 * and destroy it at the end of main(), thereby guaranteeing that it
 * is not destroyed while there are outstanding allocations.
 */

void 
isc_mem_destroy(isc_mem_t **);
/*
 * Destroy a memory context.
 */

isc_result_t 
isc_mem_ondestroy(isc_mem_t *ctx,
			       isc_task_t *task,
			       isc_event_t **event);
/*
 * Request to be notified with an event when a memory context has
 * been successfully destroyed.
 */

void 
isc_mem_stats(isc_mem_t *mctx, FILE *out);
/*
 * Print memory usage statistics for 'mctx' on the stream 'out'.
 */

void 
isc_mem_setdestroycheck(isc_mem_t *mctx,
			     isc_boolean_t on);
/*
 * Iff 'on' is ISC_TRUE, 'mctx' will check for memory leaks when
 * destroyed and abort the program if any are present.
 */

void 
isc_mem_setquota(isc_mem_t *, size_t);
size_t 
isc_mem_getquota(isc_mem_t *);
/*
 * Set/get the memory quota of 'mctx'.  This is a hard limit
 * on the amount of memory that may be allocated from mctx;
 * if it is exceeded, allocations will fail.
 */

size_t 
isc_mem_inuse(isc_mem_t *mctx);
/*
 * Get an estimate of the number of memory in use in 'mctx', in bytes.
 * This includes quantization overhead, but does not include memory
 * allocated from the system but not yet used.
 */

void
isc_mem_setwater(isc_mem_t *mctx, isc_mem_water_t water, void *water_arg,
		 size_t hiwater, size_t lowater);
/*
 * Set high and low water marks for this memory context.  When the memory
 * usage of 'mctx' exceeds 'hiwater', '(water)(water_arg, ISC_MEM_HIWATER)'
 * will be called.  When the usage drops below 'lowater', 'water' will
 * again be called, this time with ISC_MEM_LOWATER.
 *
 * If 'water' is NULL then 'water_arg', 'hi_water' and 'lo_water' are
 * ignored and the state is reset.
 *
 * Requires:
 *
 *	'water' is not NULL.
 *	hi_water >= lo_water
 */

/*
 * Memory pools
 */

isc_result_t
isc_mempool_create(isc_mem_t *mctx, size_t size, isc_mempool_t **mpctxp);
/*
 * Create a memory pool.
 *
 * Requires:
 *	mctx is a valid memory context.
 *	size > 0
 *	mpctxp != NULL and *mpctxp == NULL
 *
 * Defaults:
 *	maxalloc = UINT_MAX
 *	freemax = 1
 *	fillcount = 1
 *
 * Returns:
 *	ISC_R_NOMEMORY		-- not enough memory to create pool
 *	ISC_R_SUCCESS		-- all is well.
 */

void
isc_mempool_destroy(isc_mempool_t **mpctxp);
/*
 * Destroy a memory pool.
 *
 * Requires:
 *	mpctxp != NULL && *mpctxp is a valid pool.
 *	The pool has no un"put" allocations outstanding
 */

void
isc_mempool_setname(isc_mempool_t *mpctx, const char *name);
/*
 * Associate a name with a memory pool.  At most 15 characters may be used.
 *
 * Requires:
 *	mpctx is a valid pool.
 *	name != NULL;
 */

void
isc_mempool_associatelock(isc_mempool_t *mpctx, isc_mutex_t *lock);
/*
 * Associate a lock with this memory pool.
 *
 * This lock is used when getting or putting items using this memory pool,
 * and it is also used to set or get internal state via the isc_mempool_get*()
 * and isc_mempool_set*() set of functions.
 *
 * Mutiple pools can each share a single lock.  For instance, if "manager"
 * type object contained pools for various sizes of events, and each of
 * these pools used a common lock.  Note that this lock must NEVER be used
 * by other than mempool routines once it is given to a pool, since that can
 * easily cause double locking.
 *
 * Requires:
 *
 *	mpctpx is a valid pool.
 *
 *	lock != NULL.
 *
 *	No previous lock is assigned to this pool.
 *
 *	The lock is initialized before calling this function via the normal
 *	means of doing that.
 */

/*
 * The following functions get/set various parameters.  Note that due to
 * the unlocked nature of pools these are potentially random values unless
 * the imposed externally provided locking protocols are followed.
 *
 * Also note that the quota limits will not always take immediate effect.
 * For instance, setting "maxalloc" to a number smaller than the currently
 * allocated count is permitted.  New allocations will be refused until
 * the count drops below this threshold.
 *
 * All functions require (in addition to other requirements):
 *	mpctx is a valid memory pool
 */

unsigned int
isc_mempool_getfreemax(isc_mempool_t *mpctx);
/*
 * Returns the maximum allowed size of the free list.
 */

void
isc_mempool_setfreemax(isc_mempool_t *mpctx, unsigned int limit);
/*
 * Sets the maximum allowed size of the free list.
 */

unsigned int
isc_mempool_getfreecount(isc_mempool_t *mpctx);
/*
 * Returns current size of the free list.
 */

unsigned int
isc_mempool_getmaxalloc(isc_mempool_t *mpctx);
/*
 * Returns the maximum allowed number of allocations.
 */

void
isc_mempool_setmaxalloc(isc_mempool_t *mpctx, unsigned int limit);
/*
 * Sets the maximum allowed number of allocations.
 *
 * Additional requirements:
 *	limit > 0
 */

unsigned int
isc_mempool_getallocated(isc_mempool_t *mpctx);
/*
 * Returns the number of items allocated from this pool.
 */

unsigned int
isc_mempool_getfillcount(isc_mempool_t *mpctx);
/*
 * Returns the number of items allocated as a block from the parent memory
 * context when the free list is empty.
 */

void
isc_mempool_setfillcount(isc_mempool_t *mpctx, unsigned int limit);
/*
 * Sets the fillcount.
 *
 * Additional requirements:
 *	limit > 0
 */


/*
 * Pseudo-private functions for use via macros.  Do not call directly.
 */
void *		
isc__mem_get(isc_mem_t *, size_t _ISC_MEM_FLARG);
void 		
isc__mem_putanddetach(isc_mem_t **, void *,
				      size_t _ISC_MEM_FLARG);
void 		
isc__mem_put(isc_mem_t *, void *, size_t _ISC_MEM_FLARG);
void *		
isc__mem_allocate(isc_mem_t *, size_t _ISC_MEM_FLARG);
void		
isc__mem_free(isc_mem_t *, void * _ISC_MEM_FLARG);
char *		
isc__mem_strdup(isc_mem_t *, const char *_ISC_MEM_FLARG);
void *		
isc__mempool_get(isc_mempool_t * _ISC_MEM_FLARG);
void 		
isc__mempool_put(isc_mempool_t *, void * _ISC_MEM_FLARG);

ISC_LANG_ENDDECLS

#endif /* ISC_MEM_H */
