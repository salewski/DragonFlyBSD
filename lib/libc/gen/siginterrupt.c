/*
 * Copyright (c) 1989, 1993
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
 * $FreeBSD: src/lib/libc/gen/siginterrupt.c,v 1.2.8.1 2001/03/05 09:34:53 obrien Exp $
 * $DragonFly: src/lib/libc/gen/siginterrupt.c,v 1.3 2005/01/31 22:29:15 dillon Exp $
 *
 * @(#)siginterrupt.c	8.1 (Berkeley) 6/4/93
 */

#include "namespace.h"
#include <signal.h>
#include "un-namespace.h"
#include "libc_private.h"

/*
 * Set signal state to prevent restart of system calls
 * after an instance of the indicated signal.
 */
int
siginterrupt(sig, flag)
	int sig, flag;
{
	extern sigset_t _sigintr;
	struct sigaction sa;
	int ret;

	if ((ret = _sigaction(sig, (struct sigaction *)0, &sa)) < 0)
		return (ret);
	if (flag) {
		sigaddset(&_sigintr, sig);
		sa.sa_flags &= ~SA_RESTART;
	} else {
		sigdelset(&_sigintr, sig);
		sa.sa_flags |= SA_RESTART;
	}
	return (_sigaction(sig, &sa, (struct sigaction *)0));
}
