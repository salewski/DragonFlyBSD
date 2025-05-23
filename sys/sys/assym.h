/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD: src/sys/sys/assym.h,v 1.3.2.1 2000/07/07 01:31:16 obrien Exp $
 * $DragonFly: src/sys/sys/assym.h,v 1.3 2004/12/30 07:01:52 cpressey Exp $
 */

#ifndef _SYS_ASSYM_H_
#define	_SYS_ASSYM_H_

#if !defined(_KERNEL) && !defined(_KERNEL_STRUCTURES)
#error "This file should not be included by userland programs."
#endif

#define	ASSYM_ABS(value)	((value) < 0 ? -((value) + 1) + 1ULL : (value))

#define	ASSYM(name, value)					\
char name ## sign[(value) < 0 ? 1 : 0];				\
char name ## w0[ASSYM_ABS(value) & 0xFFFFU];			\
char name ## w1[(ASSYM_ABS(value) & 0xFFFF0000UL) >> 16];	\
char name ## w2[(ASSYM_ABS(value) & 0xFFFF00000000ULL) >> 32];	\
char name ## w3[(ASSYM_ABS(value) & 0xFFFF000000000000ULL) >> 48]

#endif /* !_SYS_ASSYM_H_ */
