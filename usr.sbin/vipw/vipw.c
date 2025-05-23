/*
 * Copyright (c) 1987, 1993, 1994
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
 * @(#) Copyright (c) 1987, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)vipw.c	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/usr.sbin/vipw/vipw.c,v 1.11 1999/10/25 09:46:57 sheldonh Exp $
 * $DragonFly: src/usr.sbin/vipw/vipw.c,v 1.4 2004/12/18 22:48:14 swildner Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pw_util.h"

extern char *mppath;
extern char *masterpasswd;
char *tempname;

void	copyfile(int, int);
static void	usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int pfd, tfd;
	struct stat begin, end;
	int ch;

	while ((ch = getopt(argc, argv, "d:")) != -1)
		switch (ch) {
		case 'd':
			if ((masterpasswd = malloc(strlen(optarg) +
			    strlen(_MASTERPASSWD) + 2)) == NULL)
				err(1, NULL);
			strcpy(masterpasswd, optarg);
			if (masterpasswd[strlen(masterpasswd) - 1] != '/')
				strcat(masterpasswd, "/" _MASTERPASSWD);
			else
				strcat(masterpasswd, _MASTERPASSWD);
			if ((mppath = strdup(optarg)) == NULL)
				err(1, NULL);
			if (mppath[strlen(mppath) - 1] == '/')
				mppath[strlen(mppath) - 1] = '\0';
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	pw_init();
	pfd = pw_lock();
	tfd = pw_tmp();
	copyfile(pfd, tfd);
	close(tfd);
	/* Force umask for partial writes made in the edit phase */
	umask(077);

	for (;;) {
		if (stat(tempname, &begin))
			pw_error(tempname, 1, 1);
		pw_edit(0);
		if (stat(tempname, &end))
			pw_error(tempname, 1, 1);
		if (begin.st_mtime == end.st_mtime) {
			warnx("no changes made");
			pw_error((char *)NULL, 0, 0);
		}
		if (pw_mkdb((char *)NULL))
			break;
		pw_prompt();
	}
	exit(0);
}

void
copyfile(from, to)
	int from, to;
{
	int nr, nw, off;
	char buf[8*1024];

	while ((nr = read(from, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(to, buf + off, nr)) < 0)
				pw_error(tempname, 1, 1);
	if (nr < 0)
		pw_error(masterpasswd, 1, 1);
}

static void
usage()
{

	fprintf(stderr, "usage: vipw [ -d directory ]\n");
	exit(1);
}
