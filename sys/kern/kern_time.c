/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_time.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/kern/kern_time.c,v 1.68.2.1 2002/10/01 08:00:41 bde Exp $
 * $DragonFly: src/sys/kern/kern_time.c,v 1.19 2005/03/29 00:35:55 drhodus Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/sysproto.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/sysunion.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <sys/msgport2.h>
#include <sys/thread2.h>

struct timezone tz;

/*
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

static int	nanosleep1 (struct timespec *rqt,
		    struct timespec *rmt);
static int	settime (struct timeval *);
static void	timevalfix (struct timeval *);
static void	no_lease_updatetime (int);

static int     sleep_hard_us = 100;
SYSCTL_INT(_kern, OID_AUTO, sleep_hard_us, CTLFLAG_RW, &sleep_hard_us, 0, "")

static void 
no_lease_updatetime(deltat)
	int deltat;
{
}

void (*lease_updatetime) (int)  = no_lease_updatetime;

static int
settime(tv)
	struct timeval *tv;
{
	struct timeval delta, tv1, tv2;
	static struct timeval maxtime, laststep;
	struct timespec ts;

	crit_enter();
	microtime(&tv1);
	delta = *tv;
	timevalsub(&delta, &tv1);

	/*
	 * If the system is secure, we do not allow the time to be 
	 * set to a value earlier than 1 second less than the highest
	 * time we have yet seen. The worst a miscreant can do in
	 * this circumstance is "freeze" time. He couldn't go
	 * back to the past.
	 *
	 * We similarly do not allow the clock to be stepped more
	 * than one second, nor more than once per second. This allows
	 * a miscreant to make the clock march double-time, but no worse.
	 */
	if (securelevel > 1) {
		if (delta.tv_sec < 0 || delta.tv_usec < 0) {
			/*
			 * Update maxtime to latest time we've seen.
			 */
			if (tv1.tv_sec > maxtime.tv_sec)
				maxtime = tv1;
			tv2 = *tv;
			timevalsub(&tv2, &maxtime);
			if (tv2.tv_sec < -1) {
				tv->tv_sec = maxtime.tv_sec - 1;
				printf("Time adjustment clamped to -1 second\n");
			}
		} else {
			if (tv1.tv_sec == laststep.tv_sec) {
				crit_exit();
				return (EPERM);
			}
			if (delta.tv_sec > 1) {
				tv->tv_sec = tv1.tv_sec + 1;
				printf("Time adjustment clamped to +1 second\n");
			}
			laststep = *tv;
		}
	}

	ts.tv_sec = tv->tv_sec;
	ts.tv_nsec = tv->tv_usec * 1000;
	set_timeofday(&ts);
	lease_updatetime(delta.tv_sec);
	crit_exit();
	resettodr();
	return (0);
}

/* ARGSUSED */
int
clock_gettime(struct clock_gettime_args *uap)
{
	struct timespec ats;

	switch(uap->clock_id) {
	case CLOCK_REALTIME:
		nanotime(&ats);
		return (copyout(&ats, uap->tp, sizeof(ats)));
	case CLOCK_MONOTONIC:
		nanouptime(&ats);
		return (copyout(&ats, uap->tp, sizeof(ats)));
	default:
		return (EINVAL);
	}
}

/* ARGSUSED */
int
clock_settime(struct clock_settime_args *uap)
{
	struct thread *td = curthread;
	struct timeval atv;
	struct timespec ats;
	int error;

	if ((error = suser(td)) != 0)
		return (error);
	switch(uap->clock_id) {
	case CLOCK_REALTIME:
		if ((error = copyin(uap->tp, &ats, sizeof(ats))) != 0)
			return (error);
		if (ats.tv_nsec < 0 || ats.tv_nsec >= 1000000000)
			return (EINVAL);
		/* XXX Don't convert nsec->usec and back */
		TIMESPEC_TO_TIMEVAL(&atv, &ats);
		error = settime(&atv);
		return (error);
	default:
		return (EINVAL);
	}
}

