/*
 * Copyright (c) 1999 Peter Wemm <peter@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_kthread.c,v 1.5.2.3 2001/12/25 01:51:14 dillon Exp $
 * $DragonFly: src/sys/kern/kern_kthread.c,v 1.10 2004/07/29 09:02:33 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/ptrace.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#include <machine/stdarg.h>

/*
 * Create a kernel process/thread/whatever.  It shares it's address space
 * with proc0 - ie: kernel only.  5.x compatible.
 *
 * NOTE!  By default kthreads are created with the MP lock held.  A
 * thread which does not require the MP lock should release it by calling
 * rel_mplock() at the start of the new thread.
 */
int
kthread_create(void (*func)(void *), void *arg,
    struct thread **tdp, const char *fmt, ...)
{
    thread_t td;
    __va_list ap;

    td = lwkt_alloc_thread(NULL, LWKT_THREAD_STACK, -1);
    if (tdp)
	*tdp = td;
    cpu_set_thread_handler(td, kthread_exit, func, arg);
    td->td_flags |= TDF_VERBOSE;
#ifdef SMP
    td->td_mpcount = 1;
#endif

    /*
     * Set up arg0 for 'ps' etc
     */
    __va_start(ap, fmt);
    vsnprintf(td->td_comm, sizeof(td->td_comm), fmt, ap);
    __va_end(ap);

    /*
     * Schedule the thread to run
     */
    lwkt_schedule(td);
    return 0;
}

/*
 * Same as kthread_create() but you can specify a custom stack size.
 */
int
kthread_create_stk(void (*func)(void *), void *arg,
    struct thread **tdp, int stksize, const char *fmt, ...)
{
    thread_t td;
    __va_list ap;

    td = lwkt_alloc_thread(NULL, stksize, -1);
    if (tdp)
	*tdp = td;
    cpu_set_thread_handler(td, kthread_exit, func, arg);
    td->td_flags |= TDF_VERBOSE;
#ifdef SMP
    td->td_mpcount = 1;
#endif
    __va_start(ap, fmt);
    vsnprintf(td->td_comm, sizeof(td->td_comm), fmt, ap);
    __va_end(ap);

    lwkt_schedule(td);
    return 0;
}

/*
 * Destroy an LWKT thread.   Warning!  This function is not called when
 * a process exits, cpu_proc_exit() directly calls cpu_thread_exit() and
 * uses a different reaping mechanism.
 *
 * XXX duplicates lwkt_exit()
 */
void
kthread_exit(void)
{
    lwkt_exit();
}


/*
 * Start a kernel process.  This is called after a fork() call in
 * mi_startup() in the file kern/init_main.c.
 *
 * This function is used to start "internal" daemons and intended
 * to be called from SYSINIT().
 */
void
kproc_start(udata)
	const void *udata;
{
	const struct kproc_desc	*kp = udata;
	int error;

	error = kthread_create((void (*)(void *))kp->func, NULL,
		    kp->global_threadpp, kp->arg0);
	lwkt_setpri(*kp->global_threadpp, TDPRI_KERN_DAEMON);
	if (error)
		panic("kproc_start: %s: error %d", kp->arg0, error);
}

/*
 * Advise a kernel process to suspend (or resume) in its main loop.
 * Participation is voluntary.
 */
int
suspend_kproc(struct thread *td, int timo)
{
	if (td->td_proc == NULL) {
		td->td_flags |= TDF_STOPREQ;	/* request thread pause */
		wakeup(td);
		while (td->td_flags & TDF_STOPREQ) {
			int error = tsleep(td, 0, "suspkp", timo);
			if (error == EWOULDBLOCK)
				break;
		}
		td->td_flags &= ~TDF_STOPREQ;
		return(0);
	} else {
		return(EINVAL);	/* not a kernel thread */
	}
}

void
kproc_suspend_loop(void)
{
	struct thread *td = curthread;

	if (td->td_flags & TDF_STOPREQ) {
		td->td_flags &= ~TDF_STOPREQ;
		while ((td->td_flags & TDF_WAKEREQ) == 0) {
			wakeup(td);
			tsleep(td, 0, "kpsusp", 0);
		}
		td->td_flags &= ~TDF_WAKEREQ;
		wakeup(td);
	}
}

