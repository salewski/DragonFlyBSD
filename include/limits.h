/*-
 * Copyright (c) 1988, 1993
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
 *	@(#)limits.h	8.2 (Berkeley) 1/4/94
 * $FreeBSD: src/include/limits.h,v 1.10 1999/08/27 23:44:50 peter Exp $
 * $DragonFly: src/include/limits.h,v 1.2 2003/06/17 04:25:56 dillon Exp $
 */

#ifndef _LIMITS_H_
#define	_LIMITS_H_
#include <sys/_posix.h>

#ifndef _ANSI_SOURCE
#define	_POSIX_ARG_MAX		4096
#define	_POSIX_CHILD_MAX	6
#define	_POSIX_LINK_MAX		8
#define	_POSIX_MAX_CANON	255
#define	_POSIX_MAX_INPUT	255
#define	_POSIX_NAME_MAX		14
#define	_POSIX_NGROUPS_MAX	0
#define	_POSIX_OPEN_MAX		16
#define	_POSIX_PATH_MAX		255
#define	_POSIX_PIPE_BUF		512
#define	_POSIX_SSIZE_MAX	32767
#define	_POSIX_STREAM_MAX	8
#define	_POSIX_TZNAME_MAX	3

#define	_POSIX2_BC_BASE_MAX	99
#define	_POSIX2_BC_DIM_MAX	2048
#define	_POSIX2_BC_SCALE_MAX	99
#define	_POSIX2_BC_STRING_MAX	1000
#define	_POSIX2_EQUIV_CLASS_MAX	2
#define	_POSIX2_EXPR_NEST_MAX	32
#define	_POSIX2_LINE_MAX	2048
#define	_POSIX2_RE_DUP_MAX	255


#ifdef _P1003_1B_VISIBLE

#define _POSIX_AIO_LISTIO_MAX	16
#define _POSIX_AIO_MAX		1
#define _POSIX_DELAYTIMER_MAX	32
#define _POSIX_MQ_OPEN_MAX	8
#define _POSIX_MQ_PRIO_MAX	32
#define _POSIX_RTSIG_MAX	0
#define _POSIX_SEM_NSEMS_MAX	256
#define _POSIX_SEM_VALUE_MAX	32767
#define _POSIX_SIGQUEUE_MAX	32
#define _POSIX_TIMER_MAX	32

#endif

#endif /* !_ANSI_SOURCE */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE) || defined(_XOPEN_SOURCE)
#define PASS_MAX		128	/* _PASSWORD_LEN from <pwd.h> */

#define NL_ARGMAX		99	/* max # of position args for printf */
#define NL_LANGMAX		31	/* max LANG name length */
#define NL_MSGMAX		32767
#define NL_NMAX			1
#define NL_SETMAX		255
#define NL_TEXTMAX		2048
#endif 

#include <machine/limits.h>
#if !defined(_ANSI_SOURCE)
#include <sys/syslimits.h>
#endif

#endif /* !_LIMITS_H_ */
