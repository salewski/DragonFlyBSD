/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)local.h	8.3 (Berkeley) 7/3/94
 *
 * $FreeBSD: src/lib/libc/stdio/local.h,v 1.1.1.2.6.1 2001/03/05 11:27:49 obrien Exp $
 * $DragonFly: src/lib/libcr/stdio/Attic/local.h,v 1.4 2003/12/08 13:56:35 eirikn Exp $
 */

#ifndef _MACHINE_STDINT_H_
#include <machine/stdint.h>	/* __size_t */
#endif

/*
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 */

extern int	__sflush (FILE *);
extern FILE	*__sfp (void);
extern int	__srefill (FILE *);
extern int	__sread (void *, char *, int);
extern int	__swrite (void *, char const *, int);
extern fpos_t	__sseek (void *, fpos_t, int);
extern int	__sclose (void *);
extern void	__sinit (void);
extern void	_cleanup (void);
extern void	(*__cleanup) (void);
extern void	__smakebuf (FILE *);
extern int	__swhatbuf (FILE *, __size_t *, int *);
extern int	_fwalk (int (*)(FILE *));
extern int	__swsetup (FILE *);
extern int	__sflags (const char *, int *);

extern int	__sdidinit;

/*
 * Return true iff the given FILE cannot be written now.
 */
#define	cantwrite(fp) \
	((((fp)->_flags & __SWR) == 0 || (fp)->_bf._base == NULL) && \
	 __swsetup(fp))

/*
 * Test whether the given stdio file has an active ungetc buffer;
 * release such a buffer, without restoring ordinary unread data.
 */
#define	HASUB(fp) ((fp)->_ub._base != NULL)
#define	FREEUB(fp) { \
	if ((fp)->_ub._base != (fp)->_ubuf) \
		free((char *)(fp)->_ub._base); \
	(fp)->_ub._base = NULL; \
}

/*
 * test for an fgetln() buffer.
 */
#define	HASLB(fp) ((fp)->_lb._base != NULL)
#define	FREELB(fp) { \
	free((char *)(fp)->_lb._base); \
	(fp)->_lb._base = NULL; \
}
