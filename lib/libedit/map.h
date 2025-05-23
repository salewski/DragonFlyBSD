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
 *	@(#)map.h	8.1 (Berkeley) 6/4/93
 * $DragonFly: src/lib/libedit/map.h,v 1.3 2003/11/12 20:21:29 eirikn Exp $
 */

/*
 * el.map.h:	Editor maps
 */
#ifndef _h_el_map
#define _h_el_map

typedef struct el_bindings_t {	/* for the "bind" shell command */
    const char   *name;		/* function name for bind command */
    int     func;		/* function numeric value */
    const char   *description;	/* description of function */
} el_bindings_t;


typedef struct el_map_t {
    el_action_t   *alt;		/* The current alternate key map	*/
    el_action_t   *key;		/* The current normal key map		*/
    el_action_t   *current;	/* The keymap we are using		*/
    el_action_t   *emacs;	/* The default emacs key map		*/
    el_action_t   *vic;		/* The vi command mode key map		*/
    el_action_t   *vii;		/* The vi insert mode key map		*/
    int		   type;	/* Emacs or vi				*/
    el_bindings_t *help;	/* The help for the editor functions	*/
    el_func_t     *func;	/* List of available functions		*/
    int  	   nfunc;	/* The number of functions/help items	*/
} el_map_t;

#define MAP_EMACS	0
#define MAP_VI		1

protected int	map_bind		(EditLine *, int, char **);
protected int	map_init		(EditLine *);
protected void	map_end			(EditLine *);
protected void	map_init_vi		(EditLine *);
protected void	map_init_emacs		(EditLine *);
protected int	map_set_editor		(EditLine *, char *);
protected int	map_addfunc		(EditLine *, const char *,
					     const char *, el_func_t);

#endif /* _h_el_map */
