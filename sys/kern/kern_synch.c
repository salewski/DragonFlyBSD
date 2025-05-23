/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
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
 *	@(#)kern_synch.c	8.9 (Berkeley) 5/19/95
 * $FreeBSD: src/sys/kern/kern_synch.c,v 1.87.2.6 2002/10/13 07:29:53 kbyanc Exp $
 * $DragonFly: src/sys/kern/kern_synch.c,v 1.41 2005/01/14 02:20:22 dillon Exp $
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <sys/thread2.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif
#include <sys/xwait.h>

#include <machine/cpu.h>
#include <machine/ipl.h>
#include <machine/smp.h>

static void sched_setup (void *dummy);
SYSINIT(sched_setup, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, sched_setup, NULL)

int	hogticks;
int	lbolt;
int	sched_quantum;		/* Roundrobin scheduling quantum in ticks. */
int	ncpus;
int	ncpus2, ncpus2_shift, ncpus2_mask;

static struct callout loadav_callout;
static struct callout roundrobin_callout;
static struct callout schedcpu_callout;

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */
/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

static void	endtsleep (void *);
static void	loadav (void *arg);
static void	roundrobin (void *arg);
static void	schedcpu (void *arg);
static void	updatepri (struct proc *p);
static void	crit_panicints(void);

static int
sysctl_kern_quantum(SYSCTL_HANDLER_ARGS)
{
	int error, new_val;

	new_val = sched_quantum * tick;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
        if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val < tick)
		return (EINVAL);
	sched_quantum = new_val / tick;
	hogticks = 2 * sched_quantum;
	return (0);
}

SYSCTL_PROC(_kern, OID_AUTO, quantum, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof sched_quantum, sysctl_kern_quantum, "I", "");

int 
roundrobin_interval(void)
{
	return (sched_quantum);
}

/*
 * Force switch among equal priority processes every 100ms. 
 *
 * WARNING!  The MP lock is not held on ipi message remotes.
 */
#ifdef SMP

static void
roundrobin_remote(void *arg)
{
	struct proc *p = lwkt_preempted_proc();
 	if (p == NULL || RTP_PRIO_NEED_RR(p->p_rtprio.type))
		need_user_resched();
}

#endif

static void
roundrobin(void *arg)
{
	struct proc *p = lwkt_preempted_proc();
 	if (p == NULL || RTP_PRIO_NEED_RR(p->p_rtprio.type))
		need_user_resched();
#ifdef SMP
	lwkt_send_ipiq_mask(mycpu->gd_other_cpus, roundrobin_remote, NULL);
#endif
 	callout_reset(&roundrobin_callout, sched_quantum, roundrobin, NULL);
}

#ifdef SMP

void
resched_cpus(u_int32_t mask)
{
	lwkt_send_ipiq_mask(mask, roundrobin_remote, NULL);
}

#endif

/*
 * The load average is scaled by FSCALE (2048 typ).  The estimated cpu is
 * incremented at a rate of ESTCPUVFREQ per second (40hz typ), but this is
 * divided up across all cpu bound processes running in the system so an
 * individual process will get less under load.  ESTCPULIM typicaly caps
 * out at ESTCPUMAX (around 376, or 11 nice levels).
 *
 * Generally speaking the decay equation needs to break-even on growth
 * at the limit at all load levels >= 1.0, so if the estimated cpu for
 * a process increases by (ESTVCPUFREQ / load) per second, then the decay
 * should reach this value when estcpu reaches ESTCPUMAX.  That calculation
 * is:
 *
 *	ESTCPUMAX * decay = ESTCPUVFREQ / load
 *	decay = ESTCPUVFREQ / (load * ESTCPUMAX)
 *	decay = estcpu * 0.053 / load
 *
 * If the load is less then 1.0 we assume a load of 1.0.
 */

#define cload(loadav)	((loadav) < FSCALE ? FSCALE : (loadav))
#define decay_cpu(loadav,estcpu)	\
    ((estcpu) * (FSCALE * ESTCPUVFREQ / ESTCPUMAX) / cload(loadav))

/* decay 95% of `p_pctcpu' in 60 seconds; see CCPU_SHIFT before changing */
static fixpt_t	ccpu = 0.95122942450071400909 * FSCALE;	/* exp(-1/20) */
SYSCTL_INT(_kern, OID_AUTO, ccpu, CTLFLAG_RD, &ccpu, 0, "");

