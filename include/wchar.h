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
 * $FreeBSD: src/include/wchar.h,v 1.3.2.2 2002/08/08 02:42:29 imp Exp $
 * $DragonFly: src/include/wchar.h,v 1.5 2003/11/15 19:28:42 asmodai Exp $
 */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: wchar.h,v 1.8 2000/12/22 05:31:42 itojun Exp $
 */

#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <sys/cdefs.h>
#ifndef _SYS_STDINT_H_
#include <sys/stdint.h>	/* __wchar_t and friends */
#endif

#ifndef NULL
#define NULL	0
#endif

#ifndef __cplusplus
#ifndef _WCHAR_T_DECLARED
#define _WCHAR_T_DECLARED
typedef __wchar_t	wchar_t;
#endif
#endif

#ifndef _MBSTATE_T_DECLARED
#define _MBSTATE_T_DECLARED
typedef	__mbstate_t	mbstate_t;
#endif

#ifndef _WINT_T_DECLARED
#define _WINT_T_DECLARED
typedef	__wint_t	wint_t;
#endif

#ifndef _SIZE_T_DECLARED
#define _SIZE_T_DECLARED
typedef __size_t        size_t;		/* open group */
#endif

#ifndef WEOF
#define	WEOF 	((wint_t)-1)
#endif

__BEGIN_DECLS

wchar_t	*wcscat (wchar_t * __restrict, const wchar_t * __restrict);
wchar_t	*wcschr (const wchar_t *, wchar_t);
int	wcscmp (const wchar_t *, const wchar_t *);
wchar_t	*wcscpy (wchar_t * __restrict, const wchar_t * __restrict);
size_t	wcscspn (const wchar_t *, const wchar_t *);
size_t	wcslen (const wchar_t *);
wchar_t	*wcsncat (wchar_t * __restrict, const wchar_t * __restrict,
	    size_t);
int	wcsncmp (const wchar_t *, const wchar_t *, size_t);
wchar_t	*wcsncpy (wchar_t * __restrict , const wchar_t * __restrict,
	    size_t);
wchar_t	*wcspbrk (const wchar_t *, const wchar_t *);
wchar_t	*wcsrchr (const wchar_t *, wchar_t);
#if 0
/* XXX: not implemented */
size_t	wcsrtombs (char * __restrict, const wchar_t ** __restrict, size_t,
	    mbstate_t * __restrict);
#endif
size_t	wcsspn (const wchar_t *, const wchar_t *);
wchar_t	*wcsstr (const wchar_t *, const wchar_t *);
wchar_t	*wmemchr (const wchar_t *, wchar_t, size_t);
int	wmemcmp (const wchar_t *, const wchar_t *, size_t);
wchar_t	*wmemcpy (wchar_t * __restrict, const wchar_t * __restrict,
	    size_t);
wchar_t	*wmemmove (wchar_t *, const wchar_t *, size_t);
wchar_t	*wmemset (wchar_t *, wchar_t, size_t);

size_t	wcslcat (wchar_t *, const wchar_t *, size_t);
size_t	wcslcpy (wchar_t *, const wchar_t *, size_t);
#if 0
/* XXX: not implemented */
int	wcswidth (const wchar_t *, size_t);
int	wcwidth (wchar_t);
#endif
__END_DECLS

#endif /* !_WCHAR_H_ */
