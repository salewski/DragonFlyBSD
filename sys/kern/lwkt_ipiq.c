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
 * $DragonFly: src/sys/kern/lwkt_ipiq.c,v 1.8.2.1 2005/07/28 05:53:12 dillon Exp $
 */

/*
 * This module implements IPI message queueing and the MI portion of IPI
 * message processing.
 */

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/rtprio.h>
#include <sys/queue.h>
#include <sys/thread2.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <machine/cpu.h>
#include <sys/lock.h>
#include <sys/caps.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

#include <machine/stdarg.h>
#include <machine/ipl.h>
#include <machine/smp.h>
#include <machine/atomic.h>

#define THREAD_STACK	(UPAGES * PAGE_SIZE)

#else

#include <sys/stdint.h>
#include <libcaps/thread.h>
#include <sys/thread.h>
#include <sys/msgport.h>
#include <sys/errno.h>
#include <libcaps/globaldata.h>
#include <machine/cpufunc.h>
#include <sys/thread2.h>
#include <sys/msgport2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <machine/lock.h>
#include <machine/cpu.h>
#include <machine/atomic.h>

#endif

#ifdef SMP
static __int64_t ipiq_count;
static __int64_t ipiq_fifofull;
static __int64_t ipiq_cscount;
#endif

#ifdef _KERNEL

#ifdef SMP
SYSCTL_QUAD(_lwkt, OID_AUTO, ipiq_count, CTLFLAG_RW, &ipiq_count, 0, "");
SYSCTL_QUAD(_lwkt, OID_AUTO, ipiq_fifofull, CTLFLAG_RW, &ipiq_fifofull, 0, "");
SYSCTL_QUAD(_lwkt, OID_AUTO, ipiq_cscount, CTLFLAG_RW, &ipiq_cscount, 0, "");
#endif

#endif

#ifdef SMP

static int lwkt_process_ipiq1(lwkt_ipiq_t ip, struct intrframe *frame);
static void lwkt_cpusync_remote1(lwkt_cpusync_t poll);
static void lwkt_cpusync_remote2(lwkt_cpusync_t poll);

/*
 * Send a function execution request to another cpu.  The request is queued
 * on the cpu<->cpu ipiq matrix.  Each cpu owns a unique ipiq FIFO for every
 * possible target cpu.  The FIFO can be written.
 *
 * YYY If the FIFO fills up we have to enable interrupts and process the
 * IPIQ while waiting for it to empty or we may deadlock with another cpu.
 * Create a CPU_*() function to do this!
 *
 * We can safely bump gd_intr_nesting_level because our crit_exit() at the
 * end will take care of any pending interrupts.
 *
 * Must be called from a critical section.
 */
int
lwkt_send_ipiq(globaldata_t target, ipifunc_t func, void *arg)
{
    lwkt_ipiq_t ip;
    int windex;
    struct globaldata *gd = mycpu;

    if (target == gd) {
	func(arg);
	return(0);
    } 
    crit_enter();
    ++gd->gd_intr_nesting_level;
#ifdef INVARIANTS
    if (gd->gd_intr_nesting_level > 20)
	panic("lwkt_send_ipiq: TOO HEAVILY NESTED!");
#endif
    KKASSERT(curthread->td_pri >= TDPRI_CRIT);
    ++ipiq_count;
    ip = &gd->gd_ipiq[target->gd_cpuid];

    /*
     * We always drain before the FIFO becomes full so it should never
     * become full.  We need to leave enough entries to deal with 
     * reentrancy.
     */
    KKASSERT(ip->ip_windex - ip->ip_rindex != MAXCPUFIFO);
    windex = ip->ip_windex & MAXCPUFIFO_MASK;
    ip->ip_func[windex] = (ipifunc2_t)func;
    ip->ip_arg[windex] = arg;
    cpu_mb1();
    ++ip->ip_windex;
    if (ip->ip_windex - ip->ip_rindex > MAXCPUFIFO / 2) {
	unsigned int eflags = read_eflags();
	cpu_enable_intr();
	++ipiq_fifofull;
	while (ip->ip_windex - ip->ip_rindex > MAXCPUFIFO / 4) {
	    KKASSERT(ip->ip_windex - ip->ip_rindex != MAXCPUFIFO - 1);
	    lwkt_process_ipiq();
	}
	write_eflags(eflags);
    }
    --gd->gd_intr_nesting_level;
    cpu_send_ipiq(target->gd_cpuid);	/* issues mem barrier if appropriate */
    crit_exit();
    return(ip->ip_windex);
}

/*
 * Send an IPI request passively, return 0 on success and ENOENT on failure.
 * This routine does not recursive through lwkt_process_ipiq() nor does it
 * block trying to queue the actual IPI.  If we successfully queue the
 * message but fail to queue the IPI, we still count it as a success.
 * The occassional small race against a target cpu HLT is recovered at
 * the next clock interrupt.
 */
