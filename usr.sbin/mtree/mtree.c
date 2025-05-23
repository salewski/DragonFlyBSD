/*-
 * Copyright (c) 1989, 1990, 1993
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
 * @(#) Copyright (c) 1989, 1990, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)mtree.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.sbin/mtree/mtree.c,v 1.8.2.3 2003/05/07 17:55:17 tobez Exp $
 * $DragonFly: src/usr.sbin/mtree/mtree.c,v 1.5 2004/03/15 16:24:22 dillon Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <unistd.h>
#include "mtree.h"
#include "extern.h"

extern long int crc_total;

int ftsoptions = FTS_PHYSICAL;
int cflag, dflag, eflag, iflag, nflag, qflag, rflag, sflag, uflag, Uflag;
u_int keys;
char fullpath[MAXPATHLEN];

static void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char *dir, *p;
	int status;

	dir = NULL;
	keys = KEYDEFAULT;
	init_excludes();

	while ((ch = getopt(argc, argv, "cdef:iK:k:LnPp:qrs:UuxX:")) != -1)
		switch((char)ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'f':
			if (!(freopen(optarg, "r", stdin)))
				err(1, "%s", optarg);
			break;
		case 'i':
			iflag = 1;
			break;
		case 'K':
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'k':
			keys = F_TYPE;
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'L':
			ftsoptions &= ~FTS_PHYSICAL;
			ftsoptions |= FTS_LOGICAL;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'P':
			ftsoptions &= ~FTS_LOGICAL;
			ftsoptions |= FTS_PHYSICAL;
			break;
		case 'p':
			dir = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			crc_total = ~strtol(optarg, &p, 0);
			if (*p)
				errx(1, "illegal seed value -- %s", optarg);
			break;
		case 'U':
			Uflag = 1;
			uflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case 'X':
			read_excludes_file(optarg);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (dir && chdir(dir))
		err(1, "%s", dir);

	if ((cflag || sflag) && !getwd(fullpath))
		errx(1, "%s", fullpath);

	if (cflag) {
		cwalk();
		exit(0);
	}
	status = verify();
	if (Uflag & (status == MISMATCHEXIT))
		status = 0;
	exit(status);
}

static void
usage(void)
{

	fprintf(stderr,
"usage: mtree [-LPUcdeinqrux] [-f spec] [-K key] [-k key] [-p path] [-s seed]\n"
"\t[-X excludes]\n");
	exit(1);
}
