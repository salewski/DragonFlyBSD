/*-
 * Copyright (c) 1999 Peter Jeremy
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
 * $FreeBSD: src/sys/i386/i386/atomic.c,v 1.3 1999/08/28 00:43:40 peter Exp $
 * $DragonFly: src/sys/i386/i386/Attic/atomic.c,v 1.2 2003/06/17 04:28:35 dillon Exp $
 */

/* This file creates publically callable functions to perform various
 * simple arithmetic on memory which is atomic in the presence of
 * interrupts and multiple processors.
 */
#include <sys/types.h>

/* Firstly make atomic.h generate prototypes as it will for kernel modules */
#define KLD_MODULE
#include <machine/atomic.h>
#undef _MACHINE_ATOMIC_H_	/* forget we included it */
#undef KLD_MODULE
#undef ATOMIC_ASM

/* Make atomic.h generate public functions */
#define static
#undef __inline
#define __inline

#include <machine/atomic.h>
