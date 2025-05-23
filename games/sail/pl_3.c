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
 * @(#)pl_3.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/sail/pl_3.c,v 1.6 1999/11/30 03:49:37 billf Exp $
 * $DragonFly: src/games/sail/pl_3.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

#include "player.h"

acceptcombat()
{
	int men = 0;
	int target, temp;
	int n, r;
	int index, rakehim, sternrake;
	int hhits = 0, ghits = 0, rhits = 0, chits = 0;
	int crew[3];
	int load;
	int guns, car, ready, shootat, hit;
	int roll;
	struct ship *closest;

	crew[0] = mc->crew1;
	crew[1] = mc->crew2;
	crew[2] = mc->crew3;
	for (n = 0; n < 3; n++) {
		if (mf->OBP[n].turnsent)
			men += mf->OBP[n].mensent;
	}
	for (n = 0; n < 3; n++) {
		if (mf->DBP[n].turnsent)
			men += mf->DBP[n].mensent;
	}
	if (men) {
		crew[0] = men/100 ? 0 : crew[0] != 0;
		crew[1] = (men%100)/10 ? 0 : crew[1] != 0;
		crew[2] = men%10 ? 0 : crew[2] != 0;
	}
	for (r = 0; r < 2; r++) {
		if (r) {
			ready = mf->readyR;
			load = mf->loadR;
			guns = mc->gunR;
			car = mc->carR;
		} else {
			ready = mf->readyL;
			load = mf->loadL;
			guns = mc->gunL;
			car = mc->carL;
		}
		if (!guns && !car || load == L_EMPTY || (ready & R_LOADED) == 0)
			goto cant;
		if (mf->struck || !crew[2])
			goto cant;
		closest = closestenemy(ms, (r ? 'r' : 'l'), 1);
		if (closest == 0)
			goto cant;
		if (closest->file->struck)
			goto cant;
		target = range(ms, closest);
		if (target > rangeofshot[load] || !guns && target >= 3)
			goto cant;
		Signal("%s (%c%c) within range of %s broadside.",
			closest, r ? "right" : "left");
		if (load > L_CHAIN && target < 6) {
			switch (sgetch("Aim for hull or rigging? ",
				(struct ship *)0, 1)) {
			case 'r':
				shootat = RIGGING;
				break;
			case 'h':
				shootat = HULL;
				break;
			default:
				shootat = -1;
				Signal("'Avast there! Hold your fire.'",
					(struct ship *)0);
			}
		} else {
			if (sgetch("Fire? ", (struct ship *)0, 1) == 'n') {
				shootat = -1;
				Signal("Belay that! Hold your fire.",
					(struct ship *)0);
			} else
				shootat = RIGGING;
		}
		if (shootat == -1)
			continue;
		fired = 1;
		rakehim = gunsbear(ms, closest) && !gunsbear(closest, ms);
		temp = portside(closest, ms, 1) - closest->file->dir + 1;
		if (temp < 1)
			temp += 8;
		else if (temp > 8)
			temp -= 8;
		sternrake = temp > 4 && temp < 6;
		if (rakehim) {
			if (!sternrake)
				Signal("Raking the %s!", closest);
			else
				Signal("Stern Rake! %s splintering!", closest);
		}
		index = guns;
		if (target < 3)
			index += car;
		index = (index - 1)/3;
		index = index > 8 ? 8 : index;
		if (!rakehim)
			hit = HDT[index][target-1];
		else
			hit = HDTrake[index][target-1];
		if (rakehim && sternrake)
			hit++;
		hit += QUAL[index][mc->qual-1];
		for (n = 0; n < 3 && mf->captured == 0; n++)
			if (!crew[n]) {
				if (index <= 5)
					hit--;
				else
					hit -= 2;
			}
		if (ready & R_INITIAL) {
			if (index <= 3)
				hit++;
			else
				hit += 2;
		}
		if (mf->captured != 0) {
			if (index <= 1)
				hit--;
			else
				hit -= 2;
		}
		hit += AMMO[index][load - 1];
		if (((temp = mc->class) >= 5 || temp == 1) && windspeed == 5)
			hit--;
		if (windspeed == 6 && temp == 4)
			hit -= 2;
		if (windspeed == 6 && temp <= 3)
			hit--;
		if (hit >= 0) {
			roll = die();
			if (load == L_GRAPE)
				chits = hit;
			else {
				struct Tables *t;
				if (hit > 10)
					hit = 10;
				t = &(shootat == RIGGING ? RigTable : HullTable)
					[hit][roll-1];
				chits = t->C;
				rhits = t->R;
				hhits = t->H;
				ghits = t->G;
				if (closest->file->FS)
					rhits *= 2;
				if (load == L_CHAIN) {
					ghits = 0;
					hhits = 0;
				}
			}
			table(shootat, load, hit, closest, ms, roll);
		}
		Signal("Damage inflicted on the %s:",
			(struct ship *)0, closest->shipname);
		Signal("\t%d HULL, %d GUNS, %d CREW, %d RIGGING",
			(struct ship *)0, hhits, ghits, chits, rhits);
		if (!r) {
			mf->loadL = L_EMPTY;
			mf->readyL = R_EMPTY;
		} else {
			mf->loadR = L_EMPTY;
			mf->readyR = R_EMPTY;
		}
		continue;
	cant:
		Signal("Unable to fire %s broadside",
			(struct ship *)0, r ? "right" : "left");
	}
	blockalarm();
	draw_stat();
	unblockalarm();
}

grapungrap()
{
	struct ship *sp;
	int i;

	foreachship(sp) {
		if (sp == ms || sp->file->dir == 0)
			continue;
		if (range(ms, sp) > 1 && !grappled2(ms, sp))
			continue;
		switch (sgetch("Attempt to grapple or ungrapple %s (%c%c): ",
			sp, 1)) {
		case 'g':
			if (die() < 3
			    || ms->nationality == capship(sp)->nationality) {
				Write(W_GRAP, ms, 0, sp->file->index, 0, 0, 0);
				Write(W_GRAP, sp, 0, player, 0, 0, 0);
				Signal("Attempt succeeds!", (struct ship *)0);
				makesignal(ms, "grappled with %s (%c%c)", sp);
			} else
				Signal("Attempt fails.", (struct ship *)0);
			break;
		case 'u':
			for (i = grappled2(ms, sp); --i >= 0;) {
				if (ms->nationality
					== capship(sp)->nationality
				    || die() < 3) {
					cleangrapple(ms, sp, 0);
					Signal("Attempt succeeds!",
						(struct ship *)0);
					makesignal(ms,
						"ungrappling with %s (%c%c)",
						sp);
				} else
					Signal("Attempt fails.",
						(struct ship *)0);
			}
			break;
		}
	}
}

unfoulplayer()
{
	struct ship *to;
	int i;

	foreachship(to) {
		if (fouled2(ms, to) == 0)
			continue;
		if (sgetch("Attempt to unfoul with the %s (%c%c)? ", to, 1) != 'y')
			continue;
		for (i = fouled2(ms, to); --i >= 0;) {
			if (die() <= 2) {
				cleanfoul(ms, to, 0);
				Signal("Attempt succeeds!", (struct ship *)0);
				makesignal(ms, "Unfouling %s (%c%c)", to);
			} else
				Signal("Attempt fails.", (struct ship *)0);
		}
	}
}
