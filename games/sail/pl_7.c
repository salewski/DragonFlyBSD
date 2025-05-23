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
 * @(#)pl_7.c	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/sail/pl_7.c,v 1.7 1999/11/30 03:49:37 billf Exp $
 * $DragonFly: src/games/sail/pl_7.c,v 1.2 2003/06/17 04:25:25 dillon Exp $
 */

#include <sys/ttydefaults.h>
#include <string.h>
#include "player.h"


/*
 * Display interface
 */

static char sc_hasprompt;
static char *sc_prompt;
static char *sc_buf;
static int sc_line;

initscreen()
{
	/* initscr() already done in SCREENTEST() */
	view_w = newwin(VIEW_Y, VIEW_X, VIEW_T, VIEW_L);
	slot_w = newwin(SLOT_Y, SLOT_X, SLOT_T, SLOT_L);
	scroll_w = newwin(SCROLL_Y, SCROLL_X, SCROLL_T, SCROLL_L);
	stat_w = newwin(STAT_Y, STAT_X, STAT_T, STAT_L);
	turn_w = newwin(TURN_Y, TURN_X, TURN_T, TURN_L);
	done_curses++;
	(void) leaveok(view_w, 1);
	(void) leaveok(slot_w, 1);
	(void) leaveok(stat_w, 1);
	(void) leaveok(turn_w, 1);
	noecho();
	crmode();
}

cleanupscreen()
{
	/* alarm already turned off */
	if (done_curses) {
		(void) wmove(scroll_w, SCROLL_Y - 1, 0);
		(void) wclrtoeol(scroll_w);
		draw_screen();
		endwin();
	}
}

void
newturn()
{
	repaired = loaded = fired = changed = 0;
	movebuf[0] = '\0';

	(void) alarm(0);
	if (mf->readyL & R_LOADING) {
		if (mf->readyL & R_DOUBLE)
			mf->readyL = R_LOADING;
		else
			mf->readyL = R_LOADED;
	}
	if (mf->readyR & R_LOADING) {
		if (mf->readyR & R_DOUBLE)
			mf->readyR = R_LOADING;
		else
			mf->readyR = R_LOADED;
	}
	if (!hasdriver)
		Write(W_DDEAD, SHIP(0), 0, 0, 0, 0, 0);

	if (sc_hasprompt) {
		(void) wmove(scroll_w, sc_line, 0);
		(void) wclrtoeol(scroll_w);
	}
	if (Sync() < 0)
		leave(LEAVE_SYNC);
	if (!hasdriver)
		leave(LEAVE_DRIVER);
	if (sc_hasprompt)
		(void) wprintw(scroll_w, "%s%s", sc_prompt, sc_buf);

	if (turn % 50 == 0)
		Write(W_ALIVE, SHIP(0), 0, 0, 0, 0, 0);
	if (mf->FS && (!mc->rig1 || windspeed == 6))
		Write(W_FS, ms, 0, 0, 0, 0, 0);
	if (mf->FS == 1)
		Write(W_FS, ms, 0, 2, 0, 0, 0);

	if (mf->struck)
		leave(LEAVE_QUIT);
	if (mf->captured != 0)
		leave(LEAVE_CAPTURED);
	if (windspeed == 7)
		leave(LEAVE_HURRICAN);

	adjustview();
	draw_screen();

	(void) signal(SIGALRM, newturn);
	(void) alarm(7);
}

/*VARARGS2*/
Signal(fmt, ship, a, b, c, d)
char *fmt;
struct ship *ship;
int a, b, c, d;
{
	if (!done_curses)
		return;
	if (*fmt == '\7')
		putchar(*fmt++);
	if (ship == 0)
		(void) wprintw(scroll_w, fmt, a, b, c, d);
	else
		(void) wprintw(scroll_w, fmt, ship->shipname,
			colours(ship), sterncolour(ship), a, b, c, d);
	Scroll();
}

Scroll()
{
	if (++sc_line >= SCROLL_Y)
		sc_line = 0;
	(void) wmove(scroll_w, sc_line, 0);
	(void) wclrtoeol(scroll_w);
}

prompt(p, ship)
char *p;
struct ship *ship;
{
	static char buf[60];

	if (ship != 0) {
		(void)sprintf(buf, p, ship->shipname, colours(ship),
			sterncolour(ship));
		p = buf;
	}
	sc_prompt = p;
	sc_buf = "";
	sc_hasprompt = 1;
	(void) waddstr(scroll_w, p);
}

endprompt(flag)
char flag;
{
	sc_hasprompt = 0;
	if (flag)
		Scroll();
}

sgetch(p, ship, flag)
char *p;
struct ship *ship;
char flag;
{
	int c;

	prompt(p, ship);
	blockalarm();
	(void) wrefresh(scroll_w);
	unblockalarm();
	while ((c = wgetch(scroll_w)) == EOF)
		;
	if (flag && c >= ' ' && c < 0x7f)
		(void) waddch(scroll_w, c);
	endprompt(flag);
	return c;
}

sgetstr(pr, buf, n)
char *pr;
char *buf;
int n;
{
	int c;
	char *p = buf;

	prompt(pr, (struct ship *)0);
	sc_buf = buf;
	for (;;) {
		*p = 0;
		blockalarm();
		(void) wrefresh(scroll_w);
		unblockalarm();
		while ((c = wgetch(scroll_w)) == EOF)
			;
		switch (c) {
		case '\n':
		case '\r':
			endprompt(1);
			return;
		case '\b':
			if (p > buf) {
				(void) waddstr(scroll_w, "\b \b");
				p--;
			}
			break;
		default:
			if (c >= ' ' && c < 0x7f && p < buf + n - 1) {
				*p++ = c;
				(void) waddch(scroll_w, c);
			} else
				(void) putchar(CTRL('g'));
		}
	}
}

draw_screen()
{
	draw_view();
	draw_turn();
	draw_stat();
	draw_slot();
	(void) wrefresh(scroll_w);		/* move the cursor */
}

draw_view()
{
	struct ship *sp;

	(void) werase(view_w);
	foreachship(sp) {
		if (sp->file->dir
		    && sp->file->row > viewrow
		    && sp->file->row < viewrow + VIEW_Y
		    && sp->file->col > viewcol
		    && sp->file->col < viewcol + VIEW_X) {
			(void) wmove(view_w, sp->file->row - viewrow,
				sp->file->col - viewcol);
			(void) waddch(view_w, colours(sp));
			(void) wmove(view_w,
				sternrow(sp) - viewrow,
				sterncol(sp) - viewcol);
			(void) waddch(view_w, sterncolour(sp));
		}
	}
	(void) wrefresh(view_w);
}

draw_turn()
{
	(void) wmove(turn_w, 0, 0);
	(void) wprintw(turn_w, "%cTurn %d", dont_adjust?'*':'-', turn);
	(void) wrefresh(turn_w);
}

draw_stat()
{
	(void) wmove(stat_w, STAT_1, 0);
	(void) wprintw(stat_w, "Points  %3d\n", mf->points);
	(void) wprintw(stat_w, "Fouls    %2d\n", fouled(ms));
	(void) wprintw(stat_w, "Grapples %2d\n", grappled(ms));

	(void) wmove(stat_w, STAT_2, 0);
	(void) wprintw(stat_w, "    0 %c(%c)\n",
		maxmove(ms, winddir + 3, -1) + '0',
		maxmove(ms, winddir + 3, 1) + '0');
	(void) waddstr(stat_w, "   \\|/\n");
	(void) wprintw(stat_w, "   -^-%c(%c)\n",
		maxmove(ms, winddir + 2, -1) + '0',
		maxmove(ms, winddir + 2, 1) + '0');
	(void) waddstr(stat_w, "   /|\\\n");
	(void) wprintw(stat_w, "    | %c(%c)\n",
		maxmove(ms, winddir + 1, -1) + '0',
		maxmove(ms, winddir + 1, 1) + '0');
	(void) wprintw(stat_w, "   %c(%c)\n",
		maxmove(ms, winddir, -1) + '0',
		maxmove(ms, winddir, 1) + '0');

	(void) wmove(stat_w, STAT_3, 0);
	(void) wprintw(stat_w, "Load  %c%c %c%c\n",
		loadname[mf->loadL], readyname(mf->readyL),
		loadname[mf->loadR], readyname(mf->readyR));
	(void) wprintw(stat_w, "Hull %2d\n", mc->hull);
	(void) wprintw(stat_w, "Crew %2d %2d %2d\n",
		mc->crew1, mc->crew2, mc->crew3);
	(void) wprintw(stat_w, "Guns %2d %2d\n", mc->gunL, mc->gunR);
	(void) wprintw(stat_w, "Carr %2d %2d\n", mc->carL, mc->carR);
	(void) wprintw(stat_w, "Rigg %d %d %d ", mc->rig1, mc->rig2, mc->rig3);
	if (mc->rig4 < 0)
		(void) waddch(stat_w, '-');
	else
		(void) wprintw(stat_w, "%d", mc->rig4);
	(void) wrefresh(stat_w);
}

draw_slot()
{
	if (!boarding(ms, 0)) {
		(void) mvwaddstr(slot_w, 0, 0, "   ");
		(void) mvwaddstr(slot_w, 1, 0, "   ");
	} else
		(void) mvwaddstr(slot_w, 1, 0, "OBP");
	if (!boarding(ms, 1)) {
		(void) mvwaddstr(slot_w, 2, 0, "   ");
		(void) mvwaddstr(slot_w, 3, 0, "   ");
	} else
		(void) mvwaddstr(slot_w, 3, 0, "DBP");

	(void) wmove(slot_w, SLOT_Y-4, 0);
	if (mf->RH)
		(void) wprintw(slot_w, "%dRH", mf->RH);
	else
		(void) waddstr(slot_w, "   ");
	(void) wmove(slot_w, SLOT_Y-3, 0);
	if (mf->RG)
		(void) wprintw(slot_w, "%dRG", mf->RG);
	else
		(void) waddstr(slot_w, "   ");
	(void) wmove(slot_w, SLOT_Y-2, 0);
	if (mf->RR)
		(void) wprintw(slot_w, "%dRR", mf->RR);
	else
		(void) waddstr(slot_w, "   ");

#define Y	(SLOT_Y/2)
	(void) wmove(slot_w, 7, 1);
	(void) wprintw(slot_w,"%d", windspeed);
	(void) mvwaddch(slot_w, Y, 0, ' ');
	(void) mvwaddch(slot_w, Y, 2, ' ');
	(void) mvwaddch(slot_w, Y-1, 0, ' ');
	(void) mvwaddch(slot_w, Y-1, 1, ' ');
	(void) mvwaddch(slot_w, Y-1, 2, ' ');
	(void) mvwaddch(slot_w, Y+1, 0, ' ');
	(void) mvwaddch(slot_w, Y+1, 1, ' ');
	(void) mvwaddch(slot_w, Y+1, 2, ' ');
	(void) wmove(slot_w, Y - dr[winddir], 1 - dc[winddir]);
	switch (winddir) {
	case 1:
	case 5:
		(void) waddch(slot_w, '|');
		break;
	case 2:
	case 6:
		(void) waddch(slot_w, '/');
		break;
	case 3:
	case 7:
		(void) waddch(slot_w, '-');
		break;
	case 4:
	case 8:
		(void) waddch(slot_w, '\\');
		break;
	}
	(void) mvwaddch(slot_w, Y + dr[winddir], 1 + dc[winddir], '+');
	(void) wrefresh(slot_w);
}

draw_board()
{
	int n;

	(void) clear();
	(void) werase(view_w);
	(void) werase(slot_w);
	(void) werase(scroll_w);
	(void) werase(stat_w);
	(void) werase(turn_w);

	sc_line = 0;

	(void) move(BOX_T, BOX_L);
	for (n = 0; n < BOX_X; n++)
		(void) addch('-');
	(void) move(BOX_B, BOX_L);
	for (n = 0; n < BOX_X; n++)
		(void) addch('-');
	for (n = BOX_T+1; n < BOX_B; n++) {
		(void) mvaddch(n, BOX_L, '|');
		(void) mvaddch(n, BOX_R, '|');
	}
	(void) mvaddch(BOX_T, BOX_L, '+');
	(void) mvaddch(BOX_T, BOX_R, '+');
	(void) mvaddch(BOX_B, BOX_L, '+');
	(void) mvaddch(BOX_B, BOX_R, '+');
	(void) refresh();

#define WSaIM "Wooden Ships & Iron Men"
	(void) wmove(view_w, 2, (VIEW_X - sizeof WSaIM - 1) / 2);
	(void) waddstr(view_w, WSaIM);
	(void) wmove(view_w, 4, (VIEW_X - strlen(cc->name)) / 2);
	(void) waddstr(view_w, cc->name);
	(void) wrefresh(view_w);

	(void) move(LINE_T, LINE_L);
	(void) printw("Class %d %s (%d guns) '%s' (%c%c)",
		mc->class,
		classname[mc->class],
		mc->guns,
		ms->shipname,
		colours(ms),
		sterncolour(ms));
	(void) refresh();
}

centerview()
{
	viewrow = mf->row - VIEW_Y / 2;
	viewcol = mf->col - VIEW_X / 2;
}

upview()
{
	viewrow -= VIEW_Y / 3;
}

downview()
{
	viewrow += VIEW_Y / 3;
}

leftview()
{
	viewcol -= VIEW_X / 5;
}

rightview()
{
	viewcol += VIEW_X / 5;
}

adjustview()
{
	if (dont_adjust)
		return;
	if (mf->row < viewrow + VIEW_Y/4)
		viewrow = mf->row - (VIEW_Y - VIEW_Y/4);
	else if (mf->row > viewrow + (VIEW_Y - VIEW_Y/4))
		viewrow = mf->row - VIEW_Y/4;
	if (mf->col < viewcol + VIEW_X/8)
		viewcol = mf->col - (VIEW_X - VIEW_X/8);
	else if (mf->col > viewcol + (VIEW_X - VIEW_X/8))
		viewcol = mf->col - VIEW_X/8;
}
