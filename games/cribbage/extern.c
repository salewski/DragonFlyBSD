/*-
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
 * @(#)extern.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/cribbage/extern.c,v 1.2 1999/11/30 03:48:45 billf Exp $
 * $DragonFly: src/games/cribbage/extern.c,v 1.3 2005/03/02 16:14:03 eirikn Exp $
 */

#include <curses.h>

#include "deck.h"
#include "cribbage.h"

BOOLEAN	explain		= FALSE;	/* player mistakes explained */
BOOLEAN	iwon		= FALSE;	/* if comp won last game */
BOOLEAN	quiet		= FALSE;	/* if suppress random mess */
BOOLEAN	rflag		= FALSE;	/* if all cuts random */

char	expl[128];			/* explanation */

int	cgames		= 0;		/* number games comp won */
int	cscore		= 0;		/* comp score in this game */
int	gamecount	= 0;		/* number games played */
int	glimit		= LGAME;	/* game playe to glimit */
int	knownum		= 0;		/* number of cards we know */
int	pgames		= 0;		/* number games player won */
int	pscore		= 0;		/* player score in this game */

CARD	chand[FULLHAND];		/* computer's hand */
CARD	crib[CINHAND];			/* the crib */
CARD	deck[CARDS];			/* a deck */
CARD	known[CARDS];			/* cards we have seen */
CARD	phand[FULLHAND];		/* player's hand */
CARD	turnover;			/* the starter */

WINDOW	*Compwin;			/* computer's hand window */
WINDOW	*Msgwin;			/* messages for the player */
WINDOW	*Playwin;			/* player's hand window */
WINDOW	*Tablewin;			/* table window */
