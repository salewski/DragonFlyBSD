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
 * $FreeBSD: src/lib/libedit/term.c,v 1.11.6.1 2000/08/16 14:43:40 ache Exp $
 * $DragonFly: src/lib/libedit/term.c,v 1.3 2003/11/12 20:21:29 eirikn Exp $
 *
 * @(#)term.c	8.2 (Berkeley) 4/30/95
 */

/*
 * term.c: Editor/termcap-curses interface
 *	   We have to declare a static variable here, since the
 *	   termcap putchar routine does not take an argument!
 */
#include "sys.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termcap.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "el.h"

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current possition assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */

#define TC_BUFSIZE 2048

#define GoodStr(a) (el->el_term.t_str[a] != NULL && \
		    el->el_term.t_str[a][0] != '\0')
#define Str(a) el->el_term.t_str[a]
#define Val(a) el->el_term.t_val[a]

private struct termcapstr {
    char   *name;
    char   *long_name;
} tstr[] = {

#define T_al	0
    {	"al",	"add new blank line"		},
#define T_bl	1
    {	"bl",	"audible bell"			},
#define T_cd	2
    {	"cd",	"clear to bottom"		},
#define T_ce	3
    {	"ce",	"clear to end of line"		},
#define T_ch	4
    {	"ch",	"cursor to horiz pos"		},
#define T_cl	5
    {	"cl",	"clear screen"			},
#define	T_dc	6
    {	"dc",	"delete a character"		},
#define	T_dl	7
    {	"dl",	"delete a line"		 	},
#define	T_dm	8
    {	"dm",	"start delete mode"		},
#define	T_ed	9
    {	"ed",	"end delete mode"		},
#define	T_ei	10
    {	"ei",	"end insert mode"		},
#define	T_fs	11
    {	"fs",	"cursor from status line"	},
#define	T_ho	12
    {	"ho",	"home cursor"			},
#define	T_ic	13
    {	"ic",	"insert character"		},
#define	T_im	14
    {	"im",	"start insert mode"		},
#define	T_ip	15
    {	"ip",	"insert padding"		},
#define	T_kd	16
    {	"kd",	"sends cursor down"		},
#define	T_kl	17
    {	"kl",	"sends cursor left"		},
#define T_kr	18
    {	"kr",	"sends cursor right"		},
#define T_ku	19
    {	"ku",	"sends cursor up"		},
#define T_md	20
    {	"md",	"begin bold"			},
#define T_me	21
    {	"me",	"end attributes"		},
#define T_nd	22
    {	"nd",	"non destructive space"	 	},
#define T_se	23
    {	"se",	"end standout"			},
#define T_so	24
    {	"so",	"begin standout"		},
#define T_ts	25
    {	"ts",	"cursor to status line"	 	},
#define T_up	26
    {	"up",	"cursor up one"		 	},
#define T_us	27
    {	"us",	"begin underline"		},
#define T_ue	28
    {	"ue",	"end underline"		 	},
#define T_vb	29
    {	"vb",	"visible bell"			},
#define T_DC	30
    {	"DC",	"delete multiple chars"	 	},
#define T_DO	31
    {	"DO",	"cursor down multiple"		},
#define T_IC	32
    {	"IC",	"insert multiple chars"	 	},
#define T_LE	33
    {	"LE",	"cursor left multiple"		},
#define T_RI	34
    {	"RI",	"cursor right multiple"	 	},
#define T_UP	35
    {	"UP",	"cursor up multiple"		},
#define T_kh	36
    {	"kh",	"sends cursor home"		},
#define T_at7	37
    {	"@7",	"sends cursor end"		},
#define T_str	38
    {	NULL,   NULL			 	}
};

private struct termcapval {
    char   *name;
    char   *long_name;
} tval[] = {
#define T_pt	0
    {	"pt",	"has physical tabs"	},
#define T_li	1
    {	"li",	"Number of lines"	},
#define T_co	2
    {	"co",	"Number of columns"	},
#define T_km	3
    {	"km",	"Has meta key"		},
#define T_xt	4
    {	"xt",	"Tab chars destructive" },
#define T_MT	5
    {	"MT",	"Has meta key"		},	/* XXX? */
#define T_val	6
    {	NULL, 	NULL,			}
};

/* do two or more of the attributes use me */

private	void	term_rebuffer_display	(EditLine *);
private	void	term_free_display	(EditLine *);
private	void	term_alloc_display	(EditLine *);
private	void	term_alloc		(EditLine *,
					     struct termcapstr *, char *);
private void	term_init_arrow		(EditLine *);
private void	term_reset_arrow	(EditLine *);


private FILE *term_outfile = NULL;	/* XXX: How do we fix that? */


/* term_setflags():
 *	Set the terminal capability flags
 */