int
lwkt_send_ipiq_passive(globaldata_t target, ipifunc_t func, void *arg)
{
    lwkt_ipiq_t ip;
    int windex;
    struct globaldata *gd = mycpu;

    KKASSERT(curthread->td_pri >= TDPRI_CRIT);
    if (target == gd) {
	func(arg);
	return(0);
    } 
    ++ipiq_count;
    ip = &gd->gd_ipiq[target->gd_cpuid];

    if (ip->ip_windex - ip->ip_rindex >= MAXCPUFIFO - 1) {
	return(ENOENT);
    }
    windex = ip->ip_windex & MAXCPUFIFO_MASK;
    ip->ip_func[windex] = (ipifunc2_t)func;
    ip->ip_arg[windex] = arg;
    cpu_mb1();
    ++ip->ip_windex;
    /*
     * passive mode doesn't work yet :-( 
     */
#if 1
    cpu_send_ipiq(target->gd_cpuid);
#else
    cpu_send_ipiq_passive(target->gd_cpuid);
#endif
    return(0);
}

/*
 * deprecated, used only by fast int forwarding.
 */
int
lwkt_send_ipiq_bycpu(int dcpu, ipifunc_t func, void *arg)
{
    return(lwkt_send_ipiq(globaldata_find(dcpu), func, arg));
}

/*
 * Send a message to several target cpus.  Typically used for scheduling.
 * The message will not be sent to stopped cpus.
 */
int
lwkt_send_ipiq_mask(u_int32_t mask, ipifunc_t func, void *arg)
{
    int cpuid;
    int count = 0;

    mask &= ~stopped_cpus;
    while (mask) {
	cpuid = bsfl(mask);
	lwkt_send_ipiq(globaldata_find(cpuid), func, arg);
	mask &= ~(1 << cpuid);
	++count;
    }
    return(count);
}

/*
 * Wait for the remote cpu to finish processing a function.
 *
 * YYY we have to enable interrupts and process the IPIQ while waiting
 * for it to empty or we may deadlock with another cpu.  Create a CPU_*()
 * function to do this!  YYY we really should 'block' here.
 *
 * MUST be called from a critical section.  This routine may be called
 * from an interrupt (for example, if an interrupt wakes a foreign thread
 * up).
 */
void
lwkt_wait_ipiq(globaldata_t target, int seq)
{
    lwkt_ipiq_t ip;
    int maxc = 100000000;

    if (target != mycpu) {
	ip = &mycpu->gd_ipiq[target->gd_cpuid];
	if ((int)(ip->ip_xindex - seq) < 0) {
	    unsigned int eflags = read_eflags();
	    cpu_enable_intr();
	    while ((int)(ip->ip_xindex - seq) < 0) {
		crit_enter();
		lwkt_process_ipiq();
		crit_exit();
		if (--maxc == 0)
			printf("LWKT_WAIT_IPIQ WARNING! %d wait %d (%d)\n", mycpu->gd_cpuid, target->gd_cpuid, ip->ip_xindex - seq);
		if (maxc < -1000000)
			panic("LWKT_WAIT_IPIQ");
	    }
	    write_eflags(eflags);
	}
    }
}

int
lwkt_seq_ipiq(globaldata_t target)
{
    lwkt_ipiq_t ip;

    ip = &mycpu->gd_ipiq[target->gd_cpuid];
    return(ip->ip_windex);
}

/*
 * Called from IPI interrupt (like a fast interrupt), which has placed
 * us in a critical section.  The MP lock may or may not be held.
 * May also be called from doreti or splz, or be reentrantly called
 * indirectly through the ip_func[] we run.
 *
 * There are two versions, one where no interrupt frame is available (when
 * called from the send code and from splz, and one where an interrupt
 * frame is available.
 */
void
lwkt_process_ipiq(void)
{
    globaldata_t gd = mycpu;
    lwkt_ipiq_t ip;
    int n;

again:
    for (n = 0; n < ncpus; ++n) {
	if (n != gd->gd_cpuid) {
	    ip = globaldata_find(n)->gd_ipiq;
	    if (ip != NULL) {
		while (lwkt_process_ipiq1(&ip[gd->gd_cpuid], NULL))
		    ;
	    }
	}
    }
    if (gd->gd_cpusyncq.ip_rindex != gd->gd_cpusyncq.ip_windex) {
	if (lwkt_process_ipiq1(&gd->gd_cpusyncq, NULL)) {
	    if (gd->gd_curthread->td_cscount == 0)
		goto again;
	    need_ipiq();
	}
    }
}

