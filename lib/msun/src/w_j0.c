/* @(#)w_j0.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_j0.c,v 1.6 1999/08/28 00:07:03 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_j0.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper j0(double x)
 */

#include "math.h"
#include "math_private.h"

double
j0(double x)		/* wrapper j0 */
{
#ifdef _IEEE_LIBM
	return __ieee754_j0(x);
#else
	double z = __ieee754_j0(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(fabs(x)>X_TLOSS) {
	        return __kernel_standard(x,x,34); /* j0(|x|>X_TLOSS) */
	} else
	    return z;
#endif
}
