/* @(#)w_cosh.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_cosh.c,v 1.5 1999/08/28 00:06:59 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_cosh.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper cosh(x)
 */

#include "math.h"
#include "math_private.h"

double
cosh(double x)		/* wrapper cosh */
{
#ifdef _IEEE_LIBM
	return __ieee754_cosh(x);
#else
	double z;
	z = __ieee754_cosh(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(fabs(x)>7.10475860073943863426e+02) {
	        return __kernel_standard(x,x,5); /* cosh overflow */
	} else
	    return z;
#endif
}