int
clock_getres(struct clock_getres_args *uap)
{
	struct timespec ts;

	switch(uap->clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
		/*
		 * Round up the result of the division cheaply
		 * by adding 1.  Rounding up is especially important
		 * if rounding down would give 0.  Perfect rounding
		 * is unimportant.
		 */
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 / cputimer_freq + 1;
		return(copyout(&ts, uap->tp, sizeof(ts)));
	default:
		return(EINVAL);
	}
}

/*
 * nanosleep1()
 *
 *	This is a general helper function for nanosleep() (aka sleep() aka
 *	usleep()).
 *
 *	If there is less then one tick's worth of time left and
 *	we haven't done a yield, or the remaining microseconds is
 *	ridiculously low, do a yield.  This avoids having
 *	to deal with systimer overheads when the system is under
 *	heavy loads.  If we have done a yield already then use
 *	a systimer and an uninterruptable thread wait.
 *
 *	If there is more then a tick's worth of time left,
 *	calculate the baseline ticks and use an interruptable
 *	tsleep, then handle the fine-grained delay on the next
 *	loop.  This usually results in two sleeps occuring, a long one
 *	and a short one.
 */
static void
ns1_systimer(systimer_t info)
{
	lwkt_schedule(info->data);
}

static int
nanosleep1(struct timespec *rqt, struct timespec *rmt)
{
	static int nanowait;
	struct timespec ts, ts2, ts3;
	struct timeval tv;
	int error;
	int tried_yield;

	if (rqt->tv_nsec < 0 || rqt->tv_nsec >= 1000000000)
		return (EINVAL);
	if (rqt->tv_sec < 0 || (rqt->tv_sec == 0 && rqt->tv_nsec == 0))
		return (0);
	nanouptime(&ts);
	timespecadd(&ts, rqt);		/* ts = target timestamp compare */
	TIMESPEC_TO_TIMEVAL(&tv, rqt);	/* tv = sleep interval */
	tried_yield = 0;

	for (;;) {
		int ticks;
		struct systimer info;

		ticks = tv.tv_usec / tick;	/* approximate */

		if (tv.tv_sec == 0 && ticks == 0) {
			thread_t td = curthread;
			if (tried_yield || tv.tv_usec < sleep_hard_us) {
				tried_yield = 0;
				uio_yield();
			} else {
				crit_enter_quick(td);
				systimer_init_oneshot(&info, ns1_systimer,
						td, tv.tv_usec);
				lwkt_deschedule_self(td);
				crit_exit_quick(td);
				lwkt_switch();
				systimer_del(&info); /* make sure it's gone */
			}
			error = iscaught(td->td_proc);
		} else if (tv.tv_sec == 0) {
			error = tsleep(&nanowait, PCATCH, "nanslp", ticks);
		} else {
			ticks = tvtohz_low(&tv); /* also handles overflow */
			error = tsleep(&nanowait, PCATCH, "nanslp", ticks);
		}
		nanouptime(&ts2);
		if (error && error != EWOULDBLOCK) {
			if (error == ERESTART)
				error = EINTR;
			if (rmt != NULL) {
				timespecsub(&ts, &ts2);
				if (ts.tv_sec < 0)
					timespecclear(&ts);
				*rmt = ts;
			}
			return (error);
		}
		if (timespeccmp(&ts2, &ts, >=))
			return (0);
		ts3 = ts;
		timespecsub(&ts3, &ts2);
		TIMESPEC_TO_TIMEVAL(&tv, &ts3);
	}
}

static void nanosleep_done(void *arg);
static void nanosleep_copyout(union sysunion *sysun);

