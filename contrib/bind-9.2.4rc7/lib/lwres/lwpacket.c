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

/* $Id: lwpacket.c,v 1.13.2.1 2004/03/09 06:12:34 marka Exp $ */

#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <lwres/lwbuffer.h>
#include <lwres/lwpacket.h>
#include <lwres/result.h>

#include "assert_p.h"

#define LWPACKET_LENGTH \
	(sizeof(lwres_uint16_t) * 4 + sizeof(lwres_uint32_t) * 5)

lwres_result_t
lwres_lwpacket_renderheader(lwres_buffer_t *b, lwres_lwpacket_t *pkt) {
	REQUIRE(b != NULL);
	REQUIRE(pkt != NULL);

	if (!SPACE_OK(b, LWPACKET_LENGTH))
		return (LWRES_R_UNEXPECTEDEND);

	lwres_buffer_putuint32(b, pkt->length);
	lwres_buffer_putuint16(b, pkt->version);
	lwres_buffer_putuint16(b, pkt->pktflags);
	lwres_buffer_putuint32(b, pkt->serial);
	lwres_buffer_putuint32(b, pkt->opcode);
	lwres_buffer_putuint32(b, pkt->result);
	lwres_buffer_putuint32(b, pkt->recvlength);
	lwres_buffer_putuint16(b, pkt->authtype);
	lwres_buffer_putuint16(b, pkt->authlength);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_lwpacket_parseheader(lwres_buffer_t *b, lwres_lwpacket_t *pkt) {
	lwres_uint32_t space;

	REQUIRE(b != NULL);
	REQUIRE(pkt != NULL);

	space = LWRES_BUFFER_REMAINING(b);
	if (space < LWPACKET_LENGTH)
		return (LWRES_R_UNEXPECTEDEND);

	pkt->length = lwres_buffer_getuint32(b);
	/*
	 * XXXBEW/MLG Checking that the buffer is long enough probably
	 * shouldn't be done here, since this function is supposed to just
	 * parse the header.
	 */
	if (pkt->length > space)
		return (LWRES_R_UNEXPECTEDEND);
	pkt->version = lwres_buffer_getuint16(b);
	pkt->pktflags = lwres_buffer_getuint16(b);
	pkt->serial = lwres_buffer_getuint32(b);
	pkt->opcode = lwres_buffer_getuint32(b);
	pkt->result = lwres_buffer_getuint32(b);
	pkt->recvlength = lwres_buffer_getuint32(b);
	pkt->authtype = lwres_buffer_getuint16(b);
	pkt->authlength = lwres_buffer_getuint16(b);

	return (LWRES_R_SUCCESS);
}
