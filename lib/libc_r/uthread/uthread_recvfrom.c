/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_recvfrom.c,v 1.9.2.5 2003/01/17 07:56:43 ru Exp $
 * $DragonFly: src/lib/libc_r/uthread/uthread_recvfrom.c,v 1.2 2003/06/17 04:26:48 dillon Exp $
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include "pthread_private.h"

ssize_t
_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr * from,
    socklen_t *from_len)
{
	struct pthread	*curthread = _get_curthread();
	int             ret;

	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
		while ((ret = __sys_recvfrom(fd, buf, len, flags, from, from_len)) < 0) {
			if (((_thread_fd_getflags(fd) & O_NONBLOCK) == 0)
			    && ((errno == EWOULDBLOCK) || (errno == EAGAIN))) {
				curthread->data.fd.fd = fd;

				/* Set the timeout: */
				_thread_kern_set_timeout(NULL);
				curthread->interrupted = 0;
				_thread_kern_sched_state(PS_FDR_WAIT, __FILE__, __LINE__);

				/* Check if the wait was interrupted: */
				if (curthread->interrupted) {
					/* Return an error status: */
					errno = EINTR;
					ret = -1;
					break;
				}
			} else {
				ret = -1;
				break;
			}
		}
		_FD_UNLOCK(fd, FD_READ);
	}
	return (ret);
}

ssize_t
recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr * from,
    socklen_t *from_len)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = _recvfrom(fd, buf, len, flags, from, from_len);
	_thread_leave_cancellation_point();

	return (ret);
}
