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
 * @(#)read.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libedit/read.c,v 1.4.6.3 2000/10/31 23:51:10 brian Exp $
 * $DragonFly: src/lib/libedit/read.c,v 1.3 2003/11/12 20:21:29 eirikn Exp $
 */

/*
 * read.c: Clean this junk up! This is horrible code.
 *	   Terminal read functions
 */
#include "sys.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "el.h"

#define OKCMD -1

private int read__fixio		(int, int);
private int read_preread	(EditLine *);
private int read_getcmd		(EditLine *, el_action_t *, char *);

#ifdef DEBUG_EDIT
private void
read_debug(el)
    EditLine *el;
{

    if (el->el_line.cursor > el->el_line.lastchar)
	(void) fprintf(el->el_errfile, "cursor > lastchar\r\n");
    if (el->el_line.cursor < el->el_line.buffer)
	(void) fprintf(el->el_errfile, "cursor < buffer\r\n");
    if (el->el_line.cursor > el->el_line.limit)
	(void) fprintf(el->el_errfile, "cursor > limit\r\n");
    if (el->el_line.lastchar > el->el_line.limit)
	(void) fprintf(el->el_errfile, "lastchar > limit\r\n");
    if (el->el_line.limit != &el->el_line.buffer[EL_BUFSIZ - 2])
	(void) fprintf(el->el_errfile, "limit != &buffer[EL_BUFSIZ-2]\r\n");
}
#endif /* DEBUG_EDIT */

/* read__fixio():
 *	Try to recover from a read error
 */
private int
read__fixio(fd, e)
    int fd, e;
{
    switch (e) {
    case -1:	/* Make sure that the code is reachable */

#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
# ifndef TRY_AGAIN
#  define TRY_AGAIN
# endif
#endif /* EWOULDBLOCK */

#if defined(POSIX) && defined(EAGAIN)
# if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
    case EAGAIN:
#  ifndef TRY_AGAIN
#   define TRY_AGAIN
#  endif
# endif /* EWOULDBLOCK && EWOULDBLOCK != EAGAIN */
#endif /* POSIX && EAGAIN */

	e = 0;
#ifdef TRY_AGAIN
# if defined(F_SETFL) && defined(O_NDELAY)
	if ((e = fcntl(fd, F_GETFL, 0)) == -1)
	    return -1;

	if (fcntl(fd, F_SETFL, e & ~O_NDELAY) == -1)
	    return -1;
	else
	    e = 1;
# endif /* F_SETFL && O_NDELAY */

# ifdef FIONBIO
	if (ioctl(fd, FIONBIO, (ioctl_t) &e) == -1)
	    return -1;
	else
	    e = 1;
# endif	/* FIONBIO */

#endif /* TRY_AGAIN */
	return e ? 0 : -1;

    case EINTR:
	return 0;

    default:
	return -1;
    }
}


/* read_preread():
 *	Try to read the stuff in the input queue;
 */
private int
read_preread(el)
    EditLine *el;
{
    int    chrs = 0;

    if (el->el_chared.c_macro.nline) {
	el_free((ptr_t) el->el_chared.c_macro.nline);
	el->el_chared.c_macro.nline = NULL;
    }

    if (el->el_tty.t_mode == ED_IO)
	return 0;

#ifdef FIONREAD
    (void) ioctl(el->el_infd, FIONREAD, (ioctl_t) &chrs);
    if (chrs > 0) {
	char    buf[EL_BUFSIZ];

	chrs = read(el->el_infd, buf, (size_t) MIN(chrs, EL_BUFSIZ - 1));
	if (chrs > 0) {
	    buf[chrs] = '\0';
	    el->el_chared.c_macro.nline = strdup(buf);
	    el_push(el->el_chared.c_macro.nline);
	}
    }
#endif  /* FIONREAD */

    return chrs > 0;
}


/* el_push():
 *	Push a macro
 */
public void
el_push(el, str)
    EditLine *el;
    const char   *str;
{
    c_macro_t *ma = &el->el_chared.c_macro;

    if (str != NULL && ma->level + 1 < EL_MAXMACRO) {
	ma->level++;
	ma->macro[ma->level] = (char *) str;
    }
    else {
	term_beep(el);
	term__flush();
    }
}


/* read_getcmd():
 *	Return next command from the input stream.
 */
private int
read_getcmd(el, cmdnum, ch)
    EditLine *el;
    el_action_t *cmdnum;
    char *ch;
{
    el_action_t  cmd = ED_UNASSIGNED;
    int     num;

    while (cmd == ED_UNASSIGNED || cmd == ED_SEQUENCE_LEAD_IN) {
	if ((num = el_getc(el, ch)) != 1)	/* if EOF or error */
	    return num;

#ifdef	KANJI
	if ((*ch & 0200)) {
	    el->el_state.metanext = 0;
	    cmd = CcViMap[' '];
	    break;
	}
	else
#endif /* KANJI */

	if (el->el_state.metanext) {
	    el->el_state.metanext = 0;
	    *ch |= 0200;
	}
	cmd = el->el_map.current[(unsigned char) *ch];
	if (cmd == ED_SEQUENCE_LEAD_IN) {
	    key_value_t val;
	    switch (key_get(el, ch, &val)) {
	    case XK_CMD:
		cmd = val.cmd;
		break;
	    case XK_STR:
		el_push(el, val.str);
		break;
#ifdef notyet
	    case XK_EXE:
		/* XXX: In the future to run a user function */
		RunCommand(val.str);
		break;
#endif
	    default:
		abort();
		break;
	    }
	}
	if (el->el_map.alt == NULL)
	    el->el_map.current = el->el_map.key;
    }
    *cmdnum = cmd;
    return OKCMD;
}


/* el_getc():
 *	Read a character
 */
public int
el_getc(el, cp)
    EditLine *el;
    char *cp;
{
    int num_read;
    unsigned char tcp;
    int tried = 0;

    c_macro_t *ma = &el->el_chared.c_macro;

    term__flush();
    for (;;) {
	if (ma->level < 0) {
	    if (!read_preread(el))
		break;
	}
	if (ma->level < 0)
	    break;

	if (*ma->macro[ma->level] == 0) {
	    ma->level--;
	    continue;
	}
	*cp = *ma->macro[ma->level]++ & 0377;
	if (*ma->macro[ma->level] == 0) {	/* Needed for QuoteMode On */
	    ma->level--;
	}
	return 1;
    }

#ifdef DEBUG_READ
    (void) fprintf(el->el_errfile, "Turning raw mode on\n");
#endif /* DEBUG_READ */
    if (tty_rawmode(el) < 0)	/* make sure the tty is set up correctly */
	return 0;

#ifdef DEBUG_READ
    (void) fprintf(el->el_errfile, "Reading a character\n");
#endif /* DEBUG_READ */
    while ((num_read = read(el->el_infd, (char *) &tcp, 1)) == -1)
	if (!tried && read__fixio(el->el_infd, errno) == 0)
	    tried = 1;
	else {
	    *cp = '\0';
	    return -1;
	}
#ifdef DEBUG_READ
    (void) fprintf(el->el_errfile, "Got it %c\n", tcp);
#endif /* DEBUG_READ */
    *cp = tcp;
    return num_read;
}



public const char *
el_gets(el, nread)
    EditLine *el;
    int *nread;
{
    int retval;
    el_action_t  cmdnum = 0;
    int     num;		/* how many chars we have read at NL */
    char    ch;

    if (el->el_flags & HANDLE_SIGNALS)
	sig_set(el);

    re_clear_display(el);		/* reset the display stuff */
    ch_reset(el);

#ifdef FIONREAD
    if (el->el_tty.t_mode == EX_IO && ma->level < 0) {
	long    chrs = 0;

	(void) ioctl(el->el_infd, FIONREAD, (ioctl_t) &chrs);
	if (chrs == 0) {
	    if (tty_rawmode(el) < 0) {
		if (nread)
			*nread = 0;
		return NULL;
	    }
	}
    }
#endif /* FIONREAD */

    re_refresh(el);			/* print the prompt */

    for (num = OKCMD; num == OKCMD;) {	/* while still editing this line */
#ifdef DEBUG_EDIT
	read_debug(el);
#endif /* DEBUG_EDIT */
	/* if EOF or error */
	if ((num = read_getcmd(el, &cmdnum, &ch)) != OKCMD) {
#ifdef DEBUG_READ
	    (void) fprintf(el->el_errfile, "Returning from el_gets %d\n", num);
#endif /* DEBUG_READ */
	    break;
	}

	if (cmdnum >= el->el_map.nfunc) {	/* BUG CHECK command */
#ifdef DEBUG_EDIT
	    (void) fprintf(el->el_errfile,
			   "ERROR: illegal command from key 0%o\r\n", ch);
#endif /* DEBUG_EDIT */
	    continue;		/* try again */
	}

	/* now do the real command */
#ifdef DEBUG_READ
	{
	    el_bindings_t *b;
	    for (b = el->el_map.help; b->name; b++)
		if (b->func == cmdnum)
		    break;
	    if (b->name)
		(void) fprintf(el->el_errfile, "Executing %s\n", b->name);
	    else
		(void) fprintf(el->el_errfile, "Error command = %d\n", cmdnum);
	}
#endif /* DEBUG_READ */
	retval = (*el->el_map.func[cmdnum])(el, ch);

	/* save the last command here */
	el->el_state.lastcmd = cmdnum;

	/* use any return value */
	switch (retval) {
	case CC_CURSOR:
	    el->el_state.argument = 1;
	    el->el_state.doingarg = 0;
	    re_refresh_cursor(el);
	    break;

	case CC_REDISPLAY:
	    re_clear_lines(el);
	    re_clear_display(el);
	    /* FALLTHROUGH */

	case CC_REFRESH:
	    el->el_state.argument = 1;
	    el->el_state.doingarg = 0;
	    re_refresh(el);
	    break;

	case CC_NORM:		/* normal char */
	    el->el_state.argument = 1;
	    el->el_state.doingarg = 0;
	    break;

	case CC_ARGHACK:	/* Suggested by Rich Salz */
	    /* <rsalz@pineapple.bbn.com> */
	    break;		/* keep going... */

	case CC_EOF:		/* end of file typed */
	    num = 0;
	    break;

	case CC_NEWLINE:	/* normal end of line */
	    num = el->el_line.lastchar - el->el_line.buffer;
	    break;

	case CC_FATAL:		/* fatal error, reset to known state */
#ifdef DEBUG_READ
	    (void) fprintf(el->el_errfile, "*** editor fatal ERROR ***\r\n\n");
#endif /* DEBUG_READ */
	    /* put (real) cursor in a known place */
	    re_clear_display(el);	/* reset the display stuff */
	    ch_reset(el);		/* reset the input pointers */
	    re_refresh(el);		/* print the prompt again */
	    el->el_state.argument = 1;
	    el->el_state.doingarg = 0;
	    break;

	case CC_ERROR:
	default:		/* functions we don't know about */
#ifdef DEBUG_READ
	    (void) fprintf(el->el_errfile, "*** editor ERROR ***\r\n\n");
#endif /* DEBUG_READ */
	    el->el_state.argument = 1;
	    el->el_state.doingarg = 0;
	    term_beep(el);
	    term__flush();
	    break;
	}
    }

    term__flush();		/* flush any buffered output */
    (void) tty_cookedmode(el);	/* make sure the tty is set up correctly */
    if (el->el_flags & HANDLE_SIGNALS)
	sig_clr(el);
    if (nread)
	    *nread = num;
    return num ? el->el_line.buffer : NULL;
}
