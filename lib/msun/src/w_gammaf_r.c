/* w_gammaf_r.c -- float version of w_gamma_r.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * $FreeBSD: src/lib/msun/src/w_gammaf_r.c,v 1.5 1999/08/28 00:07:02 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_gammaf_r.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
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
 * wrapper float gammaf_r(float x, int *signgamp)
 */

#include "math.h"
#include "math_private.h"


float
gammaf_r(float x, int *signgamp) /* wrapper lgammaf_r */
{
#ifdef _IEEE_LIBM
	return __ieee754_gammaf_r(x,signgamp);
#else
        float y;
        y = __ieee754_gammaf_r(x,signgamp);
        if(_LIB_VERSION == _IEEE_) return y;
        if(!finitef(y)&&finitef(x)) {
            if(floorf(x)==x&&x<=(float)0.0)
	        /* gammaf pole */
                return (float)__kernel_standard((double)x,(double)x,141);
            else
	        /* gamma overflow */
                return (float)__kernel_standard((double)x,(double)x,140);
        } else
            return y;
#endif
}
