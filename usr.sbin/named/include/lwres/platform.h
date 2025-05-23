/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: platform.h.in,v 1.12.2.1 2001/11/02 00:20:10 marka Exp $ */
/* $DragonFly: src/usr.sbin/named/include/lwres/platform.h,v 1.1 2004/05/27 18:15:42 dillon Exp $ */

#ifndef LWRES_PLATFORM_H
#define LWRES_PLATFORM_H 1

/*****
 ***** Platform-dependent defines.
 *****/

/***
 *** Network.
 ***/

/*
 * Define if this system needs the <netinet/in6.h> header file for IPv6.
 */
#undef LWRES_PLATFORM_NEEDNETINETIN6H

/*
 * Define if this system needs the <netinet6/in6.h> header file for IPv6.
 */
#undef LWRES_PLATFORM_NEEDNETINET6IN6H

/*
 * If sockaddrs on this system have an sa_len field, LWRES_PLATFORM_HAVESALEN
 * will be defined.
 */
#define LWRES_PLATFORM_HAVESALEN 1

/*
 * If this system has the IPv6 structure definitions, LWRES_PLATFORM_HAVEIPV6
 * will be defined.
 */
#define LWRES_PLATFORM_HAVEIPV6 1

/*
 * If this system is missing in6addr_any, LWRES_PLATFORM_NEEDIN6ADDRANY will
 * be defined.
 */
#undef LWRES_PLATFORM_NEEDIN6ADDRANY

/*
 * If this system is missing in6addr_loopback, 
 * LWRES_PLATFORM_NEEDIN6ADDRLOOPBACK will be defined.
 */
#undef LWRES_PLATFORM_NEEDIN6ADDRLOOPBACK

/*
 * If this system has in_addr6, rather than in6_addr,
 * LWRES_PLATFORM_HAVEINADDR6 will be defined.
 */
#undef LWRES_PLATFORM_HAVEINADDR6

/*
 * Defined if unistd.h does not cause fd_set to be delared.
 */
#undef LWRES_PLATFORM_NEEDSYSSELECTH

/*
 * Used to control how extern data is linked; needed for Win32 platforms.
 */
#undef LWRES_PLATFORM_USEDECLSPEC

#ifndef LWRES_PLATFORM_USEDECLSPEC
#define LIBLWRES_EXTERNAL_DATA
#else
#ifdef LIBLWRES_EXPORTS
#define LIBLWRES_EXTERNAL_DATA __declspec(dllexport)
#else
#define LIBLWRES_EXTERNAL_DATA __declspec(dllimport)
#endif
#endif

#endif /* LWRES_PLATFORM_H */
