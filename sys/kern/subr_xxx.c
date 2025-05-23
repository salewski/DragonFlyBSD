/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/kern/subr_xxx.c,v 1.15.2.1 2001/02/26 04:23:16 jlemon Exp $
 * $DragonFly: src/sys/kern/Attic/subr_xxx.c,v 1.4 2004/05/19 22:52:58 dillon Exp $
 */

/*
 * Miscellaneous trivial functions.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>

/*
 * Return error for operation not supported
 * on a specific object or file type.
 */
int
eopnotsupp(void)
{
	return (EOPNOTSUPP);
}

/*
 * Return error for an inval operation
 * on a specific object or file type.
 */
int
einval(void)
{
	return (EINVAL);
}

/*
 * Generic null operation, always returns success.
 */
int
nullop(void)
{
	return (0);
}

#include <sys/conf.h>

/*
 * Unsupported devswitch functions (e.g. for writing to read-only device).
 * XXX may belong elsewhere.
 */

int
noclone(dev_t dev)
{
	/* take no action */
	return (0);	/* allow the clone */
}

int
noopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	return (ENODEV);
}

int
noclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	return (ENODEV);
}

int
noread(dev_t dev, struct uio *uio, int ioflag)
{
	return (ENODEV);
}

int
nowrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (ENODEV);
}

int
noioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	return (ENODEV);
}

int
nokqfilter(dev_t dev, struct knote *kn)
{
	return (ENODEV);
}

int
nommap(dev_t dev, vm_offset_t offset, int nprot)
{
	/* Don't return ENODEV.  That would allow mapping address ENODEV! */
	return (-1);
}

int
nopoll(dev_t dev, int events, d_thread_t *td)
{
	return(0);
}

void
nostrategy(struct buf *bp)
{
	bp->b_flags |= B_ERROR;
	bp->b_error = EOPNOTSUPP;
	biodone(bp);
}

int
nopsize(dev_t dev)
{
	return(0);
}

int
nodump(dev_t dev, u_int count, u_int blkno, u_int secsize) 
{
	return (ENODEV);
}

/*
 * Null devswitch functions (for when the operation always succeeds).
 * XXX may belong elsewhere.
 * XXX not all are here (e.g., seltrue() isn't).
 */

/*
 * XXX this is probably bogus.  Any device that uses it isn't checking the
 * minor number.
 */
int
nullopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

int
nullclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

