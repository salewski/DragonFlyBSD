/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Arnold.
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
 * $FreeBSD: src/games/fortune/unstr/unstr.c,v 1.5 1999/11/16 02:57:01 billf Exp $
 * $DragonFly: src/games/fortune/unstr/unstr.c,v 1.2 2003/06/17 04:25:24 dillon Exp $
 *
 * @(#) Copyright (c) 1991, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)unstr.c     8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/fortune/unstr/unstr.c,v 1.5 1999/11/16 02:57:01 billf Exp $
 */

/*
 *	This program un-does what "strfile" makes, thereby obtaining the
 * original file again.  This can be invoked with the name of the output
 * file, the input file, or both. If invoked with only a single argument
 * ending in ".dat", it is pressumed to be the input file and the output
 * file will be the same stripped of the ".dat".  If the single argument
 * doesn't end in ".dat", then it is presumed to be the output file, and
 * the input file is that name prepended by a ".dat".  If both are given
 * they are treated literally as the input and output files.
 *
 *	Ken Arnold		Aug 13, 1978
 */

# include	<sys/param.h>
# include	<stdio.h>
# include	<ctype.h>
# include       <string.h>
# include	"strfile.h"

char	*Infile,			/* name of input file */
	Datafile[MAXPATHLEN],		/* name of data file */
	Delimch;			/* delimiter character */

FILE	*Inf, *Dataf;

void getargs(), order_unstr();

/* ARGSUSED */
int main(ac, av)
int	ac;
char	**av;
{
	static STRFILE	tbl;		/* description table */

	getargs(av);
	if ((Inf = fopen(Infile, "r")) == NULL) {
		perror(Infile);
		exit(1);
	}
	if ((Dataf = fopen(Datafile, "r")) == NULL) {
		perror(Datafile);
		exit(1);
	}
	(void) fread((char *) &tbl, sizeof tbl, 1, Dataf);
	tbl.str_version = ntohl(tbl.str_version);
	tbl.str_numstr = ntohl(tbl.str_numstr);
	tbl.str_longlen = ntohl(tbl.str_longlen);
	tbl.str_shortlen = ntohl(tbl.str_shortlen);
	tbl.str_flags = ntohl(tbl.str_flags);
	if (!(tbl.str_flags & (STR_ORDERED | STR_RANDOM))) {
		fprintf(stderr, "nothing to do -- table in file order\n");
		exit(1);
	}
	Delimch = tbl.str_delim;
	order_unstr(&tbl);
	(void) fclose(Inf);
	(void) fclose(Dataf);
	exit(0);
}

void getargs(av)
char	*av[];
{
	if (!*++av) {
		(void) fprintf(stderr, "usage: unstr datafile\n");
		exit(1);
	}
	Infile = *av;
	(void) strcpy(Datafile, Infile);
	(void) strcat(Datafile, ".dat");
}

void order_unstr(tbl)
STRFILE	*tbl;
{
	int	i;
	char	*sp;
	long            pos;
	char		buf[BUFSIZ];

	for (i = 0; i < tbl->str_numstr; i++) {
		(void) fread((char *) &pos, 1, sizeof pos, Dataf);
		(void) fseek(Inf, ntohl(pos), 0);
		if (i != 0)
			(void) printf("%c\n", Delimch);
		for (;;) {
			sp = fgets(buf, sizeof buf, Inf);
			if (sp == NULL || STR_ENDSTRING(sp, *tbl))
				break;
			else
				fputs(sp, stdout);
		}
	}
}
