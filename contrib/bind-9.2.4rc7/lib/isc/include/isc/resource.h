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

/* $Id: resource.h,v 1.4.2.1 2004/03/09 06:12:00 marka Exp $ */

#ifndef ISC_RESOURCE_H
#define ISC_RESOURCE_H 1

#include <isc/lang.h>
#include <isc/types.h>

#define ISC_RESOURCE_UNLIMITED ((isc_resourcevalue_t)ISC_UINT64_MAX)

ISC_LANG_BEGINDECLS

isc_result_t
isc_resource_setlimit(isc_resource_t resource, isc_resourcevalue_t value);
/*
 * Set the maximum limit for a system resource.
 *
 * Notes:
 *	If 'value' exceeds the maximum possible on the operating system,
 *	it is silently limited to that maximum -- or to "infinity", if
 *	the operating system has that concept.  ISC_RESOURCE_UNLIMITED
 *	can be used to explicitly ask for the maximum.
 *
 * Requires:
 *	'resource' is a valid member of the isc_resource_t enumeration.
 *
 * Returns:
 *	ISC_R_SUCCESS	Success.
 *	ISC_R_NOTIMPLEMENTED	'resource' is not a type known by the OS.
 *	ISC_R_NOPERM	The calling process did not have adequate permission
 *			to change the resource limit.
 */

isc_result_t
isc_resource_getlimit(isc_resource_t resource, isc_resourcevalue_t *value);
/*
 * Get the maximum limit for a system resource.
 *
 * Notes:
 *	'value' is set to the maximum limit.
 *
 *	ISC_RESOURCE_UNLIMITED is the maximum value of isc_resourcevalue_t.
 *
 *	On many (all?) Unix systems, RLIM_INFINITY is a valid value that is
 *	significantly less than ISC_RESOURCE_UNLIMITED, but which in practice
 *	behaves the same.
 *
 *	The current ISC libdns configuration file parser assigns a value
 *	of ISC_UINT32_MAX for a size_spec of "unlimited" and ISC_UNIT32_MAX - 1
 *	for "default", the latter of which is supposed to represent "the
 *	limit that was in force when the server started".  Since these are
 *	valid values in the middle of the range of isc_resourcevalue_t,
 *	there is the possibility for confusion over what exactly those
 *	particular values are supposed to represent in a particular context --
 *	discrete integral values or generalized concepts.
 *
 * Requires:
 *	'resource' is a valid member of the isc_resource_t enumeration.
 *
 * Returns:
 *	ISC_R_SUCCESS		Success.
 *	ISC_R_NOTIMPLEMENTED	'resource' is not a type known by the OS.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_RESOURCE_H */

