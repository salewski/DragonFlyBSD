/*-
 * Copyright (c) 1980, 1989, 1992, 1993
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
 *	From: @(#)systat.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/systat/systat.h,v 1.3 1999/08/28 01:06:05 peter Exp $
 * $DragonFly: src/usr.bin/systat/systat.h,v 1.2 2003/06/17 04:29:32 dillon Exp $
 */

#include <curses.h>

struct  cmdtab {
        char    *c_name;		/* command name */
        void    (*c_refresh)(void);	/* display refresh */
        void    (*c_fetch)(void);	/* sets up data structures */
        void    (*c_label)(void);	/* label display */
	int	(*c_init)(void);	/* initialize namelist, etc. */
	WINDOW	*(*c_open)(void);	/* open display */
	void	(*c_close)(WINDOW *);	/* close display */
	int	(*c_cmd)(char *, char *); /* display command interpreter */
	void	(*c_reset)(void);	/* reset ``mode since'' display */
	char	c_flags;		/* see below */
};

#define	CF_INIT		0x1		/* been initialized */
#define	CF_LOADAV	0x2		/* display w/ load average */

#define	TCP	0x1
#define	UDP	0x2

#define KREAD(addr, buf, len)  kvm_ckread((addr), (buf), (len))
#define NVAL(indx)  namelist[(indx)].n_value
#define NPTR(indx)  (void *)NVAL((indx))
#define NREAD(indx, buf, len) kvm_ckread(NPTR((indx)), (buf), (len))
#define LONG	(sizeof (long))