private void
term_setflags(el)
    EditLine *el;
{
    EL_FLAGS = 0;
    if (el->el_tty.t_tabs)
	EL_FLAGS |= (Val(T_pt) && !Val(T_xt)) ? TERM_CAN_TAB : 0;

    EL_FLAGS |= (Val(T_km) || Val(T_MT)) ? TERM_HAS_META : 0;
    EL_FLAGS |= GoodStr(T_ce) ? TERM_CAN_CEOL : 0;
    EL_FLAGS |= (GoodStr(T_dc) || GoodStr(T_DC)) ? TERM_CAN_DELETE : 0;
    EL_FLAGS |= (GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC)) ?
		 TERM_CAN_INSERT : 0;
    EL_FLAGS |= (GoodStr(T_up) || GoodStr(T_UP))  ? TERM_CAN_UP : 0;

    if (GoodStr(T_me) && GoodStr(T_ue))
	EL_FLAGS |= (strcmp(Str(T_me), Str(T_ue)) == 0) ? TERM_CAN_ME : 0;
    else
	EL_FLAGS &= ~TERM_CAN_ME;
    if (GoodStr(T_me) && GoodStr(T_se))
	EL_FLAGS |= (strcmp(Str(T_me), Str(T_se)) == 0) ? TERM_CAN_ME : 0;


#ifdef DEBUG_SCREEN
    if (!EL_CAN_UP) {
	(void) fprintf(el->el_errfile, "WARNING: Your terminal cannot move up.\n");
	(void) fprintf(el->el_errfile, "Editing may be odd for long lines.\n");
    }
    if (!EL_CAN_CEOL)
	(void) fprintf(el->el_errfile, "no clear EOL capability.\n");
    if (!EL_CAN_DELETE)
	(void) fprintf(el->el_errfile, "no delete char capability.\n");
    if (!EL_CAN_INSERT)
	(void) fprintf(el->el_errfile, "no insert char capability.\n");
#endif /* DEBUG_SCREEN */
}


/* term_init():
 *	Initialize the terminal stuff
 */
protected int
term_init(el)
    EditLine *el;
{
    el->el_term.t_buf = (char *)  el_malloc(TC_BUFSIZE);
    el->el_term.t_cap = (char *)  el_malloc(TC_BUFSIZE);
    el->el_term.t_fkey = (fkey_t *) el_malloc(A_K_NKEYS * sizeof(fkey_t));
    (void) memset(el->el_term.t_fkey, 0, A_K_NKEYS * sizeof(fkey_t));
    el->el_term.t_loc = 0;
    el->el_term.t_str = (char **) el_malloc(T_str * sizeof(char*));
    (void) memset(el->el_term.t_str, 0, T_str * sizeof(char*));
    el->el_term.t_val = (int *)   el_malloc(T_val * sizeof(int));
    (void) memset(el->el_term.t_val, 0, T_val * sizeof(int));
    term_outfile = el->el_outfile;
    (void) term_set(el, NULL);
    term_init_arrow(el);
    return 0;
}

/* term_end():
 *	Clean up the terminal stuff
 */
protected void
term_end(el)
    EditLine *el;
{
    el_free((ptr_t) el->el_term.t_buf);
    el->el_term.t_buf = NULL;
    el_free((ptr_t) el->el_term.t_cap);
    el->el_term.t_cap = NULL;
    el->el_term.t_loc = 0;
    el_free((ptr_t) el->el_term.t_str);
    el->el_term.t_str = NULL;
    el_free((ptr_t) el->el_term.t_val);
    el->el_term.t_val = NULL;
    term_free_display(el);
}


/* term_alloc():
 *	Maintain a string pool for termcap strings
 */
private void
term_alloc(el, t, cap)
    EditLine *el;
    struct termcapstr *t;
    char   *cap;
{
    char    termbuf[TC_BUFSIZE];
    int     tlen, clen;
    char    **tlist = el->el_term.t_str;
    char    **tmp, **str = &tlist[t - tstr];

    if (cap == NULL || *cap == '\0') {
	*str = NULL;
	return;
    }
    else
	clen = strlen(cap);

    tlen  = *str == NULL ? 0 : strlen(*str);

    /*
     * New string is shorter; no need to allocate space
     */
    if (clen <= tlen) {
	(void)strcpy(*str, cap);	/* XXX strcpy is safe */
	return;
    }

    /*
     * New string is longer; see if we have enough space to append
     */
    if (el->el_term.t_loc + 3 < TC_BUFSIZE) {
	/* XXX strcpy is safe */
	(void)strcpy(*str = &el->el_term.t_buf[el->el_term.t_loc], cap);
	el->el_term.t_loc += clen + 1;	/* one for \0 */
	return;
    }

    /*
     * Compact our buffer; no need to check compaction, cause we know it
     * fits...
     */
    tlen = 0;
    for (tmp = tlist; tmp < &tlist[T_str]; tmp++)
	if (*tmp != NULL && *tmp != '\0' && *tmp != *str) {
	    char   *ptr;

	    for (ptr = *tmp; *ptr != '\0'; termbuf[tlen++] = *ptr++)
		continue;
	    termbuf[tlen++] = '\0';
	}
    memcpy(el->el_term.t_buf, termbuf, TC_BUFSIZE);
    el->el_term.t_loc = tlen;
    if (el->el_term.t_loc + 3 >= TC_BUFSIZE) {
	(void) fprintf(el->el_errfile, "Out of termcap string space.\n");
	return;
    }
    /* XXX strcpy is safe */
    (void)strcpy(*str = &el->el_term.t_buf[el->el_term.t_loc], cap);
    el->el_term.t_loc += clen + 1;		/* one for \0 */
    return;
} /* end term_alloc */


