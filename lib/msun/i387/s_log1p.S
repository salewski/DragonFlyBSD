/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

/*
 * Modified by Lex Wennmacher <wennmach@NetBSD.org>
 * Still public domain.
 */

#include <machine/asm.h>

#include "abi.h"

#if 0
RCSID("$NetBSD$")
#endif
RCSID("$DragonFly: src/lib/msun/i387/Attic/s_log1p.S,v 1.3 2004/12/29 11:40:18 asmodai Exp $")

/*
 * The log1p() function is provided to compute an accurate value of
 * log(1 + x), even for tiny values of x. The i387 FPU provides the
 * fyl2xp1 instruction for this purpose. However, the range of this
 * instruction is limited to:
 * 		-(1 - (sqrt(2) / 2)) <= x <= sqrt(2) - 1
 *                         -0.292893 <= x <= 0.414214
 * at least on older processor versions.
 *
 * log1p() is implemented by testing the range of the argument.
 * If it is appropriate for fyl2xp1, this instruction is used.
 * Else, we compute log1p(x) = ln(2)*ld(1 + x) the traditional way
 * (using fyl2x).
 *
 * The range testing costs speed, but as the rationale for the very
 * existence of this function is accuracy, we accept that.
 *
 * In order to reduce the cost for testing the range, we check if
 * the argument is in the range
 *                             -0.25 <= x <= 0.25
 * which can be done with just one conditional branch. If x is
 * inside this range, we use fyl2xp1. Outside of this range,
 * the use of fyl2x is accurate enough.
 * 
 */

.text
	.align	4
ENTRY(log1p)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fabs
	fld1				/* ... x 1 */
	fadd	%st(0)			/* ... x 2 */
	fadd	%st(0)			/* ... x 4 */
	fld1				/* ... 4 1 */
	fdivp				/* ... x 0.25 */
	fcompp
	fnstsw	%ax
	andb	$69,%ah
	jne	use_fyl2x
	jmp	use_fyl2xp1

	.align	4
use_fyl2x:
	fldln2
        fldl	ARG_DOUBLE_ONE
        fld1
        faddp
        fyl2x
	XMM_DOUBLE_EPILOGUE
        ret

	.align	4
use_fyl2xp1:
	fldln2
	fldl	ARG_DOUBLE_ONE
	fyl2xp1
	XMM_DOUBLE_EPILOGUE
	ret
