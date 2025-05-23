/*
 * Copyright (c) 1988, 1993
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
 * @(#) Copyright (c) 1988, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)ppt.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/ppt/ppt.c,v 1.7 1999/12/12 06:30:45 billf Exp $
 * $DragonFly: src/games/ppt/ppt.c,v 1.4 2005/03/18 23:20:34 swildner Exp $
 */

#include <stdio.h>
#include <stdlib.h>

static void	putppt(int);

int
main(int argc, char **argv)
{
	int c;
	char *p;

	puts("___________");
	if (argc > 1)
		while ((p = *++argv))
			for (; *p != '\0'; ++p)
				putppt((int)*p);
	else while ((c = getchar()) != EOF)
		putppt(c);
	puts("___________");
	exit(0);
}

static void
putppt(int c)
{
	int i;

	putchar('|');
	for (i = 7; i >= 0; i--) {
		if (i == 2)
			putchar('.');	/* feed hole */
		if ((c & (1 << i)) != 0)
			putchar('o');
		else
			putchar(' ');
	}
	putchar('|');
	putchar('\n');
}
