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
 * @(#)isinf.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libc/amd64/gen/isinf.c,v 1.10 2003/02/12 20:03:41 mike Exp $
 * $DragonFly: src/lib/libc/amd64/gen/Attic/isinf.c,v 1.2 2004/10/25 19:38:01 drhodus Exp $
 */

/* For binary compat; to be removed in FreeBSD 6.0. */

#include <sys/types.h>

int
isnan(d)
	double d;
{
	struct IEEEdp {
		u_int manl : 32;
		u_int manh : 20;
		u_int  exp : 11;
		u_int sign :  1;
	} *p = (struct IEEEdp *)&d;

	return(p->exp == 2047 && (p->manh || p->manl));
}

int
isinf(d)
	double d;
{
	struct IEEEdp {
		u_int manl : 32;
		u_int manh : 20;
		u_int  exp : 11;
		u_int sign :  1;
	} *p = (struct IEEEdp *)&d;

	return(p->exp == 2047 && !p->manh && !p->manl);
}
