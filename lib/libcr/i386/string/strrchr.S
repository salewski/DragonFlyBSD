/*
 * Copyright (c) 1993 Winning Strategies, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/i386/string/strrchr.S,v 1.5 1999/08/27 23:59:35 peter Exp $
 * $DragonFly: src/lib/libcr/i386/string/Attic/strrchr.S,v 1.3 2004/04/25 12:40:49 joerg Exp $
 */

#include "DEFS.h"

/*
 * strrchr(s, c)
 *	return a pointer to the last occurance of the character c in
 *	string s, or NULL if c does not occur in the string.
 *
 * %edx - pointer iterating through string
 * %eax - pointer to last occurance of 'c'
 * %cl  - character we're comparing against
 * %bl  - character at %edx
 *
 * Written by:
 *	J.T. Conklin (jtc@wimsey.com), Winning Strategies, Inc.
 */

ENTRY(strrchr)
	pushl	%ebx
	movl	8(%esp),%edx
	movb	12(%esp),%cl
	xorl	%eax,%eax		/* init pointer to null */
	.align 2,0x90
L1:
	movb	(%edx),%bl
	cmpb	%bl,%cl
	jne	L2
	movl	%edx,%eax
L2:	
	incl	%edx
	testb	%bl,%bl			/* null terminator??? */
	jne	L1
	popl	%ebx
	ret
