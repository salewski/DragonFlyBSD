/* w_y0f.c -- float version of w_y0.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * $FreeBSD: src/lib/msun/src/w_y0f.c,v 1.3 1999/08/28 00:07:10 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_y0f.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
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
 * wrapper y0f(float x)
 */

#include "math.h"
#include "math_private.h"

float
y0f(float x)		/* wrapper y0f */
{
#ifdef _IEEE_LIBM
	return __ieee754_y0f(x);
#else
	float z;
	z = __ieee754_y0f(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x) ) return z;
        if(x <= (float)0.0){
                if(x==(float)0.0)
                    /* d= -one/(x-x); */
                    return (float)__kernel_standard((double)x,(double)x,108);
                else
                    /* d = zero/(x-x); */
                    return (float)__kernel_standard((double)x,(double)x,109);
        }
	if(x>(float)X_TLOSS) {
		/* y0(x>X_TLOSS) */
	        return (float)__kernel_standard((double)x,(double)x,135);
	} else
	    return z;
#endif
}
