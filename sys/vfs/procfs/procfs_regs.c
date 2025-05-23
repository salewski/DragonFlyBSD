/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_regs.c	8.4 (Berkeley) 6/15/94
 *
 * From:
 * $FreeBSD: src/sys/miscfs/procfs/procfs_regs.c,v 1.10.2.3 2002/01/22 17:22:59 nectar Exp $
 * $DragonFly: src/sys/vfs/procfs/procfs_regs.c,v 1.7 2004/05/02 03:05:11 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <machine/reg.h>
#include <vfs/procfs/procfs.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

int
procfs_doregs(struct proc *curp, struct proc *p, struct pfsnode *pfs,
	      struct uio *uio)
{
	int error;
	struct reg r;
	char *kv;
	int kl;

	/* Can't trace a process that's currently exec'ing. */ 
	if ((p->p_flag & P_INEXEC) != 0)
		return EAGAIN;
	if (!CHECKIO(curp, p) || p_trespass(curp->p_ucred, p->p_ucred))
		return EPERM;
	kl = sizeof(r);
	kv = (char *) &r;

	kv += uio->uio_offset;
	kl -= uio->uio_offset;
	if (kl > uio->uio_resid)
		kl = uio->uio_resid;

	PHOLD(p);

		error = procfs_read_regs(p, &r);
	if (error == 0)
		error = uiomove_frombuf(&r, sizeof(r), uio);
	if (error == 0 && uio->uio_rw == UIO_WRITE) {
		if (p->p_stat != SSTOP)
			error = EBUSY;
		else
			error = procfs_write_regs(p, &r);
	}
	PRELE(p);

	uio->uio_offset = 0;
	return (error);
}

int
procfs_validregs(struct proc *p)
{
	return ((p->p_flag & P_SYSTEM) == 0);
}
