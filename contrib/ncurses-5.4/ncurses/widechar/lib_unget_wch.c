/****************************************************************************
 * Copyright (c) 2002,2003 Free Software Foundation, Inc.                        *
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
 *  Author: Thomas E. Dickey 2002                                           *
 ****************************************************************************/

/*
**	lib_unget_wch.c
**
**	The routine unget_wch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_unget_wch.c,v 1.5 2003/07/05 19:46:51 tom Exp $")

NCURSES_EXPORT(int)
unget_wch(const wchar_t wch)
{
    int result = OK;
    mbstate_t state;
    size_t length;
    int n;

    T((T_CALLED("unget_wch(%#lx)"), (unsigned long) wch));

    memset(&state, 0, sizeof(state));
    length = wcrtomb(0, wch, &state);

    if (length != (size_t) (-1)
	&& length != 0) {
	char *string;

	if ((string = (char *) malloc(length)) != 0) {
	    memset(&state, 0, sizeof(state));
	    wcrtomb(string, wch, &state);

	    for (n = (int) (length - 1); n >= 0; --n) {
		if (ungetch(string[n]) != OK) {
		    result = ERR;
		    break;
		}
	    }
	    free(string);
	} else {
	    result = ERR;
	}
    } else {
	result = ERR;
    }

    returnCode(result);
}
