/* Configuration for GNU C-compiler for hosts running FreeBSD.
   Copyright (C) 1994, 1995 Free Software Foundation, Inc.

$DragonFly: src/contrib/gcc/config/Attic/xm-dragonfly.h,v 1.1 2003/11/19 00:51:32 dillon Exp $

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* This file defines machine-independent things specific to a host
   running FreeBSD.  This file should not be specified as $xm_file itself;
   instead $xm_file should be CPU/xm-freebsd.h, which should include both
   CPU/xm-CPU.h and this file xm-freebsd.h.  */

#ifndef HAVE_ATEXIT
#define HAVE_ATEXIT
#endif

/* Tell gcc and collect2 that FreeBSD targets support putenv(3).  */
#define HAVE_PUTENV

/* We have _sys_siglist, but the declaration in <signal.h> conflicts with
   the declarations in collect2.c and mips-tfile.c, so disable the declarations
   in those files.  */

#define SYS_SIGLIST_DECLARED