/* term_rebuffer_display():
 *	Rebuffer the display after the screen changed size
 */
private void
term_rebuffer_display(el)
    EditLine *el;
{
    coord_t *c = &el->el_term.t_size;

    term_free_display(el);

    /* make this public, -1 to avoid wraps */
    c->h = Val(T_co) - 1;
    c->v = (EL_BUFSIZ * 4) / c->h + 1;

    term_alloc_display(el);
} /* end term_rebuffer_display */


/* term_alloc_display():
 *	Allocate a new display.
 */
private void
term_alloc_display(el)
    EditLine *el;
{
    int i;
    char  **b;
    coord_t *c = &el->el_term.t_size;

    b = (char **) el_malloc((size_t) (sizeof(char *) * (c->v + 1)));
    for (i = 0; i < c->v; i++)
	b[i] = (char *) el_malloc((size_t) (sizeof(char) * (c->h + 1)));
    b[c->v] = NULL;
    el->el_display = b;

    b = (char **) el_malloc((size_t) (sizeof(char *) * (c->v + 1)));
    for (i = 0; i < c->v; i++)
	b[i] = (char *) el_malloc((size_t) (sizeof(char) * (c->h + 1)));
    b[c->v] = NULL;
    el->el_vdisplay = b;

} /* end term_alloc_display */


/* term_free_display():
 *	Free the display buffers
 */
private void
term_free_display(el)
    EditLine *el;
{
    char  **b;
    char  **bufp;

    b = el->el_display;
    el->el_display = NULL;
    if (b != NULL) {
	for (bufp = b; *bufp != NULL; bufp++)
	    el_free((ptr_t) *bufp);
	el_free((ptr_t) b);
    }
    b = el->el_vdisplay;
    el->el_vdisplay = NULL;
    if (b != NULL) {
	for (bufp = b; *bufp != NULL; bufp++)
	    el_free((ptr_t) * bufp);
	el_free((ptr_t) b);
    }
} /* end term_free_display */


/* term_move_to_line():
 *	move to line <where> (first line == 0)
 * 	as efficiently as possible
 */
protected void
term_move_to_line(el, where)
    EditLine *el;
    int     where;
{
    int     del, i;

    if (where == el->el_cursor.v)
	return;

    if (where > el->el_term.t_size.v) {
#ifdef DEBUG_SCREEN
	(void) fprintf(el->el_errfile,
		"term_move_to_line: where is ridiculous: %d\r\n", where);
#endif /* DEBUG_SCREEN */
	return;
    }

    if ((del = where - el->el_cursor.v) > 0) {
	if ((del > 1) && GoodStr(T_DO))
	    (void) tputs(tgoto(Str(T_DO), del, del), del, term__putc);
	else {
	    for (i = 0; i < del; i++)
		term__putc('\n');
	    el->el_cursor.h = 0;	/* because the \n will become \r\n */
	}
    }
    else {			/* del < 0 */
	if (GoodStr(T_UP) && (-del > 1 || !GoodStr(T_up)))
	    (void) tputs(tgoto(Str(T_UP), -del, -del), -del, term__putc);
	else {
	    if (GoodStr(T_up))
		for (i = 0; i < -del; i++)
		    (void) tputs(Str(T_up), 1, term__putc);
	}
    }
    el->el_cursor.v = where;		/* now where is here */
} /* end term_move_to_line */


/* term_move_to_char():
 *	Move to the character position specified
 */
protected void
term_move_to_char(el, where)
    EditLine *el;
    int     where;
{
    int     del, i;

mc_again:
    if (where == el->el_cursor.h)
	return;

    if (where > (el->el_term.t_size.h + 1)) {
#ifdef DEBUG_SCREEN
	(void) fprintf(el->el_errfile,
		"term_move_to_char: where is riduculous: %d\r\n", where);
#endif /* DEBUG_SCREEN */
	return;
    }

    if (!where) {		/* if where is first column */
	term__putc('\r');	/* do a CR */
	el->el_cursor.h = 0;
	return;
    }

    del = where - el->el_cursor.h;

    if ((del < -4 || del > 4) && GoodStr(T_ch))
	/* go there directly */
	(void) tputs(tgoto(Str(T_ch), where, where), where, term__putc);
    else {
	if (del > 0) {		/* moving forward */
	    if ((del > 4) && GoodStr(T_RI))
		(void) tputs(tgoto(Str(T_RI), del, del), del, term__putc);
	    else {
		if (EL_CAN_TAB) {	/* if I can do tabs, use them */
		    if ((el->el_cursor.h & 0370) != (where & 0370)) {
			/* if not within tab stop */
			for (i = (el->el_cursor.h & 0370);
			     i < (where & 0370); i += 8)
			    term__putc('\t');	/* then tab over */
			el->el_cursor.h = where & 0370;
		    }
		}
		/* it's usually cheaper to just write the chars, so we do. */

		/* NOTE THAT term_overwrite() WILL CHANGE el->el_cursor.h!!! */
		term_overwrite(el,
			&el->el_display[el->el_cursor.v][el->el_cursor.h],
		        where - el->el_cursor.h);

	    }
	}
	else {			/* del < 0 := moving backward */
	    if ((-del > 4) && GoodStr(T_LE))
		(void) tputs(tgoto(Str(T_LE), -del, -del), -del, term__putc);
	    else {		/* can't go directly there */
		/* if the "cost" is greater than the "cost" from col 0 */
		if (EL_CAN_TAB ? (-del > ((where >> 3) + (where & 07)))
		    : (-del > where)) {
		    term__putc('\r');	/* do a CR */
		    el->el_cursor.h = 0;
		    goto mc_again;	/* and try again */
		}
		for (i = 0; i < -del; i++)
		    term__putc('\b');
	    }
	}
    }
    el->el_cursor.h = where;		/* now where is here */
} /* end term_move_to_char */