/* kernel uses `FSCALE', userland (SHOULD) use kern.fscale */
static int	fscale __unused = FSCALE;
SYSCTL_INT(_kern, OID_AUTO, fscale, CTLFLAG_RD, 0, FSCALE, "");

/*
 * If `ccpu' is not equal to `exp(-1/20)' and you still want to use the
 * faster/more-accurate formula, you'll have to estimate CCPU_SHIFT below
 * and possibly adjust FSHIFT in "param.h" so that (FSHIFT >= CCPU_SHIFT).
 *
 * To estimate CCPU_SHIFT for exp(-1/20), the following formula was used:
 *	1 - exp(-1/20) ~= 0.0487 ~= 0.0488 == 1 (fixed pt, *11* bits).
 *
 * If you don't want to bother with the faster/more-accurate formula, you
 * can set CCPU_SHIFT to (FSHIFT + 1) which will use a slower/less-accurate
 * (more general) method of calculating the %age of CPU used by a process.
 */
#define	CCPU_SHIFT	11

/*
 * Recompute process priorities, once a second.
 */
/* ARGSUSED */
static void
schedcpu(void *arg)
{
	fixpt_t loadfac = averunnable.ldavg[0];
	struct proc *p;
	int s;
	unsigned int ndecay;

	FOREACH_PROC_IN_SYSTEM(p) {
		/*
		 * Increment time in/out of memory and sleep time
		 * (if sleeping).  We ignore overflow; with 16-bit int's
		 * (remember them?) overflow takes 45 days.
		 */
		p->p_swtime++;
		if (p->p_stat == SSLEEP || p->p_stat == SSTOP)
			p->p_slptime++;
		p->p_pctcpu = (p->p_pctcpu * ccpu) >> FSHIFT;

		/*
		 * If the process has slept the entire second,
		 * stop recalculating its priority until it wakes up.
		 *
		 * Note that interactive calculations do not occur for
		 * long sleeps (because that isn't necessarily indicative
		 * of an interactive process).
		 */
		if (p->p_slptime > 1)
			continue;
		/* prevent state changes and protect run queue */
		s = splhigh();
		/*
		 * p_cpticks runs at ESTCPUFREQ but must be divided by the
		 * load average for par-100% use.  Higher p_interactive
		 * values mean less interactive, lower values mean more 
		 * interactive.
		 */
		if ((((fixpt_t)p->p_cpticks * cload(loadfac)) >> FSHIFT)  >
		    ESTCPUFREQ / 4) {
			if (p->p_interactive < 127)
				++p->p_interactive;
		} else {
			if (p->p_interactive > -127)
				--p->p_interactive;
		}
		/*
		 * p_pctcpu is only for ps.
		 */
#if	(FSHIFT >= CCPU_SHIFT)
		p->p_pctcpu += (ESTCPUFREQ == 100)?
			((fixpt_t) p->p_cpticks) << (FSHIFT - CCPU_SHIFT):
                	100 * (((fixpt_t) p->p_cpticks)
				<< (FSHIFT - CCPU_SHIFT)) / ESTCPUFREQ;
#else
		p->p_pctcpu += ((FSCALE - ccpu) *
			(p->p_cpticks * FSCALE / ESTCPUFREQ)) >> FSHIFT;
#endif
		p->p_cpticks = 0;
		ndecay = decay_cpu(loadfac, p->p_estcpu);
		if (p->p_estcpu > ndecay)
			p->p_estcpu -= ndecay;
		else
			p->p_estcpu = 0;
		resetpriority(p);
		splx(s);
	}
	wakeup((caddr_t)&lbolt);
	callout_reset(&schedcpu_callout, hz, schedcpu, NULL);
}

/*
 * Recalculate the priority of a process after it has slept for a while.
 * For all load averages >= 1 and max p_estcpu of 255, sleeping for at
 * least six times the loadfactor will decay p_estcpu to zero.
 */
static void
updatepri(struct proc *p)
{
	unsigned int ndecay;

	ndecay = decay_cpu(averunnable.ldavg[0], p->p_estcpu) * p->p_slptime;
	if (p->p_estcpu > ndecay)
		p->p_estcpu -= ndecay;
	else
		p->p_estcpu = 0;
	resetpriority(p);
}

/*
 * We're only looking at 7 bits of the address; everything is
 * aligned to 4, lots of things are aligned to greater powers
 * of 2.  Shift right by 8, i.e. drop the bottom 256 worth.
 */
#define TABLESIZE	128
static TAILQ_HEAD(slpquehead, thread) slpque[TABLESIZE];
#define LOOKUP(x)	(((intptr_t)(x) >> 8) & (TABLESIZE - 1))

