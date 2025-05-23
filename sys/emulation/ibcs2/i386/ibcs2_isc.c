/*-
 * Copyright (c) 1994 S�ren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1995 Steven Wallace
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
 * $FreeBSD: src/sys/i386/ibcs2/ibcs2_isc.c,v 1.12 1999/08/28 00:43:58 peter Exp $
 * $DragonFly: src/sys/emulation/ibcs2/i386/Attic/ibcs2_isc.c,v 1.4 2003/08/07 21:17:17 dillon Exp $
 */

#include <sys/param.h>
#include <sys/sysent.h>
#include <sys/proc.h>

#include <machine/cpu.h>

#include "ibcs2_types.h"
#include "ibcs2_signal.h"
#include "ibcs2_proto.h"
#include "ibcs2_isc_syscall.h"

extern struct sysent isc_sysent[];

int
ibcs2_isc(struct ibcs2_isc_args *uap)
{
	struct proc *p = curproc;
	struct trapframe *tf = p->p_md.md_regs;
        struct sysent *callp;
        u_int code;             

	code = (tf->tf_eax & 0xffffff00) >> 8;
	callp = &isc_sysent[code];

	if(code < IBCS2_ISC_MAXSYSCALL)
	  return((*callp->sy_call)((void *)uap));
	else
	  return ENOSYS;
}
