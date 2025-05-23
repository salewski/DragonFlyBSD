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
 * @(#)extra.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/backgammon/backgammon/extra.c,v 1.4 1999/11/30 03:48:22 billf Exp $
 * $DragonFly: src/games/backgammon/backgammon/extra.c,v 1.2 2003/06/17 04:25:22 dillon Exp $
 */

#include "back.h"

#ifdef DEBUG
#include <stdio.h>
FILE	*trace;
#endif

/*
 * dble()
 *	Have the current player double and ask opponent to accept.
 */

dble ()  {
	int	resp;			/* response to y/n */

	for (;;)  {
		writel (" doubles.");		/* indicate double */

		if (cturn == -pnum)  {		/* see if computer accepts */
			if (dblgood())  {	    /* guess not */
				writel ("  Declined.\n");
				nexturn();
				cturn *= -2;	    /* indicate loss */
				return;
			} else  {		    /* computer accepts */
				writel ("  Accepted.\n");
				gvalue *= 2;	    /* double game value */
				dlast = cturn;
				if (tflag)
					gwrite();
				return;
			}
		}

						/* ask if player accepts */
		writel ("  Does ");
		writel (cturn == 1? color[2]: color[3]);
		writel (" accept?");

						/* get response from yorn,
						 * a "2" means he said "p"
						 * for print board. */
		if ((resp = yorn ('R')) == 2)  {
			writel ("  Reprint.\n");
			buflush();
			wrboard();
			writel (*Colorptr);
			continue;
		}

						/* check response */
		if (resp)  {
						    /* accepted */
			gvalue *= 2;
			dlast = cturn;
			if (tflag)
				gwrite();
			return;
		}

		nexturn ();			/* declined */
		cturn *= -2;
		return;
	}
}

/*
 * dblgood ()
 *	Returns 1 if the computer would double in this position.  This
 * is not an exact science.  The computer will decline a double that he
 * would have made.  Accumulated judgments are kept in the variable n,
 * which is in "pips", i.e., the position of each man summed over all
 * men, with opponent's totals negative.  Thus, n should have a positive
 * value of 7 for each move ahead, or a negative value of 7 for each one
 * behind.
 */

dblgood ()  {
	int	n;			/* accumulated judgment */
	int	OFFC = *offptr;		/* no. of computer's men off */
	int	OFFO = *offopp;		/* no. of player's men off */

#ifdef DEBUG
	int	i;
	if (trace == NULL)
		trace = fopen ("bgtrace","w");
#endif

						/* get real pip value */
	n = eval()*cturn;
#ifdef DEBUG
	fputs ("\nDoubles:\nBoard: ",trace);
	for (i = 0; i < 26; i++)
		fprintf (trace," %d",board[i]);
	fprintf (trace,"\n\tpip = %d, ",n);
#endif

						/* below adjusts pip value
						 * according to position
						 * judgments */

						/* check men moving off
						 * board */
	if (OFFC > -15 || OFFO > -15)  {
		if (OFFC < 0 && OFFO < 0)  {
			OFFC += 15;
			OFFO += 15;
			n +=((OFFC-OFFO)*7)/2;
		} else if (OFFC < 0)  {
			OFFC += 15;
			n -= OFFO*7/2;
		} else if (OFFO < 0)  {
			OFFO += 15;
			n += OFFC*7/2;
		}
		if (OFFC < 8 && OFFO > 8)
			n -= 7;
		if (OFFC < 10 && OFFO > 10)
			n -= 7;
		if (OFFC < 12 && OFFO > 12)
			n -= 7;
		if (OFFO < 8 && OFFC > 8)
			n += 7;
		if (OFFO < 10 && OFFC > 10)
			n += 7;
		if (OFFO < 12 && OFFC > 12)
			n += 7;
		n += ((OFFC-OFFO)*7)/2;
	}

#ifdef DEBUG
	fprintf (trace,"off = %d, ",n);
#endif

						/* see if men are trapped */
	n -= freemen(bar);
	n += freemen(home);
	n += trapped(home,-cturn);
	n -= trapped(bar,cturn);

#ifdef DEBUG
	fprintf (trace,"free = %d\n",n);
	fprintf (trace,"\tOFFC = %d, OFFO = %d\n",OFFC,OFFO);
	fflush (trace);
#endif

						/* double if 2-3 moves ahead */
	if (n > 10+rnum(7))
		return(1);
	return (0);
}

freemen (b)
int	b;

{
	int		i, inc, lim;

	odds(0,0,0);
	if (board[b] == 0)
		return (0);
	inc = (b == 0? 1: -1);
	lim = (b == 0? 7: 18);
	for (i = b+inc; i != lim; i += inc)
		if (board[i]*inc < -1)
			odds(abs(b-i),0,abs(board[b]));
	if (abs(board[b]) == 1)
		return ((36-count())/5);
	return (count()/5);
}

trapped (n,inc)
int	n, inc;

{
	int		i, j, k;
	int		c, l, ct;

	ct = 0;
	l = n+7*inc;
	for (i = n+inc; i != l; i += inc)  {
		odds (0,0,0);
		c = abs(i-l);
		if (board[i]*inc > 0)  {
			for (j = c; j < 13; j++)
				if (board[i+inc*j]*inc < -1)  {
					if (j < 7)
						odds (j,0,1);
					for (k = 1; k < 7 && k < j; k++)
						if (j-k < 7)
							odds (k,j-k,1);
				}
			ct += abs(board[i])*(36-count());
		}
	}
	return (ct/5);
}

eval ()  {

	int		i, j;

	for (j = i = 0; i < 26; i++)
		j += (board[i] >= 0 ? i*board[i] : (25-i)*board[i]);

	if (off[1] >= 0)
		j += 25*off[1];
	else
		j += 25*(off[1]+15);

	if (off[0] >= 0)
		j -= 25*off[0];
	else
		j -= 25*(off[0]+15);
	return (j);
}
