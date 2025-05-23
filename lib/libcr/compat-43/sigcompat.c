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
 * $FreeBSD: src/lib/libc/compat-43/sigcompat.c,v 1.7 2000/01/27 23:06:04 jasone Exp $
 * $DragonFly: src/lib/libcr/compat-43/Attic/sigcompat.c,v 1.2 2003/06/17 04:26:41 dillon Exp $
 *
 * @(#)sigcompat.c	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/lib/libc/compat-43/sigcompat.c,v 1.7 2000/01/27 23:06:04 jasone Exp $
 */

#include <sys/param.h>
#include <signal.h>

int
sigvec(signo, sv, osv)
	int signo;
	struct sigvec *sv, *osv;
{
	struct sigaction sa, osa;
	struct sigaction *sap, *osap;
	int ret;

	if (sv != NULL) {
		sa.sa_handler = sv->sv_handler;
		sa.sa_flags = sv->sv_flags ^ SV_INTERRUPT;
		sigemptyset(&sa.sa_mask);
		sa.sa_mask.__bits[0] = sv->sv_mask;
		sap = &sa;
	} else
		sap = NULL;
	osap = osv != NULL ? &osa : NULL;
	ret = sigaction(signo, sap, osap);
	if (ret == 0 && osv != NULL) {
		osv->sv_handler = osa.sa_handler;
		osv->sv_flags = osa.sa_flags ^ SV_INTERRUPT;
		osv->sv_mask = osa.sa_mask.__bits[0];
	}
	return (ret);
}

int
sigsetmask(mask)
	int mask;
{
	sigset_t set, oset;
	int n;

	sigemptyset(&set);
	set.__bits[0] = mask;
	n = sigprocmask(SIG_SETMASK, &set, &oset);
	if (n)
		return (n);
	return (oset.__bits[0]);
}

int
sigblock(mask)
	int mask;
{
	sigset_t set, oset;
	int n;

	sigemptyset(&set);
	set.__bits[0] = mask;
	n = sigprocmask(SIG_BLOCK, &set, &oset);
	if (n)
		return (n);
	return (oset.__bits[0]);
}

int
sigpause(mask)
	int mask;
{
	sigset_t set;

	sigemptyset(&set);
	set.__bits[0] = mask;
	return (_sigsuspend(&set));
}
