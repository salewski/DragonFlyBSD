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
 * @(#)play_level.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/robots/play_level.c,v 1.4 1999/11/30 03:49:20 billf Exp $
 * $DragonFly: src/games/robots/play_level.c,v 1.2 2003/06/17 04:25:24 dillon Exp $
 */

# include	"robots.h"

/*
 * play_level:
 *	Let the player play the current level
 */
play_level()
{
	COORD	*cp;
	int	y, x, bonus;

	move(My_pos.y, My_pos.x);
	addch(PLAYER);
	refresh();
	for (cp = Robots; cp < &Robots[MAXROBOTS]; cp++) {
		if (cp->y < 0)
			continue;
		move(cp->y, cp->x);
		addch(ROBOT);
	}
	refresh();
# ifdef DEBUG
	standout();
	move(Min.y, Min.x);
	addch(inch());
	move(Max.y, Max.x);
	addch(inch());
	standend();
# endif DEBUG
	setjmp(End_move);
	flush_in();
	while (!Dead && Num_robots > 0) {
		move(My_pos.y, My_pos.x);
		if (!jumping())
			refresh();
		get_move();
		if (Real_time)
			alarm(0);
		if (Field[My_pos.y][My_pos.x] != 0)
			Dead = TRUE;
		if (!Dead)
			move_robots(FALSE);
		if (Was_bonus) {
			move(Y_PROMPT, X_PROMPT);
			clrtoeol();
			move(Y_PROMPT + 1, X_PROMPT);
			clrtoeol();
			Was_bonus = FALSE;
		}
	}

	/*
	 * if the player didn't die, add on the possible bonuses
	 */

	if (!Dead) {
		Was_bonus = FALSE;

		if (Level == Start_level && Start_level > 1) {
			move(Y_PROMPT, X_PROMPT);
			printw("Advance bonus: %d", S_BONUS);
			refresh();
			add_score(S_BONUS);
			Was_bonus = TRUE;
		}

		if (Wait_bonus != 0) {
			if (!Was_bonus)
				move(Y_PROMPT, X_PROMPT);
			else
				move(Y_PROMPT + 1, X_PROMPT);
			printw("Wait bonus: %d", Wait_bonus);
			refresh();
			add_score(Wait_bonus);
			Was_bonus = TRUE;
		}
	}
}
