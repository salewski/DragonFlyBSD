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
 ****************************************************************************/

/*
 *	comp_scan.c --- Lexical scanner for terminfo compiler.
 *
 *	_nc_reset_input()
 *	_nc_get_token()
 *	_nc_panic_mode()
 *	int _nc_syntax;
 *	int _nc_curr_line;
 *	long _nc_curr_file_pos;
 *	long _nc_comment_start;
 *	long _nc_comment_end;
 */

#include <curses.priv.h>

#include <ctype.h>
#include <term_entry.h>
#include <tic.h>

MODULE_ID("$Id: comp_scan.c,v 1.66 2003/12/14 00:32:40 tom Exp $")

/*
 * Maximum length of string capability we'll accept before raising an error.
 * Yes, there is a real capability in /etc/termcap this long, an "is".
 */
#define MAXCAPLEN	600

#define iswhite(ch)	(ch == ' '  ||  ch == '\t')

NCURSES_EXPORT_VAR(int)
_nc_syntax = 0;			/* termcap or terminfo? */
NCURSES_EXPORT_VAR(long)
_nc_curr_file_pos = 0;		/* file offset of current line */
NCURSES_EXPORT_VAR(long)
_nc_comment_start = 0;		/* start of comment range before name */
NCURSES_EXPORT_VAR(long)
_nc_comment_end = 0;		/* end of comment range before name */
NCURSES_EXPORT_VAR(long)
_nc_start_line = 0;		/* start line of current entry */

NCURSES_EXPORT_VAR(struct token)
_nc_curr_token =
{
    0, 0, 0
};

/*****************************************************************************
 *
 * Token-grabbing machinery
 *
 *****************************************************************************/

static bool first_column;	/* See 'next_char()' below */
static bool had_newline;
static char separator;		/* capability separator */
static int pushtype;		/* type of pushback token */
static char *pushname;

#if NCURSES_EXT_FUNCS
NCURSES_EXPORT_VAR(bool)
_nc_disable_period = FALSE;	/* used by tic -a option */
#endif

static bool end_of_stream(void);
static int last_char(void);
static int next_char(void);
static long stream_pos(void);
static void push_back(char c);

/* Assume we may be looking at a termcap-style continuation */
static inline int
eat_escaped_newline(int ch)
{
    if (ch == '\\')
	while ((ch = next_char()) == '\n' || iswhite(ch))
	    continue;
    return ch;
}

/*
 *	int
 *	get_token()
 *
 *	Scans the input for the next token, storing the specifics in the
 *	global structure 'curr_token' and returning one of the following:
 *
 *		NAMES		A line beginning in column 1.  'name'
 *				will be set to point to everything up to but
 *				not including the first separator on the line.
 *		BOOLEAN		An entry consisting of a name followed by
 *				a separator.  'name' will be set to point to
 *				the name of the capability.
 *		NUMBER		An entry of the form
 *					name#digits,
 *				'name' will be set to point to the capability
 *				name and 'valnumber' to the number given.
 *		STRING		An entry of the form
 *					name=characters,
 *				'name' is set to the capability name and
 *				'valstring' to the string of characters, with
 *				input translations done.
 *		CANCEL		An entry of the form
 *					name@,
 *				'name' is set to the capability name and
 *				'valnumber' to -1.
 *		EOF		The end of the file has been reached.
 *
 *	A `separator' is either a comma or a semicolon, depending on whether
 *	we are in termcap or terminfo mode.
 *
 */

