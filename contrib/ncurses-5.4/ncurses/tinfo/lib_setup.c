/****************************************************************************
 * Copyright (c) 1998-2002,2003 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 *     and: Thomas E. Dickey 1996-2003                                      *
 ****************************************************************************/

/*
 * Terminal setup routines common to termcap and terminfo:
 *
 *		use_env(bool)
 *		setupterm(char *, int, int *)
 */

#include <curses.priv.h>
#include <tic.h>		/* for MAX_NAME_SIZE */
#include <term_entry.h>

#if SVR4_TERMIO && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>		/* lines, columns, cur_term */

MODULE_ID("$Id: lib_setup.c,v 1.79 2003/12/27 18:24:26 tom Exp $")

/****************************************************************************
 *
 * Terminal size computation
 *
 ****************************************************************************/

#if HAVE_SIZECHANGE
# if !defined(sun) || !TERMIOS
#  if HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#  endif
# endif
#endif

#if NEED_PTEM_H
 /* On SCO, they neglected to define struct winsize in termios.h -- it's only
  * in termio.h and ptem.h (the former conflicts with other definitions).
  */
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

/*
 * SCO defines TIOCGSIZE and the corresponding struct.  Other systems (SunOS,
 * Solaris, IRIX) define TIOCGWINSZ and struct winsize.
 */
#ifdef TIOCGSIZE
# define IOCTL_WINSIZE TIOCGSIZE
# define STRUCT_WINSIZE struct ttysize
# define WINSIZE_ROWS(n) (int)n.ts_lines
# define WINSIZE_COLS(n) (int)n.ts_cols
#else
# ifdef TIOCGWINSZ
#  define IOCTL_WINSIZE TIOCGWINSZ
#  define STRUCT_WINSIZE struct winsize
#  define WINSIZE_ROWS(n) (int)n.ws_row
#  define WINSIZE_COLS(n) (int)n.ws_col
# endif
#endif

NCURSES_EXPORT_VAR(char) ttytype[NAMESIZE] = "";
NCURSES_EXPORT_VAR(int) LINES = 0;
NCURSES_EXPORT_VAR(int) COLS = 0;
NCURSES_EXPORT_VAR(int) TABSIZE = 0;

static int _use_env = TRUE;

NCURSES_EXPORT(void)
use_env(bool f)
{
    T((T_CALLED("use_env()")));
    _use_env = f;
    returnVoid;
}

static void
_nc_get_screensize(int *linep, int *colp)
/* Obtain lines/columns values from the environment and/or terminfo entry */
{
    /* figure out the size of the screen */
    T(("screen size: terminfo lines = %d columns = %d", lines, columns));

    if (!_use_env) {
	*linep = (int) lines;
	*colp = (int) columns;
    } else {			/* usually want to query LINES and COLUMNS from environment */
	int value;

	*linep = *colp = 0;

	/* first, look for environment variables */
	if ((value = _nc_getenv_num("LINES")) > 0) {
	    *linep = value;
	}
	if ((value = _nc_getenv_num("COLUMNS")) > 0) {
	    *colp = value;
	}
	T(("screen size: environment LINES = %d COLUMNS = %d", *linep, *colp));

#ifdef __EMX__
	if (*linep <= 0 || *colp <= 0) {
	    int screendata[2];
	    _scrsize(screendata);
	    *colp = screendata[0];
	    *linep = screendata[1];
	    T(("EMX screen size: environment LINES = %d COLUMNS = %d",
	       *linep, *colp));
	}
#endif
#if HAVE_SIZECHANGE
	/* if that didn't work, maybe we can try asking the OS */
	if (*linep <= 0 || *colp <= 0) {
	    if (isatty(cur_term->Filedes)) {
		STRUCT_WINSIZE size;

		errno = 0;
		do {
		    if (ioctl(cur_term->Filedes, IOCTL_WINSIZE, &size) < 0
			&& errno != EINTR)
			goto failure;
		} while
		    (errno == EINTR);

		/*
		 * Solaris lets users override either dimension with an
		 * environment variable.
		 */
		if (*linep <= 0)
		    *linep = WINSIZE_ROWS(size);
		if (*colp <= 0)
		    *colp = WINSIZE_COLS(size);
	    }
	    /* FALLTHRU */
	  failure:;
	}
#endif /* HAVE_SIZECHANGE */

	/* if we can't get dynamic info about the size, use static */
	if (*linep <= 0) {
	    *linep = (int) lines;
	}
	if (*colp <= 0) {
	    *colp = (int) columns;
	}

	/* the ultimate fallback, assume fixed 24x80 size */
	if (*linep <= 0) {
	    *linep = 24;
	}
	if (*colp <= 0) {
	    *colp = 80;
	}

	/*
	 * Put the derived values back in the screen-size caps, so
	 * tigetnum() and tgetnum() will do the right thing.
	 */
	lines = (short) (*linep);
	columns = (short) (*colp);
    }

    T(("screen size is %dx%d", *linep, *colp));

    if (VALID_NUMERIC(init_tabs))
	TABSIZE = (int) init_tabs;
    else
	TABSIZE = 8;
    T(("TABSIZE = %d", TABSIZE));
}

