/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *	@(#)char.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/window/char.h,v 1.3.6.1 2001/05/17 09:45:00 obrien Exp $
 * $DragonFly: src/usr.bin/window/char.h,v 1.2 2003/06/17 04:29:33 dillon Exp $
 */

/*
 * Macros and things to deal with control characters.
 *
 * Unctrl() is just like the standard function, except we don't want
 * to include curses.
 * Isctrl() returns true for all characters less than space and
 * greater than or equal to delete.
 * Isprt() is tab and all characters not isctrl().  It's used
 * by wwwrite().
 * Isunctrl() includes all characters that should be expanded
 * using unctrl() by wwwrite() if ww_unctrl is set.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

extern char *_unctrl[];
#define ctrl(c)		(c & 0x1f)
#define unctrl(c)	(_unctrl[(unsigned char) (c)])
#define isctrl(c)       iscntrl((unsigned char)(c))
#define isprt(c)        (isprint((unsigned char)(c)) || (c) == '\t')
#define isunctrl(c)     (strchr("\b\t\n\r", (c)) == NULL && isctrl(c))
