/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.rip.c - version 1.0.2 */
/* $FreeBSD: src/games/hack/hack.rip.c,v 1.4 1999/11/16 10:26:37 marcel Exp $ */
/* $DragonFly: src/games/hack/hack.rip.c,v 1.2 2003/06/17 04:25:24 dillon Exp $ */

#include <stdio.h>
#include "hack.h"

extern char plname[];

static char *rip[] = {
"                       ----------",
"                      /          \\",
"                     /    REST    \\",
"                    /      IN      \\",
"                   /     PEACE      \\",
"                  /                  \\",
"                  |                  |",
"                  |                  |",
"                  |                  |",
"                  |                  |",
"                  |                  |",
"                  |       1001       |",
"                 *|     *  *  *      | *",
"        _________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______\n",
0
};

outrip(){
	char **dp = rip;
	char *dpx;
	char buf[BUFSZ];
	int x,y;

	cls();
	(void) strcpy(buf, plname);
	buf[16] = 0;
	center(6, buf);
	(void) sprintf(buf, "%ld AU", u.ugold);
	center(7, buf);
	(void) sprintf(buf, "killed by%s",
		!strncmp(killer, "the ", 4) ? "" :
		!strcmp(killer, "starvation") ? "" :
		index(vowels, *killer) ? " an" : " a");
	center(8, buf);
	(void) strcpy(buf, killer);
	if(strlen(buf) > 16) {
	    int i,i0,i1;
		i0 = i1 = 0;
		for(i = 0; i <= 16; i++)
			if(buf[i] == ' ') i0 = i, i1 = i+1;
		if(!i0) i0 = i1 = 16;
		buf[i1 + 16] = 0;
		center(10, buf+i1);
		buf[i0] = 0;
	}
	center(9, buf);
	(void) sprintf(buf, "%4d", getyear());
	center(11, buf);
	for(y=8; *dp; y++,dp++){
		x = 0;
		dpx = *dp;
		while(dpx[x]) {
			while(dpx[x] == ' ') x++;
			curs(x,y);
			while(dpx[x] && dpx[x] != ' '){
				extern int done_stopprint;
				if(done_stopprint)
					return;
				curx++;
				(void) putchar(dpx[x++]);
			}
		}
	}
	getret();
}

center(line, text) int line; char *text; {
char *ip,*op;
	ip = text;
	op = &rip[line][28 - ((strlen(text)+1)/2)];
	while(*ip) *op++ = *ip++;
}