/* term_overwrite():
 *	Overstrike num characters
 */
protected void
term_overwrite(el, cp, n)
    EditLine *el;
    char *cp;
    int n;
{
    if (n <= 0)
	return;			/* catch bugs */

    if (n > (el->el_term.t_size.h + 1)) {
#ifdef DEBUG_SCREEN
	(void) fprintf(el->el_errfile, "term_overwrite: n is riduculous: %d\r\n", n);
#endif /* DEBUG_SCREEN */
	return;
    }

    do {
	term__putc(*cp++);
	el->el_cursor.h++;
    } while (--n);
} /* end term_overwrite */


/* term_deletechars():
 *	Delete num characters
 */
protected void
term_deletechars(el, num)
    EditLine *el;
    int     num;
{
    if (num <= 0)
	return;

    if (!EL_CAN_DELETE) {
#ifdef DEBUG_EDIT
	(void) fprintf(el->el_errfile, "   ERROR: cannot delete   \n");
#endif /* DEBUG_EDIT */
	return;
    }

    if (num > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
	(void) fprintf(el->el_errfile,
		"term_deletechars: num is riduculous: %d\r\n", num);
#endif /* DEBUG_SCREEN */
	return;
    }

    if (GoodStr(T_DC))		/* if I have multiple delete */
	if ((num > 1) || !GoodStr(T_dc)) {	/* if dc would be more expen. */
	    (void) tputs(tgoto(Str(T_DC), num, num), num, term__putc);
	    return;
	}

    if (GoodStr(T_dm))		/* if I have delete mode */
	(void) tputs(Str(T_dm), 1, term__putc);

    if (GoodStr(T_dc))		/* else do one at a time */
	while (num--)
	    (void) tputs(Str(T_dc), 1, term__putc);

    if (GoodStr(T_ed))		/* if I have delete mode */
	(void) tputs(Str(T_ed), 1, term__putc);
} /* end term_deletechars */


/* term_insertwrite():
 *	Puts terminal in insert character mode or inserts num
 *	characters in the line
 */
protected void
term_insertwrite(el, cp, num)
    EditLine *el;
    char *cp;
    int num;
{
    if (num <= 0)
	return;
    if (!EL_CAN_INSERT) {
#ifdef DEBUG_EDIT
	(void) fprintf(el->el_errfile, "   ERROR: cannot insert   \n");
#endif /* DEBUG_EDIT */
	return;
    }

    if (num > el->el_term.t_size.h) {
#ifdef DEBUG_SCREEN
	(void) fprintf(el->el_errfile, "StartInsert: num is riduculous: %d\r\n", num);
#endif /* DEBUG_SCREEN */
	return;
    }

    if (GoodStr(T_IC))		/* if I have multiple insert */
	if ((num > 1) || !GoodStr(T_ic)) {	/* if ic would be more expen. */
	    (void) tputs(tgoto(Str(T_IC), num, num), num, term__putc);
	    term_overwrite(el, cp, num);	/* this updates el_cursor.h */
	    return;
	}

    if (GoodStr(T_im) && GoodStr(T_ei)) { /* if I have insert mode */
	(void) tputs(Str(T_im), 1, term__putc);

	el->el_cursor.h += num;
	do
	    term__putc(*cp++);
	while (--num);

	if (GoodStr(T_ip))	/* have to make num chars insert */
	    (void) tputs(Str(T_ip), 1, term__putc);

	(void) tputs(Str(T_ei), 1, term__putc);
	return;
    }

    do {
	if (GoodStr(T_ic))	/* have to make num chars insert */
	    (void) tputs(Str(T_ic), 1, term__putc);	/* insert a char */

	term__putc(*cp++);

	el->el_cursor.h++;

	if (GoodStr(T_ip))	/* have to make num chars insert */
	    (void) tputs(Str(T_ip), 1, term__putc);/* pad the inserted char */

    } while (--num);
} /* end term_insertwrite */


/* term_clear_EOL():
 *	clear to end of line.  There are num characters to clear
 */
protected void
term_clear_EOL(el, num)
    EditLine *el;
    int     num;
{
    int i;

    if (EL_CAN_CEOL && GoodStr(T_ce))
	(void) tputs(Str(T_ce), 1, term__putc);
    else {
	for (i = 0; i < num; i++)
	    term__putc(' ');
	el->el_cursor.h += num;		/* have written num spaces */
    }
} /* end term_clear_EOL */


/* term_clear_screen():
 *	Clear the screen
 */
