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

#ifndef GENERIC_SOA_6_H
#define GENERIC_SOA_6_H 1

/* $Id: soa_6.h,v 1.27.2.1 2004/03/09 06:11:34 marka Exp $ */

typedef struct dns_rdata_soa {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	dns_name_t		origin;
	dns_name_t		contact;
	isc_uint32_t		serial;		/* host order */
	isc_uint32_t		refresh;	/* host order */
	isc_uint32_t		retry;		/* host order */
	isc_uint32_t		expire;		/* host order */
	isc_uint32_t		minimum;	/* host order */
} dns_rdata_soa_t;


#endif /* GENERIC_SOA_6_H */
