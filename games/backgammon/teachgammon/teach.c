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
 * @(#)teach.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/backgammon/teachgammon/teach.c,v 1.12 1999/11/30 03:48:30 billf Exp $
 * $DragonFly: src/games/backgammon/teachgammon/teach.c,v 1.2 2003/06/17 04:25:22 dillon Exp $
 */

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "back.h"

extern char	*hello[];
extern char	*list[];
extern char	*intro1[];
extern char	*intro2[];
extern char	*moves[];
extern char	*remove[];
extern char	*hits[];
extern char	*endgame[];
extern char	*doubl[];
extern char	*stragy[];
extern char	*prog[];
extern char	*lastch[];

extern char	ospeed;			/* tty output speed for termlib */

const char *const helpm[] = {
	"\nEnter a space or newline to roll, or",
	"     b   to display the board",
	"     d   to double",
	"     q   to quit\n",
	0
};

const char *const contin[] = {
	"",
	0
};

main (argc,argv)
int	argc;
char	**argv;

{
	int	i;

	/* revoke privs */
	setgid(getgid());

	acnt = 1;
	signal (SIGINT,getout);
	if (gtty (0,&tty) == -1)			/* get old tty mode */
		errexit ("teachgammon(gtty)");
	old = tty.sg_flags;
#ifdef V7
	raw = ((noech = old & ~ECHO) | CBREAK);		/* set up modes */
#else
	raw = ((noech = old & ~ECHO) | RAW);		/* set up modes */
#endif
	ospeed = tty.sg_ospeed;				/* for termlib */
	tflag = getcaps (getenv ("TERM"));
	getarg (argc, argv);
	if (tflag)  {
		noech &= ~(CRMOD|XTABS);
		raw &= ~(CRMOD|XTABS);
		clear();
	}
	text (hello);
	text (list);
	i = text (contin);
	if (i == 0)
		i = 2;
	init();
	while (i)
		switch (i)  {

		case 1:
			leave();

		case 2:
			if (i = text(intro1))
				break;
			wrboard();
			if (i = text(intro2))
				break;

		case 3:
			if (i = text(moves))
				break;

		case 4:
			if (i = text(remove))
				break;

		case 5:
			if (i = text(hits))
				break;

		case 6:
			if (i = text(endgame))
				break;

		case 7:
			if (i = text(doubl))
				break;

		case 8:
			if (i = text(stragy))
				break;

		case 9:
			if (i = text(prog))
				break;

		case 10:
			if (i = text(lastch))
				break;
		}
	tutor();
}

leave()  {
	int i;
	if (tflag)
		clear();
	else
		writec ('\n');
	fixtty(old);
	args[0] = strdup("backgammon");
	args[acnt++] = strdup("-n");
	args[acnt] = 0;
	execv (EXEC,args);
	for (i = 0; i < acnt; i++)
	    free (args[i]);
	writel ("Help! Backgammon program is missing\007!!\n");
	exit (-1);
}