#if USE_SIZECHANGE
NCURSES_EXPORT(void)
_nc_update_screensize(void)
{
    int my_lines, my_cols;

    _nc_get_screensize(&my_lines, &my_cols);
    if (SP != 0 && SP->_resize != 0)
	SP->_resize(my_lines, my_cols);
}
#endif

/****************************************************************************
 *
 * Terminal setup
 *
 ****************************************************************************/

#define ret_error(code, fmt, arg)	if (errret) {\
					    *errret = code;\
					    returnCode(ERR);\
					} else {\
					    fprintf(stderr, fmt, arg);\
					    exit(EXIT_FAILURE);\
					}

#define ret_error0(code, msg)		if (errret) {\
					    *errret = code;\
					    returnCode(ERR);\
					} else {\
					    fprintf(stderr, msg);\
					    exit(EXIT_FAILURE);\
					}

#if USE_DATABASE || USE_TERMCAP
static int
grab_entry(const char *const tn, TERMTYPE * const tp)
/* return 1 if entry found, 0 if not found, -1 if database not accessible */
{
#if USE_DATABASE
    char filename[PATH_MAX];
#endif
    int status;

    /*
     * $TERM shouldn't contain pathname delimiters.
     */
    if (strchr(tn, '/'))
	return 0;

#if USE_DATABASE
    if ((status = _nc_read_entry(tn, filename, tp)) != 1) {

#if !PURE_TERMINFO
	/*
	 * Try falling back on the termcap file.
	 * Note:  allowing this call links the entire terminfo/termcap
	 * compiler into the startup code.  It's preferable to build a
	 * real terminfo database and use that.
	 */
	status = _nc_read_termcap_entry(tn, tp);
#endif /* PURE_TERMINFO */

    }
#else
    status = _nc_read_termcap_entry(tn, tp);
#endif

    /*
     * If we have an entry, force all of the cancelled strings to null
     * pointers so we don't have to test them in the rest of the library.
     * (The terminfo compiler bypasses this logic, since it must know if
     * a string is cancelled, for merging entries).
     */
    if (status == 1) {
	unsigned n;
	for_each_boolean(n, tp) {
	    if (!VALID_BOOLEAN(tp->Booleans[n]))
		tp->Booleans[n] = FALSE;
	}
	for_each_string(n, tp) {
	    if (tp->Strings[n] == CANCELLED_STRING)
		tp->Strings[n] = ABSENT_STRING;
	}
    }
    return (status);
}
#endif

/*
**	do_prototype()
**
**	Take the real command character out of the CC environment variable
**	and substitute it in for the prototype given in 'command_character'.
**
*/
static void
do_prototype(void)
{
    int i;
    char CC;
    char proto;
    char *tmp;

    tmp = getenv("CC");
    CC = *tmp;
    proto = *command_character;

    for_each_string(i, &(cur_term->type)) {
	for (tmp = cur_term->type.Strings[i]; *tmp; tmp++) {
	    if (*tmp == proto)
		*tmp = CC;
	}
    }
}

/*
 * Check if we are running in a UTF-8 locale.
 */
NCURSES_EXPORT(char *)
_nc_get_locale(void)
{
    char *env;
    if (((env = getenv("LC_ALL")) != 0 && *env != '\0')
	|| ((env = getenv("LC_CTYPE")) != 0 && *env != '\0')
	|| ((env = getenv("LANG")) != 0 && *env != '\0')) {
	return env;
    }
    return 0;
}

/*
 * Check if we are running in a UTF-8 locale.
 */
NCURSES_EXPORT(int)
_nc_unicode_locale(void)
{
    char *env = _nc_get_locale();
    if (env != 0) {
	if (strstr(env, ".UTF-8") != 0)
	    return 1;
    }
    return 0;
}

/*
 * Check for known broken cases where a UTF-8 locale breaks the alternate
 * character set.
 */
