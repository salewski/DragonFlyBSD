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
 * @(#)torped.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/trek/torped.c,v 1.5 1999/11/30 03:49:55 billf Exp $
 * $DragonFly: src/games/trek/torped.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

# include	<stdio.h>
# include	"trek.h"

/*
**  PHOTON TORPEDO CONTROL
**
**	Either one or three photon torpedoes are fired.  If three
**	are fired, it is called a "burst" and you also specify
**	a spread angle.
**
**	Torpedoes are never 100% accurate.  There is always a random
**	cludge factor in their course which is increased if you have
**	your shields up.  Hence, you will find that they are more
**	accurate at close range.  However, they have the advantage that
**	at long range they don't lose any of their power as phasers
**	do, i.e., a hit is a hit is a hit, by any other name.
**
**	When the course spreads too much, you get a misfire, and the
**	course is randomized even more.  You also have the chance that
**	the misfire damages your torpedo tubes.
*/


torped()
{
	int		ix, iy;
	double			x, y, dx, dy;
	double			angle;
	int			course, course2;
	int		k;
	double			bigger;
	double			sectsize;
	int			burst;
	int			n;

	if (Ship.cloaked)
	{
		return (printf("Federation regulations do not permit attack while cloaked.\n"));
	}
	if (check_out(TORPED))
		return;
	if (Ship.torped <= 0)
	{
		return (printf("All photon torpedos expended\n"));
	}

	/* get the course */
	course = getintpar("Torpedo course");
	if (course < 0 || course > 360)
		return;
	burst = -1;

	/* need at least three torpedoes for a burst */
	if (Ship.torped < 3)
	{
		printf("No-burst mode selected\n");
		burst = 0;
	}
	else
	{
		/* see if the user wants one */
		if (!testnl())
		{
			k = ungetc(cgetc(0), stdin);
			if (k >= '0' && k <= '9')
				burst = 1;
		}
	}
	if (burst < 0)
	{
		burst = getynpar("Do you want a burst");
	}
	if (burst)
	{
		burst = getintpar("burst angle");
		if (burst <= 0)
			return;
		if (burst > 15)
			return (printf("Maximum burst angle is 15 degrees\n"));
	}
	sectsize = NSECTS;
	n = -1;
	if (burst)
	{
		n = 1;
		course -= burst;
	}
	for (; n && n <= 3; n++)
	{
		/* select a nice random course */
		course2 = course + randcourse(n);
		angle = course2 * 0.0174532925;			/* convert to radians */
		dx = -cos(angle);
		dy =  sin(angle);
		bigger = fabs(dx);
		x = fabs(dy);
		if (x > bigger)
			bigger = x;
		dx /= bigger;
		dy /= bigger;
		x = Ship.sectx + 0.5;
		y = Ship.secty + 0.5;
		if (Ship.cond != DOCKED)
			Ship.torped -= 1;
		printf("Torpedo track");
		if (n > 0)
			printf(", torpedo number %d", n);
		printf(":\n%6.1f\t%4.1f\n", x, y);
		while (1)
		{
			ix = x += dx;
			iy = y += dy;
			if (x < 0.0 || x >= sectsize || y < 0.0 || y >= sectsize)
			{
				printf("Torpedo missed\n");
				break;
			}
			printf("%6.1f\t%4.1f\n", x, y);
			switch (Sect[ix][iy])
			{
			  case EMPTY:
				continue;

			  case HOLE:
				printf("Torpedo disappears into a black hole\n");
				break;

			  case KLINGON:
				for (k = 0; k < Etc.nkling; k++)
				{
					if (Etc.klingon[k].x != ix || Etc.klingon[k].y != iy)
						continue;
					Etc.klingon[k].power -= 500 + ranf(501);
					if (Etc.klingon[k].power > 0)
					{
						printf("*** Hit on Klingon at %d,%d: extensive damages\n",
							ix, iy);
						break;
					}
					killk(ix, iy);
					break;
				}
				break;

			  case STAR:
				nova(ix, iy);
				break;

			  case INHABIT:
				kills(ix, iy, -1);
				break;

			  case BASE:
				killb(Ship.quadx, Ship.quady);
				Game.killb += 1;
				break;
			  default:
				printf("Unknown object %c at %d,%d destroyed\n",
					Sect[ix][iy], ix, iy);
				Sect[ix][iy] = EMPTY;
				break;
			}
			break;
		}
		if (damaged(TORPED) || Quad[Ship.quadx][Ship.quady].stars < 0)
			break;
		course += burst;
	}
	Move.free = 0;
}


/*
**  RANDOMIZE COURSE
**
**	This routine randomizes the course for torpedo number 'n'.
**	Other things handled by this routine are misfires, damages
**	to the tubes, etc.
*/

randcourse(n)
int	n;
{
	double			r;
	int		d;

	d = ((franf() + franf()) - 1.0) * 20;
	if (abs(d) > 12)
	{
		printf("Photon tubes misfire");
		if (n < 0)
			printf("\n");
		else
			printf(" on torpedo %d\n", n);
		if (ranf(2))
		{
			damage(TORPED, 0.2 * abs(d) * (franf() + 1.0));
		}
		d *= 1.0 + 2.0 * franf();
	}
	if (Ship.shldup || Ship.cond == DOCKED)
	{
		r = Ship.shield;
		r = 1.0 + r / Param.shield;
		if (Ship.cond == DOCKED)
			r = 2.0;
		d *= r;
	}
	return (d);
}
