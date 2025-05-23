/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 * $DragonFly: src/usr.bin/make/str.h,v 1.5 2005/03/04 23:49:42 okumoto Exp $
 */

#ifndef str_h_44db59e6
#define	str_h_44db59e6

#include "sprite.h"

struct Buffer;

/*
 * These constants are all used by the Str_Concat function to decide how the
 * final string should look. If STR_ADDSPACE is given, a space will be
 * placed between the two strings. If STR_ADDSLASH is given, a '/' will
 * be used instead of a space. If neither is given, no intervening characters
 * will be placed between the two strings in the final output.
 */
#define	STR_ADDSPACE	0x01	/* add a space when Str_Concat'ing */
#define	STR_ADDSLASH	0x04	/* add a slash when Str_Concat'ing */

void str_init(void);
char *str_concat(const char *, const char *, int);
char **brk_string(const char *, int *, Boolean);
char *MAKEFLAGS_quote(const char *);
char **MAKEFLAGS_break(const char *, int *);
int Str_Match(const char *, const char *);
const char *Str_SYSVMatch(const char *, const char *, int *);
void Str_SYSVSubst(struct Buffer *, const char *, const char *, int);

#endif /* str_h_44db59e6 */
