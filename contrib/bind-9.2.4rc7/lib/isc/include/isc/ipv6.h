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

/* $Id: ipv6.h,v 1.17.2.1 2004/03/09 06:11:57 marka Exp $ */

#ifndef ISC_IPV6_H
#define ISC_IPV6_H 1

/*
 * Also define LWRES_IPV6_H to keep it from being included if liblwres is
 * being used, or redefinition errors will occur.
 */
#define LWRES_IPV6_H 1

/*****
 ***** Module Info
 *****/

/*
 * IPv6 definitions for systems which do not support IPv6.
 *
 * MP:
 *	No impact.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	N/A.
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	RFC 2553.
 */

/***
 *** Imports.
 ***/

#include <isc/int.h>
#include <isc/platform.h>

/***
 *** Types.
 ***/

struct in6_addr {
        union {
		isc_uint8_t	_S6_u8[16];
		isc_uint16_t	_S6_u16[8];
		isc_uint32_t	_S6_u32[4];
        } _S6_un;
};
#define s6_addr		_S6_un._S6_u8
#define s6_addr8	_S6_un._S6_u8
#define s6_addr16	_S6_un._S6_u16
#define s6_addr32	_S6_un._S6_u32

#define IN6ADDR_ANY_INIT 	{{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}}
#define IN6ADDR_LOOPBACK_INIT 	{{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}}

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

struct sockaddr_in6 {
#ifdef ISC_PLATFORM_HAVESALEN
	isc_uint8_t		sin6_len;
	isc_uint8_t		sin6_family;
#else
	isc_uint16_t		sin6_family;
#endif
	isc_uint16_t		sin6_port;
	isc_uint32_t		sin6_flowinfo;
	struct in6_addr		sin6_addr;
	isc_uint32_t		sin6_scope_id;
};

#ifdef ISC_PLATFORM_HAVESALEN
#define SIN6_LEN 1
#endif

/*
 * Unspecified
 */
#define IN6_IS_ADDR_UNSPECIFIED(a)      \
        (((a)->s6_addr32[0] == 0) &&    \
         ((a)->s6_addr32[1] == 0) &&    \
         ((a)->s6_addr32[2] == 0) &&    \
         ((a)->s6_addr32[3] == 0))

/*
 * Loopback
 */
#define IN6_IS_ADDR_LOOPBACK(a)         \
        (((a)->s6_addr32[0] == 0) &&    \
         ((a)->s6_addr32[1] == 0) &&    \
         ((a)->s6_addr32[2] == 0) &&    \
         ((a)->s6_addr32[3] == htonl(1)))

/*
 * IPv4 compatible
 */
#define IN6_IS_ADDR_V4COMPAT(a)         \
        (((a)->s6_addr32[0] == 0) &&    \
         ((a)->s6_addr32[1] == 0) &&    \
         ((a)->s6_addr32[2] == 0) &&    \
         ((a)->s6_addr32[3] != 0) &&    \
         ((a)->s6_addr32[3] != htonl(1)))

/*
 * Mapped
 */
#define IN6_IS_ADDR_V4MAPPED(a)               \
        (((a)->s6_addr32[0] == 0) &&          \
         ((a)->s6_addr32[1] == 0) &&          \
         ((a)->s6_addr32[2] == htonl(0x0000ffff)))

/*
 * Multicast
 */
#define IN6_IS_ADDR_MULTICAST(a)	\
	((a)->s6_addr8[0] == 0xffU)

#endif /* ISC_IPV6_H */
