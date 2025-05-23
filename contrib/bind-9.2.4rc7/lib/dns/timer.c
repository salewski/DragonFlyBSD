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

/* $Id: timer.c,v 1.2.2.1 2004/03/09 06:11:09 marka Exp $ */

#include <config.h>

#include <isc/result.h>
#include <isc/time.h>
#include <isc/timer.h>

#include <dns/types.h>
#include <dns/timer.h>

#define CHECK(op) \
	do { result = (op);					\
		if (result != ISC_R_SUCCESS) goto failure;	\
	} while (0)

isc_result_t
dns_timer_setidle(isc_timer_t *timer, unsigned int maxtime,
		  unsigned int idletime, isc_boolean_t purge)
{
	isc_result_t result;
	isc_interval_t maxinterval, idleinterval;
	isc_time_t expires;

	/* Compute the time of expiry. */
	isc_interval_set(&maxinterval, maxtime, 0);
	CHECK(isc_time_nowplusinterval(&expires, &maxinterval));

	/*
	 * Compute the idle interval, and add a spare nanosecond to
	 * work around the silly limitation of the ISC timer interface
	 * that you cannot specify an idle interval of zero.
	 */
	isc_interval_set(&idleinterval, idletime, 1);

	CHECK(isc_timer_reset(timer, isc_timertype_once,
			      &expires, &idleinterval,
			      purge));
 failure:
	return (result);
}
