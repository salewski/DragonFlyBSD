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
 * @(#)getcodi.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/trek/getcodi.c,v 1.2 1999/11/30 03:49:48 billf Exp $
 * $DragonFly: src/games/trek/getcodi.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

# include	"getpar.h"

/*
**  get course and distance
**
**	The user is asked for a course and distance.  This is used by
**	move, impulse, and some of the computer functions.
**
**	The return value is zero for success, one for an invalid input
**	(meaning to drop the request).
*/

getcodi(co, di)
int	*co;
double	*di;
{

	*co = getintpar("Course");

	/* course must be in the interval [0, 360] */
	if (*co < 0 || *co > 360)
		return (1);
	*di = getfltpar("Distance");

	/* distance must be in the interval [0, 15] */
	if (*di <= 0.0 || *di > 15.0)
		return (1);

	/* good return */
	return (0);
}