/* ARGSUSED */
int
nanosleep(struct nanosleep_args *uap)
{
	int error;
	struct sysmsg_sleep *smsleep = &uap->sysmsg.sm.sleep;

	error = copyin(uap->rqtp, &smsleep->rqt, sizeof(smsleep->rqt));
	if (error)
		return (error);
	/*
	 * YYY clean this up to always use the callout, note that an abort
	 * implementation should record the residual in the async case.
	 */
	if (uap->sysmsg.lmsg.ms_flags & MSGF_ASYNC) {
		quad_t ticks;

		ticks = (quad_t)smsleep->rqt.tv_nsec * hz / 1000000000LL;
		if (smsleep->rqt.tv_sec)
			ticks += (quad_t)smsleep->rqt.tv_sec * hz;
		if (ticks <= 0) {
			if (ticks == 0)
				error = 0;
			else
				error = EINVAL;
		} else {
			uap->sysmsg.copyout = nanosleep_copyout;
			uap->sysmsg.lmsg.ms_flags &= ~MSGF_DONE;
			callout_init(&smsleep->timer);
			callout_reset(&smsleep->timer, ticks, nanosleep_done, uap);
			error = EASYNC;
		}
	} else {
		/*
		 * Old synchronous sleep code, copyout the residual if
		 * nanosleep was interrupted.
		 */
		error = nanosleep1(&smsleep->rqt, &smsleep->rmt);
		if (error && uap->rmtp)
			error = copyout(&smsleep->rmt, uap->rmtp, sizeof(smsleep->rmt));
	}
	return (error);
}

/*
 * Asynch completion for the nanosleep() syscall.  This function may be
 * called from any context and cannot legally access the originating 
 * thread, proc, or its user space.
 *
 * YYY change the callout interface API so we can simply assign the replymsg
 * function to it directly.
 */
static void
nanosleep_done(void *arg)
{
	struct nanosleep_args *uap = arg;
	lwkt_msg_t msg = &uap->sysmsg.lmsg;

	lwkt_replymsg(msg, 0);
}

/*
 * Asynch return for the nanosleep() syscall, called in the context of the 
 * originating thread when it pulls the message off the reply port.  This
 * function is responsible for any copyouts to userland.  Kernel threads
 * which do their own internal system calls will not usually call the return
 * function.
 */
static void
nanosleep_copyout(union sysunion *sysun)
{
	struct nanosleep_args *uap = &sysun->nanosleep;
	struct sysmsg_sleep *smsleep = &uap->sysmsg.sm.sleep;

	if (sysun->lmsg.ms_error && uap->rmtp) {
		sysun->lmsg.ms_error = 
		    copyout(&smsleep->rmt, uap->rmtp, sizeof(smsleep->rmt));
	}
}

/* ARGSUSED */
int
gettimeofday(struct gettimeofday_args *uap)
{
	struct timeval atv;
	int error = 0;

	if (uap->tp) {
		microtime(&atv);
		if ((error = copyout((caddr_t)&atv, (caddr_t)uap->tp,
		    sizeof (atv))))
			return (error);
	}
	if (uap->tzp)
		error = copyout((caddr_t)&tz, (caddr_t)uap->tzp,
		    sizeof (tz));
	return (error);
}

/* ARGSUSED */
int
settimeofday(struct settimeofday_args *uap)
{
	struct thread *td = curthread;
	struct timeval atv;
	struct timezone atz;
	int error;

	if ((error = suser(td)))
		return (error);
	/* Verify all parameters before changing time. */
	if (uap->tv) {
		if ((error = copyin((caddr_t)uap->tv, (caddr_t)&atv,
		    sizeof(atv))))
			return (error);
		if (atv.tv_usec < 0 || atv.tv_usec >= 1000000)
			return (EINVAL);
	}
	if (uap->tzp &&
	    (error = copyin((caddr_t)uap->tzp, (caddr_t)&atz, sizeof(atz))))
		return (error);
	if (uap->tv && (error = settime(&atv)))
		return (error);
	if (uap->tzp)
		tz = atz;
	return (0);
}

int	tickdelta;			/* current clock skew, us. per tick */
long	timedelta;			/* unapplied time correction, us. */
static long	bigadj = 1000000;	/* use 10x skew above bigadj us. */

