/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_file.c,v 1.12.2.3 2002/10/22 14:44:03 fjoe Exp $
 * $DragonFly: src/lib/libc_r/uthread/uthread_file.c,v 1.3 2004/01/23 11:30:28 joerg Exp $
 *
 * POSIX stdio FILE locking functions. These assume that the locking
 * is only required at FILE structure level, not at file descriptor
 * level too.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>
#include "pthread_private.h"

/*
 * Weak symbols for externally visible functions in this file:
 */
#pragma	weak	flockfile=_flockfile
#pragma	weak	ftrylockfile=_ftrylockfile
#pragma	weak	funlockfile=_funlockfile

void	flockfile(FILE *);
int	ftrylockfile(FILE *);
void 	funlockfile(FILE *);

/*
 * The FILE lock structure. The FILE *fp is locked if the owner is
 * not NULL. If not locked, the file lock structure can be
 * reassigned to a different file by setting fp.
 */
struct	file_lock {
	LIST_ENTRY(file_lock)	entry;	/* Entry if file list.       */
	TAILQ_HEAD(lock_head, pthread)
				l_head;	/* Head of queue for threads */
					/* waiting on this lock.     */
	FILE		*fp;		/* The target file.          */
	struct pthread	*owner;		/* Thread that owns lock.    */
	int		count;		/* Lock count for owner.     */
};

/*
 * The number of file lock lists into which the file pointer is
 * hashed. Ideally, the FILE structure size would have been increased,
 * but this causes incompatibility, so separate data structures are
 * required.
 */
#define NUM_HEADS	128

/*
 * This macro casts a file pointer to a long integer and right
 * shifts this by the number of bytes in a pointer. The shifted
 * value is then remaindered using the maximum number of hash
 * entries to produce and index into the array of static lock
 * structures. If there is a collision, a linear search of the
 * dynamic list of locks linked to each static lock is perfomed.
 */
#define file_idx(_p)	((((u_long) _p) >> sizeof(void *)) % NUM_HEADS)

/*
 * Global array of file locks. The first lock for each hash bucket is
 * allocated statically in the hope that there won't be too many
 * collisions that require a malloc and an element added to the list.
 */
struct static_file_lock {
	LIST_HEAD(file_list_head, file_lock) head;
	struct	file_lock	fl;
} flh[NUM_HEADS];

/* Set to non-zero when initialisation is complete: */
static	int	init_done	= 0;

/* Lock for accesses to the hash table: */
static	spinlock_t	hash_lock	= _SPINLOCK_INITIALIZER;

/*
 * Find a lock structure for a FILE, return NULL if the file is
 * not locked:
 */
static
struct file_lock *
find_lock(int idx, FILE *fp)
{
	struct file_lock *p;

	/* Check if the file is locked using the static structure: */
	if (flh[idx].fl.fp == fp && flh[idx].fl.owner != NULL)
		/* Return a pointer to the static lock: */
		p = &flh[idx].fl;
	else {
		/* Point to the first dynamic lock: */
		p = flh[idx].head.lh_first;

		/*
		 * Loop through the dynamic locks looking for the
		 * target file:
		 */
		while (p != NULL && (p->fp != fp || p->owner == NULL))
			/* Not this file, try the next: */
			p = p->entry.le_next;
	}
	return(p);
}

/*
 * Lock a file, assuming that there is no lock structure currently
 * assigned to it.
 */
static
struct file_lock *
do_lock(int idx, FILE *fp)
{
	struct pthread	*curthread = _get_curthread();
	struct file_lock *p;

	/* Check if the static structure is not being used: */
	if (flh[idx].fl.owner == NULL) {
		/* Return a pointer to the static lock: */
		p = &flh[idx].fl;
	}
	else {
		/* Point to the first dynamic lock: */
		p = flh[idx].head.lh_first;

		/*
		 * Loop through the dynamic locks looking for a
		 * lock structure that is not being used:
		 */
		while (p != NULL && p->owner != NULL)
			/* This one is used, try the next: */
			p = p->entry.le_next;
	}

	/*
	 * If an existing lock structure has not been found,
	 * allocate memory for a new one:
	 */
	if (p == NULL && (p = (struct file_lock *)
	    malloc(sizeof(struct file_lock))) != NULL) {
		/* Add the new element to the list: */
		LIST_INSERT_HEAD(&flh[idx].head, p, entry);
	}

	/* Check if there is a lock structure to acquire: */
	if (p != NULL) {
		/* Acquire the lock for the running thread: */
		p->fp		= fp;
		p->owner	= curthread;
		p->count	= 1;
		TAILQ_INIT(&p->l_head);
	}
	return(p);
}

