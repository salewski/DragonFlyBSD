/*
 * Copyright (c) 1982, 1986, 1993, 1994
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
 *	@(#)uio.h	8.5 (Berkeley) 2/22/94
 * $FreeBSD: src/sys/sys/uio.h,v 1.11.2.1 2001/09/28 16:58:35 dillon Exp $
 * $DragonFly: src/sys/sys/uio.h,v 1.9 2004/07/27 13:11:22 hmp Exp $
 */

#ifndef _SYS_UIO_H_
#define	_SYS_UIO_H_

#include <sys/malloc.h>		/* Needed to inline iovec_free(). */

/*
 * XXX
 * iov_base should be a void *.
 */
struct iovec {
	char	*iov_base;	/* Base address. */
	size_t	 iov_len;	/* Length. */
};

enum	uio_rw { UIO_READ, UIO_WRITE };

/* Segment flag values. */
enum uio_seg {
	UIO_USERSPACE,		/* from user data space */
	UIO_SYSSPACE,		/* from system space */
	UIO_NOCOPY		/* don't copy, already in object */
};

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

/*
 * uio_td is primarily used for USERSPACE transfers, but some devices
 * like ttys may also use it to get at the process.
 */
struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_resid;
	enum	uio_seg uio_segflg;
	enum	uio_rw uio_rw;
	struct	thread *uio_td;
};

/*
 * Limits
 */
#define UIO_MAXIOV	1024		/* max 1K of iov's */
#define UIO_SMALLIOV	8		/* 8 on stack, else malloc */

#endif

#if defined(_KERNEL)

struct vm_object;

void	uio_yield (void);
int	uiomove (caddr_t, int, struct uio *);
int 	uiomove_frombuf (void *buf, int buflen, struct uio *uio);
int     uiomove_fromphys(struct vm_page *ma[], vm_offset_t offset, int n,
			    struct uio *uio);
int	uiomoveco (caddr_t, int, struct uio *, struct vm_object *);
int	uioread (int, struct uio *, struct vm_object *, int *);
int	iovec_copyin(struct iovec *, struct iovec **, struct iovec *,
	size_t, size_t *);

static __inline void
iovec_free(struct iovec **kiov, struct iovec *siov)
{
	if (*kiov != siov) {
		FREE(*kiov, M_IOV);
		*kiov = NULL;
	}
}

#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
ssize_t	readv (int, const struct iovec *, int);
ssize_t	writev (int, const struct iovec *, int);
__END_DECLS

#endif /* _KERNEL */

#endif /* !_SYS_UIO_H_ */
