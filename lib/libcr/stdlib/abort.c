/*
 * Copyright (c) 1985, 1993
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
 * $FreeBSD: src/lib/libc/stdlib/abort.c,v 1.5.6.2 2002/10/15 19:46:46 fjoe Exp $
 * $DragonFly: src/lib/libcr/stdlib/Attic/abort.c,v 1.2 2003/06/17 04:26:46 dillon Exp $
 *
 * @(#)abort.c	8.1 (Berkeley) 6/4/93
 */

#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"
#endif

void (*__cleanup)();

void
abort()
{
	sigset_t mask;

	/*
	 * POSIX requires we flush stdio buffers on abort
	 */
	if (__cleanup)
		(*__cleanup)();

	sigfillset(&mask);
	/*
	 * don't block SIGABRT to give any handler a chance; we ignore
	 * any errors -- X311J doesn't allow abort to return anyway.
	 */
	sigdelset(&mask, SIGABRT);
#ifdef _THREAD_SAFE
	(void) __sys_sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#else
	(void)sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#endif
	(void)kill(getpid(), SIGABRT);

	/*
	 * if SIGABRT ignored, or caught and the handler returns, do
	 * it again, only harder.
	 */
	(void)signal(SIGABRT, SIG_DFL);
#ifdef _THREAD_SAFE
	(void) __sys_sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#else
	(void)sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#endif
	(void)kill(getpid(), SIGABRT);
	exit(1);
}
