/*
 * Copyright (c) 1996 Jeffrey Hsu <hsu@freebsd.org>.
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
 *	This product includes software developed by Jeffrey Hsu.
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_mattr_kind_np.c,v 1.4.2.2 2002/10/22 14:44:03 fjoe Exp $
 * $DragonFly: src/lib/libc_r/uthread/uthread_mattr_kind_np.c,v 1.3 2005/01/08 03:27:21 hsu Exp $
 */
#include <errno.h>
#include <pthread.h>
#include "pthread_private.h"

__weak_reference(_pthread_mutexattr_setkind_np, pthread_mutexattr_setkind_np);
__weak_reference(_pthread_mutexattr_getkind_np, pthread_mutexattr_getkind_np);
__weak_reference(_pthread_mutexattr_gettype, pthread_mutexattr_gettype);
__weak_reference(_pthread_mutexattr_settype, pthread_mutexattr_settype);

int
_pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind)
{
	int	ret;
	if (attr == NULL || *attr == NULL) {
		errno = EINVAL;
		ret = -1;
	} else {
		(*attr)->m_type = kind;
		ret = 0;
	}
	return(ret);
}

int
_pthread_mutexattr_getkind_np(pthread_mutexattr_t attr)
{
	int	ret;
	if (attr == NULL) {
		errno = EINVAL;
		ret = -1;
	} else {
		ret = attr->m_type;
	}
	return(ret);
}

int
_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	int	ret;
	if (attr == NULL || *attr == NULL || type >= MUTEX_TYPE_MAX) {
		errno = EINVAL;
		ret = -1;
	} else {
		(*attr)->m_type = type;
		ret = 0;
	}
	return(ret);
}

int
_pthread_mutexattr_gettype(pthread_mutexattr_t *attr, int *type)
{
	int	ret;

	if (attr == NULL || *attr == NULL || (*attr)->m_type >=
	    MUTEX_TYPE_MAX) {
		ret = EINVAL;
	} else {
		*type = (*attr)->m_type;
		ret = 0;
	}
	return ret;
}
