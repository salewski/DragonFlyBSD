/* from: @(#)w_j1.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_y1.c,v 1.3 1999/08/28 00:07:10 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_y1.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper of y1
 */

#include "math.h"
#include "math_private.h"

double
y1(double x)		/* wrapper y1 */
{
#ifdef _IEEE_LIBM
	return __ieee754_y1(x);
#else
	double z;
	z = __ieee754_y1(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x) ) return z;
        if(x <= 0.0){
                if(x==0.0)
                    /* d= -one/(x-x); */
                    return __kernel_standard(x,x,10);
                else
                    /* d = zero/(x-x); */
                    return __kernel_standard(x,x,11);
        }
	if(x>X_TLOSS) {
	        return __kernel_standard(x,x,37); /* y1(x>X_TLOSS) */
	} else
	    return z;
#endif
}
