/****************************************************************************
 * Copyright (c) 1998,2001-2002 Free Software Foundation, Inc.              *
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

MODULE_ID("$Id: lib_bkgd.c,v 1.30 2003/07/05 16:46:49 tom Exp $")

/*
 * Set the window's background information.
 */
#if USE_WIDEC_SUPPORT
NCURSES_EXPORT(void)
#else
static inline void
#endif
wbkgrndset(WINDOW *win, const ARG_CH_T ch)
{
    T((T_CALLED("wbkgdset(%p,%s)"), win, _tracech_t(ch)));

    if (win) {
	attr_t off = AttrOf(win->_nc_bkgd);
	attr_t on = AttrOf(CHDEREF(ch));

	toggle_attr_off(win->_attrs, off);
	toggle_attr_on(win->_attrs, on);

	if (CharOf(CHDEREF(ch)) == L('\0'))
	    SetChar(win->_nc_bkgd, BLANK_TEXT, AttrOf(CHDEREF(ch)));
	else
	    win->_nc_bkgd = CHDEREF(ch);
#if USE_WIDEC_SUPPORT
	/*
	 * If we're compiled for wide-character support, _bkgrnd is the
	 * preferred location for the background information since it stores
	 * more than _bkgd.  Update _bkgd each time we modify _bkgrnd, so the
	 * macro getbkgd() will work.
	 */
	{
	    cchar_t wch;
	    int tmp;

	    wgetbkgrnd(win, &wch);
	    tmp = _nc_to_char(CharOf(wch));

	    win->_bkgd = ((tmp == EOF) ? ' ' : (chtype) tmp) | AttrOf(wch);
	}
#endif
    }
    returnVoid;
}

NCURSES_EXPORT(void)
wbkgdset(WINDOW *win, chtype ch)
{
    NCURSES_CH_T wch;
    SetChar2(wch, ch);
    wbkgrndset(win, CHREF(wch));
}

/*
 * Set the window's background information and apply it to each cell.
 */
#if USE_WIDEC_SUPPORT
NCURSES_EXPORT(int)
#else
static inline int
#undef wbkgrnd
#endif
wbkgrnd(WINDOW *win, const ARG_CH_T ch)
{
    int code = ERR;
    int x, y;
    NCURSES_CH_T new_bkgd = CHDEREF(ch);

    T((T_CALLED("wbkgd(%p,%s)"), win, _tracech_t(ch)));

    if (win) {
	NCURSES_CH_T old_bkgrnd;
	wgetbkgrnd(win, &old_bkgrnd);

	wbkgrndset(win, CHREF(new_bkgd));
	wattrset(win, AttrOf(win->_nc_bkgd));

	for (y = 0; y <= win->_maxy; y++) {
	    for (x = 0; x <= win->_maxx; x++) {
		if (CharEq(win->_line[y].text[x], old_bkgrnd))
		    win->_line[y].text[x] = win->_nc_bkgd;
		else {
		    NCURSES_CH_T wch = win->_line[y].text[x];
		    RemAttr(wch, (~A_ALTCHARSET));
		    win->_line[y].text[x] = _nc_render(win, wch);
		}
	    }
	}
	touchwin(win);
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
wbkgd(WINDOW *win, chtype ch)
{
    NCURSES_CH_T wch;
    SetChar2(wch, ch);
    return wbkgrnd(win, CHREF(wch));
}
