/*
 * Copyright (c) 1987, 1993
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
 * @(#)herror.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libc/net/herror.c,v 1.8 1999/08/28 00:00:09 peter Exp $
 * $DragonFly: src/lib/libcr/net/Attic/herror.c,v 1.3 2004/10/25 19:38:25 drhodus Exp $
 */

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

const char *h_errlist[] = {
	"Resolver Error 0 (no error)",
	"Unknown host",				/* 1 HOST_NOT_FOUND */
	"Host name lookup failure",		/* 2 TRY_AGAIN */
	"Unknown server error",			/* 3 NO_RECOVERY */
	"No address associated with name",	/* 4 NO_ADDRESS */
};
int	h_nerr = { sizeof h_errlist / sizeof h_errlist[0] };

int	h_errno;

/*
 * herror --
 *	print the error indicated by the h_errno value.
 */
void
herror(s)
	const char *s;
{
	struct iovec iov[4];
	struct iovec *v = iov;

	if (s && *s) {
		v->iov_base = (char *)s;
		v->iov_len = strlen(s);
		v++;
		v->iov_base = ": ";
		v->iov_len = 2;
		v++;
	}
	v->iov_base = (char *)hstrerror(h_errno);
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = "\n";
	v->iov_len = 1;
	writev(STDERR_FILENO, iov, (v - iov) + 1);
}

const char *
hstrerror(err)
	int err;
{
	if (err < 0)
		return ("Resolver internal error");
	else if (err < h_nerr)
		return (h_errlist[err]);
	return ("Unknown resolver error");
}
