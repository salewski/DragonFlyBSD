/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 *
 * 18 July 1993
 *
 * Semi-convenient place to stick some needed globals.
 *
 * $FreeBSD: src/usr.sbin/pkg_install/lib/global.c,v 1.10 2004/10/18 05:34:54 obrien Exp $
 * $DragonFly: src/usr.sbin/pkg_install/lib/Attic/global.c,v 1.4 2005/03/08 19:11:30 joerg Exp $
 */

#include "lib.h"

/* These are global for all utils */
Boolean	Quiet		= FALSE;
Boolean	Verbose		= FALSE;
Boolean	Fake		= FALSE;
Boolean	Force		= FALSE;
int AutoAnswer		= FALSE;