#ifdef _KERNEL
void
lwkt_process_ipiq_frame(struct intrframe frame)
{
    globaldata_t gd = mycpu;
    lwkt_ipiq_t ip;
    int n;

again:
    for (n = 0; n < ncpus; ++n) {
	if (n != gd->gd_cpuid) {
	    ip = globaldata_find(n)->gd_ipiq;
	    if (ip != NULL) {
		while (lwkt_process_ipiq1(&ip[gd->gd_cpuid], &frame))
		    ;
	    }
	}
    }
    if (gd->gd_cpusyncq.ip_rindex != gd->gd_cpusyncq.ip_windex) {
	if (lwkt_process_ipiq1(&gd->gd_cpusyncq, &frame)) {
	    if (gd->gd_curthread->td_cscount == 0)
		goto again;
	    need_ipiq();
	}
    }
}
#endif

static int
lwkt_process_ipiq1(lwkt_ipiq_t ip, struct intrframe *frame)
{
    int ri;
    int wi = ip->ip_windex;
    /*
     * Note: xindex is only updated after we are sure the function has
     * finished execution.  Beware lwkt_process_ipiq() reentrancy!  The
     * function may send an IPI which may block/drain.
     *
     * Note: due to additional IPI operations that the callback function
     * may make, it is possible for both rindex and windex to advance and
     * thus for rindex to advance passed our cached windex.
     */
    while (wi - (ri = ip->ip_rindex) > 0) {
	ip->ip_rindex = ri + 1;
	ri &= MAXCPUFIFO_MASK;
	ip->ip_func[ri](ip->ip_arg[ri], frame);
	/* YYY memory barrier */
	ip->ip_xindex = ip->ip_rindex;
    }
    return(wi != ip->ip_windex);
}

#else

/*
 * !SMP dummy routines
 */

int
lwkt_send_ipiq(globaldata_t target, ipifunc_t func, void *arg)
{
    panic("lwkt_send_ipiq: UP box! (%d,%p,%p)", target->gd_cpuid, func, arg);
    return(0); /* NOT REACHED */
}

void
lwkt_wait_ipiq(globaldata_t target, int seq)
{
    panic("lwkt_wait_ipiq: UP box! (%d,%d)", target->gd_cpuid, seq);
}

#endif

/*
 * CPU Synchronization Support
 *
 * lwkt_cpusync_simple()
 *
 *	The function is executed synchronously before return on remote cpus.
 *	A lwkt_cpusync_t pointer is passed as an argument.  The data can
 *	be accessed via arg->cs_data.
 *
 *	XXX should I just pass the data as an argument to be consistent?
 */

void
lwkt_cpusync_simple(cpumask_t mask, cpusync_func_t func, void *data)
{
    struct lwkt_cpusync cmd;

    cmd.cs_run_func = NULL;
    cmd.cs_fin1_func = func;
    cmd.cs_fin2_func = NULL;
    cmd.cs_data = data;
    lwkt_cpusync_start(mask & mycpu->gd_other_cpus, &cmd);
    if (mask & (1 << mycpu->gd_cpuid))
	func(&cmd);
    lwkt_cpusync_finish(&cmd);
}

/*
 * lwkt_cpusync_fastdata()
 *
 *	The function is executed in tandem with return on remote cpus.
 *	The data is directly passed as an argument.  Do not pass pointers to
 *	temporary storage as the storage might have
 *	gone poof by the time the target cpu executes
 *	the function.
 *
 *	At the moment lwkt_cpusync is declared on the stack and we must wait
 *	for all remote cpus to ack in lwkt_cpusync_finish(), but as a future
 *	optimization we should be able to put a counter in the globaldata
 *	structure (if it is not otherwise being used) and just poke it and
 *	return without waiting. XXX
 */
void
lwkt_cpusync_fastdata(cpumask_t mask, cpusync_func2_t func, void *data)
{
    struct lwkt_cpusync cmd;

    cmd.cs_run_func = NULL;
    cmd.cs_fin1_func = NULL;
    cmd.cs_fin2_func = func;
    cmd.cs_data = NULL;
    lwkt_cpusync_start(mask & mycpu->gd_other_cpus, &cmd);
    if (mask & (1 << mycpu->gd_cpuid))
	func(data);
    lwkt_cpusync_finish(&cmd);
}

/*
 * lwkt_cpusync_start()
 *
 *	Start synchronization with a set of target cpus, return once they are
 *	known to be in a synchronization loop.  The target cpus will execute
 *	poll->cs_run_func() IN TANDEM WITH THE RETURN.
 *
 *	XXX future: add lwkt_cpusync_start_quick() and require a call to
 *	lwkt_cpusync_add() or lwkt_cpusync_wait(), allowing the caller to
 *	potentially absorb the IPI latency doing something useful.
 */
