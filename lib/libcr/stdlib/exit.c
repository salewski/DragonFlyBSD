/*-
 * Copyright (c) 1990, 1993
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
 * $FreeBSD: src/lib/libc/stdlib/exit.c,v 1.3.6.1 2001/03/05 11:33:57 obrien Exp $
 * $DragonFly: src/lib/libcr/stdlib/Attic/exit.c,v 1.4 2004/03/13 19:46:56 eirikn Exp $
 *
 * @(#)exit.c	8.1 (Berkeley) 6/4/93
 */

#include <stdlib.h>
#include <unistd.h>
#include "atexit.h"

void (*__cleanup)();

/*
 * This variable is zero until a process has created a thread.
 * It is used to avoid calling locking functions in libc when they
 * are not required. By default, libc is intended to be(come)
 * thread-safe, but without a (significant) penalty to non-threaded
 * processes.
 */
int	__isthreaded	= 0;

/*
 * Exit, flushing stdio buffers if necessary.
 */
void
exit(status)
	int status;
{
#ifdef	_THREAD_SAFE
	extern int _thread_autoinit_dummy_decl;
	/* Ensure that the auto-initialization routine is linked in: */
	_thread_autoinit_dummy_decl = 1;
#endif

	__cxa_finalize(NULL);

	if (__cleanup)
		(*__cleanup)();
	_exit(status);
}
