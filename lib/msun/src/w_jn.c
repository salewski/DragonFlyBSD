/* @(#)w_jn.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_jn.c,v 1.7 1999/08/28 00:07:04 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_jn.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper jn(int n, double x)
 */

#include "math.h"
#include "math_private.h"

double
jn(int n, double x)	/* wrapper jn */
{
#ifdef _IEEE_LIBM
	return __ieee754_jn(n,x);
#else
	double z;
	z = __ieee754_jn(n,x);
	if(_LIB_VERSION == _IEEE_ || isnan(x) ) return z;
	if(fabs(x)>X_TLOSS) {
	    return __kernel_standard((double)n,x,38); /* jn(|x|>X_TLOSS,n) */
	} else
	    return z;
#endif
}
