/*
 * Copyright (c) 2004 Alan L. Cox <alc@cs.rice.edu>
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * @(#)kern_subr.c	8.3 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/i386/i386/uio_machdep.c,v 1.1 2004/03/21 20:28:36 alc Exp $
 * $DragonFly: src/sys/i386/i386/Attic/uio_machdep.c,v 1.8 2005/03/02 18:42:08 hmp Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sfbuf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/thread2.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

/*
 * Implement uiomove(9) from physical memory using sf_bufs to reduce
 * the creation and destruction of ephemeral mappings.
 */
int
uiomove_fromphys(vm_page_t *ma, vm_offset_t offset, int n, struct uio *uio)
{
	struct sf_buf *sf;
	struct thread *td = curthread;
	struct iovec *iov;
	void *cp;
	vm_offset_t page_offset;
	vm_page_t m;
	size_t cnt;
	int error = 0;
	int save = 0;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove_fromphys: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomove_fromphys proc"));

	crit_enter();
	save = td->td_flags & TDF_DEADLKTREAT;
	td->td_flags |= TDF_DEADLKTREAT;
	crit_exit();

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;
		page_offset = offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - page_offset);
		m = ma[offset >> PAGE_SHIFT];
		sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
		cp = (char *)sf_buf_kva(sf) + page_offset;
		switch (uio->uio_segflg) {
		case UIO_USERSPACE:
			/*
			 * note: removed uioyield (it was the wrong place to
			 * put it).
			 */
			if (uio->uio_rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error) {
				sf_buf_free(sf);
				goto out;
			}
			break;
		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		case UIO_NOCOPY:
			break;
		}
		sf_buf_free(sf);
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		offset += cnt;
		n -= cnt;
	}
out:
	if (save == 0) {
		crit_enter();
		td->td_flags &= ~TDF_DEADLKTREAT;
		crit_exit();
	}
	return (error);
}
