/*
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
 * $DragonFly: src/sys/kern/kern_mpipe.c,v 1.8 2004/07/16 05:51:10 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/slaballoc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/lock.h>
#include <sys/thread.h>
#include <sys/globaldata.h>
#include <sys/mpipe.h>

#include <sys/thread2.h>

#define arysize(ary)	(sizeof(ary)/sizeof((ary)[0]))

static MALLOC_DEFINE(M_MPIPEARY, "MPipe Array", "Auxillary MPIPE structure");

/*
 * Initialize a malloc pipeline for the specified malloc type and allocation
 * size.  Create an array to cache up to nom_count buffers and preallocate
 * them.
 */
void
mpipe_init(malloc_pipe_t mpipe, malloc_type_t type, int bytes,
	int nnom, int nmax, 
	int mpflags, void (*deconstruct)(struct malloc_pipe *, void *))
{
    int n;

    if (nnom < 1)
	nnom = 1;
    if (nmax < 0)
	nmax = 0x7FFF0000;	/* some very large number */
    if (nmax < nnom)
	nmax = nnom;
    bzero(mpipe, sizeof(struct malloc_pipe));
    mpipe->type = type;
    mpipe->bytes = bytes;
    mpipe->mpflags = mpflags;
    mpipe->deconstruct = deconstruct;
    if ((mpflags & MPF_NOZERO) == 0)
	mpipe->mflags |= M_ZERO;
    if (mpflags & MPF_INT)
	mpipe->mflags |= M_USE_RESERVE | M_USE_INTERRUPT_RESERVE;
    mpipe->ary_count = nnom;
    mpipe->max_count = nmax;
    mpipe->array = malloc(nnom * sizeof(mpipe->array[0]), M_MPIPEARY, 
			    M_WAITOK | M_ZERO);

    while (mpipe->free_count < nnom) {
	n = mpipe->free_count;
	mpipe->array[n] = malloc(bytes, mpipe->type, M_WAITOK | mpipe->mflags);
	++mpipe->free_count;
	++mpipe->total_count;
    }
}

/*
 * Destroy a previously initialized mpipe.  This routine can also safely be
 * called on an uninitialized mpipe structure if it was zero'd or mpipe_done()
 * was previously called on it.
 */
void
mpipe_done(malloc_pipe_t mpipe)
{
    void *buf;
    int n;

    KKASSERT(mpipe->free_count == mpipe->total_count);	/* no outstanding mem */
    for (n = mpipe->free_count - 1; n >= 0; --n) {
	buf = mpipe->array[n];
	mpipe->array[n] = NULL;
	KKASSERT(buf != NULL);
	if (mpipe->deconstruct)
	    mpipe->deconstruct(mpipe, buf);
	free(buf, mpipe->type);
    }
    mpipe->free_count = 0;
    mpipe->total_count = 0;
    if (mpipe->array) {
	free(mpipe->array, M_MPIPEARY);
	mpipe->array = NULL;
    }
}

/*
 * Allocate an entry, nominally non-blocking.  The allocation is guarenteed
 * to return non-NULL up to the nominal count after which it may return NULL.
 * Note that the implementation is defined to be allowed to block for short
 * periods of time.  Use mpipe_alloc_waitok() to guarentee the allocation.
 */
void *
mpipe_alloc_nowait(malloc_pipe_t mpipe)
{
    void *buf;
    int n;

    crit_enter();
    if ((n = mpipe->free_count) != 0) {
	/*
	 * Use a free entry if it exists.
	 */
	--n;
	buf = mpipe->array[n];
	mpipe->array[n] = NULL;	/* sanity check, not absolutely needed */
	mpipe->free_count = n;
    } else if (mpipe->total_count >= mpipe->max_count) {
	/*
	 * Return NULL if we have hit our limit
	 */
	buf = NULL;
    } else {
	/*
	 * Otherwise try to malloc() non-blocking.
	 */
	buf = malloc(mpipe->bytes, mpipe->type, M_NOWAIT | mpipe->mflags);
	if (buf)
	    ++mpipe->total_count;
    }
    crit_exit();
    return(buf);
}

/*
 * Allocate an entry, block until the allocation succeeds.  This may cause
 * us to block waiting for a prior allocation to be freed.
 */
void *
mpipe_alloc_waitok(malloc_pipe_t mpipe)
{
    void *buf;
    int n;
    int mfailed;

    crit_enter();
    mfailed = 0;
    for (;;) {
	if ((n = mpipe->free_count) != 0) {
	    /*
	     * Use a free entry if it exists.
	     */
	    --n;
	    buf = mpipe->array[n];
	    mpipe->array[n] = NULL;
	    mpipe->free_count = n;
	    break;
	}
	if (mpipe->total_count >= mpipe->max_count || mfailed) {
	    /*
	     * Block if we have hit our limit
	     */
	    mpipe->pending = 1;
	    tsleep(mpipe, 0, "mpipe1", 0);
	    continue;
	}
	/*
	 * Otherwise try to malloc() non-blocking.  If that fails loop to
	 * recheck, and block instead of trying to malloc() again.
	 */
	buf = malloc(mpipe->bytes, mpipe->type, M_NOWAIT | mpipe->mflags);
	if (buf) {
	    ++mpipe->total_count;
	    break;
	}
	mfailed = 1;
    }
    crit_exit();
    return(buf);
}

/*
 * Free an entry, unblock any waiters.  Allow NULL.
 */
void
mpipe_free(malloc_pipe_t mpipe, void *buf)
{
    int n;

    if (buf == NULL)
	return;

    crit_enter();
    if ((n = mpipe->free_count) < mpipe->ary_count) {
	/*
	 * Free slot available in free array (LIFO)
	 */
	mpipe->array[n] = buf;
	++mpipe->free_count;
	if ((mpipe->mpflags & (MPF_CACHEDATA|MPF_NOZERO)) == 0) 
	    bzero(buf, mpipe->bytes);
	crit_exit();

	/*
	 * Wakeup anyone blocked in mpipe_alloc_*().
	 */
	if (mpipe->pending) {
	    mpipe->pending = 0;
	    wakeup(mpipe);
	}
    } else {
	/*
	 * All the free slots are full, free the buffer directly.
	 */
	--mpipe->total_count;
	KKASSERT(mpipe->total_count >= mpipe->free_count);
	if (mpipe->deconstruct)
	    mpipe->deconstruct(mpipe, buf);
	crit_exit();
	free(buf, mpipe->type);
    }
}

