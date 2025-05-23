/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 *	@(#)hist.h	8.1 (Berkeley) 6/4/93
 * $DragonFly: src/lib/libedit/hist.h,v 1.3 2003/11/12 20:21:29 eirikn Exp $
 */

/*
 * el.hist.c: History functions
 */
#ifndef _h_el_hist
#define _h_el_hist

#include "histedit.h"

typedef const HistEvent *	(*hist_fun_t) (ptr_t, int, ...);

typedef struct el_history_t {
    char *buf;				/* The history buffer		*/
    char *last;				/* The last character		*/
    int eventno;			/* Event we are looking for	*/
    ptr_t ref;				/* Argument for history fcns	*/
    hist_fun_t fun;			/* Event access			*/
    const HistEvent *ev;		/* Event cookie			*/
} el_history_t;

#define HIST_FUN(el, fn, arg)	\
    ((((el)->el_history.ev = \
       (*(el)->el_history.fun)((el)->el_history.ref, fn, arg)) == NULL) ? \
     NULL : (el)->el_history.ev->str)

#define	HIST_NEXT(el)		HIST_FUN(el, H_NEXT, NULL)
#define	HIST_FIRST(el)		HIST_FUN(el, H_FIRST, NULL)
#define	HIST_LAST(el)		HIST_FUN(el, H_LAST, NULL)
#define	HIST_PREV(el)		HIST_FUN(el, H_PREV, NULL)
#define	HIST_EVENT(el, num)	HIST_FUN(el, H_EVENT, num)
#define HIST_LOAD(el, fname)	HIST_FUN(el, H_LOAD fname)
#define HIST_SAVE(el, fname)	HIST_FUN(el, H_SAVE fname)

protected int 		hist_init	(EditLine *);
protected void 		hist_end	(EditLine *);
protected el_action_t	hist_get	(EditLine *);
protected int		hist_set	(EditLine *, hist_fun_t, ptr_t);
protected int		hist_list	(EditLine *, int, char **);

#endif /* _h_el_hist */
