/* @(#)w_fmod.c 5.1 93/09/24 */
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
 * $FreeBSD: src/lib/msun/src/w_fmod.c,v 1.5 1999/08/28 00:07:00 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/w_fmod.c,v 1.3 2004/12/29 15:22:57 asmodai Exp $
 */

/*
 * wrapper fmod(x,y)
 */

#include "math.h"
#include "math_private.h"


double
fmod(double x, double y)	/* wrapper fmod */
{
#ifdef _IEEE_LIBM
	return __ieee754_fmod(x,y);
#else
	double z;
	z = __ieee754_fmod(x,y);
	if(_LIB_VERSION == _IEEE_ ||isnan(y)||isnan(x)) return z;
	if(y==0.0) {
	        return __kernel_standard(x,y,27); /* fmod(x,0) */
	} else
	    return z;
#endif
}
