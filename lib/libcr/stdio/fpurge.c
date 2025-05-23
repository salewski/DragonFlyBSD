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
 * @(#)fpurge.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libc/stdio/fpurge.c,v 1.7 1999/08/28 00:01:02 peter Exp $
 * $DragonFly: src/lib/libcr/stdio/Attic/fpurge.c,v 1.3 2004/07/05 17:31:00 eirikn Exp $
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "local.h"
#include "libc_private.h"

/*
 * fpurge: like fflush, but without writing anything: leave the
 * given FILE's buffer empty.
 */
int
fpurge(FILE *fp)
{
	int retval;
	FLOCKFILE(fp);
	if (!fp->_flags) {
		errno = EBADF;
		retval = EOF;
	} else {
		if (HASUB(fp))
			FREEUB(fp);
		fp->_p = fp->_bf._base;
		fp->_r = 0;
		fp->_w = fp->_flags & (__SLBF|__SNBF) ? 0 : fp->_bf._size;
		retval = 0;
	}
	FUNLOCKFILE(fp);
	return (retval);
}
