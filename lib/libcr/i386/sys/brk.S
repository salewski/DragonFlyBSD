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
 * $FreeBSD: src/lib/libc/i386/sys/brk.S,v 1.7 1999/08/27 23:59:38 peter Exp $
 * $DragonFly: src/lib/libcr/i386/sys/Attic/brk.S,v 1.3 2004/04/25 12:40:50 joerg Exp $
 */

#include "SYS.h"

	.globl	HIDENAME(curbrk)
	.globl	HIDENAME(minbrk)
ENTRY(_brk)
	jmp	ok

ENTRY(brk)
#ifdef PIC
	movl	4(%esp),%eax
	PIC_PROLOGUE
	movl	PIC_GOT(HIDENAME(curbrk)),%edx	# set up GOT addressing
	movl	PIC_GOT(HIDENAME(minbrk)),%ecx	#
	PIC_EPILOGUE
	cmpl	%eax,(%ecx)
	jbe	ok
	movl	(%ecx),%eax
	movl	%eax,4(%esp)
ok:
	lea	SYS_break,%eax
	KERNCALL
	jb	err
	movl	4(%esp),%eax
	movl	%eax,(%edx)
	movl	$0,%eax
	ret
err:
	PIC_PROLOGUE
	jmp	PIC_PLT(HIDENAME(cerror))

#else

	movl	4(%esp),%eax
	cmpl	%eax,HIDENAME(minbrk)
	jbe	ok
	movl	HIDENAME(minbrk),%eax
	movl	%eax,4(%esp)
ok:
	lea	SYS_break,%eax
	KERNCALL
	jb	err
	movl	4(%esp),%eax
	movl	%eax,HIDENAME(curbrk)
	movl	$0,%eax
	ret
err:
	jmp	HIDENAME(cerror)
#endif
