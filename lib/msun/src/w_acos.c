/* @(#)w_acos.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_acos.c,v 1.5 1999/08/28 00:06:57 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_acos.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrap_acos(x)
 */

#include "math.h"
#include "math_private.h"


double
acos(double x)		/* wrapper acos */
{
#ifdef _IEEE_LIBM
	return __ieee754_acos(x);
#else
	double z;
	z = __ieee754_acos(x);
	if(_LIB_VERSION == _IEEE_ || isnan(x)) return z;
	if(fabs(x)>1.0) {
	        return __kernel_standard(x,x,1); /* acos(|x|>1) */
	} else
	    return z;
#endif
}