NCURSES_EXPORT(int)
_nc_get_token(bool silent)
{
    static const char terminfo_punct[] = "@%&*!#";
    static char *buffer;

    char *after_list;
    char *after_name;
    char *numchk;
    char *ptr;
    char *s;
    char numbuf[80];
    int ch;
    int dot_flag = FALSE;
    int type;
    long number;
    long token_start;
    unsigned found;

    if (pushtype != NO_PUSHBACK) {
	int retval = pushtype;

	_nc_set_type(pushname != 0 ? pushname : "");
	DEBUG(3, ("pushed-back token: `%s', class %d",
		  _nc_curr_token.tk_name, pushtype));

	pushtype = NO_PUSHBACK;
	if (pushname != 0)
	    pushname[0] = '\0';

	/* currtok wasn't altered by _nc_push_token() */
	return (retval);
    }

    if (end_of_stream()) {
	if (buffer != 0) {
	    FreeAndNull(buffer);
	}
	return (EOF);
    }

  start_token:
    token_start = stream_pos();
    while ((ch = next_char()) == '\n' || iswhite(ch)) {
	if (ch == '\n')
	    had_newline = TRUE;
	continue;
    }

    ch = eat_escaped_newline(ch);

    if (ch == EOF)
	type = EOF;
    else {
	/* if this is a termcap entry, skip a leading separator */
	if (separator == ':' && ch == ':')
	    ch = next_char();

	if (ch == '.'
#if NCURSES_EXT_FUNCS
	    && !_nc_disable_period
#endif
	    ) {
	    dot_flag = TRUE;
	    DEBUG(8, ("dot-flag set"));

	    while ((ch = next_char()) == '.' || iswhite(ch))
		continue;
	}

	if (ch == EOF) {
	    type = EOF;
	    goto end_of_token;
	}

	/* have to make some punctuation chars legal for terminfo */
	if (!isalnum(ch)
#if NCURSES_EXT_FUNCS
	    && !(ch == '.' && _nc_disable_period)
#endif
	    && !strchr(terminfo_punct, (char) ch)) {
	    if (!silent)
		_nc_warning("Illegal character (expected alphanumeric or %s) - %s",
			    terminfo_punct, unctrl((chtype) ch));
	    _nc_panic_mode(separator);
	    goto start_token;
	}

	if (buffer == 0)
	    buffer = typeMalloc(char, MAX_ENTRY_SIZE);

	ptr = buffer;
	*(ptr++) = ch;

	if (first_column) {
	    _nc_comment_start = token_start;
	    _nc_comment_end = _nc_curr_file_pos;
	    _nc_start_line = _nc_curr_line;

	    _nc_syntax = ERR;
	    after_name = 0;
	    after_list = 0;
	    while ((ch = next_char()) != '\n') {
		if (ch == EOF) {
		    _nc_err_abort(MSG_NO_MEMORY);
		} else if (ch == '|') {
		    after_list = ptr;
		    if (after_name == 0)
			after_name = ptr;
		} else if (ch == ':' && last_char() != ',') {
		    _nc_syntax = SYN_TERMCAP;
		    separator = ':';
		    break;
		} else if (ch == ',') {
		    _nc_syntax = SYN_TERMINFO;
		    separator = ',';
		    /*
		     * If we did not see a '|', then we found a name with no
		     * aliases or description.
		     */
		    if (after_name == 0)
			break;
		    /*
		     * If we see a comma, we assume this is terminfo unless we
		     * subsequently run into a colon.  But we don't stop
		     * looking for a colon until hitting a newline.  This
		     * allows commas to be embedded in description fields of
		     * either syntax.
		     */
		} else
		    ch = eat_escaped_newline(ch);

		*ptr++ = ch;
	    }
	    ptr[0] = '\0';
	    if (_nc_syntax == ERR) {
		/*
		 * Grrr...what we ought to do here is barf, complaining that
		 * the entry is malformed.  But because a couple of name fields
		 * in the 8.2 termcap file end with |\, we just have to assume
		 * it's termcap syntax.
		 */
		_nc_syntax = SYN_TERMCAP;
		separator = ':';
	    } else if (_nc_syntax == SYN_TERMINFO) {
		/* throw away trailing /, *$/ */
		for (--ptr; iswhite(*ptr) || *ptr == ','; ptr--)
		    continue;
		ptr[1] = '\0';
	    }

	    /*
	     * This is the soonest we have the terminal name fetched.  Set up
	     * for following warning messages.  If there's no '|', then there
	     * is no description.
	     */
	    if (after_name != 0) {
		ch = *after_name;
		*after_name = '\0';
		_nc_set_type(buffer);
		*after_name = ch;
	    }

	    /*
	     * Compute the boundary between the aliases and the description
	     * field for syntax-checking purposes.
	     */
	    if (after_list != 0) {
		if (!silent) {
		    if (*after_list == '\0')
			_nc_warning("empty longname field");
		    else if (strchr(after_list, ' ') == 0)
			_nc_warning("older tic versions may treat the description field as an alias");
		}
	    } else {
		after_list = buffer + strlen(buffer);
		DEBUG(1, ("missing description"));
	    }

	    /*
	     * Whitespace in a name field other than the long name can confuse
	     * rdist and some termcap tools.  Slashes are a no-no.  Other
	     * special characters can be dangerous due to shell expansion.
	     */
	    for (s = buffer; s < after_list; ++s) {
		if (isspace(UChar(*s))) {
		    if (!silent)
			_nc_warning("whitespace in name or alias field");
		    break;
		} else if (*s == '/') {
		    if (!silent)
			_nc_warning("slashes aren't allowed in names or aliases");
		    break;
		} else if (strchr("$[]!*?", *s)) {
		    if (!silent)
			_nc_warning("dubious character `%c' in name or alias field", *s);
		    break;
		}
	    }

	    _nc_curr_token.tk_name = buffer;
	    type = NAMES;
	} else {
	    if (had_newline && _nc_syntax == SYN_TERMCAP) {
		_nc_warning("Missing backslash before newline");
		had_newline = FALSE;
	    }
	    while ((ch = next_char()) != EOF) {
		if (!isalnum(ch)) {
		    if (_nc_syntax == SYN_TERMINFO) {
			if (ch != '_')
			    break;
		    } else {	/* allow ';' for "k;" */
			if (ch != ';')
			    break;
		    }
		}
		*(ptr++) = ch;
	    }

	    *ptr++ = '\0';
	    switch (ch) {
	    case ',':
	    case ':':
		if (ch != separator)
		    _nc_err_abort("Separator inconsistent with syntax");
		_nc_curr_token.tk_name = buffer;
		type = BOOLEAN;
		break;
	    case '@':
		if ((ch = next_char()) != separator && !silent)
		    _nc_warning("Missing separator after `%s', have %s",
				buffer, unctrl((chtype) ch));
		_nc_curr_token.tk_name = buffer;
		type = CANCEL;
		break;

	    case '#':
		found = 0;
		while (isalnum(ch = next_char())) {
		    numbuf[found++] = ch;
		    if (found >= sizeof(numbuf) - 1)
			break;
		}
		numbuf[found] = '\0';
		number = strtol(numbuf, &numchk, 0);
		if (!silent) {
		    if (numchk == numbuf)
			_nc_warning("no value given for `%s'", buffer);
		    if ((*numchk != '\0') || (ch != separator))
			_nc_warning("Missing separator");
		}
		_nc_curr_token.tk_name = buffer;
		_nc_curr_token.tk_valnumber = number;
		type = NUMBER;
		break;

	    case '=':
		ch = _nc_trans_string(ptr, buffer + MAX_ENTRY_SIZE);
		if (!silent && ch != separator)
		    _nc_warning("Missing separator");
		_nc_curr_token.tk_name = buffer;
		_nc_curr_token.tk_valstring = ptr;
		type = STRING;
		break;

	    case EOF:
		type = EOF;
		break;
	    default:
		/* just to get rid of the compiler warning */
		type = UNDEF;
		if (!silent)
		    _nc_warning("Illegal character - %s", unctrl((chtype) ch));
	    }
	}			/* end else (first_column == FALSE) */
    }				/* end else (ch != EOF) */

  end_of_token:

#ifdef TRACE
    if (dot_flag == TRUE)
	DEBUG(8, ("Commented out "));

    if (_nc_tracing >= DEBUG_LEVEL(7)) {
	switch (type) {
	case BOOLEAN:
	    _tracef("Token: Boolean; name='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case NUMBER:
	    _tracef("Token: Number;  name='%s', value=%d",
		    _nc_curr_token.tk_name,
		    _nc_curr_token.tk_valnumber);
	    break;

	case STRING:
	    _tracef("Token: String;  name='%s', value=%s",
		    _nc_curr_token.tk_name,
		    _nc_visbuf(_nc_curr_token.tk_valstring));
	    break;

	case CANCEL:
	    _tracef("Token: Cancel; name='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case NAMES:

	    _tracef("Token: Names; value='%s'",
		    _nc_curr_token.tk_name);
	    break;

	case EOF:
	    _tracef("Token: End of file");
	    break;

	default:
	    _nc_warning("Bad token type");
	}
    }
#endif

    if (dot_flag == TRUE)	/* if commented out, use the next one */
	type = _nc_get_token(silent);

    DEBUG(3, ("token: `%s', class %d",
	      _nc_curr_token.tk_name != 0 ? _nc_curr_token.tk_name :
	      "<null>",
	      type));

    return (type);
}

/*
 *	char
 *	trans_string(ptr)
 *
 *	Reads characters using next_char() until encountering a separator, nl,
 *	or end-of-file.  The returned value is the character which caused
 *	reading to stop.  The following translations are done on the input:
 *
 *		^X  goes to  ctrl-X (i.e. X & 037)
 *		{\E,\n,\r,\b,\t,\f}  go to
 *			{ESCAPE,newline,carriage-return,backspace,tab,formfeed}
 *		{\^,\\}  go to  {carat,backslash}
 *		\ddd (for ddd = up to three octal digits)  goes to the character ddd
 *
 *		\e == \E
 *		\0 == \200
 *
 */

NCURSES_EXPORT(char)
_nc_trans_string(char *ptr, char *last)
{
    int count = 0;
    int number = 0;
    int i, c;
    chtype ch, last_ch = '\0';
    bool ignored = FALSE;
    bool long_warning = FALSE;

    while ((ch = c = next_char()) != (chtype) separator && c != EOF) {
	if (ptr == (last - 1))
	    break;
	if ((_nc_syntax == SYN_TERMCAP) && c == '\n')
	    break;
	if (ch == '^' && last_ch != '%') {
	    ch = c = next_char();
	    if (c == EOF)
		_nc_err_abort(MSG_NO_INPUTS);

	    if (!(is7bits(ch) && isprint(ch))) {
		_nc_warning("Illegal ^ character - %s", unctrl(ch));
	    }
	    if (ch == '?') {
		*(ptr++) = '\177';
		if (_nc_tracing)
		    _nc_warning("Allow ^? as synonym for \\177");
	    } else {
		if ((ch &= 037) == 0)
		    ch = 128;
		*(ptr++) = (char) (ch);
	    }
	} else if (ch == '\\') {
	    ch = c = next_char();
	    if (c == EOF)
		_nc_err_abort(MSG_NO_INPUTS);

	    if (ch >= '0' && ch <= '7') {
		number = ch - '0';
		for (i = 0; i < 2; i++) {
		    ch = c = next_char();
		    if (c == EOF)
			_nc_err_abort(MSG_NO_INPUTS);

		    if (c < '0' || c > '7') {
			if (isdigit(c)) {
			    _nc_warning("Non-octal digit `%c' in \\ sequence", c);
			    /* allow the digit; it'll do less harm */
			} else {
			    push_back((char) c);
			    break;
			}
		    }

		    number = number * 8 + c - '0';
		}

		if (number == 0)
		    number = 0200;
		*(ptr++) = (char) number;
	    } else {
		switch (c) {
		case 'E':
		case 'e':
		    *(ptr++) = '\033';
		    break;

		case 'a':
		    *(ptr++) = '\007';
		    break;

		case 'l':
		case 'n':
		    *(ptr++) = '\n';
		    break;

		case 'r':
		    *(ptr++) = '\r';
		    break;

		case 'b':
		    *(ptr++) = '\010';
		    break;

		case 's':
		    *(ptr++) = ' ';
		    break;

		case 'f':
		    *(ptr++) = '\014';
		    break;

		case 't':
		    *(ptr++) = '\t';
		    break;

		case '\\':
		    *(ptr++) = '\\';
		    break;

		case '^':
		    *(ptr++) = '^';
		    break;

		case ',':
		    *(ptr++) = ',';
		    break;

		case ':':
		    *(ptr++) = ':';
		    break;

		case '\n':
		    continue;

		default:
		    _nc_warning("Illegal character %s in \\ sequence",
				unctrl(ch));
		    /* FALLTHRU */
		case '|':
		    *(ptr++) = (char) ch;
		}		/* endswitch (ch) */
	    }			/* endelse (ch < '0' ||  ch > '7') */
	}
	/* end else if (ch == '\\') */
	else if (ch == '\n' && (_nc_syntax == SYN_TERMINFO)) {
	    /* newlines embedded in a terminfo string are ignored */
	    ignored = TRUE;
	} else {
	    *(ptr++) = (char) ch;
	}

	if (!ignored) {
	    last_ch = ch;
	    count++;
	}
	ignored = FALSE;

	if (count > MAXCAPLEN && !long_warning) {
	    _nc_warning("Very long string found.  Missing separator?");
	    long_warning = TRUE;
	}
    }				/* end while */

    *ptr = '\0';

    return (ch);
}

/*
 *	_nc_push_token()
 *
 *	Push a token of given type so that it will be reread by the next
 *	get_token() call.
 */

NCURSES_EXPORT(void)
_nc_push_token(int tokclass)
{
    /*
     * This implementation is kind of bogus, it will fail if we ever do more
     * than one pushback at a time between get_token() calls.  It relies on the
     * fact that _nc_curr_token is static storage that nothing but
     * _nc_get_token() touches.
     */
    pushtype = tokclass;
    if (pushname == 0)
	pushname = typeMalloc(char, MAX_NAME_SIZE + 1);
    _nc_get_type(pushname);

    DEBUG(3, ("pushing token: `%s', class %d",
	      _nc_curr_token.tk_name, pushtype));
}

/*
 * Panic mode error recovery - skip everything until a "ch" is found.
 */
NCURSES_EXPORT(void)
_nc_panic_mode(char ch)
{
    int c;

    for (;;) {
	c = next_char();
	if (c == ch)
	    return;
	if (c == EOF)
	    return;
    }
}

/*****************************************************************************
 *
 * Character-stream handling
 *
 *****************************************************************************/

#define LEXBUFSIZ	1024

static char *bufptr;		/* otherwise, the input buffer pointer */
static char *bufstart;		/* start of buffer so we can compute offsets */
static FILE *yyin;		/* scanner's input file descriptor */

/*
 *	_nc_reset_input()
 *
 *	Resets the input-reading routines.  Used on initialization,
 *	or after a seek has been done.  Exactly one argument must be
 *	non-null.
 */

NCURSES_EXPORT(void)
_nc_reset_input(FILE *fp, char *buf)
{
    pushtype = NO_PUSHBACK;
    if (pushname != 0)
	pushname[0] = '\0';
    yyin = fp;
    bufstart = bufptr = buf;
    _nc_curr_file_pos = 0L;
    if (fp != 0)
	_nc_curr_line = 0;
    _nc_curr_col = 0;
}

/*
 *	int last_char()
 *
 *	Returns the final nonblank character on the current input buffer
 */
static int
last_char(void)
{
    size_t len = strlen(bufptr);
    while (len--) {
	if (!isspace(UChar(bufptr[len])))
	    return bufptr[len];
    }
    return 0;
}

/*
 *	int next_char()
 *
 *	Returns the next character in the input stream.  Comments and leading
 *	white space are stripped.
 *
 *	The global state variable 'firstcolumn' is set TRUE if the character
 *	returned is from the first column of the input line.
 *
 *	The global variable _nc_curr_line is incremented for each new line.
 *	The global variable _nc_curr_file_pos is set to the file offset of the
 *	beginning of each line.
 */

static int
next_char(void)
{
    if (!yyin) {
	/*
	 * An string with an embedded null will truncate the input.  This is
	 * intentional (we don't read binary files here).
	 */
	if (*bufptr == '\0')
	    return (EOF);
	if (*bufptr == '\n') {
	    _nc_curr_line++;
	    _nc_curr_col = 0;
	}
    } else if (!bufptr || !*bufptr) {
	/*
	 * In theory this could be recoded to do its I/O one character at a
	 * time, saving the buffer space.  In practice, this turns out to be
	 * quite hard to get completely right.  Try it and see.  If you
	 * succeed, don't forget to hack push_back() correspondingly.
	 */
	static char *result;
	static size_t allocated;
	size_t used;
	size_t len;

	do {
	    bufstart = 0;
	    used = 0;
	    do {
		if (used + (LEXBUFSIZ / 4) >= allocated) {
		    allocated += (allocated + LEXBUFSIZ);
		    result = typeRealloc(char, allocated, result);
		    if (result == 0)
			return (EOF);
		}
		if (used == 0)
		    _nc_curr_file_pos = ftell(yyin);

		if (fgets(result + used, allocated - used, yyin) != 0) {
		    bufstart = result;
		    if (used == 0) {
			_nc_curr_line++;
			_nc_curr_col = 0;
		    }
		} else {
		    if (used != 0)
			strcat(result, "\n");
		}
		if ((bufptr = bufstart) != 0) {
		    used = strlen(bufptr);
		    while (iswhite(*bufptr))
			bufptr++;

		    /*
		     * Treat a trailing <cr><lf> the same as a <newline> so we
		     * can read files on OS/2, etc.
		     */
		    if ((len = strlen(bufptr)) > 1) {
			if (bufptr[len - 1] == '\n'
			    && bufptr[len - 2] == '\r') {
			    len--;
			    bufptr[len - 1] = '\n';
			    bufptr[len] = '\0';
			}
		    }
		} else {
		    return (EOF);
		}
	    } while (bufptr[len - 1] != '\n');	/* complete a line */
	} while (result[0] == '#');	/* ignore comments */
    }

    first_column = (bufptr == bufstart);
    if (first_column)
	had_newline = FALSE;

    _nc_curr_col++;
    return (*bufptr++);
}

static void
push_back(char c)
/* push a character back onto the input stream */
{
    if (bufptr == bufstart)
	_nc_syserr_abort("Can't backspace off beginning of line");
    *--bufptr = c;
}

static long
stream_pos(void)
/* return our current character position in the input stream */
{
    return (yyin ? ftell(yyin) : (bufptr ? bufptr - bufstart : 0));
}

static bool
end_of_stream(void)
/* are we at end of input? */
{
    return ((yyin ? feof(yyin) : (bufptr && *bufptr == '\0'))
	    ? TRUE : FALSE);
}