NCURSES_EXPORT(int)
_nc_locale_breaks_acs(void)
{
    char *env = getenv("TERM");
    if (env != 0) {
	if (strstr(env, "linux"))
	    return 1;		/* always broken */
	if (strstr(env, "screen") != 0
	    && ((env = getenv("TERMCAP")) != 0
		&& strstr(env, "screen") != 0)
	    && strstr(env, "hhII00") != 0) {
	    return 1;
	}
    }
    return 0;
}

/*
 *	setupterm(termname, Filedes, errret)
 *
 *	Find and read the appropriate object file for the terminal
 *	Make cur_term point to the structure.
 *
 */

NCURSES_EXPORT(int)
setupterm(NCURSES_CONST char *tname, int Filedes, int *errret)
{
    int status;

    START_TRACE();
    T((T_CALLED("setupterm(%s,%d,%p)"), _nc_visbuf(tname), Filedes, errret));

    if (tname == 0) {
	tname = getenv("TERM");
	if (tname == 0 || *tname == '\0') {
	    ret_error0(-1, "TERM environment variable not set.\n");
	}
    }
    if (strlen(tname) > MAX_NAME_SIZE) {
	ret_error(-1, "TERM environment must be <= %d characters.\n",
		  MAX_NAME_SIZE);
    }

    T(("your terminal name is %s", tname));

    /*
     * Allow output redirection.  This is what SVr3 does.  If stdout is
     * directed to a file, screen updates go to standard error.
     */
    if (Filedes == STDOUT_FILENO && !isatty(Filedes))
	Filedes = STDERR_FILENO;

    /*
     * Check if we have already initialized to use this terminal.  If so, we
     * do not need to re-read the terminfo entry, or obtain TTY settings.
     *
     * This is an improvement on SVr4 curses.  If an application mixes curses
     * and termcap calls, it may call both initscr and tgetent.  This is not
     * really a good thing to do, but can happen if someone tries using ncurses
     * with the readline library.  The problem we are fixing is that when
     * tgetent calls setupterm, the resulting Ottyb struct in cur_term is
     * zeroed.  A subsequent call to endwin uses the zeroed terminal settings
     * rather than the ones saved in initscr.  So we check if cur_term appears
     * to contain terminal settings for the same output file as our current
     * call - and copy those terminal settings.  (SVr4 curses does not do this,
     * however applications that are working around the problem will still work
     * properly with this feature).
     */
    if (cur_term != 0
	&& cur_term->Filedes == Filedes
	&& cur_term->_termname != 0
	&& !strcmp(cur_term->_termname, tname)
	&& _nc_name_match(cur_term->type.term_names, tname, "|")) {
	T(("reusing existing terminal information and mode-settings"));
    } else {
	TERMINAL *term_ptr;

	term_ptr = typeCalloc(TERMINAL, 1);

	if (term_ptr == 0) {
	    ret_error0(-1,
		       "Not enough memory to create terminal structure.\n");
	}
#if USE_DATABASE || USE_TERMCAP
	status = grab_entry(tname, &term_ptr->type);
#else
	status = 0;
#endif

	/* try fallback list if entry on disk */
	if (status != 1) {
	    const TERMTYPE *fallback = _nc_fallback(tname);

	    if (fallback) {
		term_ptr->type = *fallback;
		status = 1;
	    }
	}

	if (status <= 0) {
	    del_curterm(term_ptr);
	    if (status == -1) {
		ret_error0(-1, "terminals database is inaccessible\n");
	    } else if (status == 0) {
		ret_error(0, "'%s': unknown terminal type.\n", tname);
	    }
	}

	set_curterm(term_ptr);

	if (command_character && getenv("CC"))
	    do_prototype();

	strncpy(ttytype, cur_term->type.term_names, NAMESIZE - 1);
	ttytype[NAMESIZE - 1] = '\0';

	cur_term->Filedes = Filedes;
	cur_term->_termname = strdup(tname);

	/*
	 * If an application calls setupterm() rather than initscr() or
	 * newterm(), we will not have the def_prog_mode() call in
	 * _nc_setupscreen().  Do it now anyway, so we can initialize the
	 * baudrate.
	 */
	if (isatty(Filedes)) {
	    def_prog_mode();
	    baudrate();
	}
    }

    /*
     * We should always check the screensize, just in case.
     */
    _nc_get_screensize(&LINES, &COLS);

    if (errret)
	*errret = 1;

    T((T_CREATE("screen %s %dx%d"), tname, LINES, COLS));

    if (generic_type) {
	ret_error(0, "'%s': I need something more specific.\n", tname);
    }
    if (hard_copy) {
	ret_error(1, "'%s': I can't handle hardcopy terminals.\n", tname);
    }
    returnCode(OK);
}