/* ARGSUSED */
int
adjtime(struct adjtime_args *uap)
{
	struct thread *td = curthread;
	struct timeval atv;
	long ndelta, ntickdelta, odelta;
	int error;

	if ((error = suser(td)))
		return (error);
	if ((error =
	    copyin((caddr_t)uap->delta, (caddr_t)&atv, sizeof(struct timeval))))
		return (error);

	/*
	 * Compute the total correction and the rate at which to apply it.
	 * Round the adjustment down to a whole multiple of the per-tick
	 * delta, so that after some number of incremental changes in
	 * hardclock(), tickdelta will become zero, lest the correction
	 * overshoot and start taking us away from the desired final time.
	 */
	ndelta = atv.tv_sec * 1000000 + atv.tv_usec;
	if (ndelta > bigadj || ndelta < -bigadj)
		ntickdelta = 10 * tickadj;
	else
		ntickdelta = tickadj;
	if (ndelta % ntickdelta)
		ndelta = ndelta / ntickdelta * ntickdelta;

	/*
	 * To make hardclock()'s job easier, make the per-tick delta negative
	 * if we want time to run slower; then hardclock can simply compute
	 * tick + tickdelta, and subtract tickdelta from timedelta.
	 */
	if (ndelta < 0)
		ntickdelta = -ntickdelta;
	/* 
	 * XXX not MP safe , but will probably work anyway.
	 */
	crit_enter();
	odelta = timedelta;
	timedelta = ndelta;
	tickdelta = ntickdelta;
	crit_exit();

	if (uap->olddelta) {
		atv.tv_sec = odelta / 1000000;
		atv.tv_usec = odelta % 1000000;
		(void) copyout((caddr_t)&atv, (caddr_t)uap->olddelta,
		    sizeof(struct timeval));
	}
	return (0);
}

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept in the p_stats area, since
 * they can be swapped out.  These are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer is kept in the process table slot
 * for the process, and its value (it_value) is kept as an
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a timeout
 * routine, called from the softclock() routine.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */
/* ARGSUSED */
int
getitimer(struct getitimer_args *uap)
{
	struct proc *p = curproc;
	struct timeval ctv;
	struct itimerval aitv;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	crit_enter();
	if (uap->which == ITIMER_REAL) {
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		aitv = p->p_realtimer;
		if (timevalisset(&aitv.it_value)) {
			getmicrouptime(&ctv);
			if (timevalcmp(&aitv.it_value, &ctv, <))
				timevalclear(&aitv.it_value);
			else
				timevalsub(&aitv.it_value, &ctv);
		}
	} else {
		aitv = p->p_stats->p_timer[uap->which];
	}
	crit_exit();
	return (copyout((caddr_t)&aitv, (caddr_t)uap->itv,
	    sizeof (struct itimerval)));
}

/* ARGSUSED */
int
setitimer(struct setitimer_args *uap)
{
	struct itimerval aitv;
	struct timeval ctv;
	struct itimerval *itvp;
	struct proc *p = curproc;
	int error;

	if (uap->which > ITIMER_PROF)
		return (EINVAL);
	itvp = uap->itv;
	if (itvp && (error = copyin((caddr_t)itvp, (caddr_t)&aitv,
	    sizeof(struct itimerval))))
		return (error);
	if ((uap->itv = uap->oitv) &&
	    (error = getitimer((struct getitimer_args *)uap)))
		return (error);
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value))
		return (EINVAL);
	if (!timevalisset(&aitv.it_value))
		timevalclear(&aitv.it_interval);
	else if (itimerfix(&aitv.it_interval))
		return (EINVAL);
	crit_enter();
	if (uap->which == ITIMER_REAL) {
		if (timevalisset(&p->p_realtimer.it_value))
			callout_stop(&p->p_ithandle);
		if (timevalisset(&aitv.it_value)) 
			callout_reset(&p->p_ithandle,
			    tvtohz_high(&aitv.it_value), realitexpire, p);
		getmicrouptime(&ctv);
		timevaladd(&aitv.it_value, &ctv);
		p->p_realtimer = aitv;
	} else {
		p->p_stats->p_timer[uap->which] = aitv;
	}
	crit_exit();
	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 * tvtohz_high() always adds 1 to allow for the time until the next clock
 * interrupt being strictly less than 1 clock tick, but we don't want
 * that here since we want to appear to be in sync with the clock
 * interrupt even when we're delayed.
 */