void
_flockfile_debug(FILE * fp, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();
	int	idx = file_idx(fp);
	struct	file_lock	*p;

	/* Check if this is a real file: */
	if (fp->_file >= 0) {
		/* Lock the hash table: */
		_SPINLOCK(&hash_lock);

		/* Check if the static array has not been initialised: */
		if (!init_done) {
			/* Initialise the global array: */
			memset(flh,0,sizeof(flh));

			/* Flag the initialisation as complete: */
			init_done = 1;
		}

		/* Get a pointer to any existing lock for the file: */
		if ((p = find_lock(idx, fp)) == NULL) {
			/*
			 * The file is not locked, so this thread can
			 * grab the lock:
			 */
			p = do_lock(idx, fp);

			/* Unlock the hash table: */
			_SPINUNLOCK(&hash_lock);

		/*
		 * The file is already locked, so check if the
		 * running thread is the owner:
		 */
		} else if (p->owner == curthread) {
			/*
			 * The running thread is already the
			 * owner, so increment the count of
			 * the number of times it has locked
			 * the file:
			 */
			p->count++;

			/* Unlock the hash table: */
			_SPINUNLOCK(&hash_lock);
		} else {
			/* Clear the interrupted flag: */
			curthread->interrupted = 0;

			/*
			 * Prevent being context switched out while
			 * adding this thread to the file lock queue.
			 */
			_thread_kern_sig_defer();

			/*
			 * The file is locked for another thread.
			 * Append this thread to the queue of
			 * threads waiting on the lock.
			 */
			TAILQ_INSERT_TAIL(&p->l_head,curthread,qe);
			curthread->flags |= PTHREAD_FLAGS_IN_FILEQ;

			/* Unlock the hash table: */
			_SPINUNLOCK(&hash_lock);

			curthread->data.fp = fp;

			/* Wait on the FILE lock: */
			_thread_kern_sched_state(PS_FILE_WAIT, fname, lineno);

			if ((curthread->flags & PTHREAD_FLAGS_IN_FILEQ) != 0) {
				TAILQ_REMOVE(&p->l_head,curthread,qe);
				curthread->flags &= ~PTHREAD_FLAGS_IN_FILEQ;
			}

			_thread_kern_sig_undefer();

			if (curthread->interrupted != 0 &&
			    curthread->continuation != NULL)
				curthread->continuation((void *)curthread);
		}
	}
}

void
_flockfile(FILE * fp)
{
	_flockfile_debug(fp, __FILE__, __LINE__);
}

int
_ftrylockfile(FILE * fp)
{
	struct pthread	*curthread = _get_curthread();
	int	ret = -1;
	int	idx = file_idx(fp);
	struct	file_lock	*p;

	/* Check if this is a real file: */
	if (fp->_file >= 0) {
		/* Lock the hash table: */
		_SPINLOCK(&hash_lock);

		/* Get a pointer to any existing lock for the file: */
		if ((p = find_lock(idx, fp)) == NULL) {
			/*
			 * The file is not locked, so this thread can
			 * grab the lock:
			 */
			p = do_lock(idx, fp);

		/*
		 * The file is already locked, so check if the
		 * running thread is the owner:
		 */
		} else if (p->owner == curthread) {
			/*
			 * The running thread is already the
			 * owner, so increment the count of
			 * the number of times it has locked
			 * the file:
			 */
			p->count++;
		} else {
			/*
			 * The file is locked for another thread,
			 * so this try fails.
			 */
			p = NULL;
		}

		/* Check if the lock was obtained: */
		if (p != NULL)
			/* Return success: */
			ret = 0;

		/* Unlock the hash table: */
		_SPINUNLOCK(&hash_lock);

	}
	return (ret);
}

