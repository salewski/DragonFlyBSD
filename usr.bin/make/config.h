/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)config.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/make/config.h,v 1.16 2005/02/01 10:50:35 harti Exp $
 * $DragonFly: src/usr.bin/make/config.h,v 1.10 2005/02/15 01:01:18 okumoto Exp $
 */

#ifndef config_h_efe0765e
#define	config_h_efe0765e

/*
 * DEFMAXJOBS
 *	This control the default concurrency. On no occasion will more
 *	than DEFMAXJOBS targets be created at once.
 */
#define	DEFMAXJOBS	1

/*
 * INCLUDES
 * LIBRARIES
 *	These control the handling of the .INCLUDES and .LIBS variables.
 *	If INCLUDES is defined, the .INCLUDES variable will be filled
 *	from the search paths of those suffixes which are marked by
 *	.INCLUDES dependency lines. Similarly for LIBRARIES and .LIBS
 *	See suff.c for more details.
 */
#define	INCLUDES
#define	LIBRARIES

/*
 * LIBSUFF
 *	Is the suffix used to denote libraries and is used by the Suff module
 *	to find the search path on which to seek any -l<xx> targets.
 *
 * RECHECK
 *	If defined, Make_Update will check a target for its current
 *	modification time after it has been re-made, setting it to the
 *	starting time of the make only if the target still doesn't exist.
 *	Unfortunately, under NFS the modification time often doesn't
 *	get updated in time, so a target will appear to not have been
 *	re-made, causing later targets to appear up-to-date. On systems
 *	that don't have this problem, you should defined this. Under
 *	NFS you probably should not, unless you aren't exporting jobs.
 */
#define	LIBSUFF	".a"
#define	RECHECK

/*
 * SYSVINCLUDE
 *	Recognize system V like include directives [include "filename"]
 * SYSVVARSUB
 *	Recognize system V like ${VAR:x=y} variable substitutions
 */
#define	SYSVINCLUDE
#define	SYSVVARSUB

/*
 * SUNSHCMD
 *	Recognize SunOS and Solaris:
 *		VAR :sh= CMD	# Assign VAR to the command substitution of CMD
 *		${VAR:sh}	# Return the command substitution of the value
 *				# of ${VAR}
 */
#define	SUNSHCMD

#if !defined(__svr4__) && !defined(__SVR4) && !defined(__ELF__)
# ifndef RANLIBMAG
#  define RANLIBMAG "__.SYMDEF"
# endif
#else
# ifndef RANLIBMAG
#  define RANLIBMAG "/"
# endif
#endif

#endif /* config_h_efe0765e */
