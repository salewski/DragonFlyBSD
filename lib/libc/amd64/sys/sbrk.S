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
 * @(#)sbrk.s	5.1 (Berkeley) 4/23/90
 * $FreeBSD: src/lib/libc/amd64/sys/sbrk.S,v 1.13 2003/05/24 17:35:23 peter Exp $
 * $DragonFly: src/lib/libc/amd64/sys/sbrk.S,v 1.1 2004/02/02 05:43:14 dillon Exp $
 */

#include <machine/asm.h>
#include "SYS.h"

	.globl	CNAME(_end)
	.globl	HIDENAME(minbrk)
	.globl	HIDENAME(curbrk)

	.data
HIDENAME(minbrk):	.quad	CNAME(_end)
HIDENAME(curbrk):	.quad	CNAME(_end)
	.text

ENTRY(sbrk)
	pushq	%rdi
	movq	%rdi,%rcx
#ifdef PIC
	movq	PIC_GOT(HIDENAME(curbrk)),%rdx
	movq	(%rdx),%rax
#else
	movq	HIDENAME(curbrk)(%rip),%rax
#endif
	testq	%rcx,%rcx
	jz	back
	addq	%rax,%rdi
	mov	$SYS_break,%eax
	KERNCALL
	jb	err
#ifdef PIC
	movq	PIC_GOT(HIDENAME(curbrk)),%rdx
	movq	(%rdx),%rax
#else
	movq	HIDENAME(curbrk)(%rip),%rax
#endif
	movq	0(%rsp), %rcx
#ifdef PIC
	addq	%rcx,(%rdx)
#else
	addq	%rcx,HIDENAME(curbrk)(%rip)
#endif
back:
	addq	$8, %rsp
	ret
err:
	addq	$8, %rsp
#ifdef PIC
	movq	PIC_GOT(HIDENAME(cerror)),%rdx
	jmp	*%rdx
#else
	jmp	HIDENAME(cerror)
#endif