void
realitexpire(arg)
	void *arg;
{
	struct proc *p;
	struct timeval ctv, ntv;

	p = (struct proc *)arg;
	psignal(p, SIGALRM);
	if (!timevalisset(&p->p_realtimer.it_interval)) {
		timevalclear(&p->p_realtimer.it_value);
		return;
	}
	for (;;) {
		crit_enter();
		timevaladd(&p->p_realtimer.it_value,
		    &p->p_realtimer.it_interval);
		getmicrouptime(&ctv);
		if (timevalcmp(&p->p_realtimer.it_value, &ctv, >)) {
			ntv = p->p_realtimer.it_value;
			timevalsub(&ntv, &ctv);
			callout_reset(&p->p_ithandle, tvtohz_low(&ntv),
				      realitexpire, p);
			crit_exit();
			return;
		}
		crit_exit();
	}
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable, and
 * fix it to have at least minimal value (i.e. if it is less
 * than the resolution of the clock, round it up.)
 */
int
itimerfix(tv)
	struct timeval *tv;
{

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);
	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;
	return (0);
}

/*
 * Decrement an interval timer by a specified number
 * of microseconds, which must be less than a second,
 * i.e. < 1000000.  If the timer expires, then reload
 * it.  In this case, carry over (usec - old value) to
 * reduce the value reloaded into the timer so that
 * the timer does not drift.  This routine assumes
 * that it is called in a context where the timers
 * on which it is operating cannot change in value.
 */
int
itimerdecr(itp, usec)
	struct itimerval *itp;
	int usec;
{

	if (itp->it_value.tv_usec < usec) {
		if (itp->it_value.tv_sec == 0) {
			/* expired, and already in next interval */
			usec -= itp->it_value.tv_usec;
			goto expire;
		}
		itp->it_value.tv_usec += 1000000;
		itp->it_value.tv_sec--;
	}
	itp->it_value.tv_usec -= usec;
	usec = 0;
	if (timevalisset(&itp->it_value))
		return (1);
	/* expired, exactly at end of interval */
expire:
	if (timevalisset(&itp->it_interval)) {
		itp->it_value = itp->it_interval;
		itp->it_value.tv_usec -= usec;
		if (itp->it_value.tv_usec < 0) {
			itp->it_value.tv_usec += 1000000;
			itp->it_value.tv_sec--;
		}
	} else
		itp->it_value.tv_usec = 0;		/* sec is already 0 */
	return (0);
}

/*
 * Add and subtract routines for timevals.
 * N.B.: subtract routine doesn't deal with
 * results which are before the beginning,
 * it just gets very confused in this case.
 * Caveat emptor.
 */
void
timevaladd(t1, t2)
	struct timeval *t1, *t2;
{

	t1->tv_sec += t2->tv_sec;
	t1->tv_usec += t2->tv_usec;
	timevalfix(t1);
}

void
timevalsub(t1, t2)
	struct timeval *t1, *t2;
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static void
timevalfix(t1)
	struct timeval *t1;
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

/*
 * ratecheck(): simple time-based rate-limit checking.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int rv = 0;

	getmicrouptime(&tv);		/* NB: 10ms precision */
	delta = tv;
	timevalsub(&delta, lasttime);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timevalcmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

/*
 * ppsratecheck(): packets (or events) per second limitation.
 *
 * Return 0 if the limit is to be enforced (e.g. the caller
 * should drop a packet because of the rate limitation).
 *
 * maxpps of 0 always causes zero to be returned.  maxpps of -1
 * always causes 1 to be returned; this effectively defeats rate
 * limiting.
 *
 * Note that we maintain the struct timeval for compatibility
 * with other bsd systems.  We reuse the storage and just monitor
 * clock ticks for minimal overhead.  
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	int now;

	/*
	 * Reset the last time and counter if this is the first call
	 * or more than a second has passed since the last update of
	 * lasttime.
	 */
	now = ticks;
	if (lasttime->tv_sec == 0 || (u_int)(now - lasttime->tv_sec) >= hz) {
		lasttime->tv_sec = now;
		*curpps = 1;
		return (maxpps != 0);
	} else {
		(*curpps)++;		/* NB: ignore potential overflow */
		return (maxpps < 0 || *curpps < maxpps);
	}
}

