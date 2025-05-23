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

/* $Id: fixedname.h,v 1.12.2.1 2004/03/09 06:11:15 marka Exp $ */

#ifndef DNS_FIXEDNAME_H
#define DNS_FIXEDNAME_H 1

/*****
 ***** Module Info
 *****/

/*
 * Fixed-size Names
 *
 * dns_fixedname_t is a convenience type containing a name, an offsets table,
 * and a dedicated buffer big enough for the longest possible name.
 *
 * MP:
 *	The caller must ensure any required synchronization.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	Per dns_fixedname_t:
 *		sizeof(dns_name_t) + sizeof(dns_offsets_t) +
 *		sizeof(isc_buffer_t) + 255 bytes + structure padding
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/*****
 ***** Imports
 *****/

#include <isc/buffer.h>

#include <dns/name.h>

/*****
 ***** Types
 *****/

struct dns_fixedname {
	dns_name_t			name;
	dns_offsets_t			offsets;
	isc_buffer_t			buffer;
	unsigned char			data[DNS_NAME_MAXWIRE];
};

#define dns_fixedname_init(fn) \
	do { \
		dns_name_init(&((fn)->name), (fn)->offsets); \
		isc_buffer_init(&((fn)->buffer), (fn)->data, \
                                  DNS_NAME_MAXWIRE); \
		dns_name_setbuffer(&((fn)->name), &((fn)->buffer)); \
	} while (0)

#define dns_fixedname_invalidate(fn) \
	dns_name_invalidate(&((fn)->name))

#define dns_fixedname_name(fn)		(&((fn)->name))

#endif /* DNS_FIXEDNAME_H */
