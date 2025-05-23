/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

#if 0
RCSID("$NetBSD: e_acos.S,v 1.7 2001/06/19 00:26:29 fvdl Exp $")
#endif
RCSID("$DragonFly: src/lib/msun/i387/Attic/e_acos.S,v 1.3 2004/12/29 11:40:17 asmodai Exp $")

/* acos = atan (sqrt(1 - x^2) / x) */
ENTRY(__ieee754_acos)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE		/* x */
	fld	%st(0)
	fmul	%st(0)			/* x^2 */
	fld1
	fsubp				/* 1 - x^2 */
	fsqrt				/* sqrt (1 - x^2) */
	fxch	%st(1)
	fpatan
	XMM_DOUBLE_EPILOGUE
	ret
