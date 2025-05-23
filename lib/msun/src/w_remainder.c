/* @(#)w_remainder.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_remainder.c,v 1.5 1999/08/28 00:07:07 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_remainder.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper remainder(x,p)
 */

#include "math.h"
#include "math_private.h"

double
remainder(double x, double y)	/* wrapper remainder */
{
#ifdef _IEEE_LIBM
	return __ieee754_remainder(x,y);
#else
	double z;
	z = __ieee754_remainder(x,y);
	if(_LIB_VERSION == _IEEE_ || isnan(y)) return z;
	if(y==0.0)
	    return __kernel_standard(x,y,28); /* remainder(x,0) */
	else
	    return z;
#endif
}
