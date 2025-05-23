/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: quota.h,v 1.8.2.1 2004/03/09 06:11:59 marka Exp $ */

#ifndef ISC_QUOTA_H
#define ISC_QUOTA_H 1

/*****
 ***** Module Info
 *****/

/*
 * Quota
 *
 * The isc_quota_t object is a simple helper object for implementing
 * quotas on things like the number of simultaneous connections to
 * a server.  It keeps track of the amount of quota in use, and
 * encapsulates the locking necessary to allow multiple tasks to
 * share a quota.
 */

/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isc/mutex.h>
#include <isc/types.h>

/*****
 ***** Types.
 *****/

ISC_LANG_BEGINDECLS

struct isc_quota {
	isc_mutex_t	lock;
	/* Locked by lock. */
	int 		max;
	int 		used;
};

isc_result_t
isc_quota_init(isc_quota_t *quota, int max);
/*
 * Initialize a quota object.
 *
 * Returns:
 * 	ISC_R_SUCCESS
 *	Other error	Lock creation failed.
 */

void
isc_quota_destroy(isc_quota_t *quota);
/*
 * Destroy a quota object.
 */

isc_result_t
isc_quota_reserve(isc_quota_t *quota);
/*
 * Attempt to reserve one unit of 'quota'.
 *
 * Returns:
 * 	ISC_R_SUCCESS	Success
 *	ISC_R_QUOTA	Quota is full
 */

void
isc_quota_release(isc_quota_t *quota);
/*
 * Release one unit of quota.
 */

isc_result_t
isc_quota_attach(isc_quota_t *quota, isc_quota_t **p);
/*
 * Like isc_quota_reserve, and also attaches '*p' to the
 * quota if successful.
 */

void
isc_quota_detach(isc_quota_t **p);
/*
 * Like isc_quota_release, and also detaches '*p' from the
 * quota.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_QUOTA_H */
