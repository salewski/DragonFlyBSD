/*-
 * Copyright (c) 1980, 1988, 1993
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
 * @(#) Copyright (c) 1980, 1988, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)tput.c	8.2 (Berkeley) 3/19/94
 * $FreeBSD: src/usr.bin/tput/tput.c,v 1.7.6.3 2002/08/17 14:52:50 tjr Exp $
 * $DragonFly: src/usr.bin/tput/tput.c,v 1.4 2005/02/04 20:00:25 cpressey Exp $
 */

#include <termios.h>

#include <err.h>
#include <termcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef putchar
#define outc putchar

static void   prlongname(char *);
static void   usage(void);
static char **process(const char *, const char *, char **);

int
main(int argc, char **argv)
{
	int ch, exitval, n;
	char *cptr, *term, buf[1024], tbuf[1024];
	const char *p;

	term = NULL;
	while ((ch = getopt(argc, argv, "T:")) != -1)
		switch(ch) {
		case 'T':
			term = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!term && !(term = getenv("TERM")))
errx(2, "no terminal type specified and no TERM environmental variable.");
	if (tgetent(tbuf, term) != 1)
		err(2, "tgetent failure");
	for (exitval = 0; (p = *argv) != NULL; ++argv) {
		switch (*p) {
		case 'c':
			if (!strcmp(p, "clear"))
				p = "cl";
			break;
		case 'i':
			if (!strcmp(p, "init"))
				p = "is";
			break;
		case 'l':
			if (!strcmp(p, "longname")) {
				prlongname(tbuf);
				continue;
			}
			break;
		case 'r':
			if (!strcmp(p, "reset"))
				p = "rs";
			break;
		}
		cptr = buf;
		if (tgetstr(p, &cptr))
			argv = process(p, buf, argv);
		else if ((n = tgetnum(p)) != -1)
			(void)printf("%d\n", n);
		else
			exitval = !tgetflag(p);
	}
	exit(exitval);
}

static void
prlongname(char *buf)
{
	int savech;
	char *p, *savep;

	for (p = buf; *p && *p != ':'; ++p);
	savech = *(savep = p);
	for (*p = '\0'; p >= buf && *p != '|'; --p);
	(void)printf("%s\n", p + 1);
	*savep = savech;
}

static char **
process(const char *cap, const char *str, char **argv)
{
	static char errfew[] =
	    "not enough arguments (%d) for capability `%s'";
	static char errmany[] =
	    "too many arguments (%d) for capability `%s'";
	static char erresc[] =
	    "unknown %% escape `%c' for capability `%s'";
	const char *cp;
	int arg_need, arg_rows, arg_cols;

	/* Count how many values we need for this capability. */
	for (cp = str, arg_need = 0; *cp != '\0'; cp++)
		if (*cp == '%')
			    switch (*++cp) {
			    case 'd':
			    case '2':
			    case '3':
			    case '.':
			    case '+':
				    arg_need++;
				    break;
			    case '%':
			    case '>':
			    case 'i':
			    case 'r':
			    case 'n':
			    case 'B':
			    case 'D':
				    break;
			    case 'p':
				    if (cp[1]) {
					cp++;
					break;
				    }
			    default:
				/*
				 * hpux has lot's of them, but we complain
				 */
				 warnx(erresc, *cp, cap);
			    }

	/* And print them. */
	switch (arg_need) {
	case 0:
		(void)tputs(str, 1, outc);
		break;
	case 1:
		arg_cols = 0;

		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 1, cap);
		arg_rows = atoi(*argv);

		(void)tputs(tgoto(str, arg_cols, arg_rows), 1, outc);
		break;
	case 2:
		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 2, cap);
		arg_cols = atoi(*argv);

		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 2, cap);
		arg_rows = atoi(*argv);

		(void) tputs(tgoto(str, arg_cols, arg_rows), arg_rows, outc);
		break;

	default:
		errx(2, errmany, arg_need, cap);
	}
	return (argv);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: tput [-T term] attribute ...\n");
	exit(1);
}
