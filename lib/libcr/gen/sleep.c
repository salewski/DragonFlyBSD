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
 * @(#)sleep.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libc/gen/sleep.c,v 1.28.2.1 2000/03/18 23:13:24 jasone Exp $
 * $DragonFly: src/lib/libcr/gen/Attic/sleep.c,v 1.2 2003/06/17 04:26:42 dillon Exp $
 */

#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

unsigned int
__sleep(seconds)
	unsigned int seconds;
{
	struct timespec time_to_sleep;
	struct timespec time_remaining;

	/*
	 * Avoid overflow when `seconds' is huge.  This assumes that
	 * the maximum value for a time_t is >= INT_MAX.
	 */
	if (seconds > INT_MAX)
		return (seconds - INT_MAX + __sleep(INT_MAX));

	time_to_sleep.tv_sec = seconds;
	time_to_sleep.tv_nsec = 0;
	if (_nanosleep(&time_to_sleep, &time_remaining) != -1)
		return (0);
	if (errno != EINTR)
		return (seconds);		/* best guess */
	return (time_remaining.tv_sec +
		(time_remaining.tv_nsec != 0)); /* round up */
}

#ifndef _THREAD_SAFE
__weak_reference(__sleep, sleep);
#endif
