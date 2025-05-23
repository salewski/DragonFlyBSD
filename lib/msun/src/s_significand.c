/* @(#)s_signif.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/s_significand.c,v 1.6 1999/08/28 00:06:55 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/s_significand.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * significand(x) computes just
 * 	scalb(x, (double) -ilogb(x)),
 * for exercising the fraction-part(F) IEEE 754-1985 test vector.
 */

#include "math.h"
#include "math_private.h"

double
__generic_significand(double x)
{
	return __ieee754_scalb(x,(double) -ilogb(x));
}
