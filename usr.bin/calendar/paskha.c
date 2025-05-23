/*
 * Copyright (C) 1993-1996 by Andrey A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 * $FreeBSD: src/usr.bin/calendar/paskha.c,v 1.4.10.2 2002/05/27 12:14:46 dwmalone Exp $
 * $DragonFly: src/usr.bin/calendar/paskha.c,v 1.3 2003/10/02 17:42:26 hmp Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"

#define PASKHA "paskha"
#define PASKHALEN (sizeof(PASKHA) - 1)

static int paskha (int);

/* return year day for Orthodox Easter using Gauss formula */
/* (old style result) */

/* R: year */
static int
paskha (int R)
{
	int a, b, c, d, e;
	static int x = 15;
	static int y = 6;
	extern int *cumdays;

	a = R % 19;
	b = R % 4;
	c = R % 7;
	d = (19*a + x) % 30;
	e = (2*b + 4*c + 6*d + y) % 7;
	return (((cumdays[3] + 1) + 22) + (d + e));
}

/* return year day for Orthodox Easter depending days */

int
getpaskha(char *s, int year)
{
	int offset;
	extern struct fixs npaskha;

	if (strncasecmp(s, PASKHA, PASKHALEN) == 0)
	    s += PASKHALEN;
	else if (   npaskha.name != NULL
		 && strncasecmp(s, npaskha.name, npaskha.len) == 0
		)
	    s += npaskha.len;
	else
	    return 0;


	/* Paskha+1  or Paskha-2
	 *       ^            ^   */

	switch(*s) {

	case '-':
	case '+':
	    offset = atoi(s);
	    break;

	default:
	    offset = 0;
	    break;
	}
	    
	return (paskha(year) + offset + 13/* new style */);
}
