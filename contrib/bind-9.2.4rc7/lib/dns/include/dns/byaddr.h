/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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

/* $Id: byaddr.h,v 1.12.2.3 2004/03/09 06:11:13 marka Exp $ */

#ifndef DNS_BYADDR_H
#define DNS_BYADDR_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS ByAddr
 *
 * The byaddr module provides reverse lookup services for IPv4 and IPv6
 * addresses.
 *
 * MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	RFCs:	1034, 1035, 2181, <TBS>
 *	Drafts:	<TBS>
 */

#include <isc/lang.h>
#include <isc/event.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*
 * A 'dns_byaddrevent_t' is returned when a byaddr completes.
 * The sender field will be set to the byaddr that completed.  If 'result'
 * is ISC_R_SUCCESS, then 'names' will contain a list of names associated
 * with the address.  The recipient of the event must not change the list
 * and must not refer to any of the name data after the event is freed.
 */
typedef struct dns_byaddrevent {
	ISC_EVENT_COMMON(struct dns_byaddrevent);
	isc_result_t			result;
	dns_namelist_t			names;
} dns_byaddrevent_t;

#define DNS_BYADDROPT_IPV6NIBBLE	0x0001
#define DNS_BYADDROPT_IPV6INT		0x0002	/* Use IP6.INT nibble lookups */

isc_result_t
dns_byaddr_create(isc_mem_t *mctx, isc_netaddr_t *address, dns_view_t *view,
		  unsigned int options, isc_task_t *task,
		  isc_taskaction_t action, void *arg, dns_byaddr_t **byaddrp);
/*
 * Find the domain name of 'address'.
 *
 * Notes:
 *
 *	There is a reverse lookup format for IPv6 addresses, 'nibble'
 *
 *	The 'nibble' format for that address is
 *
 *   1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.e.f.ip6.arpa.
 *
 *	DNS_BYADDROPT_IPV6INT can be used to get nibble lookups under ip6.int.
 *
 * Requires:
 *
 *	'mctx' is a valid mctx.
 *
 *	'address' is a valid IPv4 or IPv6 address.
 *
 *	'view' is a valid view which has a resolver.
 *
 *	'task' is a valid task.
 *
 *	byaddrp != NULL && *byaddrp == NULL
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Any resolver-related error (e.g. ISC_R_SHUTTINGDOWN) may also be
 *	returned.
 */

void
dns_byaddr_cancel(dns_byaddr_t *byaddr);
/*
 * Cancel 'byaddr'.
 *
 * Notes:
 *
 *	If 'byaddr' has not completed, post its BYADDRDONE event with a
 *	result code of ISC_R_CANCELED.
 *
 * Requires:
 *
 *	'byaddr' is a valid byaddr.
 */

void
dns_byaddr_destroy(dns_byaddr_t **byaddrp);
/*
 * Destroy 'byaddr'.
 *
 * Requires:
 *
 *	'*byaddrp' is a valid byaddr.
 *
 *	The caller has received the BYADDRDONE event (either because the
 *	byaddr completed or because dns_byaddr_cancel() was called).
 *
 * Ensures:
 *
 *	*byaddrp == NULL.
 */

isc_result_t
dns_byaddr_createptrname(isc_netaddr_t *address, isc_boolean_t nibble,
			 dns_name_t *name);

isc_result_t
dns_byaddr_createptrname2(isc_netaddr_t *address, unsigned int options,
			  dns_name_t *name);
/*
 * Creates a name that would be used in a PTR query for this address.  The
 * nibble flag indicates that the 'nibble' format is to be used if an IPv6
 * address is provided, instead of the 'bitstring' format.  'options' are
 * the same as for dns_byaddr_create().
 *
 * Requires:
 * 
 * 	'address' is a valid address.
 * 	'name' is a valid name with a dedicated buffer.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_BYADDR_H */
