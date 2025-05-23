/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
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
 * @(#)reverse.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/tail/reverse.c,v 1.9.2.3 2001/12/19 20:29:31 iedowse Exp $
 * $DragonFly: src/usr.bin/tail/reverse.c,v 1.4 2005/03/01 21:37:33 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "extern.h"

static void r_buf(FILE *);
static void r_reg(FILE *, enum STYLE, off_t, struct stat *);

/*
 * reverse -- display input in reverse order by line.
 *
 * There are six separate cases for this -- regular and non-regular
 * files by bytes, lines or the whole file.
 *
 * BYTES	display N bytes
 *	REG	mmap the file and display the lines
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * LINES	display N lines
 *	REG	mmap the file and display the lines
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 *
 * FILE		display the entire file
 *	REG	mmap the file and display the lines
 *	NOREG	cyclically read input into a linked list of buffers
 */
void
reverse(FILE *fp, enum STYLE style, off_t off, struct stat *sbp)
{
	if (style != REVERSE && off == 0)
		return;

	if (S_ISREG(sbp->st_mode))
		r_reg(fp, style, off, sbp);
	else
		switch(style) {
		case FBYTES:
		case RBYTES:
			display_bytes(fp, off);
			break;
		case FLINES:
		case RLINES:
			display_lines(fp, off);
			break;
		case REVERSE:
			r_buf(fp);
			break;
		}
}

/*
 * r_reg -- display a regular file in reverse order by line.
 */
static void
r_reg(FILE *fp, enum STYLE style, off_t off, struct stat *sbp)
{
	struct mapinfo map;
	off_t curoff, size, lineend;
	int i;

	if (!(size = sbp->st_size))
		return;

	map.start = NULL;
	map.mapoff = map.maxoff = size;
	map.fd = fileno(fp);

	/*
	 * Last char is special, ignore whether newline or not. Note that
	 * size == 0 is dealt with above, and size == 1 sets curoff to -1.
	 */
	curoff = size - 2;
	lineend = size;
	while (curoff >= 0) {
		if (curoff < map.mapoff || curoff >= map.mapoff + map.maplen) {
			if (maparound(&map, curoff) != 0) {
				ierr();
				return;
			}
		}
		for (i = curoff - map.mapoff; i >= 0; i--) {
			if (style == RBYTES && --off == 0)
				break;
			if (map.start[i] == '\n')
				break;
		}
		/* `i' is either the map offset of a '\n', or -1. */
		curoff = map.mapoff + i;
		if (i < 0)
			continue;

		/* Print the line and update offsets. */
		if (mapprint(&map, curoff + 1, lineend - curoff - 1) != 0) {
			ierr();
			return;
		}
		lineend = curoff + 1;
		curoff--;

		if (style == RLINES)
			off--;

		if (off == 0 && style != REVERSE) {
			/* Avoid printing anything below. */
			curoff = 0;
			break;
		}
	}
	if (curoff < 0 && mapprint(&map, 0, lineend) != 0) {
		ierr();
		return;
	}
	if (map.start != NULL && munmap(map.start, map.maplen))
		ierr();
}

typedef struct bf {
	struct bf *next;
	struct bf *prev;
	int len;
	char *l;
} BF;

/*
 * r_buf -- display a non-regular file in reverse order by line.
 *
 * This is the function that saves the entire input, storing the data in a
 * doubly linked list of buffers and then displays them in reverse order.
 * It has the usual nastiness of trying to find the newlines, as there's no
 * guarantee that a newline occurs anywhere in the file, let alone in any
 * particular buffer.  If we run out of memory, input is discarded (and the
 * user warned).
 */
static void
r_buf(FILE *fp)
{
	BF *mark, *tl = NULL, *tr = NULL;
	int ch = 0, len, llen;
	char *p;
	off_t enomem;

#define	BSZ	(128 * 1024)
	for (mark = NULL, enomem = 0;;) {
		/*
		 * Allocate a new block and link it into place in a doubly
		 * linked list.  If out of memory, toss the LRU block and
		 * keep going.
		 */
		if (enomem || (tl = malloc(sizeof(BF))) == NULL ||
		    (tl->l = malloc(BSZ)) == NULL) {
			if (!mark)
				err(1, "malloc");
			tl = enomem ? tl->next : mark;
			enomem += tl->len;
		} else if (mark) {
			tl->next = mark;
			tl->prev = mark->prev;
			mark->prev->next = tl;
			mark->prev = tl;
		} else
			mark->next = mark->prev = (mark = tl);

		/* Fill the block with input data. */
		for (p = tl->l, len = 0;
		    len < BSZ && (ch = getc(fp)) != EOF; ++len)
			*p++ = ch;

		if (ferror(fp)) {
			ierr();
			return;
		}

		/*
		 * If no input data for this block and we tossed some data,
		 * recover it.
		 */
		if (!len) {
			if (enomem)
				enomem -= tl->len;
			tl = tl->prev;
			break;
		}

		tl->len = len;
		if (ch == EOF)
			break;
	}

	if (enomem) {
		warnx("warning: %qd bytes discarded", enomem);
		rval = 1;
	}

	/*
	 * Step through the blocks in the reverse order read.  The last char
	 * is special, ignore whether newline or not.
	 */
	for (mark = tl;;) {
		for (p = tl->l + (len = tl->len) - 1, llen = 0; len--;
		    --p, ++llen)
			if (*p == '\n') {
				if (llen) {
					WR(p + 1, llen);
					llen = 0;
				}
				if (tl == mark)
					continue;
				for (tr = tl->next; tr->len; tr = tr->next) {
					WR(tr->l, tr->len);
					tr->len = 0;
					if (tr == mark)
						break;
				}
			}
		tl->len = llen;
		if ((tl = tl->prev) == mark)
			break;
	}
	tl = tl->next;
	if (tl->len) {
		WR(tl->l, tl->len);
		tl->len = 0;
	}
	while ((tl = tl->next)->len) {
		WR(tl->l, tl->len);
		tl->len = 0;
	}
}
