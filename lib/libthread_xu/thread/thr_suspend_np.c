/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/lib/libpthread/thread/thr_suspend_np.c,v 1.19 2003/05/04 16:17:01 deischen Exp $
 * $DragonFly: src/lib/libthread_xu/thread/thr_suspend_np.c,v 1.2 2005/03/29 19:26:20 joerg Exp $
 */

#include <machine/tls.h>
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

static void suspend_common(struct pthread *thread);

__weak_reference(_pthread_suspend_np, pthread_suspend_np);
__weak_reference(_pthread_suspend_all_np, pthread_suspend_all_np);

/* Suspend a thread: */
int
_pthread_suspend_np(pthread_t thread)
{
	struct pthread *curthread = tls_get_curthread();
	int ret;

	/* Suspending the current thread doesn't make sense. */
	if (thread == curthread)
		ret = EDEADLK;

	/* Add a reference to the thread: */
	else if ((ret = _thr_ref_add(curthread, thread, /*include dead*/0))
	    == 0) {
		/* Lock the threads scheduling queue: */
		THR_THREAD_LOCK(curthread, thread);
		suspend_common(thread);
		/* Unlock the threads scheduling queue: */
		THR_THREAD_UNLOCK(curthread, thread);

		/* Don't forget to remove the reference: */
		_thr_ref_delete(curthread, thread);
	}
	return (ret);
}

void
_pthread_suspend_all_np(void)
{
	struct pthread	*curthread = tls_get_curthread();
	struct pthread	*thread;

	/* Take the thread list lock: */
	THREAD_LIST_LOCK(curthread);

	TAILQ_FOREACH(thread, &_thread_list, tle) {
		if (thread != curthread) {
			THR_THREAD_LOCK(curthread, thread);
			suspend_common(thread);
			THR_THREAD_UNLOCK(curthread, thread);
		}
	}

	/* Release the thread list lock: */
	THREAD_LIST_UNLOCK(curthread);
}

static void
suspend_common(struct pthread *thread)
{
	if (thread->state != PS_DEAD) {
		thread->flags |= THR_FLAGS_NEED_SUSPEND;
		_thr_send_sig(thread, SIGCANCEL);
	}
}
