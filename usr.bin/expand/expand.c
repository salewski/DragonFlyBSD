/*
 * Copyright (c) 1980, 1993
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
 * @(#) Copyright (c) 1980, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)expand.c	8.1 (Berkeley) 6/9/93
 * $FreeBSD: src/usr.bin/expand/expand.c,v 1.5.2.6 2002/07/09 10:47:59 tjr Exp $
 * $DragonFly: src/usr.bin/expand/expand.c,v 1.3 2003/10/04 20:36:44 hmp Exp $
 */

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * expand - expand tabs to equivalent spaces
 */
int	nstops;
int	tabstops[100];

static void getstops(char *);
static void usage(void);

int
main(int argc, char **argv)
{
	register int c, column;
	register int n;
	int rval;

	setlocale(LC_CTYPE, "");

	/* handle obsolete syntax */
	while (argc > 1 && argv[1][0] == '-' &&
	    isdigit((unsigned char)argv[1][1])) {
		getstops(&argv[1][1]);
		argc--; argv++;
	}

	while ((c = getopt (argc, argv, "t:")) != -1) {
		switch (c) {
		case 't':
			getstops(optarg);
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	rval = 0;
	do {
		if (argc > 0) {
			if (freopen(argv[0], "r", stdin) == NULL) {
				warn("%s", argv[0]);
				rval = 1;
				argc--, argv++;
				continue;
			}
			argc--, argv++;
		}
		column = 0;
		while ((c = getchar()) != EOF) {
			switch (c) {
			case '\t':
				if (nstops == 0) {
					do {
						putchar(' ');
						column++;
					} while (column & 07);
					continue;
				}
				if (nstops == 1) {
					do {
						putchar(' ');
						column++;
					} while (((column - 1) % tabstops[0]) != (tabstops[0] - 1));
					continue;
				}
				for (n = 0; n < nstops; n++)
					if (tabstops[n] > column)
						break;
				if (n == nstops) {
					putchar(' ');
					column++;
					continue;
				}
				while (column < tabstops[n]) {
					putchar(' ');
					column++;
				}
				continue;

			case '\b':
				if (column)
					column--;
				putchar('\b');
				continue;

			default:
				putchar(c);
				if (isprint(c))
					column++;
				continue;

			case '\n':
				putchar(c);
				column = 0;
				continue;
			}
		}
	} while (argc > 0);
	exit(rval);
}

static void
getstops(register char *cp)
{
	register int i;

	nstops = 0;
	for (;;) {
		i = 0;
		while (*cp >= '0' && *cp <= '9')
			i = i * 10 + *cp++ - '0';
		if (i <= 0)
			errx(1, "bad tab stop spec");
		if (nstops > 0 && i <= tabstops[nstops-1])
			errx(1, "bad tab stop spec");
		if (nstops == sizeof(tabstops) / sizeof(*tabstops))
			errx(1, "too many tab stops");
		tabstops[nstops++] = i;
		if (*cp == 0)
			break;
		if (*cp != ',' && !isblank((unsigned char)*cp))
			errx(1, "bad tab stop spec");
		cp++;
	}
}

static void
usage(void)
{
	(void)fprintf (stderr, "usage: expand [-t tablist] [file ...]\n");
	exit(1);
}