void
lwkt_cpusync_start(cpumask_t mask, lwkt_cpusync_t poll)
{
    globaldata_t gd = mycpu;

    poll->cs_count = 0;
    poll->cs_mask = mask;
#ifdef SMP
    poll->cs_maxcount = lwkt_send_ipiq_mask(
		mask & gd->gd_other_cpus & smp_active_mask,
		(ipifunc_t)lwkt_cpusync_remote1, poll);
#endif
    if (mask & gd->gd_cpumask) {
	if (poll->cs_run_func)
	    poll->cs_run_func(poll);
    }
#ifdef SMP
    if (poll->cs_maxcount) {
	++ipiq_cscount;
	++gd->gd_curthread->td_cscount;
	while (poll->cs_count != poll->cs_maxcount) {
	    crit_enter();
	    lwkt_process_ipiq();
	    crit_exit();
	}
    }
#endif
}

void
lwkt_cpusync_add(cpumask_t mask, lwkt_cpusync_t poll)
{
    globaldata_t gd = mycpu;
#ifdef SMP
    int count;
#endif

    mask &= ~poll->cs_mask;
    poll->cs_mask |= mask;
#ifdef SMP
    count = lwkt_send_ipiq_mask(
		mask & gd->gd_other_cpus & smp_active_mask,
		(ipifunc_t)lwkt_cpusync_remote1, poll);
#endif
    if (mask & gd->gd_cpumask) {
	if (poll->cs_run_func)
	    poll->cs_run_func(poll);
    }
#ifdef SMP
    poll->cs_maxcount += count;
    if (poll->cs_maxcount) {
	if (poll->cs_maxcount == count)
	    ++gd->gd_curthread->td_cscount;
	while (poll->cs_count != poll->cs_maxcount) {
	    crit_enter();
	    lwkt_process_ipiq();
	    crit_exit();
	}
    }
#endif
}

/*
 * Finish synchronization with a set of target cpus.  The target cpus will
 * execute cs_fin1_func(poll) prior to this function returning, and will
 * execute cs_fin2_func(data) IN TANDEM WITH THIS FUNCTION'S RETURN.
 *
 * If cs_maxcount is non-zero then we are mastering a cpusync with one or
 * more remote cpus and must account for it in our thread structure.
 */
void
lwkt_cpusync_finish(lwkt_cpusync_t poll)
{
    globaldata_t gd = mycpu;

    poll->cs_count = -1;
    if (poll->cs_mask & gd->gd_cpumask) {
	if (poll->cs_fin1_func)
	    poll->cs_fin1_func(poll);
	if (poll->cs_fin2_func)
	    poll->cs_fin2_func(poll->cs_data);
    }
#ifdef SMP
    if (poll->cs_maxcount) {
	while (poll->cs_count != -(poll->cs_maxcount + 1)) {
	    crit_enter();
	    lwkt_process_ipiq();
	    crit_exit();
	}
	--gd->gd_curthread->td_cscount;
    }
#endif
}

#ifdef SMP

/*
 * helper IPI remote messaging function.
 * 
 * Called on remote cpu when a new cpu synchronization request has been
 * sent to us.  Execute the run function and adjust cs_count, then requeue
 * the request so we spin on it.
 */
static void
lwkt_cpusync_remote1(lwkt_cpusync_t poll)
{
    atomic_add_int(&poll->cs_count, 1);
    if (poll->cs_run_func)
	poll->cs_run_func(poll);
    lwkt_cpusync_remote2(poll);
}

/*
 * helper IPI remote messaging function.
 *
 * Poll for the originator telling us to finish.  If it hasn't, requeue
 * our request so we spin on it.  When the originator requests that we
 * finish we execute cs_fin1_func(poll) synchronously and cs_fin2_func(data)
 * in tandem with the release.
 */
static void
lwkt_cpusync_remote2(lwkt_cpusync_t poll)
{
    if (poll->cs_count < 0) {
	cpusync_func2_t savef;
	void *saved;

	if (poll->cs_fin1_func)
	    poll->cs_fin1_func(poll);
	if (poll->cs_fin2_func) {
	    savef = poll->cs_fin2_func;
	    saved = poll->cs_data;
	    atomic_add_int(&poll->cs_count, -1);
	    savef(saved);
	} else {
	    atomic_add_int(&poll->cs_count, -1);
	}
    } else {
	globaldata_t gd = mycpu;
	lwkt_ipiq_t ip;
	int wi;

	ip = &gd->gd_cpusyncq;
	wi = ip->ip_windex & MAXCPUFIFO_MASK;
	ip->ip_func[wi] = (ipifunc2_t)lwkt_cpusync_remote2;
	ip->ip_arg[wi] = poll;
	++ip->ip_windex;
    }
}

#endif