protected void
term_clear_screen(el)
    EditLine *el;
{				/* clear the whole screen and home */
    if (GoodStr(T_cl))
	/* send the clear screen code */
	(void) tputs(Str(T_cl), Val(T_li), term__putc);
    else if (GoodStr(T_ho) && GoodStr(T_cd)) {
	(void) tputs(Str(T_ho), Val(T_li), term__putc);	/* home */
	/* clear to bottom of screen */
	(void) tputs(Str(T_cd), Val(T_li), term__putc);
    }
    else {
	term__putc('\r');
	term__putc('\n');
    }
} /* end term_clear_screen */


/* term_beep():
 *	Beep the way the terminal wants us
 */
protected void
term_beep(el)
    EditLine *el;
{
    if (GoodStr(T_vb))
	(void) tputs(Str(T_vb), 1, term__putc);	/* visible bell */
    else if (GoodStr(T_bl))
	/* what termcap says we should use */
	(void) tputs(Str(T_bl), 1, term__putc);
    else
	term__putc('\007');	/* an ASCII bell; ^G */
} /* end term_beep */


#ifdef notdef
/* term_clear_to_bottom():
 *	Clear to the bottom of the screen
 */
protected void
term_clear_to_bottom(el)
    EditLine *el;
{
    if (GoodStr(T_cd))
	(void) tputs(Str(T_cd), Val(T_li), term__putc);
    else if (GoodStr(T_ce))
	(void) tputs(Str(T_ce), Val(T_li), term__putc);
} /* end term_clear_to_bottom */
#endif


/* term_set():
 *	Read in the terminal capabilities from the requested terminal
 */
protected int
term_set(el, term)
    EditLine *el;
    char *term;
{
    int i;
    char    buf[TC_BUFSIZE];
    char   *area;
    struct termcapstr *t;
    sigset_t oset, nset;
    int     lins, cols;

    (void) sigemptyset(&nset);
    (void) sigaddset(&nset, SIGWINCH);
    (void) sigprocmask(SIG_BLOCK, &nset, &oset);

    area = buf;


    if (term == NULL)
	term = getenv("TERM");

    if (!term || !term[0])
	term = "dumb";

    memset(el->el_term.t_cap, 0, TC_BUFSIZE);

    i = tgetent(el->el_term.t_cap, term);

    if (i <= 0) {
	if (i == -1)
	    (void) fprintf(el->el_errfile, "Cannot read termcap database;\n");
	else if (i == 0)
	    (void) fprintf(el->el_errfile,
			   "No entry for terminal type \"%s\";\n", term);
	(void) fprintf(el->el_errfile, "using dumb terminal settings.\n");
	Val(T_co) = 80;		/* do a dumb terminal */
	Val(T_pt) = Val(T_km) = Val(T_li) = 0;
	Val(T_xt) = Val(T_MT);
	for (t = tstr; t->name != NULL; t++)
	    term_alloc(el, t, NULL);
    }
    else {
	/* Can we tab */
	Val(T_pt) = tgetflag("pt");
	Val(T_xt) = tgetflag("xt");
	/* do we have a meta? */
	Val(T_km) = tgetflag("km");
	Val(T_MT) = tgetflag("MT");
	/* Get the size */
	Val(T_co) = tgetnum("co");
	Val(T_li) = tgetnum("li");
	for (t = tstr; t->name != NULL; t++)
	    term_alloc(el, t, tgetstr(t->name, &area));
    }

    if (Val(T_co) < 2)
	Val(T_co) = 80;		/* just in case */
    if (Val(T_li) < 1)
	Val(T_li) = 24;

    el->el_term.t_size.v = Val(T_co);
    el->el_term.t_size.h = Val(T_li);

    term_setflags(el);

    (void) term_get_size(el, &lins, &cols);/* get the correct window size */
    term_change_size(el, lins, cols);
    (void) sigprocmask(SIG_SETMASK, &oset, NULL);
    term_bind_arrow(el);
    return i <= 0 ? -1 : 0;
} /* end term_set */


/* term_get_size():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed.
 */
protected int
term_get_size(el, lins, cols)
    EditLine *el;
    int    *lins, *cols;
{

    *cols = Val(T_co);
    *lins = Val(T_li);

#ifdef TIOCGWINSZ
    {
	struct winsize ws;
	if (ioctl(el->el_infd, TIOCGWINSZ, (ioctl_t) &ws) != -1) {
	    if (ws.ws_col)
		*cols = ws.ws_col;
	    if (ws.ws_row)
		*lins = ws.ws_row;
	}
    }
#endif
#ifdef TIOCGSIZE
    {
	struct ttysize ts;
	if (ioctl(el->el_infd, TIOCGSIZE, (ioctl_t) &ts) != -1) {
	    if (ts.ts_cols)
		*cols = ts.ts_cols;
	    if (ts.ts_lines)
		*lins = ts.ts_lines;
	}
    }
#endif
    return (Val(T_co) != *cols || Val(T_li) != *lins);
} /* end term_get_size */


/* term_change_size():
 *	Change the size of the terminal
 */
