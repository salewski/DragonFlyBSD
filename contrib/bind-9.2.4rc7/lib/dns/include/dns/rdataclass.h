/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001  Internet Software Consortium.
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

/* $Id: rdataclass.h,v 1.17.2.1 2004/03/09 06:11:20 marka Exp $ */

#ifndef DNS_RDATACLASS_H
#define DNS_RDATACLASS_H 1

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

isc_result_t
dns_rdataclass_fromtext(dns_rdataclass_t *classp, isc_textregion_t *source);
/*
 * Convert the text 'source' refers to into a DNS class.
 *
 * Requires:
 *	'classp' is a valid pointer.
 *
 *	'source' is a valid text region.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	DNS_R_UNKNOWN			class is unknown
 */

isc_result_t
dns_rdataclass_totext(dns_rdataclass_t rdclass, isc_buffer_t *target);
/*
 * Put a textual representation of class 'rdclass' into 'target'.
 *
 * Requires:
 *	'rdclass' is a valid class.
 *
 *	'target' is a valid text buffer.
 *
 * Ensures:
 *	If the result is success:
 *		The used space in 'target' is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	ISC_R_NOSPACE			target buffer is too small
 */

void
dns_rdataclass_format(dns_rdataclass_t rdclass,
		      char *array, unsigned int size);
/*
 * Format a human-readable representation of the class 'rdclass'
 * into the character array 'array', which is of size 'size'.
 * The resulting string is guaranteed to be null-terminated.
 */

#define DNS_RDATACLASS_FORMATSIZE sizeof("CLASS65535")
/*
 * Minimum size of array to pass to dns_rdataclass_format().
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATACLASS_H */
