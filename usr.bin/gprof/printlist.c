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
 * @(#)printlist.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/gprof/printlist.c,v 1.3 1999/08/28 01:01:56 peter Exp $
 * $DragonFly: src/usr.bin/gprof/printlist.c,v 1.3 2003/10/04 20:36:45 hmp Exp $
 */

#include <err.h>
#include "gprof.h"

    /*
     *	these are the lists of names:
     *	there is the list head and then the listname
     *	is a pointer to the list head
     *	(for ease of passing to stringlist functions).
     */
struct stringlist	kfromhead = { 0 , 0 };
struct stringlist	*kfromlist = &kfromhead;
struct stringlist	ktohead = { 0 , 0 };
struct stringlist	*ktolist = &ktohead;
struct stringlist	fhead = { 0 , 0 };
struct stringlist	*flist = &fhead;
struct stringlist	Fhead = { 0 , 0 };
struct stringlist	*Flist = &Fhead;
struct stringlist	ehead = { 0 , 0 };
struct stringlist	*elist = &ehead;
struct stringlist	Ehead = { 0 , 0 };
struct stringlist	*Elist = &Ehead;

addlist(struct stringlist *listp, char *funcname)
{
    struct stringlist	*slp;

    slp = (struct stringlist *) malloc( sizeof(struct stringlist));
    if ( slp == (struct stringlist *) 0 ) {
	warnx("ran out room for printlist");
	done();
    }
    slp -> next = listp -> next;
    slp -> string = funcname;
    listp -> next = slp;
}

bool
onlist(struct stringlist *listp, char *funcname)
{
    struct stringlist	*slp;

    for ( slp = listp -> next ; slp ; slp = slp -> next ) {
	if ( ! strcmp( slp -> string , funcname ) ) {
	    return TRUE;
	}
	if ( funcname[0] == '_' && ! strcmp( slp -> string , &funcname[1] ) ) {
	    return TRUE;
	}
    }
    return FALSE;
}
