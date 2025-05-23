/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)DEFS.h	5.1 (Berkeley) 4/23/90
 * $FreeBSD: src/sys/i386/include/asm.h,v 1.7 2000/01/25 09:01:55 bde Exp $
 * $DragonFly: src/sys/i386/include/Attic/asm.h,v 1.3 2004/03/20 16:27:41 drhodus Exp $
 */

#ifndef _MACHINE_ASM_H_
#define	_MACHINE_ASM_H_

#include <sys/cdefs.h>

#ifdef PIC
#define	PIC_PROLOGUE	\
	pushl	%ebx;	\
	call	1f;	\
1:			\
	popl	%ebx;	\
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b],%ebx
#define	PIC_EPILOGUE	\
	popl	%ebx
#define	PIC_PLT(x)	x@PLT
#define	PIC_GOT(x)	x@GOT(%ebx)
#define	PIC_GOTOFF(x)	x@GOTOFF(%ebx)
#else
#define	PIC_PROLOGUE
#define	PIC_EPILOGUE
#define	PIC_PLT(x)	x
#define	PIC_GOT(x)	x
#define	PIC_GOTOFF(x)	x
#endif

/*
 * CNAME and HIDENAME manage the relationship between symbol names in C
 * and the equivalent assembly language names.  CNAME is given a name as
 * it would be used in a C program.  It expands to the equivalent assembly
 * language name.  HIDENAME is given an assembly-language name, and expands
 * to a possibly-modified form that will be invisible to C programs.
 */
#define CNAME(csym)		csym
#define HIDENAME(asmsym)	__CONCAT(.,asmsym)

/* XXX should use .p2align 4,0x90 for -m486. */
#define _START_ENTRY	.text; .p2align 2,0x90

#define _ENTRY(x)	_START_ENTRY; \
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x):

#ifdef PROF
#define	ALTENTRY(x)	_ENTRY(x); \
			pushl %ebp; movl %esp,%ebp; \
			call PIC_PLT(HIDENAME(mcount)); \
			popl %ebp; \
			jmp 9f
#define	ENTRY(x)	_ENTRY(x); \
			pushl %ebp; movl %esp,%ebp; \
			call PIC_PLT(HIDENAME(mcount)); \
			popl %ebp; \
			9:
#else
#define	ALTENTRY(x)	_ENTRY(x)
#define	ENTRY(x)	_ENTRY(x)
#endif

#define RCSID(x)	.text; .asciz x

#ifdef _ARCH_INDIRECT
/*
 * Generate code to select between the generic functions and _ARCH_INDIRECT
 * specific ones.
 * XXX nested __CONCATs don't work with non-ANSI cpp's.
 */
#define	ANAME(x)	CNAME(__CONCAT(__CONCAT(__,_ARCH_INDIRECT),x))
#define	ASELNAME(x)	CNAME(__CONCAT(__arch_select_,x))
#define	AVECNAME(x)	CNAME(__CONCAT(__arch_,x))
#define	GNAME(x)	CNAME(__CONCAT(__generic_,x))

/* Don't bother profiling this. */
#ifdef PIC
#define	ARCH_DISPATCH(x) \
			_START_ENTRY; \
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x): ; \
			PIC_PROLOGUE; \
			movl PIC_GOT(AVECNAME(x)),%eax; \
			PIC_EPILOGUE; \
			jmpl *(%eax)

#define	ARCH_SELECT(x)	_START_ENTRY; \
			.type ASELNAME(x),@function; \
			ASELNAME(x): \
			PIC_PROLOGUE; \
			call PIC_PLT(CNAME(__get_hw_float)); \
			testl %eax,%eax; \
			movl PIC_GOT(ANAME(x)),%eax; \
			jne 8f; \
			movl PIC_GOT(GNAME(x)),%eax; \
			8: \
			movl PIC_GOT(AVECNAME(x)),%edx; \
			movl %eax,(%edx); \
			PIC_EPILOGUE; \
			jmpl *%eax
#else /* !PIC */
#define	ARCH_DISPATCH(x) \
			_START_ENTRY; \
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x): ; \
			jmpl *AVECNAME(x)

#define	ARCH_SELECT(x)	_START_ENTRY; \
			.type ASELNAME(x),@function; \
			ASELNAME(x): \
			call CNAME(__get_hw_float); \
			testl %eax,%eax; \
			movl $ANAME(x),%eax; \
			jne 8f; \
			movl $GNAME(x),%eax; \
			8: \
			movl %eax,AVECNAME(x); \
			jmpl *%eax
#endif /* PIC */

#define	ARCH_VECTOR(x)	.data; .p2align 2; \
			.globl AVECNAME(x); \
			.type AVECNAME(x),@object; \
			.size AVECNAME(x),4; \
			AVECNAME(x): .long ASELNAME(x)

#undef _ENTRY
#define	_ENTRY(x)	ARCH_VECTOR(x); ARCH_SELECT(x); ARCH_DISPATCH(x); \
			_START_ENTRY; \
			.globl ANAME(x); .type ANAME(x),@function; ANAME(x):

#endif /* _ARCH_INDIRECT */

#endif /* !_MACHINE_ASM_H_ */
