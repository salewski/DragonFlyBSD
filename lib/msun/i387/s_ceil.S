/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

#if 0
RCSID("$NetBSD: s_ceil.S,v 1.6 2002/03/26 15:17:37 fvdl Exp $")
#endif
RCSID("$DragonFly: src/lib/msun/i387/Attic/s_ceil.S,v 1.3 2004/12/29 11:40:18 asmodai Exp $")

ENTRY(ceil)
#ifdef __i386__
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp

	fstcw	-4(%ebp)		/* store fpu control word */
	movw	-4(%ebp),%dx
	orw	$0x0800,%dx		/* round towards +oo */
	andw	$0xfbff,%dx
	movw	%dx,-8(%ebp)
	fldcw	-8(%ebp)		/* load modfied control word */

	fldl	8(%ebp);		/* round */
	frndint

	fldcw	-4(%ebp)		/* restore original control word */

	leave
#else
	fstcw	-12(%rsp)
	movw	-12(%rsp),%dx
	orw	$0x0800,%dx
	andw	$0xfbff,%dx
	movw	%dx,-16(%rsp)
	fldcw	-16(%rsp)
	movsd	%xmm0,-8(%rsp)
	fldl	-8(%rsp)
	frndint
	fldcw	-12(%rsp)
	fstpl	-8(%rsp)
	movsd	-8(%rsp),%xmm0
#endif
	ret
