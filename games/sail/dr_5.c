/*
 * Copyright (c) 1983, 1993
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
 * @(#)dr_5.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/sail/dr_5.c,v 1.4 1999/11/30 03:49:33 billf Exp $
 * $DragonFly: src/games/sail/dr_5.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

#include "externs.h"

subtract(from, totalfrom, crewfrom, fromcap, pcfrom)
struct ship *from, *fromcap;
int pcfrom;
int  totalfrom, crewfrom[3];
{
	int n;

	if (fromcap == from && totalfrom) {		/* if not captured */
		for (n = 0; n < 3; n++) {
			if (totalfrom > crewfrom[n]) {
				totalfrom -= crewfrom[n];
				crewfrom[n] = 0;
			} else {
				crewfrom[n] -= totalfrom;
				totalfrom = 0;
			}
		}
		Write(W_CREW, from, 0, crewfrom[0], crewfrom[1], crewfrom[2], 0);
	} else if (totalfrom) {
		pcfrom -= totalfrom;
		pcfrom = pcfrom < 0 ? 0 : pcfrom;
		Write(W_PCREW, from, 0, pcfrom, 0, 0, 0);
	}
}

mensent(from, to, crew, captured, pc, isdefense)
struct ship *from, *to, **captured;
int crew[3], *pc;
char isdefense;
{					/* returns # of crew squares sent */
	int men = 0;
	int n;
	int c1, c2, c3;
	struct BP *bp;

	*pc = from->file->pcrew;
	*captured = from->file->captured;
	crew[0] = from->specs->crew1;
	crew[1] = from->specs->crew2;
	crew[2] = from->specs->crew3;
	bp = isdefense ? from->file->DBP : from->file->OBP;
	for (n=0; n < NBP; n++, bp++) {
		if (bp->turnsent && bp->toship == to)
			men += bp->mensent;
	}
	if (men) {
		c1 = men/100 ? crew[0] : 0;
		c2 = (men%100)/10 ? crew[1] : 0;
		c3 = men/10 ? crew[2] : 0;
		c3 = *captured == 0 ? crew[2] : *pc;
	} else
		c1 = c2 = c3 = 0;
	return(c1 + c2 + c3);
}
