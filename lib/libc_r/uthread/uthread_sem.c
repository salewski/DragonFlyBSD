/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sem.c,v 1.3.2.5 2002/10/22 14:44:03 fjoe Exp $
 * $DragonFly: src/lib/libc_r/uthread/uthread_sem.c,v 1.2 2003/06/17 04:26:48 dillon Exp $
 */

#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include "pthread_private.h"

#define _SEM_CHECK_VALIDITY(sem)		\
	if ((*(sem))->magic != SEM_MAGIC) {	\
		errno = EINVAL;			\
		retval = -1;			\
		goto RETURN;			\
	}

__weak_reference(_sem_init, sem_init);
__weak_reference(_sem_destroy, sem_destroy);
__weak_reference(_sem_open, sem_open);
__weak_reference(_sem_close, sem_close);
__weak_reference(_sem_unlink, sem_unlink);
__weak_reference(_sem_wait, sem_wait);
__weak_reference(_sem_trywait, sem_trywait);
__weak_reference(_sem_post, sem_post);
__weak_reference(_sem_getvalue, sem_getvalue);

int
_sem_init(sem_t *sem, int pshared, unsigned int value)
{
	int	retval;

	/*
	 * Range check the arguments.
	 */
	if (pshared != 0) {
		/*
		 * The user wants a semaphore that can be shared among
		 * processes, which this implementation can't do.  Sounds like a
		 * permissions problem to me (yeah right).
		 */
		errno = EPERM;
		retval = -1;
		goto RETURN;
	}

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		retval = -1;
		goto RETURN;
	}

	*sem = (sem_t)malloc(sizeof(struct sem));
	if (*sem == NULL) {
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	/*
	 * Initialize the semaphore.
	 */
	if (pthread_mutex_init(&(*sem)->lock, NULL) != 0) {
		free(*sem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	if (pthread_cond_init(&(*sem)->gtzero, NULL) != 0) {
		pthread_mutex_destroy(&(*sem)->lock);
		free(*sem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}
	
	(*sem)->count = (u_int32_t)value;
	(*sem)->nwaiters = 0;
	(*sem)->magic = SEM_MAGIC;

	retval = 0;
  RETURN:
	return retval;
}

int
_sem_destroy(sem_t *sem)
{
	int	retval;
	
	_SEM_CHECK_VALIDITY(sem);

	/* Make sure there are no waiters. */
	pthread_mutex_lock(&(*sem)->lock);
	if ((*sem)->nwaiters > 0) {
		pthread_mutex_unlock(&(*sem)->lock);
		errno = EBUSY;
		retval = -1;
		goto RETURN;
	}
	pthread_mutex_unlock(&(*sem)->lock);
	
	pthread_mutex_destroy(&(*sem)->lock);
	pthread_cond_destroy(&(*sem)->gtzero);
	(*sem)->magic = 0;

	free(*sem);

	retval = 0;
  RETURN:
	return retval;
}

sem_t *
_sem_open(const char *name, int oflag, ...)
{
	errno = ENOSYS;
	return SEM_FAILED;
}

int
_sem_close(sem_t *sem)
{
	errno = ENOSYS;
	return -1;
}

int
_sem_unlink(const char *name)
{
	errno = ENOSYS;
	return -1;
}

int
_sem_wait(sem_t *sem)
{
	int	retval;

	_thread_enter_cancellation_point();
	
	_SEM_CHECK_VALIDITY(sem);

	pthread_mutex_lock(&(*sem)->lock);

	while ((*sem)->count == 0) {
		(*sem)->nwaiters++;
		pthread_cond_wait(&(*sem)->gtzero, &(*sem)->lock);
		(*sem)->nwaiters--;
	}
	(*sem)->count--;

	pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
  RETURN:
	_thread_leave_cancellation_point();
	return retval;
}

int
_sem_trywait(sem_t *sem)
{
	int	retval;

	_SEM_CHECK_VALIDITY(sem);

	pthread_mutex_lock(&(*sem)->lock);

	if ((*sem)->count > 0) {
		(*sem)->count--;
		retval = 0;
	} else {
		errno = EAGAIN;
		retval = -1;
	}
	
	pthread_mutex_unlock(&(*sem)->lock);

  RETURN:
	return retval;
}

int
_sem_post(sem_t *sem)
{
	int	retval;

	_SEM_CHECK_VALIDITY(sem);

	/*
	 * sem_post() is required to be safe to call from within signal
	 * handlers.  Thus, we must defer signals.
	 */
	_thread_kern_sig_defer();

	pthread_mutex_lock(&(*sem)->lock);

	(*sem)->count++;
	if ((*sem)->nwaiters > 0)
		pthread_cond_signal(&(*sem)->gtzero);

	pthread_mutex_unlock(&(*sem)->lock);

	_thread_kern_sig_undefer();
	retval = 0;
  RETURN:
	return retval;
}

int
_sem_getvalue(sem_t *sem, int *sval)
{
	int	retval;

	_SEM_CHECK_VALIDITY(sem);

	pthread_mutex_lock(&(*sem)->lock);
	*sval = (int)(*sem)->count;
	pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
  RETURN:
	return retval;
}
