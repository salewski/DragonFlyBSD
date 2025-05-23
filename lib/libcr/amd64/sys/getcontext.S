/*-
 * Copyright (c) 2003 Peter Wemm <peter@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/amd64/sys/getcontext.S,v 1.2 2003/09/04 00:31:45 peter Exp $
 * $DragonFly: src/lib/libcr/amd64/sys/Attic/getcontext.S,v 1.1 2004/03/13 19:46:55 eirikn Exp $
 */

#include <machine/asm.h>
#include <SYS.h>

/*
 * This has to be magic to handle the multiple returns.
 * Otherwise, the setcontext() syscall will return here and we'll
 * pop off the return address and go to the *setcontext* call.
 */
	.weak	_getcontext
	.set	_getcontext,__sys_getcontext
	.weak	getcontext
	.set	getcontext,__sys_getcontext
ENTRY(__sys_getcontext)
	movq	(%rsp),%rsi	/* save getcontext return address */
	mov	$SYS_getcontext,%rax
	KERNCALL
	jb	1f
	addq	$8,%rsp		/* remove stale (setcontext) return address */
	jmp	*%rsi		/* restore return address */
1:
#ifdef PIC
	movq	PIC_GOT(HIDENAME(cerror)),%rdx
	jmp	*%rdx
#else
	jmp	HIDENAME(cerror)
#endif
