/*
 * Copyright (c) 1980, 1987, 1993
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
 * @(#) Copyright (c) 1980, 1987, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)users.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/users/users.c,v 1.5 1999/08/28 01:07:14 peter Exp $
 * $DragonFly: src/usr.bin/users/users.c,v 1.4 2004/09/12 11:54:00 joerg Exp $
 */

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

typedef char   namebuf[UT_NAMESIZE];

static int scmp(const void *, const void *);
static void usage(void);

int
main(int argc, char **argv)
{
	namebuf *names = NULL;
	int ncnt = 0;
	int nmax = 0;
	int cnt;
	struct utmp utmp;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch(ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!freopen(_PATH_UTMP, "r", stdin))
		err(1, "can't open %s", _PATH_UTMP);
	while (fread((char *)&utmp, sizeof(utmp), 1, stdin) == 1) {
		if (*utmp.ut_name) {
			if (ncnt >= nmax) {
				nmax += 32;
				names = realloc(names,
					sizeof (*names) * nmax);
				if (names == NULL)
					err(1, "realloc");
			}
			strncpy(names[ncnt], utmp.ut_name, UT_NAMESIZE);
			++ncnt;
		}
	}
	if (ncnt) {
		qsort(names, ncnt, UT_NAMESIZE, scmp);
		printf("%.*s", UT_NAMESIZE, names[0]);
		for (cnt = 1; cnt < ncnt; ++cnt) {
			if (strncmp(names[cnt], names[cnt - 1], UT_NAMESIZE))
				printf(" %.*s", UT_NAMESIZE, names[cnt]);
		}
		printf("\n");
	}
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: users\n");
	exit(1);
}
	
int
scmp(const void *p, const void *q)
{
	return(strncmp((const char *)p, (const char *)q, UT_NAMESIZE));
}
