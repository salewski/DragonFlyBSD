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

/* $Id: netaddr.h,v 1.18.2.2 2004/03/09 06:11:59 marka Exp $ */

#ifndef ISC_NETADDR_H
#define ISC_NETADDR_H 1

#include <isc/lang.h>
#include <isc/net.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

struct isc_netaddr {
	unsigned int family;
	union {
    		struct in_addr in;
		struct in6_addr in6;
	} type;
};

isc_boolean_t
isc_netaddr_equal(const isc_netaddr_t *a, const isc_netaddr_t *b);

isc_boolean_t
isc_netaddr_eqprefix(const isc_netaddr_t *a, const isc_netaddr_t *b,
		     unsigned int prefixlen);
/*
 * Compare the 'prefixlen' most significant bits of the network
 * addresses 'a' and 'b'.  Return ISC_TRUE if they are equal,
 * ISC_FALSE if not.
 */

isc_result_t
isc_netaddr_masktoprefixlen(const isc_netaddr_t *s, unsigned int *lenp);
/*
 * Convert a netmask in 's' into a prefix length in '*lenp'.
 * The mask should consist of zero or more '1' bits in the most
 * most significant part of the address, followed by '0' bits.
 * If this is not the case, ISC_R_MASKNONCONTIG is returned.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_MASKNONCONTIG
 */

isc_result_t
isc_netaddr_totext(const isc_netaddr_t *netaddr, isc_buffer_t *target);
/*
 * Append a text representation of 'sockaddr' to the buffer 'target'.
 * The text is NOT null terminated.  Handles IPv4 and IPv6 addresses.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOSPACE	The text or the null termination did not fit.
 *	ISC_R_FAILURE	Unspecified failure
 */

void
isc_netaddr_format(isc_netaddr_t *na, char *array, unsigned int size);
/*
 * Format a human-readable representation of the network address '*na'
 * into the character array 'array', which is of size 'size'.
 * The resulting string is guaranteed to be null-terminated.
 */

#define ISC_NETADDR_FORMATSIZE \
	sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:XXX.XXX.XXX.XXX")
/*
 * Minimum size of array to pass to isc_netaddr_format().
 */

void
isc_netaddr_fromsockaddr(isc_netaddr_t *netaddr, const isc_sockaddr_t *source);

void
isc_netaddr_fromin(isc_netaddr_t *netaddr, const struct in_addr *ina);

void
isc_netaddr_fromin6(isc_netaddr_t *netaddr, const struct in6_addr *ina6);

void
isc_netaddr_any(isc_netaddr_t *netaddr);
/*
 * Return the IPv4 wildcard address.
 */

void
isc_netaddr_any6(isc_netaddr_t *netaddr);
/*
 * Return the IPv6 wildcard address.
 */

isc_boolean_t
isc_netaddr_ismulticast(isc_netaddr_t *na);
/*
 * Returns ISC_TRUE if the address is a multicast address
 */

isc_boolean_t
isc_netaddr_isexperimental(isc_netaddr_t *na);
/*
 * Returns ISC_TRUE if the address is a experimental (CLASS E) address.
 */

void
isc_netaddr_fromv4mapped(isc_netaddr_t *t, const isc_netaddr_t *s);
/*
 * Convert an IPv6 v4mapped address into an IPv4 address.
 */


ISC_LANG_ENDDECLS

#endif /* ISC_NETADDR_H */
