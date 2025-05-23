/* @(#)w_sinh.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_sinh.c,v 1.5 1999/08/28 00:07:08 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_sinh.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper sinh(x)
 */

#include "math.h"
#include "math_private.h"

double
sinh(double x)		/* wrapper sinh */
{
#ifdef _IEEE_LIBM
	return __ieee754_sinh(x);
#else
	double z;
	z = __ieee754_sinh(x);
	if(_LIB_VERSION == _IEEE_) return z;
	if(!finite(z)&&finite(x)) {
	    return __kernel_standard(x,x,25); /* sinh overflow */
	} else
	    return z;
#endif
}
