/****************************************************************************
 * Copyright (c) 1998,2000 Free Software Foundation, Inc.                   *
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

/* $Id: termcap.h.in,v 1.16 2001/03/24 21:53:27 tom Exp $ */
/* $DragonFly: src/lib/libncurses/include/termcap.h,v 1.1 2005/03/12 19:13:54 eirikn Exp $ */

#ifndef NCURSES_TERMCAP_H_incl
#define NCURSES_TERMCAP_H_incl	1

#undef  NCURSES_VERSION
#define NCURSES_VERSION "5.4"

#if 0
#include <ncurses_dll.h>
#else
/* From ncurses_dll.h */
/* This is copied so we don't need to install ncurses_dll.h as well */
/* Take care of non-cygwin platforms */
#if !defined(NCURSES_IMPEXP)
#  define NCURSES_IMPEXP /* nothing */
#endif
#if !defined(NCURSES_API)
#  define NCURSES_API /* nothing */
#endif
#if !defined(NCURSES_EXPORT)
#  define NCURSES_EXPORT(type) NCURSES_IMPEXP type NCURSES_API
#endif
#if !defined(NCURSES_EXPORT_VAR)
#  define NCURSES_EXPORT_VAR(type) NCURSES_IMPEXP type
#endif
#endif


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <sys/types.h>

#undef  NCURSES_CONST 
#define NCURSES_CONST /*nothing*/ 

#undef  NCURSES_OSPEED 
#define NCURSES_OSPEED short 

extern NCURSES_EXPORT_VAR(char) PC;
extern NCURSES_EXPORT_VAR(char *) UP;
extern NCURSES_EXPORT_VAR(char *) BC;
extern NCURSES_EXPORT_VAR(NCURSES_OSPEED) ospeed; 

#if !defined(NCURSES_TERM_H_incl)
extern NCURSES_EXPORT(char *) tgetstr (NCURSES_CONST char *, char **);
extern NCURSES_EXPORT(char *) tgoto (const char *, int, int);
extern NCURSES_EXPORT(int) tgetent (char *, const char *);
extern NCURSES_EXPORT(int) tgetflag (NCURSES_CONST char *);
extern NCURSES_EXPORT(int) tgetnum (NCURSES_CONST char *);
extern NCURSES_EXPORT(int) tputs (const char *, int, int (*)(int));
#endif

#ifdef __cplusplus
}
#endif

#endif /* NCURSES_TERMCAP_H_incl */
