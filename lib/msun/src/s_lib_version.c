/* @(#)s_lib_ver.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * $FreeBSD: src/lib/msun/src/s_lib_version.c,v 1.5 1999/08/28 00:06:52 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/s_lib_version.c,v 1.2 2003/06/17 04:26:53 dillon Exp $
 */

/*
 * MACRO for standards
 */

#include "math.h"
#include "math_private.h"

/*
 * define and initialize _LIB_VERSION
 */
#ifdef _POSIX_MODE
_LIB_VERSION_TYPE _LIB_VERSION = _POSIX_;
#else
#ifdef _XOPEN_MODE
_LIB_VERSION_TYPE _LIB_VERSION = _XOPEN_;
#else
#ifdef _SVID3_MODE
_LIB_VERSION_TYPE _LIB_VERSION = _SVID_;
#else					/* default _IEEE_MODE */
_LIB_VERSION_TYPE _LIB_VERSION = _IEEE_;
#endif
#endif
#endif
