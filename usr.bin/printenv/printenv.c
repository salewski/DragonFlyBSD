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
 * @(#) Copyright (c) 1987, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)printenv.c	8.2 (Berkeley) 5/4/95
 *
 * $DragonFly: src/usr.bin/printenv/printenv.c,v 1.4 2005/01/07 02:32:50 cpressey Exp $
 */

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static void	usage(void);

/*
 * printenv
 *
 * Bill Joy, UCB
 * February, 1979
 */
int
main(int argc, char **argv)
{
	char *cp, **ep;
	size_t len;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch(ch) {
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		for (ep = environ; *ep != NULL; ep++)
			printf("%s\n", *ep);
		exit(0);
	}
	len = strlen(*argv);
	for (ep = environ; *ep != NULL; ep++) {
		if (memcmp(*ep, *argv, len) == 0) {
			cp = *ep + len;
			if (*cp == '=') {
				printf("%s\n", cp + 1);
				exit(0);
			}
			if (*cp == '\0')
				exit(0);
		}
	}
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: printenv [name]\n");
	exit(1);
}
