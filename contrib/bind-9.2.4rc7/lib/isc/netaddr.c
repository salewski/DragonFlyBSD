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

/* $Id: netaddr.c,v 1.18.2.2 2004/03/09 06:11:49 marka Exp $ */

#include <config.h>

#include <stdio.h>

#include <isc/buffer.h>
#include <isc/msgs.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/print.h>
#include <isc/sockaddr.h>
#include <isc/string.h>
#include <isc/util.h>

isc_boolean_t
isc_netaddr_equal(const isc_netaddr_t *a, const isc_netaddr_t *b) {
	REQUIRE(a != NULL && b != NULL);

	if (a->family != b->family)
		return (ISC_FALSE);

	switch (a->family) {
	case AF_INET:
		if (a->type.in.s_addr != b->type.in.s_addr)
			return (ISC_FALSE);
		break;
	case AF_INET6:
		if (memcmp(&a->type.in6, &b->type.in6, sizeof a->type.in6)
		    != 0)
			return (ISC_FALSE);
		break;
	default:
		return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

isc_boolean_t
isc_netaddr_eqprefix(const isc_netaddr_t *a, const isc_netaddr_t *b,
		     unsigned int prefixlen)
{
	const unsigned char *pa, *pb;
	unsigned int ipabytes; /* Length of whole IP address in bytes */
	unsigned int nbytes;   /* Number of significant whole bytes */
	unsigned int nbits;    /* Number of significant leftover bits */

	REQUIRE(a != NULL && b != NULL);

	if (a->family != b->family)
		return (ISC_FALSE);

	switch (a->family) {
	case AF_INET:
		pa = (const unsigned char *) &a->type.in;
		pb = (const unsigned char *) &b->type.in;
		ipabytes = 4;
		break;
	case AF_INET6:
		pa = (const unsigned char *) &a->type.in6;
		pb = (const unsigned char *) &b->type.in6;
		ipabytes = 16;
		break;
	default:
		pa = pb = NULL; /* Avoid silly compiler warning. */
		ipabytes = 0; /* Ditto. */
		return (ISC_FALSE);
	}

	/*
	 * Don't crash if we get a pattern like 10.0.0.1/9999999.
	 */
	if (prefixlen > ipabytes * 8)
		prefixlen = ipabytes * 8;

	nbytes = prefixlen / 8;
	nbits = prefixlen % 8;

	if (nbytes > 0) {
		if (memcmp(pa, pb, nbytes) != 0)
			return (ISC_FALSE);
	}
	if (nbits > 0) {
		unsigned int bytea, byteb, mask;
		INSIST(nbytes < ipabytes);
		INSIST(nbits < 8);
		bytea = pa[nbytes];
		byteb = pb[nbytes];
		mask = (0xFF << (8-nbits)) & 0xFF;
		if ((bytea & mask) != (byteb & mask))
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

isc_result_t
isc_netaddr_totext(const isc_netaddr_t *netaddr, isc_buffer_t *target) {
	char abuf[sizeof "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255"];
	unsigned int alen;
	const char *r;

	REQUIRE(netaddr != NULL);

	r = inet_ntop(netaddr->family, &netaddr->type, abuf, sizeof abuf);
	if (r == NULL)
		return (ISC_R_FAILURE);

	alen = strlen(abuf);
	INSIST(alen < sizeof(abuf));

	if (alen > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);

	isc_buffer_putmem(target, (unsigned char *)abuf, alen);

	return (ISC_R_SUCCESS);
}

void
isc_netaddr_format(isc_netaddr_t *na, char *array, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	isc_buffer_init(&buf, array, size);
	result = isc_netaddr_totext(na, &buf);

	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}

	if (result != ISC_R_SUCCESS) {
		snprintf(array, size,
			 isc_msgcat_get(isc_msgcat, ISC_MSGSET_NETADDR,
					ISC_MSG_UNKNOWNADDR,
					"<unknown address, family %u>"),
			 na->family);
		array[size - 1] = '\0';
	}
}

isc_result_t
isc_netaddr_masktoprefixlen(const isc_netaddr_t *s, unsigned int *lenp) {
	unsigned int nbits, nbytes, ipbytes, i;
	const unsigned char *p;

	switch (s->family) {
	case AF_INET:
		p = (const unsigned char *) &s->type.in;
		ipbytes = 4;
		break;
	case AF_INET6:
		p = (const unsigned char *) &s->type.in6;
		ipbytes = 16;
		break;
	default:
		ipbytes = 0;
		return (ISC_R_NOTIMPLEMENTED);
	}
	nbytes = nbits = 0;
	for (i = 0; i < ipbytes; i++) {
		if (p[i] != 0xFF)
			break;
	}
	nbytes = i;
	if (i < ipbytes) {
		unsigned int c = p[nbytes];
		while ((c & 0x80) != 0 && nbits < 8) {
			c <<= 1; nbits++;
		}
		if ((c & 0xFF) != 0)
			return (ISC_R_MASKNONCONTIG);
		i++;
	}
	for (; i < ipbytes; i++) {
		if (p[i] != 0)
			return (ISC_R_MASKNONCONTIG);
		i++;
	}
	*lenp = nbytes * 8 + nbits;
	return (ISC_R_SUCCESS);
}

void
isc_netaddr_fromin(isc_netaddr_t *netaddr, const struct in_addr *ina) {
	memset(netaddr, 0, sizeof *netaddr);
	netaddr->family = AF_INET;
	netaddr->type.in = *ina;
}

void
isc_netaddr_fromin6(isc_netaddr_t *netaddr, const struct in6_addr *ina6) {
	memset(netaddr, 0, sizeof *netaddr);
	netaddr->family = AF_INET6;
	netaddr->type.in6 = *ina6;
}

void
isc_netaddr_fromsockaddr(isc_netaddr_t *t, const isc_sockaddr_t *s) {
	int family = s->type.sa.sa_family;
	t->family = family;
	switch (family) {
        case AF_INET:
		t->type.in = s->type.sin.sin_addr;
                break;
        case AF_INET6:
		memcpy(&t->type.in6, &s->type.sin6.sin6_addr, 16);
                break;
        default:
                INSIST(0);
        }
}

void
isc_netaddr_any(isc_netaddr_t *netaddr) {
	memset(netaddr, 0, sizeof *netaddr);
	netaddr->family = AF_INET;
	netaddr->type.in.s_addr = INADDR_ANY;
}

void
isc_netaddr_any6(isc_netaddr_t *netaddr) {
	memset(netaddr, 0, sizeof *netaddr);
	netaddr->family = AF_INET6;
	netaddr->type.in6 = in6addr_any;
}

isc_boolean_t
isc_netaddr_ismulticast(isc_netaddr_t *na) {
	switch (na->family) {
	case AF_INET:
		return (ISC_TF(ISC_IPADDR_ISMULTICAST(na->type.in.s_addr)));
	case AF_INET6:
		return (ISC_TF(IN6_IS_ADDR_MULTICAST(&na->type.in6)));
	default:
		return (ISC_FALSE);  /* XXXMLG ? */
	}
}

isc_boolean_t
isc_netaddr_isexperimental(isc_netaddr_t *na) {
	switch (na->family) {
	case AF_INET:
		return (ISC_TF(ISC_IPADDR_ISEXPERIMENTAL(na->type.in.s_addr)));
	default:
		return (ISC_FALSE);  /* XXXMLG ? */
	}
}

void
isc_netaddr_fromv4mapped(isc_netaddr_t *t, const isc_netaddr_t *s) {
	isc_netaddr_t *src;

	DE_CONST(s, src);	/* Must come before IN6_IS_ADDR_V4MAPPED. */

	REQUIRE(s->family == AF_INET6);
	REQUIRE(IN6_IS_ADDR_V4MAPPED(&src->type.in6));

	memset(t, 0, sizeof(*t));
	t->family = AF_INET;
	memcpy(&t->type.in, (char *)&src->type.in6 + 12, 4);
	return;
}