/*
 * During autoconfiguration or after a panic, a sleep will simply
 * lower the priority briefly to allow interrupts, then return.
 * The priority to be used (safepri) is machine-dependent, thus this
 * value is initialized and maintained in the machine-dependent layers.
 * This priority will typically be 0, or the lowest priority
 * that is safe for use on the interrupt stack; it can be made
 * higher to block network software interrupts after panics.
 */
int safepri;

void
sleepinit(void)
{
	int i;

	sched_quantum = hz/10;
	hogticks = 2 * sched_quantum;
	for (i = 0; i < TABLESIZE; i++)
		TAILQ_INIT(&slpque[i]);
}

/*
 * General sleep call.  Suspends the current process until a wakeup is
 * performed on the specified identifier.  The process will then be made
 * runnable with the specified priority.  Sleeps at most timo/hz seconds
 * (0 means no timeout).  If flags includes PCATCH flag, signals are checked
 * before and after sleeping, else signals are not checked.  Returns 0 if
 * awakened, EWOULDBLOCK if the timeout expires.  If PCATCH is set and a
 * signal needs to be delivered, ERESTART is returned if the current system
 * call should be restarted if possible, and EINTR is returned if the system
 * call should be interrupted by the signal (return EINTR).
 *
 * Note that if we are a process, we release_curproc() before messing with
 * the LWKT scheduler.
 */
int
tsleep(void *ident, int flags, const char *wmesg, int timo)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;		/* may be NULL */
	int sig = 0, catch = flags & PCATCH;
	int id = LOOKUP(ident);
	struct callout thandle;

	/*
	 * NOTE: removed KTRPOINT, it could cause races due to blocking
	 * even in stable.  Just scrap it for now.
	 */
	if (cold || panicstr) {
		/*
		 * After a panic, or during autoconfiguration,
		 * just give interrupts a chance, then just return;
		 * don't run any other procs or panic below,
		 * in case this is the idle process and already asleep.
		 */
		crit_panicints();
		return (0);
	}
	KKASSERT(td != &mycpu->gd_idlethread);	/* you must be kidding! */
	crit_enter_quick(td);
	KASSERT(ident != NULL, ("tsleep: no ident"));
	KASSERT(p == NULL || p->p_stat == SRUN, ("tsleep %p %s %d",
		ident, wmesg, p->p_stat));

	td->td_wchan = ident;
	td->td_wmesg = wmesg;
	td->td_wdomain = flags & PDOMAIN_MASK;
	if (p) {
		if (flags & PNORESCHED)
			td->td_flags |= TDF_NORESCHED;
		release_curproc(p);
		p->p_slptime = 0;
	}
	lwkt_deschedule_self(td);
	TAILQ_INSERT_TAIL(&slpque[id], td, td_threadq);
	if (timo) {
		callout_init(&thandle);
		callout_reset(&thandle, timo, endtsleep, td);
	}
	/*
	 * We put ourselves on the sleep queue and start our timeout
	 * before calling CURSIG, as we could stop there, and a wakeup
	 * or a SIGCONT (or both) could occur while we were stopped.
	 * A SIGCONT would cause us to be marked as SSLEEP
	 * without resuming us, thus we must be ready for sleep
	 * when CURSIG is called.  If the wakeup happens while we're
	 * stopped, td->td_wchan will be 0 upon return from CURSIG.
	 */
	if (p) {
		if (catch) {
			p->p_flag |= P_SINTR;
			if ((sig = CURSIG(p))) {
				if (td->td_wchan) {
					unsleep(td);
					lwkt_schedule_self(td);
				}
				p->p_stat = SRUN;
				goto resume;
			}
			if (td->td_wchan == NULL) {
				catch = 0;
				goto resume;
			}
		} else {
			sig = 0;
		}

		/*
		 * If we are not the current process we have to remove ourself
		 * from the run queue.
		 */
		KASSERT(p->p_stat == SRUN, ("PSTAT NOT SRUN %d %d", p->p_pid, p->p_stat));
		/*
		 * If this is the current 'user' process schedule another one.
		 */
		clrrunnable(p, SSLEEP);
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch(p);
		KASSERT(p->p_stat == SRUN, ("tsleep: stat not srun"));
	} else {
		lwkt_switch();
	}
