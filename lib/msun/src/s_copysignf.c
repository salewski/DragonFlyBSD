/* s_copysignf.c -- float version of s_copysign.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * $FreeBSD: src/lib/msun/src/s_copysignf.c,v 1.5 1999/08/28 00:06:45 peter Exp $
 * $DragonFly: src/lib/msun/src/Attic/s_copysignf.c,v 1.4 2004/12/29 17:48:27 asmodai Exp $
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
 * copysignf(float x, float y)
 * copysignf(x,y) returns a value with the magnitude of x and
 * with the sign bit of y.
 */

#include "math.h"
#include "math_private.h"

float
__generic_copysignf(float x, float y)
{
	u_int32_t ix,iy;
	GET_FLOAT_WORD(ix,x);
	GET_FLOAT_WORD(iy,y);
	SET_FLOAT_WORD(x,(ix&0x7fffffff)|(iy&0x80000000));
        return x;
}
