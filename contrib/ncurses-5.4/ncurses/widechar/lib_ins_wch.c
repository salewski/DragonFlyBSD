/****************************************************************************
 * Copyright (c) 2002,2003 Free Software Foundation, Inc.                   *
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
 *  Author: Thomas Dickey 2002                                              *
 ****************************************************************************/

/*
**	lib_ins_wch.c
**
**	The routine wins_wch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_ins_wch.c,v 1.3 2003/03/29 21:52:29 tom Exp $")

/*
 * Insert the given character, updating the current location to simplify
 * inserting a string.
 */
static void
_nc_insert_wch(WINDOW *win, const cchar_t * wch)
{
    if (win->_curx <= win->_maxx) {
	struct ldat *line = &(win->_line[win->_cury]);
	NCURSES_CH_T *end = &(line->text[win->_curx]);
	NCURSES_CH_T *temp1 = &(line->text[win->_maxx]);
	NCURSES_CH_T *temp2 = temp1 - 1;

	CHANGED_TO_EOL(line, win->_curx, win->_maxx);
	while (temp1 > end)
	    *temp1-- = *temp2--;

	*temp1 = _nc_render(win, *wch);

	win->_curx++;
    }
}

NCURSES_EXPORT(int)
wins_wch(WINDOW *win, const cchar_t * wch)
{
    NCURSES_SIZE_T oy;
    NCURSES_SIZE_T ox;
    int code = ERR;

    T((T_CALLED("wins_wch(%p, %s)"), win, _tracecchar_t(wch)));

    if (win != 0) {
	oy = win->_cury;
	ox = win->_curx;

	_nc_insert_wch(win, wch);

	win->_curx = ox;
	win->_cury = oy;
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
wins_nwstr(WINDOW *win, const wchar_t * wstr, int n)
{
    int code = ERR;
    NCURSES_SIZE_T oy;
    NCURSES_SIZE_T ox;
    const wchar_t *cp;

    T((T_CALLED("wins_nwstr(%p,%s,%d)"), win, _nc_viswbufn(wstr, n), n));

    if (win != 0
	&& wstr != 0) {
	if (n < 1)
	    n = wcslen(wstr);
	if (n > 0) {
	    oy = win->_cury;
	    ox = win->_curx;
	    for (cp = wstr; *cp && ((cp - wstr) < n); cp++) {
		if (wcwidth(*cp) > 1) {
		    cchar_t tmp_cchar;
		    wchar_t tmp_wchar = *cp;
		    memset(&tmp_cchar, 0, sizeof(tmp_cchar));
		    (void) setcchar(&tmp_cchar,
				    &tmp_wchar,
				    WA_NORMAL,
				    0,
				    (void *) 0);
		    _nc_insert_wch(win, &tmp_cchar);
		} else {
		    _nc_insert_ch(win, *cp);	/* tabs, other ASCII stuff */
		}
	    }

	    win->_curx = ox;
	    win->_cury = oy;
	    _nc_synchook(win);
	}
	code = OK;
    }
    returnCode(code);
}
