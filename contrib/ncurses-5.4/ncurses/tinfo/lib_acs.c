/****************************************************************************
 * Copyright (c) 1998-2001,2002 Free Software Foundation, Inc.              *
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

#include <curses.priv.h>
#include <term.h>		/* ena_acs, acs_chars */

MODULE_ID("$Id: lib_acs.c,v 1.25 2002/12/28 16:26:46 tom Exp $")

#if BROKEN_LINKER
NCURSES_EXPORT_VAR(chtype *)
_nc_acs_map(void)
{
    static chtype *the_map = 0;
    if (the_map == 0)
	the_map = typeCalloc(chtype, ACS_LEN);
    return the_map;
}
#else
NCURSES_EXPORT_VAR(chtype) acs_map[ACS_LEN] =
{
    0
};
#endif

NCURSES_EXPORT(void)
_nc_init_acs(void)
{
    chtype *fake_map = acs_map;
    chtype *real_map = SP != 0 ? SP->_acs_map : fake_map;
    int j;

    T(("initializing ACS map"));

    /*
     * If we're using this from curses (rather than terminfo), we are storing
     * the mapping information in the SCREEN struct so we can decide how to
     * render it.
     */
    if (real_map != fake_map) {
	for (j = 1; j < ACS_LEN; ++j) {
	    real_map[j] = 0;
	    fake_map[j] = A_ALTCHARSET | j;
	}
    } else {
	for (j = 1; j < ACS_LEN; ++j) {
	    real_map[j] = 0;
	}
    }

    /*
     * Initializations for a UNIX-like multi-terminal environment.  Use
     * ASCII chars and count on the terminfo description to do better.
     */
    real_map['l'] = '+';	/* should be upper left corner */
    real_map['m'] = '+';	/* should be lower left corner */
    real_map['k'] = '+';	/* should be upper right corner */
    real_map['j'] = '+';	/* should be lower right corner */
    real_map['u'] = '+';	/* should be tee pointing left */
    real_map['t'] = '+';	/* should be tee pointing right */
    real_map['v'] = '+';	/* should be tee pointing up */
    real_map['w'] = '+';	/* should be tee pointing down */
    real_map['q'] = '-';	/* should be horizontal line */
    real_map['x'] = '|';	/* should be vertical line */
    real_map['n'] = '+';	/* should be large plus or crossover */
    real_map['o'] = '~';	/* should be scan line 1 */
    real_map['s'] = '_';	/* should be scan line 9 */
    real_map['`'] = '+';	/* should be diamond */
    real_map['a'] = ':';	/* should be checker board (stipple) */
    real_map['f'] = '\'';	/* should be degree symbol */
    real_map['g'] = '#';	/* should be plus/minus */
    real_map['~'] = 'o';	/* should be bullet */
    real_map[','] = '<';	/* should be arrow pointing left */
    real_map['+'] = '>';	/* should be arrow pointing right */
    real_map['.'] = 'v';	/* should be arrow pointing down */
    real_map['-'] = '^';	/* should be arrow pointing up */
    real_map['h'] = '#';	/* should be board of squares */
    real_map['i'] = '#';	/* should be lantern symbol */
    real_map['0'] = '#';	/* should be solid square block */
    /* these defaults were invented for ncurses */
    real_map['p'] = '-';	/* should be scan line 3 */
    real_map['r'] = '-';	/* should be scan line 7 */
    real_map['y'] = '<';	/* should be less-than-or-equal-to */
    real_map['z'] = '>';	/* should be greater-than-or-equal-to */
    real_map['{'] = '*';	/* should be greek pi */
    real_map['|'] = '!';	/* should be not-equal */
    real_map['}'] = 'f';	/* should be pound-sterling symbol */

#if !USE_WIDEC_SUPPORT
    if (_nc_unicode_locale() && _nc_locale_breaks_acs()) {
	acs_chars = NULL;
	ena_acs = NULL;
    }
#endif

    if (ena_acs != NULL) {
	TPUTS_TRACE("ena_acs");
	putp(ena_acs);
    }

    if (acs_chars != NULL) {
	size_t i = 0;
	size_t length = strlen(acs_chars);

	while (i + 1 < length) {
	    if (acs_chars[i] != 0 && UChar(acs_chars[i]) < ACS_LEN) {
		real_map[UChar(acs_chars[i])] = UChar(acs_chars[i + 1]) | A_ALTCHARSET;
	    }
	    i += 2;
	}
    }
#ifdef TRACE
    /* Show the equivalent mapping, noting if it does not match the
     * given attribute, whether by re-ordering or duplication.
     */
    if (_nc_tracing & TRACE_CALLS) {
	size_t n, m;
	char show[ACS_LEN + 1];
	for (n = 1, m = 0; n < ACS_LEN; n++) {
	    if (real_map[n] != 0) {
		show[m++] = (char) n;
		show[m++] = ChCharOf(real_map[n]);
	    }
	}
	show[m] = 0;
	_tracef("%s acs_chars %s",
		(acs_chars == NULL)
		? "NULL"
		: (strcmp(acs_chars, show)
		   ? "DIFF"
		   : "SAME"),
		_nc_visbuf(show));
    }
#endif /* TRACE */
}
