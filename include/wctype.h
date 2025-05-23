/*-
 * Copyright (c)1999 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	citrus Id: wctype.h,v 1.4 2000/12/21 01:50:21 itojun Exp
 *	$NetBSD: wctype.h,v 1.3 2000/12/22 14:16:16 itojun Exp $
 * $FreeBSD: src/include/wctype.h,v 1.3.2.1 2001/07/11 23:48:38 obrien Exp $
 * $DragonFly: src/include/wctype.h,v 1.5 2003/11/15 19:28:42 asmodai Exp $
 */

#if 0
/* XXX: not implemented */
#ifndef _WCTYPE_H_
#define	_WCTYPE_H_

#include <sys/cdefs.h>
#ifndef _SYS_STDINT_H_
#include <sys/stdint.h>
#endif

#ifndef _WINT_T_DECLARED
#define _WINT_T_DECLARED
typedef __wint_t	wint_t;
#endif

#ifndef WEOF
#define	WEOF	((wint_t)-1)
#endif

__BEGIN_DECLS
int	iswalnum (wint_t);
int	iswalpha (wint_t);
int	iswblank (wint_t);
int	iswcntrl (wint_t);
int	iswdigit (wint_t);
int	iswgraph (wint_t);
int	iswlower (wint_t);
int	iswprint (wint_t);
int	iswpunct (wint_t);
int	iswspace (wint_t);
int	iswupper (wint_t);
int	iswxdigit (wint_t);
wint_t	towlower (wint_t);
wint_t	towupper (wint_t);
__END_DECLS

#endif		/* _WCTYPE_H_ */
#endif
