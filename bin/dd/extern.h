/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/bin/dd/extern.h,v 1.9.2.1 2000/08/07 08:30:17 ps Exp $
 * $DragonFly: src/bin/dd/extern.h,v 1.3 2003/09/21 04:19:42 drhodus Exp $
 */

#include <sys/cdefs.h>

void block (void);
void block_close (void);
void dd_out (int);
void def (void);
void def_close (void);
void jcl (char **);
void pos_in (void);
void pos_out (void);
void summary (void);
void summaryx (int);
void terminate (int);
void unblock (void);
void unblock_close (void);
void bitswab (void *, size_t);

extern IO in, out;
extern STAT st;
extern void (*cfunc) (void);
extern quad_t cpy_cnt;
extern size_t cbsz;
extern u_int ddflags;
extern quad_t files_cnt;
extern const u_char *ctab;
extern const u_char a2e_32V[], a2e_POSIX[];
extern const u_char e2a_32V[], e2a_POSIX[];
extern const u_char a2ibm_32V[], a2ibm_POSIX[];
extern u_char casetab[];
