/*-
 * Copyright (c) 1991, 1993
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
 * @(#)print.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/cksum/print.c,v 1.4 1999/12/05 20:03:22 charnier Exp $
 * $DragonFly: src/usr.bin/cksum/print.c,v 1.3 2003/10/02 17:42:26 hmp Exp $
 */

#include <sys/types.h>
#include <stdio.h>
#include "extern.h"

void
pcrc(char *fn, u_int32_t val, u_int32_t len)
{
	(void)printf("%lu %lu", (u_long) val, (u_long) len);
	if (fn)
		(void)printf(" %s", fn);
	(void)printf("\n");
}

void
psum1(char *fn, u_int32_t val, u_int32_t len)
{
	(void)printf("%lu %lu", (u_long) val, (u_long) (len + 1023) / 1024);
	if (fn)
		(void)printf(" %s", fn);
	(void)printf("\n");
}

void
psum2(char *fn, u_int32_t val, u_int32_t len)
{
	(void)printf("%lu %lu", (u_long) val, (u_long) (len + 511) / 512);
	if (fn)
		(void)printf(" %s", fn);
	(void)printf("\n");
}
