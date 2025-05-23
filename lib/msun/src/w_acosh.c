/* @(#)w_acosh.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_acosh.c,v 1.5 1999/08/28 00:06:57 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_acosh.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper acosh(x)
 */

#include "math.h"
#include "math_private.h"

double
acosh(double x)		/* wrapper acosh */
{
#ifdef _IEEE_LIBM
	return __ieee754_acosh(x);
#else
	double z;
	z = __ieee754_acosh(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(x<1.0) {
	        return __kernel_standard(x,x,29); /* acosh(x<1) */
	} else
	    return z;
#endif
}
