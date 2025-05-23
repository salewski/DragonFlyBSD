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
 * @(#)kill.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/trek/kill.c,v 1.4 1999/11/30 03:49:49 billf Exp $
 * $DragonFly: src/games/trek/kill.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

# include	"trek.h"

/*
**  KILL KILL KILL !!!
**
**	This file handles the killing off of almost anything.
*/

/*
**  Handle a Klingon's death
**
**	The Klingon at the sector given by the parameters is killed
**	and removed from the Klingon list.  Notice that it is not
**	removed from the event list; this is done later, when the
**	the event is to be caught.  Also, the time left is recomputed,
**	and the game is won if that was the last klingon.
*/

killk(ix, iy)
int	ix, iy;
{
	int		i, j;

	printf("   *** Klingon at %d,%d destroyed ***\n", ix, iy);

	/* remove the scoundrel */
	Now.klings -= 1;
	Sect[ix][iy] = EMPTY;
	Quad[Ship.quadx][Ship.quady].klings -= 1;
	/* %%% IS THIS SAFE???? %%% */
	Quad[Ship.quadx][Ship.quady].scanned -= 100;
	Game.killk += 1;

	/* find the Klingon in the Klingon list */
	for (i = 0; i < Etc.nkling; i++)
		if (ix == Etc.klingon[i].x && iy == Etc.klingon[i].y)
		{
			/* purge him from the list */
			Etc.nkling -= 1;
			for (; i < Etc.nkling; i++)
				bmove(&Etc.klingon[i+1], &Etc.klingon[i], sizeof Etc.klingon[i]);
			break;
		}

	/* find out if that was the last one */
	if (Now.klings <= 0)
		win();

	/* recompute time left */
	Now.time = Now.resource / Now.klings;
	return;
}


/*
**  handle a starbase's death
*/

killb(qx, qy)
int	qx, qy;
{
	struct quad	*q;
	struct xy	*b;

	q = &Quad[qx][qy];

	if (q->bases <= 0)
		return;
	if (!damaged(SSRADIO))
		/* then update starchart */
		if (q->scanned < 1000)
			q->scanned -= 10;
		else
			if (q->scanned > 1000)
				q->scanned = -1;
	q->bases = 0;
	Now.bases -= 1;
	for (b = Now.base; ; b++)
		if (qx == b->x && qy == b->y)
			break;
	bmove(&Now.base[Now.bases], b, sizeof *b);
	if (qx == Ship.quadx && qy == Ship.quady)
	{
		Sect[Etc.starbase.x][Etc.starbase.y] = EMPTY;
		if (Ship.cond == DOCKED)
			undock();
		printf("Starbase at %d,%d destroyed\n", Etc.starbase.x, Etc.starbase.y);
	}
	else
	{
		if (!damaged(SSRADIO))
		{
			printf("Uhura: Starfleet command reports that the starbase in\n");
			printf("   quadrant %d,%d has been destroyed\n", qx, qy);
		}
		else
			schedule(E_KATSB | E_GHOST, 1e50, qx, qy, 0);
	}
}


/**
 **	kill an inhabited starsystem
 **/

kills(x, y, f)
int	x, y;	/* quad coords if f == 0, else sector coords */
int	f;	/* f != 0 -- this quad;  f < 0 -- Enterprise's fault */
{
	struct quad	*q;
	struct event	*e;
	char		*name;
	char			*systemname();

	if (f)
	{
		/* current quadrant */
		q = &Quad[Ship.quadx][Ship.quady];
		Sect[x][y] = EMPTY;
		name = systemname(q);
		if (name == 0)
			return;
		printf("Inhabited starsystem %s at %d,%d destroyed\n",
			name, x, y);
		if (f < 0)
			Game.killinhab += 1;
	}
	else
	{
		/* different quadrant */
		q = &Quad[x][y];
	}
	if (q->qsystemname & Q_DISTRESSED)
	{
		/* distressed starsystem */
		e = &Event[q->qsystemname & Q_SYSTEM];
		printf("Distress call for %s invalidated\n",
			Systemname[e->systemname]);
		unschedule(e);
	}
	q->qsystemname = 0;
	q->stars -= 1;
}


/**
 **	"kill" a distress call
 **/

killd(x, y, f)
int	x, y;		/* quadrant coordinates */
int	f;		/* set if user is to be informed */
{
	struct event	*e;
	int		i;
	struct quad	*q;

	q = &Quad[x][y];
	for (i = 0; i < MAXEVENTS; i++)
	{
		e = &Event[i];
		if (e->x != x || e->y != y)
			continue;
		switch (e->evcode)
		{
		  case E_KDESB:
			if (f)
			{
				printf("Distress call for starbase in %d,%d nullified\n",
					x, y);
				unschedule(e);
			}
			break;

		  case E_ENSLV:
		  case E_REPRO:
			if (f)
			{
				printf("Distress call for %s in quadrant %d,%d nullified\n",
					Systemname[e->systemname], x, y);
				q->qsystemname = e->systemname;
				unschedule(e);
			}
			else
			{
				e->evcode |= E_GHOST;
			}
		}
	}
}
