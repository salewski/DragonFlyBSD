/*
 * Copyright (c) 2004 The DragonFly Project.  All rights reserved.
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
 */
/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	From: @(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/kern/kern_timeout.c,v 1.59.2.1 2001/11/13 18:24:52 archie Exp $
 * $DragonFly: src/sys/kern/kern_timeout.c,v 1.14 2004/09/19 02:52:26 dillon Exp $
 */
/*
 * DRAGONFLY BGL STATUS
 *
 *	All the API functions should be MP safe.
 *
 *	The callback functions will be flagged as being MP safe if the
 *	timeout structure is initialized with callout_init_mp() instead of
 *	callout_init().
 *
 *	The helper threads cannot be made preempt-capable until after we
 *	clean up all the uses of splsoftclock() and related interlocks (which
 *	require the related functions to be MP safe as well).
 */
/*
 * The callout mechanism is based on the work of Adam M. Costello and 
 * George Varghese, published in a technical report entitled "Redesigning
 * the BSD Callout and Timer Facilities" and modified slightly for inclusion
 * in FreeBSD by Justin T. Gibbs.  The original work on the data structures
 * used in this implementation was published by G. Varghese and T. Lauck in
 * the paper "Hashed and Hierarchical Timing Wheels: Data Structures for
 * the Efficient Implementation of a Timer Facility" in the Proceedings of
 * the 11th ACM Annual Symposium on Operating Systems Principles,
 * Austin, Texas Nov 1987.
 *
 * The per-cpu augmentation was done by Matthew Dillon.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/interrupt.h>
#include <sys/thread.h>
#include <sys/thread2.h>
#include <machine/ipl.h>
#include <ddb/ddb.h>

#ifndef MAX_SOFTCLOCK_STEPS
#define MAX_SOFTCLOCK_STEPS 100 /* Maximum allowed value of steps. */
#endif


struct softclock_pcpu {
	struct callout_tailq *callwheel;
	struct callout * volatile next;
	int softticks;		/* softticks index */
	int curticks;		/* per-cpu ticks counter */
	int isrunning;
	struct thread thread;

};

typedef struct softclock_pcpu *softclock_pcpu_t;

/*
 * TODO:
 *	allocate more timeout table slots when table overflows.
 */
static MALLOC_DEFINE(M_CALLOUT, "callout", "callout structures");
static int callwheelsize;
static int callwheelbits;
static int callwheelmask;
static struct softclock_pcpu softclock_pcpu_ary[MAXCPU];

static void softclock_handler(void *arg);

static void
swi_softclock_setup(void *arg)
{
	int cpu;
	int i;

	/*
	 * Figure out how large a callwheel we need.  It must be a power of 2.
	 */
	callwheelsize = 1;
	callwheelbits = 0;
	while (callwheelsize < ncallout) {
		callwheelsize <<= 1;
		++callwheelbits;
	}
	callwheelmask = callwheelsize - 1;

	/*
	 * Initialize per-cpu data structures.
	 */
	for (cpu = 0; cpu < ncpus; ++cpu) {
		softclock_pcpu_t sc;

		sc = &softclock_pcpu_ary[cpu];

		sc->callwheel = malloc(sizeof(*sc->callwheel) * callwheelsize,
					M_CALLOUT, M_WAITOK|M_ZERO);
		for (i = 0; i < callwheelsize; ++i)
			TAILQ_INIT(&sc->callwheel[i]);

		/*
		 * Create a preemption-capable thread for each cpu to handle
		 * softclock timeouts on that cpu.  The preemption can only
		 * be blocked by a critical section.  The thread can itself
		 * be preempted by normal interrupts.
		 */
		lwkt_create(softclock_handler, sc, NULL,
			    &sc->thread, TDF_STOPREQ|TDF_INTTHREAD, -1,
			    "softclock %d", cpu);
		lwkt_setpri(&sc->thread, TDPRI_SOFT_NORM);
#if 0
		/* 
		 * Do not make the thread preemptable until we clean up all
		 * the splsoftclock() calls in the system.  Since the threads
		 * are no longer operated as a software interrupt, the 
		 * splsoftclock() calls will not have any effect on them.
		 */
		sc->thread.td_preemptable = lwkt_preempt;
#endif
	}
}

SYSINIT(softclock_setup, SI_SUB_CPU, SI_ORDER_ANY, swi_softclock_setup, NULL);

/*
 * This routine is called from the hardclock() (basically a FASTint/IPI) on
 * each cpu in the system.  sc->curticks is this cpu's notion of the timebase.
 * It IS NOT NECESSARILY SYNCHRONIZED WITH 'ticks'!  sc->softticks is where
 * the callwheel is currently indexed.
 *
 * WARNING!  The MP lock is not necessarily held on call, nor can it be
 * safely obtained.
 *
 * sc->softticks is adjusted by either this routine or our helper thread
 * depending on whether the helper thread is running or not.
 */
void
hardclock_softtick(globaldata_t gd)
{
	softclock_pcpu_t sc;

	sc = &softclock_pcpu_ary[gd->gd_cpuid];
	++sc->curticks;
	if (sc->isrunning)
		return;
	if (sc->softticks == sc->curticks) {
		/*
		 * in sync, only wakeup the thread if there is something to
		 * do.
		 */
		if (TAILQ_FIRST(&sc->callwheel[sc->softticks & callwheelmask]))
		{
			sc->isrunning = 1;
			lwkt_schedule(&sc->thread);
		} else {
			++sc->softticks;
		}
	} else {
		/*
		 * out of sync, wakeup the thread unconditionally so it can
		 * catch up.
		 */
		sc->isrunning = 1;
		lwkt_schedule(&sc->thread);
	}
}

/*
 * This procedure is the main loop of our per-cpu helper thread.  The
 * sc->isrunning flag prevents us from racing hardclock_softtick() and
 * a critical section is sufficient to interlock sc->curticks and protect
 * us from remote IPI's / list removal.
 *
 * The thread starts with the MP lock held and not in a critical section.
 * The loop itself is MP safe while individual callbacks may or may not
 * be, so we obtain or release the MP lock as appropriate.
 */
static void
softclock_handler(void *arg)
{
	softclock_pcpu_t sc;
	struct callout *c;
	struct callout_tailq *bucket;
	void (*c_func)(void *);
	void *c_arg;
#ifdef SMP
	int mpsafe = 0;
#endif

	sc = arg;
	crit_enter();
loop:
	while (sc->softticks != (int)(sc->curticks + 1)) {
		bucket = &sc->callwheel[sc->softticks & callwheelmask];

		for (c = TAILQ_FIRST(bucket); c; c = sc->next) {
			if (c->c_time != sc->softticks) {
				sc->next = TAILQ_NEXT(c, c_links.tqe);
				continue;
			}
#ifdef SMP
			if (c->c_flags & CALLOUT_MPSAFE) {
				if (mpsafe == 0) {
					mpsafe = 1;
					rel_mplock();
				}
			} else {
				/*
				 * The request might be removed while we 
				 * are waiting to get the MP lock.  If it
				 * was removed sc->next will point to the
				 * next valid request or NULL, loop up.
				 */
				if (mpsafe) {
					mpsafe = 0;
					sc->next = c;
					get_mplock();
					if (c != sc->next)
						continue;
				}
			}
#endif
			sc->next = TAILQ_NEXT(c, c_links.tqe);
			TAILQ_REMOVE(bucket, c, c_links.tqe);

			c_func = c->c_func;
			c_arg = c->c_arg;
			c->c_func = NULL;
			KKASSERT(c->c_flags & CALLOUT_DID_INIT);
			c->c_flags &= ~CALLOUT_PENDING;
			crit_exit();
			c_func(c_arg);
			crit_enter();
			/* NOTE: list may have changed */
		}
		++sc->softticks;
	}
	sc->isrunning = 0;
	lwkt_deschedule_self(&sc->thread);	/* == curthread */
	lwkt_switch();
	goto loop;
	/* NOT REACHED */
}

#if 0

/*
 * timeout --
 *	Execute a function after a specified length of time.
 *
 * untimeout --
 *	Cancel previous timeout function call.
 *
 * callout_handle_init --
 *	Initialize a handle so that using it with untimeout is benign.
 *
 *	See AT&T BCI Driver Reference Manual for specification.  This
 *	implementation differs from that one in that although an 
 *	identification value is returned from timeout, the original
 *	arguments to timeout as well as the identifier are used to
 *	identify entries for untimeout.
 */
struct callout_handle
timeout(timeout_t *ftn, void *arg, int to_ticks)
{
	softclock_pcpu_t sc;
	struct callout *new;
	struct callout_handle handle;

	sc = &softclock_pcpu_ary[mycpu->gd_cpuid];
	crit_enter();

	/* Fill in the next free callout structure. */
	new = SLIST_FIRST(&sc->callfree);
	if (new == NULL) {
		/* XXX Attempt to malloc first */
		panic("timeout table full");
	}
	SLIST_REMOVE_HEAD(&sc->callfree, c_links.sle);
	
	callout_reset(new, to_ticks, ftn, arg);

	handle.callout = new;
	crit_exit();
	return (handle);
}

void
untimeout(timeout_t *ftn, void *arg, struct callout_handle handle)
{
	/*
	 * Check for a handle that was initialized
	 * by callout_handle_init, but never used
	 * for a real timeout.
	 */
	if (handle.callout == NULL)
		return;

	crit_enter();
	if (handle.callout->c_func == ftn && handle.callout->c_arg == arg)
		callout_stop(handle.callout);
	crit_exit();
}

void
callout_handle_init(struct callout_handle *handle)
{
	handle->callout = NULL;
}

#endif

/*
 * New interface; clients allocate their own callout structures.
 *
 * callout_reset() - establish or change a timeout
 * callout_stop() - disestablish a timeout
 * callout_init() - initialize a callout structure so that it can
 *			safely be passed to callout_reset() and callout_stop()
 * callout_init_mp() - same but any installed functions must be MP safe.
 *
 * <sys/callout.h> defines three convenience macros:
 *
 * callout_active() - returns truth if callout has not been serviced
 * callout_pending() - returns truth if callout is still waiting for timeout
 * callout_deactivate() - marks the callout as having been serviced
 */

/*
 * Start or restart a timeout.  Install the callout structure in the 
 * callwheel.  Callers may legally pass any value, even if 0 or negative,
 * but since the sc->curticks index may have already been processed a
 * minimum timeout of 1 tick will be enforced.
 *
 * The callout is installed on and will be processed on the current cpu's
 * callout wheel.
 */
void
callout_reset(struct callout *c, int to_ticks, void (*ftn)(void *), 
		void *arg)
{
	softclock_pcpu_t sc;
	globaldata_t gd;

#ifdef INVARIANTS
        if ((c->c_flags & CALLOUT_DID_INIT) == 0) {
		callout_init(c);
		printf(
		    "callout_reset(%p) from %p: callout was not initialized\n",
		    c, ((int **)&c)[-1]);
#ifdef DDB
		db_print_backtrace();
#endif
	}
#endif
	gd = mycpu;
	sc = &softclock_pcpu_ary[gd->gd_cpuid];
	crit_enter_gd(gd);

	if (c->c_flags & CALLOUT_PENDING)
		callout_stop(c);

	if (to_ticks <= 0)
		to_ticks = 1;

	c->c_arg = arg;
	c->c_flags |= (CALLOUT_ACTIVE | CALLOUT_PENDING);
	c->c_func = ftn;
	c->c_time = sc->curticks + to_ticks;
#ifdef SMP
	c->c_gd = gd;
#endif

	TAILQ_INSERT_TAIL(&sc->callwheel[c->c_time & callwheelmask], 
			  c, c_links.tqe);
	crit_exit_gd(gd);
}

/*
 * Stop a running timer.  WARNING!  If called on a cpu other then the one
 * the callout was started on this function will liveloop on its IPI to
 * the target cpu to process the request.  It is possible for the callout
 * to execute in that case.
 *
 * WARNING! This routine may be called from an IPI
 */
int
callout_stop(struct callout *c)
{
	globaldata_t gd = mycpu;
#ifdef SMP
	globaldata_t tgd;
#endif
	softclock_pcpu_t sc;

#ifdef INVARIANTS
        if ((c->c_flags & CALLOUT_DID_INIT) == 0) {
		callout_init(c);
		printf(
		    "callout_reset(%p) from %p: callout was not initialized\n",
		    c, ((int **)&c)[-1]);
#ifdef DDB
		db_print_backtrace();
#endif
	}
#endif
	crit_enter_gd(gd);

	/*
	 * Don't attempt to delete a callout that's not on the queue.
	 */
	if ((c->c_flags & CALLOUT_PENDING) == 0) {
		c->c_flags &= ~CALLOUT_ACTIVE;
		crit_exit_gd(gd);
		return (0);
	}
#ifdef SMP
	if ((tgd = c->c_gd) != gd) {
		/*
		 * If the callout is owned by a different CPU we have to
		 * execute the function synchronously on the target cpu.
		 */
		int seq;

		cpu_mb1();	/* don't let tgd alias c_gd */
		seq = lwkt_send_ipiq(tgd, (void *)callout_stop, c);
		lwkt_wait_ipiq(tgd, seq);
	} else 
#endif
	{
		/*
		 * If the callout is owned by the same CPU we can
		 * process it directly, but if we are racing our helper
		 * thread (sc->next), we have to adjust sc->next.  The
		 * race is interlocked by a critical section.
		 */
		sc = &softclock_pcpu_ary[gd->gd_cpuid];

		c->c_flags &= ~(CALLOUT_ACTIVE | CALLOUT_PENDING);
		if (sc->next == c)
			sc->next = TAILQ_NEXT(c, c_links.tqe);

		TAILQ_REMOVE(&sc->callwheel[c->c_time & callwheelmask], 
				c, c_links.tqe);
		c->c_func = NULL;
	}
	crit_exit_gd(gd);
	return (1);
}

/*
 * Prepare a callout structure for use by callout_reset() and/or 
 * callout_stop().  The MP version of this routine requires that the callback
 * function installed by callout_reset() by MP safe.
 */
void
callout_init(struct callout *c)
{
	bzero(c, sizeof *c);
	c->c_flags = CALLOUT_DID_INIT;
}

void
callout_init_mp(struct callout *c)
{
	callout_init(c);
	c->c_flags |= CALLOUT_MPSAFE;
}

/* What, are you joking?  This is nuts! -Matt */
#if 0
#ifdef APM_FIXUP_CALLTODO
/* 
 * Adjust the kernel calltodo timeout list.  This routine is used after 
 * an APM resume to recalculate the calltodo timer list values with the 
 * number of hz's we have been sleeping.  The next hardclock() will detect 
 * that there are fired timers and run softclock() to execute them.
 *
 * Please note, I have not done an exhaustive analysis of what code this
 * might break.  I am motivated to have my select()'s and alarm()'s that
 * have expired during suspend firing upon resume so that the applications
 * which set the timer can do the maintanence the timer was for as close
 * as possible to the originally intended time.  Testing this code for a 
 * week showed that resuming from a suspend resulted in 22 to 25 timers 
 * firing, which seemed independant on whether the suspend was 2 hours or
 * 2 days.  Your milage may vary.   - Ken Key <key@cs.utk.edu>
 */
void
adjust_timeout_calltodo(struct timeval *time_change)
{
	struct callout *p;
	unsigned long delta_ticks;
	int s;

	/* 
	 * How many ticks were we asleep?
	 * (stolen from tvtohz()).
	 */

	/* Don't do anything */
	if (time_change->tv_sec < 0)
		return;
	else if (time_change->tv_sec <= LONG_MAX / 1000000)
		delta_ticks = (time_change->tv_sec * 1000000 +
			       time_change->tv_usec + (tick - 1)) / tick + 1;
	else if (time_change->tv_sec <= LONG_MAX / hz)
		delta_ticks = time_change->tv_sec * hz +
			      (time_change->tv_usec + (tick - 1)) / tick + 1;
	else
		delta_ticks = LONG_MAX;

	if (delta_ticks > INT_MAX)
		delta_ticks = INT_MAX;

	/* 
	 * Now rip through the timer calltodo list looking for timers
	 * to expire.
	 */

	/* don't collide with softclock() */
	s = splhigh(); 
	for (p = calltodo.c_next; p != NULL; p = p->c_next) {
		p->c_time -= delta_ticks;

		/* Break if the timer had more time on it than delta_ticks */
		if (p->c_time > 0)
			break;

		/* take back the ticks the timer didn't use (p->c_time <= 0) */
		delta_ticks = -p->c_time;
	}
	splx(s);

	return;
}
#endif /* APM_FIXUP_CALLTODO */
#endif

