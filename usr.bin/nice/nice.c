/*
 * Copyright (c) 1989, 1993, 1994
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
 * @(#) Copyright (c) 1989, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)nice.c	8.2 (Berkeley) 4/16/94
 * $FreeBSD: src/usr.bin/nice/nice.c,v 1.4.2.1 2002/06/18 08:40:28 tjr Exp $
 * $DragonFly: src/usr.bin/nice/nice.c,v 1.5 2005/02/07 18:00:46 liamfoy Exp $
 */

#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	DEFNICE	10

static void	usage(void);

int
main(int argc, char **argv)
{
	long niceness = DEFNICE;
	int ch;
	char *ep;

	/* Obsolescent syntax: -number, --number */
	if (argc >= 2 && argv[1][0] == '-' && (argv[1][1] == '-' ||
	    isdigit(argv[1][1])) && strcmp(argv[1], "--") != 0)
		if (asprintf(&argv[1], "-n%s", argv[1] + 1) < 0)
			err(1, "asprintf");

	while ((ch = getopt(argc, argv, "n:")) != -1) {
		switch (ch) {
		case 'n':
			errno = 0;
			niceness = strtol(optarg, &ep, 10);
			if (ep == optarg || *ep != '\0' || errno ||
			    niceness < INT_MIN || niceness > INT_MAX)
				errx(1, "%s: invalid nice value", optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	errno = 0;
	niceness += getpriority(PRIO_PROCESS, 0);
	if (errno)
		warn("getpriority");
	else if (setpriority(PRIO_PROCESS, 0, (int)niceness))
		warn("setpriority");
	execvp(*argv, argv);
	err(errno == ENOENT ? 127 : 126, "%s", *argv);
}

static void
usage(void)
{

	fprintf(stderr, "usage: nice [-n incr] utility [arguments]\n");
	exit(1);
}
