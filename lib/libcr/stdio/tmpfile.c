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
 * $FreeBSD: src/lib/libc/stdio/tmpfile.c,v 1.4.2.2 2003/02/15 05:34:52 kris Exp $
 * $DragonFly: src/lib/libcr/stdio/Attic/tmpfile.c,v 1.2 2003/06/17 04:26:46 dillon Exp $
 *
 * @(#)tmpfile.c	8.1 (Berkeley) 6/4/93
 */

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

FILE *
tmpfile()
{
	sigset_t set, oset;
	FILE *fp;
	int fd, sverrno;
#define	TRAILER	"tmp.XXXXXX"
	char *buf;
	const char *tmpdir;

	tmpdir = NULL;
	if (issetugid() == 0)
		tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = _PATH_TMP;

	(void)asprintf(&buf, "%s%s%s", tmpdir,
	    (tmpdir[strlen(tmpdir) - 1] == '/') ? "" : "/", TRAILER);
	if (buf == NULL)
		return (NULL);

	sigfillset(&set);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);

	fd = mkstemp(buf);
	if (fd != -1)
		(void)unlink(buf);

	free(buf);

	(void)sigprocmask(SIG_SETMASK, &oset, NULL);

	if (fd == -1)
		return (NULL);

	if ((fp = fdopen(fd, "w+")) == NULL) {
		sverrno = errno;
		(void)_close(fd);
		errno = sverrno;
		return (NULL);
	}
	return (fp);
}
