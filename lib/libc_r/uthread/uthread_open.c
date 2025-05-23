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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_open.c,v 1.9.2.2 2002/10/22 14:44:03 fjoe Exp $
 * $DragonFly: src/lib/libc_r/uthread/uthread_open.c,v 1.2 2003/06/17 04:26:48 dillon Exp $
 *
 */
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include "pthread_private.h"

int
_open(const char *path, int flags,...)
{
	int	fd;
	int	mode = 0;
	va_list	ap;

	/* Check if the file is being created: */
	if (flags & O_CREAT) {
		/* Get the creation mode: */
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	/* Open the file: */
	if ((fd = __sys_open(path, flags, mode)) < 0) {
	}
	/* Initialise the file descriptor table entry: */
	else if (_thread_fd_table_init(fd) != 0) {
		/* Quietly close the file: */
		__sys_close(fd);

		/* Reset the file descriptor: */
		fd = -1;
	}

	/* Return the file descriptor or -1 on error: */
	return (fd);
}

int
open(const char *path, int flags,...)
{
	int	ret;
	int	mode = 0;
	va_list	ap;

	_thread_enter_cancellation_point();
	
	/* Check if the file is being created: */
	if (flags & O_CREAT) {
		/* Get the creation mode: */
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	
	ret = _open(path, flags, mode);
	_thread_leave_cancellation_point();

	return ret;
}
