/* $FreeBSD: src/contrib/libreadline/shell.c,v 1.4.2.2 2000/07/06 23:04:24 ache Exp $ */
/* $DragonFly: src/contrib/libreadline/Attic/shell.c,v 1.2 2003/06/17 04:24:03 dillon Exp $ */
/* shell.c -- readline utility functions that are normally provided by
	      bash when readline is linked as part of the shell. */

/* Copyright (C) 1997 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif /* !HAVE_STRING_H */

#include <fcntl.h>
#include <pwd.h>

#include <stdio.h>

#include "rlshell.h"
#include "xmalloc.h"

#if !defined (HAVE_GETPW_DECLS)
extern struct passwd *getpwuid ();
#endif /* !HAVE_GETPW_DECLS */

#ifndef NULL
#  define NULL 0
#endif

/* All of these functions are resolved from bash if we are linking readline
   as part of bash. */

/* Does shell-like quoting using single quotes. */
char *
single_quote (string)
     char *string;
{
  register int c;
  char *result, *r, *s;

  result = (char *)xmalloc (3 + (4 * strlen (string)));
  r = result;
  *r++ = '\'';

  for (s = string; s && (c = *s); s++)
    {
      *r++ = c;

      if (c == '\'')
	{
	  *r++ = '\\';	/* insert escaped single quote */
	  *r++ = '\'';
	  *r++ = '\'';	/* start new quoted string */
	}
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

/* Set the environment variables LINES and COLUMNS to lines and cols,
   respectively. */
void
set_lines_and_columns (lines, cols)
     int lines, cols;
{
  char *b;

#if defined (HAVE_PUTENV)
  b = xmalloc (24);
  sprintf (b, "LINES=%d", lines);
  putenv (b);
  b = xmalloc (24);
  sprintf (b, "COLUMNS=%d", cols);
  putenv (b);
#else /* !HAVE_PUTENV */
#  if defined (HAVE_SETENV)
  b = xmalloc (8);
  sprintf (b, "%d", lines);
  setenv ("LINES", b, 1);
  b = xmalloc (8);
  sprintf (b, "%d", cols);
  setenv ("COLUMNS", b, 1);
#  endif /* HAVE_SETENV */
#endif /* !HAVE_PUTENV */
}

char *
get_env_value (varname)
     char *varname;
{
  return ((char *)getenv (varname));
}

char *
get_home_dir ()
{
  char *home_dir;
  struct passwd *entry;

  home_dir = (char *)NULL;
  entry = getpwuid (getuid ());
  if (entry)
    home_dir = entry->pw_dir;
  return (home_dir);
}

#if !defined (O_NDELAY)
#  if defined (FNDELAY)
#    define O_NDELAY FNDELAY
#  endif
#endif

int
unset_nodelay_mode (fd)
     int fd;
{
  int flags, bflags;

  if ((flags = fcntl (fd, F_GETFL, 0)) < 0)
    return -1;

  bflags = 0;

#ifdef O_NONBLOCK
  bflags |= O_NONBLOCK;
#endif

#ifdef O_NDELAY
  bflags |= O_NDELAY;
#endif

  if (flags & bflags)
    {
      flags &= ~bflags;
      return (fcntl (fd, F_SETFL, flags));
    }

  return 0;
}
