/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 * $FreeBSD: src/lib/libpthread/thread/thr_clean.c,v 1.9 2004/12/18 18:07:37 deischen Exp $
 * $DragonFly: src/lib/libthread_xu/thread/thr_clean.c,v 1.2 2005/03/29 19:26:20 joerg Exp $
 */

#include <machine/tls.h>

#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(_pthread_cleanup_push, pthread_cleanup_push);
__weak_reference(_pthread_cleanup_pop, pthread_cleanup_pop);

void
_pthread_cleanup_push(void (*routine) (void *), void *routine_arg)
{
	struct pthread	*curthread = tls_get_curthread();
	struct pthread_cleanup *new;

	if ((new = (struct pthread_cleanup *)
	    malloc(sizeof(struct pthread_cleanup))) != NULL) {
		new->routine = routine;
		new->routine_arg = routine_arg;
		new->onstack = 0;
		new->next = curthread->cleanup;

		curthread->cleanup = new;
	}
}

void
_pthread_cleanup_pop(int execute)
{
	struct pthread	*curthread = tls_get_curthread();
	struct pthread_cleanup *old;

	if ((old = curthread->cleanup) != NULL) {
		curthread->cleanup = old->next;
		if (execute) {
			old->routine(old->routine_arg);
		}
		if (old->onstack == 0)
			free(old);
	}
}
