/* w_acosf.c -- float version of w_acos.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * $FreeBSD: src/lib/msun/src/w_acosf.c,v 1.5 1999/08/28 00:06:57 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_acosf.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/*
 * wrap_acosf(x)
 */

#include "math.h"
#include "math_private.h"


float
acosf(float x)		/* wrapper acosf */
{
#ifdef _IEEE_LIBM
	return __ieee754_acosf(x);
#else
	float z;
	z = __ieee754_acosf(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(fabsf(x)>(float)1.0) {
	        /* acosf(|x|>1) */
	        return (float)__kernel_standard((double)x,(double)x,101);
	} else
	    return z;
#endif
}
