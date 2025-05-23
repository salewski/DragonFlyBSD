/****************************************************************************
 * Copyright (c) 2002,2004 Free Software Foundation, Inc.                   *
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
 *  Author: Thomas E. Dickey 2002,2004                                      *
 ****************************************************************************/

/*
**	lib_get_wch.c
**
**	The routine get_wch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_get_wch.c,v 1.6 2004/01/18 01:18:17 tom Exp $")

#if HAVE_MBTOWC && HAVE_MBLEN
#define reset_mbytes(state) mblen(NULL, 0), mbtowc(NULL, NULL, 0)
#define count_mbytes(buffer,length,state) mblen(buffer,length)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbtowc(&wch, buffer, count)
#elif HAVE_MBRTOWC && HAVE_MBRLEN
#define reset_mbytes(state) memset(&state, 0, sizeof(state))
#define count_mbytes(buffer,length,state) mbrlen(buffer,length,&state)
#define check_mbytes(wch,buffer,length,state) \
	(int) mbrtowc(&wch, buffer, count, &state)
#else
make an error
#endif

NCURSES_EXPORT(int)
wget_wch(WINDOW *win, wint_t * result)
{
    int code;
    char buffer[(MB_LEN_MAX * 9) + 1];	/* allow some redundant shifts */
    int status;
    mbstate_t state;
    size_t count = 0;
    unsigned long value;
    wchar_t wch;

    T((T_CALLED("wget_wch(%p)"), win));
    (void) state;

    /*
     * We can get a stream of single-byte characters and KEY_xxx codes from
     * _nc_wgetch(), while we want to return a wide character or KEY_xxx code.
     */
    for (;;) {
	T(("reading %d of %d", count + 1, sizeof(buffer)));
	code = _nc_wgetch(win, &value, TRUE);
	if (code == ERR) {
	    break;
	} else if (code == KEY_CODE_YES) {
	    /*
	     * If we were processing an incomplete multibyte character, return
	     * an error since we have a KEY_xxx code which interrupts it.  For
	     * some cases, we could improve this by writing a new version of
	     * lib_getch.c(!), but it is not clear whether the improvement
	     * would be worth the effort.
	     */
	    if (count != 0) {
		ungetch(value);
		code = ERR;
	    }
	    break;
	} else if (count + 1 >= sizeof(buffer)) {
	    ungetch(value);
	    code = ERR;
	    break;
	} else {
	    buffer[count++] = UChar(value);
	    reset_mbytes(state);
	    status = count_mbytes(buffer, count, state);
	    if (status >= 0) {
		reset_mbytes(state);
		if (check_mbytes(wch, buffer, count, state) != status) {
		    code = ERR;	/* the two calls should match */
		}
		value = wch;
		break;
	    }
	}
    }
    *result = value;
    T(("result %#lo", value));
    returnCode(code);
}
