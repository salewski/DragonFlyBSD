/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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
 * @(#)misc.c	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/usr.bin/ar/misc.c,v 1.6.6.1 2001/08/02 00:51:00 obrien Exp $
 * $DragonFly: src/usr.bin/ar/Attic/misc.c,v 1.4 2005/01/13 18:57:56 okumoto Exp $
 */

#include <sys/param.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "extern.h"
#include "pathnames.h"

char const *tname = "temporary file";		/* temporary file "name" */

int
tmp(void)
{
	sigset_t set, oset;
	static int first;
	int fd;
	char path[MAXPATHLEN];

	if (!first && !envtmp) {
		envtmp = getenv("TMPDIR");
		first = 1;
	}

	if (envtmp)
		sprintf(path, "%s/%s", envtmp, _NAME_ARTMP);
	else
		strcpy(path, _PATH_ARTMP);

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oset);
	if ((fd = mkstemp(path)) == -1)
		error(tname);
        unlink(path);
	sigprocmask(SIG_SETMASK, &oset, NULL);
	return (fd);
}

/*
 * files --
 *	See if the current file matches any file in the argument list; if it
 * 	does, remove it from the argument list.
 */
char *
files(char **argv)
{
	char **list, *p;

	for (list = argv; *list; ++list)
		if (compare(*list)) {
			p = *list;
			for (; (list[0] = list[1]); ++list)
				continue;
			return (p);
		}
	return (NULL);
}

void
orphans(char **argv)
{

	for (; *argv; ++argv)
		warnx("%s: not found in archive", *argv);
}

int
compare(char *dest)
{
	int maxname = (options & AR_TR) ? OLDARMAXNAME : MAXNAMLEN;
	return (!strncmp(chdr.name, basename(dest), maxname));
}

void
badfmt(void)
{

	errx(1, "%s: %s", archive, strerror(EFTYPE));
}

void
error(char const *name)
{

	err(1, "%s", name);
}