resume:
	if (p)
		p->p_flag &= ~P_SINTR;
	crit_exit_quick(td);
	td->td_flags &= ~TDF_NORESCHED;
	if (td->td_flags & TDF_TIMEOUT) {
		td->td_flags &= ~TDF_TIMEOUT;
		if (sig == 0)
			return (EWOULDBLOCK);
	} else if (timo) {
		callout_stop(&thandle);
	} else if (td->td_wmesg) {
		/*
		 * This can happen if a thread is woken up directly.  Clear
		 * wmesg to avoid debugging confusion.
		 */
		td->td_wmesg = NULL;
	}
	/* inline of iscaught() */
	if (p) {
		if (catch && (sig != 0 || (sig = CURSIG(p)))) {
			if (SIGISMEMBER(p->p_sigacts->ps_sigintr, sig))
				return (EINTR);
			return (ERESTART);
		}
	}
	return (0);
}

/*
 * Implement the timeout for tsleep.  We interlock against
 * wchan when setting TDF_TIMEOUT.  For processes we remove
 * the sleep if the process is stopped rather then sleeping,
 * so it remains stopped.
 */
static void
endtsleep(void *arg)
{
	thread_t td = arg;
	struct proc *p;

	crit_enter();
	if (td->td_wchan) {
		td->td_flags |= TDF_TIMEOUT;
		if ((p = td->td_proc) != NULL) {
			if (p->p_stat == SSLEEP)
				setrunnable(p);
			else
				unsleep(td);
		} else {
			unsleep(td);
			lwkt_schedule(td);
		}
	}
	crit_exit();
}

/*
 * Remove a process from its wait queue
 */
void
unsleep(struct thread *td)
{
	crit_enter();
	if (td->td_wchan) {
#if 0
		if (p->p_flag & P_XSLEEP) {
			struct xwait *w = p->p_wchan;
			TAILQ_REMOVE(&w->waitq, p, p_procq);
			p->p_flag &= ~P_XSLEEP;
		} else
#endif
		TAILQ_REMOVE(&slpque[LOOKUP(td->td_wchan)], td, td_threadq);
		td->td_wchan = NULL;
	}
	crit_exit();
}

#if 0
/*
 * Make all processes sleeping on the explicit lock structure runnable.
 */
void
xwakeup(struct xwait *w)
{
	struct proc *p;

	crit_enter();
	++w->gen;
	while ((p = TAILQ_FIRST(&w->waitq)) != NULL) {
		TAILQ_REMOVE(&w->waitq, p, p_procq);
		KASSERT(p->p_wchan == w && (p->p_flag & P_XSLEEP),
		    ("xwakeup: wchan mismatch for %p (%p/%p) %08x", p, p->p_wchan, w, p->p_flag & P_XSLEEP));
		p->p_wchan = NULL;
		p->p_flag &= ~P_XSLEEP;
		if (p->p_stat == SSLEEP) {
			/* OPTIMIZED EXPANSION OF setrunnable(p); */
			if (p->p_slptime > 1)
				updatepri(p);
			p->p_slptime = 0;
			p->p_stat = SRUN;
			if (p->p_flag & P_INMEM) {
				lwkt_schedule(td);
			} else {
				p->p_flag |= P_SWAPINREQ;
				wakeup((caddr_t)&proc0);
			}
		}
	}
	crit_exit();
}
#endif

/*
 * Make all processes sleeping on the specified identifier runnable.
 */
static void
_wakeup(void *ident, int domain, int count)
{
	struct slpquehead *qp;
	struct thread *td;
	struct thread *ntd;
	struct proc *p;
	int id = LOOKUP(ident);

	crit_enter();
	qp = &slpque[id];
restart:
	for (td = TAILQ_FIRST(qp); td != NULL; td = ntd) {
		ntd = TAILQ_NEXT(td, td_threadq);
		if (td->td_wchan == ident && td->td_wdomain == domain) {
			TAILQ_REMOVE(qp, td, td_threadq);
			td->td_wchan = NULL;
			if ((p = td->td_proc) != NULL && p->p_stat == SSLEEP) {
				/* OPTIMIZED EXPANSION OF setrunnable(p); */
				if (p->p_slptime > 1)
					updatepri(p);
				p->p_slptime = 0;
				p->p_stat = SRUN;
				if (p->p_flag & P_INMEM) {
					/*
					 * LWKT scheduled now, there is no
					 * userland runq interaction until
					 * the thread tries to return to user
					 * mode.
					 *
					 * setrunqueue(p); 
					 */
					lwkt_schedule(td);
				} else {
					p->p_flag |= P_SWAPINREQ;
					wakeup((caddr_t)&proc0);
				}
				/* END INLINE EXPANSION */
			} else if (p == NULL) {
				lwkt_schedule(td);
			}
			if (--count == 0)
				break;
			goto restart;
		}
	}
	crit_exit();
}