void 
_funlockfile(FILE * fp)
{
	struct pthread	*curthread = _get_curthread();
	int	idx = file_idx(fp);
	struct	file_lock	*p;

	/* Check if this is a real file: */
	if (fp->_file >= 0) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/* Lock the hash table: */
		_SPINLOCK(&hash_lock);

		/*
		 * Get a pointer to the lock for the file and check that
		 * the running thread is the one with the lock:
		 */
		if ((p = find_lock(idx, fp)) != NULL &&
		    p->owner == curthread) {
			/*
			 * Check if this thread has locked the FILE
			 * more than once:
			 */
			if (p->count > 1)
				/*
				 * Decrement the count of the number of
				 * times the running thread has locked this
				 * file:
				 */
				p->count--;
			else {
				/*
				 * The running thread will release the
				 * lock now:
				 */
				p->count = 0;

				/* Get the new owner of the lock: */
				while ((p->owner = TAILQ_FIRST(&p->l_head)) != NULL) {
					/* Pop the thread off the queue: */
					TAILQ_REMOVE(&p->l_head,p->owner,qe);
					p->owner->flags &= ~PTHREAD_FLAGS_IN_FILEQ;

					if (p->owner->interrupted == 0) {
						/*
						 * This is the first lock for
						 * the new owner:
						 */
						p->count = 1;

						/* Allow the new owner to run: */
						PTHREAD_NEW_STATE(p->owner,PS_RUNNING);

						/* End the loop when we find a
						 * thread that hasn't been
						 * cancelled or interrupted;
						 */
						break;
					}
				}
			}
		}

		/* Unlock the hash table: */
		_SPINUNLOCK(&hash_lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
}

void
_funlock_owned(struct pthread *pthread)
{
	int			idx;
	struct file_lock	*p, *next_p;

	/*
	 * Defer signals to protect the scheduling queues from
	 * access by the signal handler:
	 */
	_thread_kern_sig_defer();

	/* Lock the hash table: */
	_SPINLOCK(&hash_lock);

	for (idx = 0; idx < NUM_HEADS; idx++) {
		/* Check the static file lock first: */
		p = &flh[idx].fl;
		next_p = LIST_FIRST(&flh[idx].head);

		while (p != NULL) {
			if (p->owner == pthread) {
				/*
				 * The running thread will release the
				 * lock now:
				 */
				p->count = 0;

				/* Get the new owner of the lock: */
				while ((p->owner = TAILQ_FIRST(&p->l_head)) != NULL) {
					/* Pop the thread off the queue: */
					TAILQ_REMOVE(&p->l_head,p->owner,qe);
					p->owner->flags &= ~PTHREAD_FLAGS_IN_FILEQ;

					if (p->owner->interrupted == 0) {
						/*
						 * This is the first lock for
						 * the new owner:
						 */
						p->count = 1;

						/* Allow the new owner to run: */
						PTHREAD_NEW_STATE(p->owner,PS_RUNNING);

						/* End the loop when we find a
						 * thread that hasn't been
						 * cancelled or interrupted;
						 */
						break;
					}
				}
			}
			p = next_p;
			if (next_p != NULL)
				next_p = LIST_NEXT(next_p, entry);
		}
	}

	/* Unlock the hash table: */
	_SPINUNLOCK(&hash_lock);

	/*
	 * Undefer and handle pending signals, yielding if
	 * necessary:
	 */
	_thread_kern_sig_undefer();
}

void
_flockfile_backout(struct pthread *pthread)
{
	int	idx = file_idx(pthread->data.fp);
	struct	file_lock	*p;

	/*
	 * Defer signals to protect the scheduling queues from
	 * access by the signal handler:
	 */
	_thread_kern_sig_defer();

	/*
	 * Get a pointer to the lock for the file and check that
	 * the running thread is the one with the lock:
	 */
	if (((pthread->flags & PTHREAD_FLAGS_IN_FILEQ) != 0) &&
	    ((p = find_lock(idx, pthread->data.fp)) != NULL)) {
		/* Lock the hash table: */
		_SPINLOCK(&hash_lock);

		/* Remove the thread from the queue: */
		TAILQ_REMOVE(&p->l_head, pthread, qe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_FILEQ;

		/* Unlock the hash table: */
		_SPINUNLOCK(&hash_lock);
	}

	/*
	 * Undefer and handle pending signals, yielding if necessary:
	 */
	_thread_kern_sig_undefer();
}

