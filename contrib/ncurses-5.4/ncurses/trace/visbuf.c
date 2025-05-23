/****************************************************************************
 * Copyright (c) 2001-2003,2004 Free Software Foundation, Inc.              *
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
 *  Author: Thomas E. Dickey 1996-2004                                      *
 *     and: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 *	visbuf.c - Tracing/Debugging support routines
 */

#include <curses.priv.h>

#include <tic.h>
#include <ctype.h>

MODULE_ID("$Id: visbuf.c,v 1.9 2004/02/03 01:16:37 tom Exp $")

static char *
_nc_vischar(char *tp, unsigned c)
{
    if (c == '"' || c == '\\') {
	*tp++ = '\\';
	*tp++ = c;
    } else if (is7bits(c) && (isgraph(c) || c == ' ')) {
	*tp++ = c;
    } else if (c == '\n') {
	*tp++ = '\\';
	*tp++ = 'n';
    } else if (c == '\r') {
	*tp++ = '\\';
	*tp++ = 'r';
    } else if (c == '\b') {
	*tp++ = '\\';
	*tp++ = 'b';
    } else if (c == '\033') {
	*tp++ = '\\';
	*tp++ = 'e';
    } else if (is7bits(c) && iscntrl(UChar(c))) {
	*tp++ = '\\';
	*tp++ = '^';
	*tp++ = '@' + c;
    } else {
	sprintf(tp, "\\%03lo", ChCharOf(c));
	tp += strlen(tp);
    }
    *tp = 0;
    return tp;
}

static const char *
_nc_visbuf2n(int bufnum, const char *buf, int len)
{
    char *vbuf;
    char *tp;
    int c;

    if (buf == 0)
	return ("(null)");
    if (buf == CANCELLED_STRING)
	return ("(cancelled)");

    if (len < 0)
	len = strlen(buf);

#ifdef TRACE
    tp = vbuf = _nc_trace_buf(bufnum, (unsigned) (len * 4) + 5);
#else
    {
	static char *mybuf[2];
	mybuf[bufnum] = typeRealloc(char, (unsigned) (len * 4) + 5, mybuf[bufnum]);
	tp = vbuf = mybuf[bufnum];
    }
#endif
    *tp++ = D_QUOTE;
    while ((--len >= 0) && (c = *buf++) != '\0') {
	tp = _nc_vischar(tp, UChar(c));
    }
    *tp++ = D_QUOTE;
    *tp++ = '\0';
    return (vbuf);
}

NCURSES_EXPORT(const char *)
_nc_visbuf2(int bufnum, const char *buf)
{
    return _nc_visbuf2n(bufnum, buf, -1);
}

NCURSES_EXPORT(const char *)
_nc_visbuf(const char *buf)
{
    return _nc_visbuf2(0, buf);
}

NCURSES_EXPORT(const char *)
_nc_visbufn(const char *buf, int len)
{
    return _nc_visbuf2n(0, buf, len);
}

#if USE_WIDEC_SUPPORT
#ifdef TRACE
static const char *
_nc_viswbuf2n(int bufnum, const wchar_t * buf, int len)
{
    char *vbuf;
    char *tp;
    wchar_t c;

    if (buf == 0)
	return ("(null)");

    if (len < 0)
	len = wcslen(buf);

#ifdef TRACE
    tp = vbuf = _nc_trace_buf(bufnum, (unsigned) (len * 4) + 5);
#else
    {
	static char *mybuf[2];
	mybuf[bufnum] = typeRealloc(char, (unsigned) (len * 4) + 5, mybuf[bufnum]);
	tp = vbuf = mybuf[bufnum];
    }
#endif
    *tp++ = D_QUOTE;
    while ((--len >= 0) && (c = *buf++) != '\0') {
	char temp[CCHARW_MAX + 80];
	int j = wctomb(temp, c), k;
	if (j <= 0) {
	    sprintf(temp, "\\u%08X", (wint_t) c);
	    j = strlen(temp);
	}
	for (k = 0; k < j; ++k) {
	    tp = _nc_vischar(tp, temp[k]);
	}
    }
    *tp++ = D_QUOTE;
    *tp++ = '\0';
    return (vbuf);
}

NCURSES_EXPORT(const char *)
_nc_viswbuf2(int bufnum, const wchar_t * buf)
{
    return _nc_viswbuf2n(bufnum, buf, -1);
}

NCURSES_EXPORT(const char *)
_nc_viswbuf(const wchar_t * buf)
{
    return _nc_viswbuf2(0, buf);
}

NCURSES_EXPORT(const char *)
_nc_viswbufn(const wchar_t * buf, int len)
{
    return _nc_viswbuf2n(0, buf, len);
}

NCURSES_EXPORT(const char *)
_nc_viscbuf2(int bufnum, const cchar_t * buf, int len)
{
    char *result = _nc_trace_buf(bufnum, BUFSIZ);
    int n;
    bool same = TRUE;
    attr_t attr = A_NORMAL;
    const char *found;

    if (len < 0)
	len = _nc_wchstrlen(buf);

    for (n = 1; n < len; n++) {
	if (AttrOf(buf[n]) != AttrOf(buf[0])) {
	    same = FALSE;
	    break;
	}
    }

    /*
     * If the rendition is the same for the whole string, display it as a
     * quoted string, followed by the rendition.  Otherwise, use the more
     * detailed trace function that displays each character separately.
     */
    if (same) {
	static const char d_quote[] =
	{D_QUOTE, 0};

	result = _nc_trace_bufcat(bufnum, d_quote);
	while (len-- > 0) {
	    if ((found = _nc_altcharset_name(attr, CharOfD(buf))) != 0) {
		result = _nc_trace_bufcat(bufnum, found);
		attr &= ~A_ALTCHARSET;
	    } else if (!isnac(CHDEREF(buf))) {
		PUTC_DATA;

		PUTC_INIT;
		do {
		    PUTC_ch = PUTC_i < CCHARW_MAX ? buf->chars[PUTC_i] : L'\0';
		    PUTC_n = wcrtomb(PUTC_buf, buf->chars[PUTC_i], &PUT_st);
		    if (PUTC_ch == L'\0')
			--PUTC_n;
		    if (PUTC_n <= 0)
			break;
		    for (n = 0; n < PUTC_n; n++) {
			char temp[80];
			_nc_vischar(temp, UChar(PUTC_buf[n]));
			result = _nc_trace_bufcat(bufnum, temp);
		    }
		    ++PUTC_i;
		} while (PUTC_ch != L'\0');
	    }
	    buf++;
	}
	result = _nc_trace_bufcat(bufnum, d_quote);
	if (attr != A_NORMAL) {
	    result = _nc_trace_bufcat(bufnum, " | ");
	    result = _nc_trace_bufcat(bufnum, _traceattr2(bufnum + 20, attr));
	}
    } else {
	static const char l_brace[] =
	{L_BRACE, 0};
	static const char r_brace[] =
	{R_BRACE, 0};
	strcpy(result, l_brace);
	while (len-- > 0) {
	    result = _nc_trace_bufcat(bufnum,
				      _tracecchar_t2(bufnum + 20, buf++));
	}
	result = _nc_trace_bufcat(bufnum, r_brace);
    }
    return result;
}

NCURSES_EXPORT(const char *)
_nc_viscbuf(const cchar_t * buf, int len)
{
    return _nc_viscbuf2(0, buf, len);
}
#endif /* TRACE */
#endif /* USE_WIDEC_SUPPORT */
