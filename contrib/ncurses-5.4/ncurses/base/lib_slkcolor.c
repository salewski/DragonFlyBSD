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
 *  Author:  Juergen Pfeifer, 1998                                          *
 ****************************************************************************/

/*
 *	lib_slkcolor.c
 */
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkcolor.c,v 1.9 2003/10/25 15:27:03 tom Exp $")

NCURSES_EXPORT(int)
slk_color(short color_pair_number)
{
    T((T_CALLED("slk_color(%d)"), color_pair_number));

    if (SP != 0 && SP->_slk != 0 &&
	color_pair_number >= 0 && color_pair_number < COLOR_PAIRS) {
	T(("... current %ld", (long) PAIR_NUMBER(SP->_slk->attr)));
	toggle_attr_on(SP->_slk->attr, COLOR_PAIR(color_pair_number));
	returnCode(OK);
    } else
	returnCode(ERR);
}
