/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

#if 0
RCSID("$NetBSD: e_log10f.S,v 1.2 2001/06/19 00:26:30 fvdl Exp $")
#endif
RCSID("$DragonFly: src/lib/msun/i387/Attic/e_log10f.S,v 1.1 2004/12/29 12:55:50 asmodai Exp $")

ENTRY(__ieee754_log10f)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	fldlg2
	flds	ARG_FLOAT_ONE
	fyl2x
	XMM_FLOAT_EPILOGUE
	ret
