/* w_remainderf.c -- float version of w_remainder.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * $FreeBSD: src/lib/msun/src/w_remainderf.c,v 1.5 1999/08/28 00:07:08 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_remainderf.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
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
 * wrapper remainderf(x,p)
 */

#include "math.h"
#include "math_private.h"

float
remainderf(float x, float y)	/* wrapper remainder */
{
#ifdef _IEEE_LIBM
	return __ieee754_remainderf(x,y);
#else
	float z;
	z = __ieee754_remainderf(x,y);
	if(_LIB_VERSION == _IEEE_ || isnanf(y)) return z;
	if(y==(float)0.0)
	    /* remainder(x,0) */
	    return (float)__kernel_standard((double)x,(double)y,128);
	else
	    return z;
#endif
}