protected void
term_change_size(el, lins, cols)
    EditLine *el;
    int     lins, cols;
{
    /*
     * Just in case
     */
    Val(T_co) = (cols < 2) ? 80 : cols;
    Val(T_li) = (lins < 1) ? 24 : lins;

    term_rebuffer_display(el);		/* re-make display buffers */
    re_clear_display(el);
} /* end term_change_size */


/* term_init_arrow():
 *	Initialize the arrow key bindings from termcap
 */
private void
term_init_arrow(el)
    EditLine *el;
{
    fkey_t *arrow = el->el_term.t_fkey;

    arrow[A_K_DN].name    = "down";
    arrow[A_K_DN].key	  = T_kd;
    arrow[A_K_DN].fun.cmd = ED_NEXT_HISTORY;
    arrow[A_K_DN].type    = XK_CMD;

    arrow[A_K_UP].name    = "up";
    arrow[A_K_UP].key	  = T_ku;
    arrow[A_K_UP].fun.cmd = ED_PREV_HISTORY;
    arrow[A_K_UP].type    = XK_CMD;

    arrow[A_K_LT].name    = "left";
    arrow[A_K_LT].key	  = T_kl;
    arrow[A_K_LT].fun.cmd = ED_PREV_CHAR;
    arrow[A_K_LT].type    = XK_CMD;

    arrow[A_K_RT].name    = "right";
    arrow[A_K_RT].key	  = T_kr;
    arrow[A_K_RT].fun.cmd = ED_NEXT_CHAR;
    arrow[A_K_RT].type    = XK_CMD;

    arrow[A_K_HO].name    = "home";
    arrow[A_K_HO].key     = T_kh;
    arrow[A_K_HO].fun.cmd = ED_MOVE_TO_BEG;
    arrow[A_K_HO].type    = XK_CMD;

    arrow[A_K_EN].name    = "end";
    arrow[A_K_EN].key     = T_at7;
    arrow[A_K_EN].fun.cmd = ED_MOVE_TO_END;
    arrow[A_K_EN].type    = XK_CMD;
}


/* term_reset_arrow():
 *	Reset arrow key bindings
 */