void
wakeup(void *ident)
{
    _wakeup(ident, 0, 0);
}

void
wakeup_one(void *ident)
{
    _wakeup(ident, 0, 1);
}

void
wakeup_domain(void *ident, int domain)
{
    _wakeup(ident, domain, 0);
}

void
wakeup_domain_one(void *ident, int domain)
{
    _wakeup(ident, domain, 1);
}

/*
 * The machine independent parts of mi_switch().
 *
 * 'p' must be the current process.
 */
void
mi_switch(struct proc *p)
{
	thread_t td = p->p_thread;
	struct rlimit *rlim;
	u_int64_t ttime;

	KKASSERT(td == mycpu->gd_curthread);

	crit_enter_quick(td);

	/*
	 * Check if the process exceeds its cpu resource allocation.
	 * If over max, kill it.  Time spent in interrupts is not 
	 * included.  YYY 64 bit match is expensive.  Ick.
	 */
	ttime = td->td_sticks + td->td_uticks;
	if (p->p_stat != SZOMB && p->p_limit->p_cpulimit != RLIM_INFINITY &&
	    ttime > p->p_limit->p_cpulimit) {
		rlim = &p->p_rlimit[RLIMIT_CPU];
		if (ttime / (rlim_t)1000000 >= rlim->rlim_max) {
			killproc(p, "exceeded maximum CPU limit");
		} else {
			psignal(p, SIGXCPU);
			if (rlim->rlim_cur < rlim->rlim_max) {
				/* XXX: we should make a private copy */
				rlim->rlim_cur += 5;
			}
		}
	}

	/*
	 * If we are in a SSTOPped state we deschedule ourselves.  
	 * YYY this needs to be cleaned up, remember that LWKTs stay on
	 * their run queue which works differently then the user scheduler
	 * which removes the process from the runq when it runs it.
	 */
	mycpu->gd_cnt.v_swtch++;
	if (p->p_stat == SSTOP)
		lwkt_deschedule_self(td);
	lwkt_switch();
	crit_exit_quick(td);
}

/*
 * Change process state to be runnable,
 * placing it on the run queue if it is in memory,
 * and awakening the swapper if it isn't in memory.
 */
void
setrunnable(struct proc *p)
{
	int s;

	s = splhigh();
	switch (p->p_stat) {
	case 0:
	case SRUN:
	case SZOMB:
	default:
		panic("setrunnable");
	case SSTOP:
	case SSLEEP:
		unsleep(p->p_thread);	/* e.g. when sending signals */
		break;

	case SIDL:
		break;
	}
	p->p_stat = SRUN;

	/*
	 * The process is controlled by LWKT at this point, we do not mess
	 * around with the userland scheduler until the thread tries to 
	 * return to user mode.
	 */
#if 0
	if (p->p_flag & P_INMEM)
		setrunqueue(p);
#endif
	if (p->p_flag & P_INMEM)
		lwkt_schedule(p->p_thread);
	splx(s);
	if (p->p_slptime > 1)
		updatepri(p);
	p->p_slptime = 0;
	if ((p->p_flag & P_INMEM) == 0) {
		p->p_flag |= P_SWAPINREQ;
		wakeup((caddr_t)&proc0);
	}
}

/*
 * Change the process state to NOT be runnable, removing it from the run
 * queue.
 */
void
clrrunnable(struct proc *p, int stat)
{
	crit_enter_quick(p->p_thread);
	if (p->p_stat == SRUN && (p->p_flag & P_ONRUNQ))
		remrunqueue(p);
	p->p_stat = stat;
	crit_exit_quick(p->p_thread);
}

/*
 * Compute the priority of a process when running in user mode.
 * Arrange to reschedule if the resulting priority is better
 * than that of the current process.
 */
