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
 *	@(#)SYS.h	5.5 (Berkeley) 5/7/91
 * $FreeBSD: src/lib/libc/amd64/SYS.h,v 1.26 2003/05/24 17:35:23 peter Exp $
 * $DragonFly: src/lib/libcr/amd64/Attic/SYS.h,v 1.1 2004/03/13 19:46:54 eirikn Exp $
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#ifdef PIC
#define	SYSCALL(x)	2: movq PIC_GOT(HIDENAME(cerror)),%rcx;		\
			jmp *%rcx;					\
			ENTRY(__CONCAT(__sys_,x));			\
			.weak CNAME(x);					\
			.set CNAME(x),CNAME(__CONCAT(__sys_,x));	\
			.weak CNAME(__CONCAT(_,x));			\
			.set CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x)); \
			mov __CONCAT($SYS_,x),%rax; KERNCALL; jb 2b
#else
#define	SYSCALL(x)	2: jmp HIDENAME(cerror);			\
			ENTRY(__CONCAT(__sys_,x));			\
			.weak CNAME(x);					\
			.set CNAME(x),CNAME(__CONCAT(__sys_,x));	\
			.weak CNAME(__CONCAT(_,x));			\
			.set CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x)); \
			mov __CONCAT($SYS_,x),%rax; KERNCALL; jb 2b
#endif

#define	RSYSCALL(x)	SYSCALL(x); ret

#define	PSEUDO(x,y)	ENTRY(__CONCAT(__sys_,x));			\
			.weak CNAME(__CONCAT(_,x));			\
			.set CNAME(__CONCAT(_,x)),CNAME(__CONCAT(__sys_,x)); \
			mov __CONCAT($SYS_,x),%rax; KERNCALL; ret

#define KERNCALL	movq %rcx, %r10; syscall
