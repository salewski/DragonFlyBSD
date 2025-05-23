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
 * @(#)common.c	8.1 (Berkeley) 6/4/93
 * $DragonFly: src/lib/libedit/common.c,v 1.3 2004/10/25 19:38:45 drhodus Exp $
 */

/*
 * common.c: Common Editor functions
 */
#include "sys.h"
#include "el.h"

/* ed_end_of_file():
 *	Indicate end of file
 *	[^D]
 */
protected el_action_t
/*ARGSUSED*/
ed_end_of_file(el, c)
    EditLine *el;
    int c;
{
    re_goto_bottom(el);
    *el->el_line.lastchar = '\0';
    return CC_EOF;
}


/* ed_insert():
 *	Add character to the line
 *	Insert a character [bound to all insert keys]
 */
protected el_action_t
ed_insert(el, c)
    EditLine *el;
    int c;
{
    int i;

    if (c == '\0')
	return CC_ERROR;

    if (el->el_line.lastchar + el->el_state.argument >=
	el->el_line.limit)
	return CC_ERROR;	/* end of buffer space */

    if (el->el_state.argument == 1) {
	if (el->el_state.inputmode != MODE_INSERT) {
	    el->el_chared.c_undo.buf[el->el_chared.c_undo.isize++] =
		*el->el_line.cursor;
	    el->el_chared.c_undo.buf[el->el_chared.c_undo.isize] = '\0';
	    c_delafter(el, 1);
    	}

        c_insert(el, 1);

	*el->el_line.cursor++ = c;
	el->el_state.doingarg = 0;		/* just in case */
	re_fastaddc(el);			/* fast refresh for one char. */
    }
    else {
	if (el->el_state.inputmode != MODE_INSERT) {

	    for(i = 0;i < el->el_state.argument; i++)
		el->el_chared.c_undo.buf[el->el_chared.c_undo.isize++] =
			el->el_line.cursor[i];

	    el->el_chared.c_undo.buf[el->el_chared.c_undo.isize] = '\0';
	    c_delafter(el, el->el_state.argument);
    	}

        c_insert(el, el->el_state.argument);

	while (el->el_state.argument--)
	    *el->el_line.cursor++ = c;
	re_refresh(el);
    }

    if (el->el_state.inputmode == MODE_REPLACE_1 || el->el_state.inputmode == MODE_REPLACE)
	el->el_chared.c_undo.action=CHANGE;

    if (el->el_state.inputmode == MODE_REPLACE_1)
	return vi_command_mode(el, 0);

    return CC_NORM;
}


/* ed_delete_prev_word():
 *	Delete from beginning of current word to cursor
 *	[M-^?] [^W]
 */
protected el_action_t
/*ARGSUSED*/
ed_delete_prev_word(el, c)
    EditLine *el;
    int c;
{
    char *cp, *p, *kp;

    if (el->el_line.cursor == el->el_line.buffer)
	return CC_ERROR;

    cp = c__prev_word(el->el_line.cursor, el->el_line.buffer,
		      el->el_state.argument, ce__isword);

    for (p = cp, kp = el->el_chared.c_kill.buf; p < el->el_line.cursor; p++)
	*kp++ = *p;
    el->el_chared.c_kill.last = kp;

    c_delbefore(el, el->el_line.cursor - cp);	/* delete before dot */
    el->el_line.cursor = cp;
    if (el->el_line.cursor < el->el_line.buffer)
	el->el_line.cursor = el->el_line.buffer;	/* bounds check */
    return CC_REFRESH;
}


/* ed_delete_next_char():
 *	Delete character under cursor
 *	[^D] [x]
 */
protected el_action_t
/*ARGSUSED*/
ed_delete_next_char(el, c)
    EditLine *el;
    int c;
{
#ifdef notdef /* XXX */
#define EL el->el_line
fprintf(stderr, "\nD(b: %x(%s)  c: %x(%s) last: %x(%s) limit: %x(%s)\n",
	EL.buffer, EL.buffer, EL.cursor, EL.cursor, EL.lastchar, EL.lastchar, EL.limit, EL.limit);
#endif
    if (el->el_line.cursor == el->el_line.lastchar) {/* if I'm at the end */
	if (el->el_map.type == MAP_VI) {
	    if (el->el_line.cursor == el->el_line.buffer) {
		/* if I'm also at the beginning */
#ifdef KSHVI
		return CC_ERROR;
#else
		term_overwrite(el, STReof, 4);/* then do a EOF */
		term__flush();
		return CC_EOF;
#endif
	    }
	    else  {
#ifdef KSHVI
		el->el_line.cursor--;
#else
		return CC_ERROR;
#endif
	    }
	}
	else {
	    if (el->el_line.cursor != el->el_line.buffer)
		el->el_line.cursor--;
	    else
		return CC_ERROR;
	}
    }
    c_delafter(el, el->el_state.argument);	/* delete after dot */
    if (el->el_line.cursor >= el->el_line.lastchar && el->el_line.cursor > el->el_line.buffer)
	el->el_line.cursor = el->el_line.lastchar - 1;	/* bounds check */
    return CC_REFRESH;
}


/* ed_kill_line():
 *	Cut to the end of line
 *	[^K] [^K]
 */
protected el_action_t
/*ARGSUSED*/
ed_kill_line(el, c)
    EditLine *el;
    int c;
{
    char *kp, *cp;

    cp = el->el_line.cursor;
    kp = el->el_chared.c_kill.buf;
    while (cp < el->el_line.lastchar)
	*kp++ = *cp++;		/* copy it */
    el->el_chared.c_kill.last = kp;
    el->el_line.lastchar = el->el_line.cursor; /* zap! -- delete to end */
    return CC_REFRESH;
}


/* ed_move_to_end():
 *	Move cursor to the end of line
 *	[^E] [^E]
 */
protected el_action_t
/*ARGSUSED*/
ed_move_to_end(el, c)
    EditLine *el;
    int c;
{
    el->el_line.cursor = el->el_line.lastchar;
    if (el->el_map.type == MAP_VI) {
#ifdef VI_MOVE
	el->el_line.cursor--;
#endif
	if (el->el_chared.c_vcmd.action & DELETE) {
	    cv_delfini(el);
	    return CC_REFRESH;
	}
    }
    return CC_CURSOR;
}


/* ed_move_to_beg():
 *	Move cursor to the beginning of line
 *	[^A] [^A]
 */
protected el_action_t
/*ARGSUSED*/
ed_move_to_beg(el, c)
    EditLine *el;
    int c;
{
    el->el_line.cursor = el->el_line.buffer;

    if (el->el_map.type == MAP_VI) {
        /* We want FIRST non space character */
	while (isspace((unsigned char) *el->el_line.cursor))
	    el->el_line.cursor++;
	if (el->el_chared.c_vcmd.action & DELETE) {
	    cv_delfini(el);
	    return CC_REFRESH;
	}
    }

    return CC_CURSOR;
}


/* ed_transpose_chars():
 *	Exchange the character to the left of the cursor with the one under it
 *	[^T] [^T]
 */
protected el_action_t
ed_transpose_chars(el, c)
    EditLine *el;
    int c;
{
    if (el->el_line.cursor < el->el_line.lastchar) {
	if (el->el_line.lastchar <= &el->el_line.buffer[1])
	    return CC_ERROR;
	else
	    el->el_line.cursor++;
    }
    if (el->el_line.cursor > &el->el_line.buffer[1]) {
	/* must have at least two chars entered */
	c = el->el_line.cursor[-2];
	el->el_line.cursor[-2] = el->el_line.cursor[-1];
	el->el_line.cursor[-1] = c;
	return CC_REFRESH;
    }
    else
	return CC_ERROR;
}


/* ed_next_char():
 *	Move to the right one character
 *	[^F] [^F]
 */
protected el_action_t
/*ARGSUSED*/
ed_next_char(el, c)
    EditLine *el;
    int c;
{
    if (el->el_line.cursor >= el->el_line.lastchar)
	return CC_ERROR;

    el->el_line.cursor += el->el_state.argument;
    if (el->el_line.cursor > el->el_line.lastchar)
	el->el_line.cursor = el->el_line.lastchar;

    if (el->el_map.type == MAP_VI)
	if (el->el_chared.c_vcmd.action & DELETE) {
	    cv_delfini(el);
	    return CC_REFRESH;
	}

    return CC_CURSOR;
}


/* ed_prev_word():
 *	Move to the beginning of the current word
 *	[M-b] [b]
 */
protected el_action_t
/*ARGSUSED*/
ed_prev_word(el, c)
    EditLine *el;
    int c;
{
    if (el->el_line.cursor == el->el_line.buffer)
	return CC_ERROR;

    el->el_line.cursor = c__prev_word(el->el_line.cursor, el->el_line.buffer,
				      el->el_state.argument,
				      ce__isword);

    if (el->el_map.type == MAP_VI)
	if (el->el_chared.c_vcmd.action & DELETE) {
	    cv_delfini(el);
	    return CC_REFRESH;
	}

    return CC_CURSOR;
}


/* ed_prev_char():
 *	Move to the left one character
 *	[^B] [^B]
 */
protected el_action_t
/*ARGSUSED*/
ed_prev_char(el, c)
    EditLine *el;
    int c;
{
    if (el->el_line.cursor > el->el_line.buffer) {
	el->el_line.cursor -= el->el_state.argument;
	if (el->el_line.cursor < el->el_line.buffer)
	    el->el_line.cursor = el->el_line.buffer;

	if (el->el_map.type == MAP_VI)
	    if (el->el_chared.c_vcmd.action & DELETE) {
		cv_delfini(el);
		return CC_REFRESH;
	    }

	return CC_CURSOR;
    }
    else
	return CC_ERROR;
}


/* ed_quoted_insert():
 *	Add the next character typed verbatim
 *	[^V] [^V]
 */
protected el_action_t
ed_quoted_insert(el, c)
    EditLine *el;
    int c;
{
    int     num;
    char    tc;

    tty_quotemode(el);
    num = el_getc(el, &tc);
    c = (unsigned char) tc;
    tty_noquotemode(el);
    if (num == 1)
	return ed_insert(el, c);
    else
	return ed_end_of_file(el, 0);
}


/* ed_digit():
 *	Adds to argument or enters a digit
 */
protected el_action_t
ed_digit(el, c)
    EditLine *el;
    int c;
{
    if (!isdigit((unsigned char) c))
	return CC_ERROR;

    if (el->el_state.doingarg) {
	/* if doing an arg, add this in... */
	if (el->el_state.lastcmd == EM_UNIVERSAL_ARGUMENT)
	    el->el_state.argument = c - '0';
	else {
	    if (el->el_state.argument > 1000000)
		return CC_ERROR;
	    el->el_state.argument =
		(el->el_state.argument * 10) + (c - '0');
	}
	return CC_ARGHACK;
    }
    else {
	if (el->el_line.lastchar + 1 >= el->el_line.limit)
	    return CC_ERROR;

	if (el->el_state.inputmode != MODE_INSERT) {
	    el->el_chared.c_undo.buf[el->el_chared.c_undo.isize++] =
		*el->el_line.cursor;
	    el->el_chared.c_undo.buf[el->el_chared.c_undo.isize] = '\0';
	    c_delafter(el, 1);
    	}
	c_insert(el, 1);
	*el->el_line.cursor++ = c;
	el->el_state.doingarg = 0;
	re_fastaddc(el);
    }
    return CC_NORM;
}


/* ed_argument_digit():
 *	Digit that starts argument
 *	For ESC-n
 */
protected el_action_t
ed_argument_digit(el, c)
    EditLine *el;
    int c;
{
    if (!isdigit((unsigned char) c))
	return CC_ERROR;

    if (el->el_state.doingarg) {
	if (el->el_state.argument > 1000000)
	    return CC_ERROR;
	el->el_state.argument = (el->el_state.argument * 10) + (c - '0');
    }
    else {			/* else starting an argument */
	el->el_state.argument = c - '0';
	el->el_state.doingarg = 1;
    }
    return CC_ARGHACK;
}


/* ed_unassigned():
 *	Indicates unbound character
 *	Bound to keys that are not assigned
 */
protected el_action_t
/*ARGSUSED*/
ed_unassigned(el, c)
    EditLine *el;
    int c;
{
    term_beep(el);
    term__flush();
    return CC_NORM;
}


/**
 ** TTY key handling.
 **/

/* ed_tty_sigint():
 *	Tty interrupt character
 *	[^C]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_sigint(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_dsusp():
 *	Tty delayed suspend character
 *	[^Y]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_dsusp(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_flush_output():
 *	Tty flush output characters
 *	[^O]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_flush_output(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_sigquit():
 *	Tty quit character
 *	[^\]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_sigquit(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_sigtstp():
 *	Tty suspend character
 *	[^Z]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_sigtstp(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_stop_output():
 *	Tty disallow output characters
 *	[^S]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_stop_output(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_tty_start_output():
 *	Tty allow output characters
 *	[^Q]
 */
protected el_action_t
/*ARGSUSED*/
ed_tty_start_output(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_newline():
 *	Execute command
 *	[^J]
 */
protected el_action_t
/*ARGSUSED*/
ed_newline(el, c)
    EditLine *el;
    int c;
{
    re_goto_bottom(el);
    *el->el_line.lastchar++ = '\n';
    *el->el_line.lastchar = '\0';
    if (el->el_map.type == MAP_VI)
	el->el_chared.c_vcmd.ins = el->el_line.buffer;
    return CC_NEWLINE;
}


/* ed_delete_prev_char():
 *	Delete the character to the left of the cursor
 *	[^?]
 */
protected el_action_t
/*ARGSUSED*/
ed_delete_prev_char(el, c)
    EditLine *el;
    int c;
{
    if (el->el_line.cursor <= el->el_line.buffer)
	return CC_ERROR;

    c_delbefore(el, el->el_state.argument);
    el->el_line.cursor -= el->el_state.argument;
    if (el->el_line.cursor < el->el_line.buffer)
	el->el_line.cursor = el->el_line.buffer;
    return CC_REFRESH;
}


/* ed_clear_screen():
 *	Clear screen leaving current line at the top
 *	[^L]
 */
protected el_action_t
/*ARGSUSED*/
ed_clear_screen(el, c)
    EditLine *el;
    int c;
{
    term_clear_screen(el);	/* clear the whole real screen */
    re_clear_display(el);		/* reset everything */
    return CC_REFRESH;
}


/* ed_redisplay():
 *	Redisplay everything
 *	^R
 */
protected el_action_t
/*ARGSUSED*/
ed_redisplay(el, c)
    EditLine *el;
    int c;
{
    return CC_REDISPLAY;
}


/* ed_start_over():
 *	Erase current line and start from scratch
 *	[^G]
 */
protected el_action_t
/*ARGSUSED*/
ed_start_over(el, c)
    EditLine *el;
    int c;
{
    ch_reset(el);
    return CC_REFRESH;
}


/* ed_sequence_lead_in():
 *	First character in a bound sequence
 *	Placeholder for external keys
 */
protected el_action_t
/*ARGSUSED*/
ed_sequence_lead_in(el, c)
    EditLine *el;
    int c;
{
    return CC_NORM;
}


/* ed_prev_history():
 *	Move to the previous history line
 *	[^P] [k]
 */
protected el_action_t
/*ARGSUSED*/
ed_prev_history(el, c)
    EditLine *el;
    int c;
{
    char    beep = 0;

    el->el_chared.c_undo.action = NOP;
    *el->el_line.lastchar = '\0';		/* just in case */

    if (el->el_history.eventno == 0) {	/* save the current buffer away */
	(void) strncpy(el->el_history.buf, el->el_line.buffer, EL_BUFSIZ);
	el->el_history.last = el->el_history.buf +
		(el->el_line.lastchar - el->el_line.buffer);
    }

    el->el_history.eventno += el->el_state.argument;

    if (hist_get(el) == CC_ERROR) {
	beep = 1;
	/* el->el_history.eventno was fixed by first call */
	(void) hist_get(el);
    }

    re_refresh(el);
    if (beep)
	return CC_ERROR;
    else
	return CC_NORM;	/* was CC_UP_HIST */
}


/* ed_next_history():
 *	Move to the next history line
 *	[^N] [j]
 */
protected el_action_t
/*ARGSUSED*/
ed_next_history(el, c)
    EditLine *el;
    int c;
{
    el->el_chared.c_undo.action = NOP;
    *el->el_line.lastchar = '\0';		/* just in case */

    el->el_history.eventno -= el->el_state.argument;

    if (el->el_history.eventno < 0) {
	el->el_history.eventno = 0;
	return CC_ERROR;	/* make it beep */
    }

    return hist_get(el);
}


/* ed_search_prev_history():
 *	Search previous in history for a line matching the current
 *	next search history [M-P] [K]
 */
protected el_action_t
/*ARGSUSED*/
ed_search_prev_history(el, c)
    EditLine *el;
    int c;
{
    const char *hp;
    int h;
    bool_t    found = 0;

    el->el_chared.c_vcmd.action = NOP;
    el->el_chared.c_undo.action = NOP;
    *el->el_line.lastchar = '\0';		/* just in case */
    if (el->el_history.eventno < 0) {
#ifdef DEBUG_EDIT
	(void) fprintf(el->el_errfile, "e_prev_search_hist(): eventno < 0;\n");
#endif
	el->el_history.eventno = 0;
	return CC_ERROR;
    }

    if (el->el_history.eventno == 0) {
	(void) strncpy(el->el_history.buf, el->el_line.buffer, EL_BUFSIZ);
	el->el_history.last = el->el_history.buf +
		(el->el_line.lastchar - el->el_line.buffer);
    }


    if (el->el_history.ref == NULL)
	return CC_ERROR;

    hp = HIST_FIRST(el);
    if (hp == NULL)
	return CC_ERROR;

    c_setpat(el);		/* Set search pattern !! */

    for (h = 1; h <= el->el_history.eventno; h++)
	hp = HIST_NEXT(el);

    while (hp != NULL) {
#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "Comparing with \"%s\"\n", hp);
#endif
	if ((strncmp(hp, el->el_line.buffer,
		     el->el_line.lastchar - el->el_line.buffer) ||
	    hp[el->el_line.lastchar-el->el_line.buffer]) &&
	    c_hmatch(el, hp)) {
	    found++;
	    break;
	}
	h++;
	hp = HIST_NEXT(el);
    }

    if (!found) {
#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "not found\n");
#endif
	return CC_ERROR;
    }

    el->el_history.eventno = h;

    return hist_get(el);
}


/* ed_search_next_history():
 *	Search next in history for a line matching the current
 *	[M-N] [J]
 */
protected el_action_t
/*ARGSUSED*/
ed_search_next_history(el, c)
    EditLine *el;
    int c;
{
    const char *hp;
    int h;
    bool_t    found = 0;

    el->el_chared.c_vcmd.action = NOP;
    el->el_chared.c_undo.action = NOP;
    *el->el_line.lastchar = '\0';		/* just in case */

    if (el->el_history.eventno == 0)
	return CC_ERROR;

    if (el->el_history.ref == NULL)
	return CC_ERROR;

    hp = HIST_FIRST(el);
    if (hp == NULL)
	return CC_ERROR;

    c_setpat(el);		/* Set search pattern !! */

    for (h = 1; h < el->el_history.eventno && hp; h++) {
#ifdef SDEBUG
	(void) fprintf(el->el_errfile, "Comparing with \"%s\"\n", hp);
#endif
	if ((strncmp(hp, el->el_line.buffer,
		     el->el_line.lastchar - el->el_line.buffer) ||
	     hp[el->el_line.lastchar-el->el_line.buffer]) &&
	    c_hmatch(el, hp))
	    found = h;
	hp = HIST_NEXT(el);
    }

    if (!found) {		/* is it the current history number? */
	if (!c_hmatch(el, el->el_history.buf)) {
#ifdef SDEBUG
	    (void) fprintf(el->el_errfile, "not found\n");
#endif
	    return CC_ERROR;
	}
    }

    el->el_history.eventno = found;

    return hist_get(el);
}


/* ed_prev_line():
 *	Move up one line
 *	Could be [k] [^p]
 */
protected el_action_t
/*ARGSUSED*/
ed_prev_line(el, c)
    EditLine *el;
    int c;
{
    char *ptr;
    int nchars = c_hpos(el);

    /*
     * Move to the line requested
     */
    if (*(ptr = el->el_line.cursor) == '\n')
	ptr--;

    for (; ptr >= el->el_line.buffer; ptr--)
	if (*ptr == '\n' && --el->el_state.argument <= 0)
	    break;

    if (el->el_state.argument > 0)
	return CC_ERROR;

    /*
     * Move to the beginning of the line
     */
    for (ptr--; ptr >= el->el_line.buffer && *ptr != '\n'; ptr--)
	continue;

    /*
     * Move to the character requested
     */
    for (ptr++;
	 nchars-- > 0 && ptr < el->el_line.lastchar && *ptr != '\n';
	 ptr++)
	continue;

    el->el_line.cursor = ptr;
    return CC_CURSOR;
}


/* ed_next_line():
 *	Move down one line
 *	Could be [j] [^n]
 */
protected el_action_t
/*ARGSUSED*/
ed_next_line(el, c)
    EditLine *el;
    int c;
{
    char *ptr;
    int nchars = c_hpos(el);

    /*
     * Move to the line requested
     */
    for (ptr = el->el_line.cursor; ptr < el->el_line.lastchar; ptr++)
	if (*ptr == '\n' && --el->el_state.argument <= 0)
	    break;

    if (el->el_state.argument > 0)
	return CC_ERROR;

    /*
     * Move to the character requested
     */
    for (ptr++;
	 nchars-- > 0 && ptr < el->el_line.lastchar && *ptr != '\n';
    	 ptr++)
	continue;

    el->el_line.cursor = ptr;
    return CC_CURSOR;
}


/* ed_command():
 *	Editline extended command
 *	[M-X] [:]
 */
protected el_action_t
/*ARGSUSED*/
ed_command(el, c)
    EditLine *el;
    int c;
{
    char tmpbuf[EL_BUFSIZ];
    int tmplen;

    el->el_line.buffer[0] = '\0';
    el->el_line.lastchar = el->el_line.buffer;
    el->el_line.cursor = el->el_line.buffer;

    c_insert(el, 3);	/* prompt + ": " */
    *el->el_line.cursor++ = '\n';
    *el->el_line.cursor++ = ':';
    *el->el_line.cursor++ = ' ';
    re_refresh(el);

    tmplen = c_gets(el, tmpbuf);
    tmpbuf[tmplen] = '\0';

    el->el_line.buffer[0] = '\0';
    el->el_line.lastchar = el->el_line.buffer;
    el->el_line.cursor = el->el_line.buffer;

    if (parse_line(el, tmpbuf) == -1)
	return CC_ERROR;
    else
	return CC_REFRESH;
}