void
resetpriority(struct proc *p)
{
	int newpriority;
	int interactive;
	int opq;
	int npq;

	/*
	 * Set p_priority for general process comparisons
	 */
	switch(p->p_rtprio.type) {
	case RTP_PRIO_REALTIME:
		p->p_priority = PRIBASE_REALTIME + p->p_rtprio.prio;
		return;
	case RTP_PRIO_NORMAL:
		break;
	case RTP_PRIO_IDLE:
		p->p_priority = PRIBASE_IDLE + p->p_rtprio.prio;
		return;
	case RTP_PRIO_THREAD:
		p->p_priority = PRIBASE_THREAD + p->p_rtprio.prio;
		return;
	}

	/*
	 * NORMAL priorities fall through.  These are based on niceness
	 * and cpu use.  Lower numbers == higher priorities.
	 */
	newpriority = (int)(NICE_ADJUST(p->p_nice - PRIO_MIN) +
			p->p_estcpu / ESTCPURAMP);

	/*
	 * p_interactive is -128 to +127 and represents very long term
	 * interactivity or batch (whereas estcpu is a much faster variable).
	 * Interactivity can modify the priority by up to 8 units either way.
	 * (8 units == approximately 4 nice levels).
	 */
	interactive = p->p_interactive / 10;
	newpriority += interactive;

	newpriority = MIN(newpriority, MAXPRI);
	newpriority = MAX(newpriority, 0);
	npq = newpriority / PPQ;
	crit_enter();
	opq = (p->p_priority & PRIMASK) / PPQ;
	if (p->p_stat == SRUN && (p->p_flag & P_ONRUNQ) && opq != npq) {
		/*
		 * We have to move the process to another queue
		 */
		remrunqueue(p);
		p->p_priority = PRIBASE_NORMAL + newpriority;
		setrunqueue(p);
	} else {
		/*
		 * We can just adjust the priority and it will be picked
		 * up later.
		 */
		KKASSERT(opq == npq || (p->p_flag & P_ONRUNQ) == 0);
		p->p_priority = PRIBASE_NORMAL + newpriority;
	}
	crit_exit();
}

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(void *arg)
{
	int i, nrun;
	struct loadavg *avg;
	struct proc *p;
	thread_t td;

	avg = &averunnable;
	nrun = 0;
	FOREACH_PROC_IN_SYSTEM(p) {
		switch (p->p_stat) {
		case SRUN:
			if ((td = p->p_thread) == NULL)
				break;
			if (td->td_flags & TDF_BLOCKED)
				break;
			/* fall through */
		case SIDL:
			nrun++;
			break;
		default:
			break;
		}
	}
	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;

	/*
	 * Schedule the next update to occur after 5 seconds, but add a
	 * random variation to avoid synchronisation with processes that
	 * run at regular intervals.
	 */
	callout_reset(&loadav_callout, hz * 4 + (int)(random() % (hz * 2 + 1)),
	    loadav, NULL);
}

/* ARGSUSED */
static void
sched_setup(void *dummy)
{
	callout_init(&loadav_callout);
	callout_init(&roundrobin_callout);
	callout_init(&schedcpu_callout);

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);
	schedcpu(NULL);
	loadav(NULL);
}

/*
 * We adjust the priority of the current process.  The priority of
 * a process gets worse as it accumulates CPU time.  The cpu usage
 * estimator (p_estcpu) is increased here.  resetpriority() will
 * compute a different priority each time p_estcpu increases by
 * INVERSE_ESTCPU_WEIGHT * (until MAXPRI is reached).
 *
 * The cpu usage estimator ramps up quite quickly when the process is 
 * running (linearly), and decays away exponentially, at a rate which
 * is proportionally slower when the system is busy.  The basic principle
 * is that the system will 90% forget that the process used a lot of CPU
 * time in 5 * loadav seconds.  This causes the system to favor processes
 * which haven't run much recently, and to round-robin among other processes.
 *
 * The actual schedulerclock interrupt rate is ESTCPUFREQ, but we generally
 * want to ramp-up at a faster rate, ESTCPUVFREQ, so p_estcpu is scaled
 * by (ESTCPUVFREQ / ESTCPUFREQ).  You can control the ramp-up/ramp-down
 * rate by adjusting ESTCPUVFREQ in sys/proc.h in integer multiples
 * of ESTCPUFREQ.
 *
 * WARNING! called from a fast-int or an IPI, the MP lock MIGHT NOT BE HELD
 * and we cannot block.
 */
void
schedulerclock(void *dummy)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	if ((p = td->td_proc) != NULL) {
		p->p_cpticks++;		/* cpticks runs at ESTCPUFREQ */
		p->p_estcpu = ESTCPULIM(p->p_estcpu + ESTCPUVFREQ / ESTCPUFREQ);
		if (try_mplock()) {
			resetpriority(p);
			rel_mplock();
		}
	}
}

static
void
crit_panicints(void)
{
    int s;
    int cpri;

    s = splhigh();
    cpri = crit_panic_save();
    splx(safepri);
    crit_panic_restore(cpri);
    splx(s);
}

