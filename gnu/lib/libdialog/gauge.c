/*
 * gauge.c
 *
 * progress indicator for libdialog
 *
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 * $FreeBSD: src/gnu/lib/libdialog/gauge.c,v 1.2.12.2 2002/06/18 07:55:55 dougb Exp $
 * $DragonFly: src/gnu/lib/libdialog/gauge.c,v 1.2 2003/06/17 04:25:43 dillon Exp $
 */

#include <stdlib.h>

#include "dialog.h"

void
dialog_gauge(char *title, char *prompt, int y, int x,
	     int height, int width, int perc)
/*
 * Desc: display a progress bar, progress indicated by <perc>
 */
{
    WINDOW 	*gw;
    int		glen, i;
    char	percs[5];

    gw = newwin(height, width, y, x);
    if (!gw) {
	fprintf(stderr, "dialog_gauge: Error creating window (%d, %d, %d, %d)",
		height, width, y, x);
	exit(-1);
    }

    draw_box(gw, 0, 0, height, width, dialog_attr, border_attr);
    draw_shadow(stdscr, y, x, height, width);

    wattrset(gw, title_attr);
    if (title) {
	wmove(gw, 0, (width - strlen(title))/2 - 1);
	waddstr(gw, "[ ");
	waddstr(gw, title);
	waddstr(gw, " ]");
    }
    wattrset(gw, dialog_attr);
    if (prompt) {
	wmove(gw, 1, (width - strlen(prompt))/2 - 1);
	waddstr(gw, prompt);
    }

    draw_box(gw, 2, 2, 3, width-4, dialog_attr, border_attr);
    glen = (int) ((float) perc/100 * (width-6));

    wattrset(gw, dialog_attr);
    sprintf(percs, "%3d%%", perc);
    wmove(gw, 5, width/2 - 2);
    waddstr(gw, percs);

    wattrset(gw, A_BOLD);
    wmove(gw, 3, 3);
    for (i=0; i<glen; i++) waddch(gw, ' ');

    wrefresh(gw);
    delwin(gw);

    return;
} /* dialog_gauge() */