private void
term_reset_arrow(el)
    EditLine *el;
{
    fkey_t *arrow = el->el_term.t_fkey;
    static char strA[] = {033, '[', 'A', '\0'};
    static char strB[] = {033, '[', 'B', '\0'};
    static char strC[] = {033, '[', 'C', '\0'};
    static char strD[] = {033, '[', 'D', '\0'};
    static char str1[] = {033, '[', '1', '~', '\0'};
    static char str4[] = {033, '[', '4', '~', '\0'};
    static char stOA[] = {033, 'O', 'A', '\0'};
    static char stOB[] = {033, 'O', 'B', '\0'};
    static char stOC[] = {033, 'O', 'C', '\0'};
    static char stOD[] = {033, 'O', 'D', '\0'};

    key_add(el, strA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
    key_add(el, strB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
    key_add(el, strC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
    key_add(el, strD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
    key_add(el, str1, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
    key_add(el, str4, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
    key_add(el, stOA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
    key_add(el, stOB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
    key_add(el, stOC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
    key_add(el, stOD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);

    if (el->el_map.type == MAP_VI) {
	key_add(el, &strA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	key_add(el, &strB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	key_add(el, &strC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	key_add(el, &strD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	key_add(el, &str1[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	key_add(el, &str4[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	key_add(el, &stOA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	key_add(el, &stOB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	key_add(el, &stOC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	key_add(el, &stOD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
    }
}


/* term_set_arrow():
 *	Set an arrow key binding
 */
protected int
term_set_arrow(el, name, fun, type)
    EditLine *el;
    char *name;
    key_value_t *fun;
    int type;
{
    fkey_t *arrow = el->el_term.t_fkey;
    int i;

    for (i = 0; i < A_K_NKEYS; i++)
	if (strcmp(name, arrow[i].name) == 0) {
	    arrow[i].fun  = *fun;
	    arrow[i].type = type;
	    return 0;
	}
    return -1;
}


/* term_clear_arrow():
 *	Clear an arrow key binding
 */
protected int
term_clear_arrow(el, name)
    EditLine *el;
    char *name;
{
    fkey_t *arrow = el->el_term.t_fkey;
    int i;

    for (i = 0; i < A_K_NKEYS; i++)
	if (strcmp(name, arrow[i].name) == 0) {
	    arrow[i].type = XK_NOD;
	    return 0;
	}
    return -1;
}


/* term_print_arrow():
 *	Print the arrow key bindings
 */
protected void
term_print_arrow(el, name)
    EditLine *el;
    char *name;
{
    int i;
    fkey_t *arrow = el->el_term.t_fkey;

    for (i = 0; i < A_K_NKEYS; i++)
	if (*name == '\0' || strcmp(name, arrow[i].name) == 0)
	    if (arrow[i].type != XK_NOD)
		key_kprint(el, arrow[i].name, &arrow[i].fun, arrow[i].type);
}


/* term_bind_arrow():
 *	Bind the arrow keys
 */
protected void
term_bind_arrow(el)
    EditLine *el;
{
    el_action_t *map, *dmap;
    int     i, j;
    char   *p;
    fkey_t *arrow = el->el_term.t_fkey;

    /* Check if the components needed are initialized */
    if (el->el_term.t_buf == NULL || el->el_map.key == NULL)
	return;

    map  = el->el_map.type == MAP_VI ? el->el_map.alt : el->el_map.key;
    dmap = el->el_map.type == MAP_VI ? el->el_map.vic : el->el_map.emacs;

    term_reset_arrow(el);

    for (i = 0; i < A_K_NKEYS; i++) {
	p = el->el_term.t_str[arrow[i].key];
	if (p && *p) {
	    j = (unsigned char) *p;
	    /*
	     * Assign the arrow keys only if:
	     *
	     * 1. They are multi-character arrow keys and the user
	     *    has not re-assigned the leading character, or
	     *    has re-assigned the leading character to be
	     *	  ED_SEQUENCE_LEAD_IN
	     * 2. They are single arrow keys pointing to an unassigned key.
	     */
	    if (arrow[i].type == XK_NOD)
		key_clear(el, map, p);
	    else {
		if (p[1] && (dmap[j] == map[j] ||
			     map[j] == ED_SEQUENCE_LEAD_IN)) {
		    key_add(el, p, &arrow[i].fun, arrow[i].type);
		    map[j] = ED_SEQUENCE_LEAD_IN;
		}
		else if (map[j] == ED_UNASSIGNED) {
		    key_clear(el, map, p);
		    if (arrow[i].type == XK_CMD)
			map[j] = arrow[i].fun.cmd;
		    else
			key_add(el, p, &arrow[i].fun, arrow[i].type);
		}
	    }
	}
    }
}


/* term__putc():
 *	Add a character
 */
protected int
term__putc(c)
    int c;
{
    return fputc(c, term_outfile);
} /* end term__putc */


/* term__flush():
 *	Flush output
 */
protected void
term__flush()
{
    (void) fflush(term_outfile);
} /* end term__flush */


/* term_telltc():
 *	Print the current termcap characteristics
 */
protected int
/*ARGSUSED*/
term_telltc(el, argc, argv)
    EditLine *el;
    int argc;
    char **argv;
{
    struct termcapstr *t;
    char **ts;
    char upbuf[EL_BUFSIZ];

    (void) fprintf(el->el_outfile, "\n\tYour terminal has the\n");
    (void) fprintf(el->el_outfile, "\tfollowing characteristics:\n\n");
    (void) fprintf(el->el_outfile, "\tIt has %d columns and %d lines\n",
	    Val(T_co), Val(T_li));
    (void) fprintf(el->el_outfile,
		   "\tIt has %s meta key\n", EL_HAS_META ? "a" : "no");
    (void) fprintf(el->el_outfile,
		   "\tIt can%suse tabs\n", EL_CAN_TAB ? " " : "not ");
#ifdef notyet
    (void) fprintf(el->el_outfile, "\tIt %s automatic margins\n",
		   (T_Margin&MARGIN_AUTO)? "has": "does not have");
    if (T_Margin & MARGIN_AUTO)
	(void) fprintf(el->el_outfile, "\tIt %s magic margins\n",
			(T_Margin&MARGIN_MAGIC)?"has":"does not have");
#endif

    for (t = tstr, ts = el->el_term.t_str; t->name != NULL; t++, ts++)
	(void) fprintf(el->el_outfile, "\t%25s (%s) == %s\n", t->long_name,
		       t->name, *ts && **ts ?
			key__decode_str(*ts, upbuf, "") : "(empty)");
    (void) fputc('\n', el->el_outfile);
    return 0;
}


/* term_settc():
 *	Change the current terminal characteristics
 */
protected int
/*ARGSUSED*/
term_settc(el, argc, argv)
    EditLine *el;
    int argc;
    char **argv;
{
    struct termcapstr *ts;
    struct termcapval *tv;
    char   *what, *how;

    if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
	return -1;

    what = argv[1];
    how = argv[2];

    /*
     * Do the strings first
     */
    for (ts = tstr; ts->name != NULL; ts++)
	if (strcmp(ts->name, what) == 0)
	    break;

    if (ts->name != NULL) {
	term_alloc(el, ts, how);
	term_setflags(el);
	return 0;
    }

    /*
     * Do the numeric ones second
     */
    for (tv = tval; tv->name != NULL; tv++)
	if (strcmp(tv->name, what) == 0)
	    break;

    if (tv->name != NULL) {
	if (tv == &tval[T_pt] || tv == &tval[T_km]
#ifdef notyet
	    || tv == &tval[T_am] || tv == &tval[T_xn]
#endif
	    ) {
	    if (strcmp(how, "yes") == 0)
		el->el_term.t_val[tv - tval] = 1;
	    else if (strcmp(how, "no") == 0)
		el->el_term.t_val[tv - tval] = 0;
	    else {
		(void) fprintf(el->el_errfile, "settc: Bad value `%s'.\n", how);
		return -1;
	    }
	    term_setflags(el);
	    term_change_size(el, Val(T_li), Val(T_co));
	    return 0;
	}
	else {
	    el->el_term.t_val[tv - tval] = atoi(how);
	    el->el_term.t_size.v = Val(T_co);
	    el->el_term.t_size.h = Val(T_li);
	    if (tv == &tval[T_co] || tv == &tval[T_li])
		term_change_size(el, Val(T_li), Val(T_co));
	    return 0;
	}
    }
    return -1;
}


/* term_echotc():
 *	Print the termcap string out with variable substitution
 */
protected int
/*ARGSUSED*/
term_echotc(el, argc, argv)
    EditLine *el;
    int argc;
    char **argv;
{
    char   *cap, *scap;
    int     arg_need, arg_cols, arg_rows;
    int     verbose = 0, silent = 0;
    char   *area;
    static char *fmts = "%s\n", *fmtd = "%d\n";
    struct termcapstr *t;
    char    buf[TC_BUFSIZE];

    area = buf;

    if (argv == NULL || argv[1] == NULL)
	return -1;
    argv++;

    if (argv[0][0] == '-') {
	switch (argv[0][1]) {
	case 'v':
	    verbose = 1;
	    break;
	case 's':
	    silent = 1;
	    break;
	default:
	    /* stderror(ERR_NAME | ERR_TCUSAGE); */
	    break;
	}
	argv++;
    }
    if (!*argv || *argv[0] == '\0')
	return 0;
    if (strcmp(*argv, "tabs") == 0) {
	(void) fprintf(el->el_outfile, fmts, EL_CAN_TAB ? "yes" : "no");
	return 0;
    }
    else if (strcmp(*argv, "meta") == 0) {
	(void) fprintf(el->el_outfile, fmts, Val(T_km) ? "yes" : "no");
	return 0;
    }
#ifdef notyet
    else if (strcmp(*argv, "xn") == 0) {
	(void) fprintf(el->el_outfile, fmts, T_Margin & MARGIN_MAGIC ?
			"yes" : "no");
	return 0;
    }
    else if (strcmp(*argv, "am") == 0) {
	(void) fprintf(el->el_outfile, fmts, T_Margin & MARGIN_AUTO ?
			"yes" : "no");
	return 0;
    }
#endif
    else if (strcmp(*argv, "baud") == 0) {
	(void) fprintf(el->el_outfile, "%lu\n", (u_long)el->el_tty.t_speed);
	return 0;
    }
    else if (strcmp(*argv, "rows") == 0 || strcmp(*argv, "lines") == 0) {
	(void) fprintf(el->el_outfile, fmtd, Val(T_li));
	return 0;
    }
    else if (strcmp(*argv, "cols") == 0) {
	(void) fprintf(el->el_outfile, fmtd, Val(T_co));
	return 0;
    }

    /*
     * Try to use our local definition first
     */
    scap = NULL;
    for (t = tstr; t->name != NULL; t++)
	if (strcmp(t->name, *argv) == 0) {
	    scap = el->el_term.t_str[t - tstr];
	    break;
	}
    if (t->name == NULL)
	scap = tgetstr(*argv, &area);
    if (!scap || scap[0] == '\0') {
	if (!silent)
	    (void) fprintf(el->el_errfile,
		"echotc: Termcap parameter `%s' not found.\n", *argv);
	return -1;
    }

    /*
     * Count home many values we need for this capability.
     */
    for (cap = scap, arg_need = 0; *cap; cap++)
	if (*cap == '%')
	    switch (*++cap) {
	    case 'd':
	    case '2':
	    case '3':
	    case '.':
	    case '+':
		arg_need++;
		break;
	    case '%':
	    case '>':
	    case 'i':
	    case 'r':
	    case 'n':
	    case 'B':
	    case 'D':
		break;
	    default:
		/*
		 * hpux has lot's of them...
		 */
		if (verbose)
		    (void) fprintf(el->el_errfile,
			"echotc: Warning: unknown termcap %% `%c'.\n", *cap);
		/* This is bad, but I won't complain */
		break;
	    }

    switch (arg_need) {
    case 0:
	argv++;
	if (*argv && *argv[0]) {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Extra argument `%s'.\n", *argv);
	    return -1;
	}
	(void) tputs(scap, 1, term__putc);
	break;
    case 1:
	argv++;
	if (!*argv || *argv[0] == '\0') {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Missing argument.\n");
	    return -1;
	}
	arg_cols = 0;
	arg_rows = atoi(*argv);
	argv++;
	if (*argv && *argv[0]) {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Extra argument `%s'.\n", *argv);
	    return -1;
	}
	(void) tputs(tgoto(scap, arg_cols, arg_rows), 1, term__putc);
	break;
    default:
	/* This is wrong, but I will ignore it... */
	if (verbose)
	    (void) fprintf(el->el_errfile,
		"echotc: Warning: Too many required arguments (%d).\n",
		arg_need);
	/*FALLTHROUGH*/
    case 2:
	argv++;
	if (!*argv || *argv[0] == '\0') {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Missing argument.\n");
	    return -1;
	}
	arg_cols = atoi(*argv);
	argv++;
	if (!*argv || *argv[0] == '\0') {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Missing argument.\n");
	    return -1;
	}
	arg_rows = atoi(*argv);
	argv++;
	if (*argv && *argv[0]) {
	    if (!silent)
		(void) fprintf(el->el_errfile,
		    "echotc: Warning: Extra argument `%s'.\n", *argv);
	    return -1;
	}
	(void) tputs(tgoto(scap, arg_cols, arg_rows), arg_rows, term__putc);
	break;
    }
    return 0;
}
