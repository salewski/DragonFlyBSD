/* @(#)w_atan2.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_atan2.c,v 1.5 1999/08/28 00:06:58 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_atan2.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper atan2(y,x)
 */

#include "math.h"
#include "math_private.h"


double
atan2(double y, double x)	/* wrapper atan2 */
{
#ifdef _IEEE_LIBM
	return __ieee754_atan2(y,x);
#else
	double z;
	z = __ieee754_atan2(y,x);
	if(_LIB_VERSION == _IEEE_||isnan(x)||isnan(y)) return z;
	if(x==0.0&&y==0.0) {
	        return __kernel_standard(y,x,3); /* atan2(+-0,+-0) */
	} else
	    return z;
#endif
}
