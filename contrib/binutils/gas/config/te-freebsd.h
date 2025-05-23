/* te-freebsd.h -- FreeBSD target environment declarations.
   Copyright 2000 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* $FreeBSD: src/contrib/binutils/gas/config/te-freebsd.h,v 1.1.6.4 2002/09/01 23:44:05 obrien Exp $ */
/* $DragonFly: src/contrib/binutils/gas/config/Attic/te-freebsd.h,v 1.2 2003/06/17 04:23:58 dillon Exp $ */

/* Target environment for FreeBSD.  It is the same as the generic
   target, except that it arranges via the TE_FreeBSD define to
   suppress the use of "/" as a comment character.  Some code in the
   FreeBSD kernel uses "/" to mean division.  (What a concept!)  */
#define TE_FreeBSD 1

#define LOCAL_LABELS_DOLLAR 1
#define LOCAL_LABELS_FB 1

#include "obj-format.h"
