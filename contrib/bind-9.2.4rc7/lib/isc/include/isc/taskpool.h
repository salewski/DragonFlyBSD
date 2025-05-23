/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: taskpool.h,v 1.8.2.1 2004/03/09 06:12:03 marka Exp $ */

#ifndef ISC_TASKPOOL_H
#define ISC_TASKPOOL_H 1

/*****
 ***** Module Info
 *****/

/*
 * Task Pool
 *
 * A task pool is a mechanism for sharing a small number of tasks
 * among a large number of objects such that each object is
 * assigned a unique task, but each task may be shared by several
 * objects.
 *
 * Task pools are used to let objects that can exist in large
 * numbers (e.g., zones) use tasks for synchronization without
 * the memory overhead and unfair scheduling competition that
 * could result from creating a separate task for each object.
 */


/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isc/task.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types.
 *****/

typedef struct isc_taskpool isc_taskpool_t;

/*****
 ***** Functions.
 *****/

isc_result_t
isc_taskpool_create(isc_taskmgr_t *tmgr, isc_mem_t *mctx,
		    unsigned int ntasks, unsigned int quantum,
		    isc_taskpool_t **poolp);
/*
 * Create a task pool of "ntasks" tasks, each with quantum
 * "quantum".
 *
 * Requires:
 *
 *	'tmgr' is a valid task manager.
 *
 *	'mctx' is a valid memory context.
 *
 *	poolp != NULL && *poolp == NULL
 *
 * Ensures:
 *
 *	On success, '*taskp' points to the new task pool.
 *
 * Returns:
 *
 *     	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 */

void 
isc_taskpool_gettask(isc_taskpool_t *pool, unsigned int hash,
			  isc_task_t **targetp);
/*
 * Attach to the task corresponding to the hash value "hash".
 */

void
isc_taskpool_destroy(isc_taskpool_t **poolp);
/*
 * Destroy a task pool.  The tasks in the pool are detached but not
 * shut down.
 *
 * Requires:
 * 	'*poolp' is a valid task pool.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_TASKPOOL_H */
