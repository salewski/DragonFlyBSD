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
 * citrus Id: wcsrchr.c,v 1.2 2000/12/21 05:07:25 itojun Exp
 * $NetBSD: wcsrchr.c,v 1.1 2000/12/23 23:14:37 itojun Exp $
 * $FreeBSD: src/lib/libc/string/wcsrchr.c,v 1.3.2.1 2001/07/11 23:48:38 obrien Exp $
 * $DragonFly: src/lib/libc/string/wcsrchr.c,v 1.2 2003/06/17 04:26:47 dillon Exp $
 */

#include <assert.h>
#include <wchar.h>

wchar_t *
wcsrchr(s, c)
	const wchar_t *s;
	wchar_t c;
{
	const wchar_t *p;

	p = s;
	while (*p)
		p++;
	while (s <= p) {
		if (*p == c) {
			/* LINTED interface specification */
			return (wchar_t *)p;
		}
		p--;
	}
	return NULL;
}
