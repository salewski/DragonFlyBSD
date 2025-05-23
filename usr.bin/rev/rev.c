/*-
 * Copyright (c) 1987, 1992, 1993
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
 * @(#) Copyright (c) 1987, 1992, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)rev.c	8.3 (Berkeley) 5/4/95
 * $DragonFly: src/usr.bin/rev/rev.c,v 1.6 2004/12/14 18:22:09 liamfoy Exp $
 */

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void	usage(void);
static int	dorev(FILE *, const char *);

int
main(int argc, char **argv)
{
	FILE *fp;
	int ch, i, rval;

	rval = 0;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		rval += dorev(stdin, "stdin");

	for (i = 0; i < argc; ++i) {
		if ((fp = fopen(argv[i], "r")) != NULL) {
			rval += dorev(fp, argv[i]);
			fclose(fp);
		} else {
			warn("failed to open %s", argv[i]);
			rval++ ;
		}
	}

	exit(rval != 0 ? 1 : 0);
}

static void
usage()
{
	fprintf(stderr, "usage: rev [file ...]\n");
	exit(1);
}

static int 
dorev(FILE *fp, const char *filepath)
{
	const char *p, *t;
	size_t len;
	
	while ((p = fgetln(fp, &len)) != NULL) {
			if (p[len - 1] == '\n')
				--len;
			t = p + len - 1;
			for (t = p + len - 1; t >= p; --t)
				putchar(*t);
			putchar('\n');
	}
	
	if (ferror(fp)) {
		warn("%s", filepath);
		/* Reset error indicator */
		clearerr(fp);
		return(1);
	}

	return(0);
}
