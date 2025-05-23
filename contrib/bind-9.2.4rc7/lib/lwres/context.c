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

/* $Id: context.c,v 1.41.2.3 2004/03/09 06:12:32 marka Exp $ */

#include <config.h>

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <lwres/lwres.h>
#include <lwres/net.h>
#include <lwres/platform.h>

#ifdef LWRES_PLATFORM_NEEDSYSSELECTH
#include <sys/select.h>
#endif

#include "context_p.h"
#include "assert_p.h"

/*
 * Some systems define the socket length argument as an int, some as size_t,
 * some as socklen_t.  The last is what the current POSIX standard mandates.
 * This definition is here so it can be portable but easily changed if needed.
 */
#ifndef LWRES_SOCKADDR_LEN_T
#define LWRES_SOCKADDR_LEN_T unsigned int
#endif

/*
 * Make a socket nonblocking.
 */
#ifndef MAKE_NONBLOCKING
#define MAKE_NONBLOCKING(sd, retval) \
do { \
	retval = fcntl(sd, F_GETFL, 0); \
	if (retval != -1) { \
		retval |= O_NONBLOCK; \
		retval = fcntl(sd, F_SETFL, retval); \
	} \
} while (0)
#endif

lwres_uint16_t lwres_udp_port = LWRES_UDP_PORT;
const char *lwres_resolv_conf = LWRES_RESOLV_CONF;

static void *
lwres_malloc(void *, size_t);

static void
lwres_free(void *, void *, size_t);

static lwres_result_t
context_connect(lwres_context_t *);

lwres_result_t
lwres_context_create(lwres_context_t **contextp, void *arg,
		     lwres_malloc_t malloc_function,
		     lwres_free_t free_function,
		     unsigned int flags)
{
	lwres_context_t *ctx;

	REQUIRE(contextp != NULL && *contextp == NULL);
	UNUSED(flags);

	/*
	 * If we were not given anything special to use, use our own
	 * functions.  These are just wrappers around malloc() and free().
	 */
	if (malloc_function == NULL || free_function == NULL) {
		REQUIRE(malloc_function == NULL);
		REQUIRE(free_function == NULL);
		malloc_function = lwres_malloc;
		free_function = lwres_free;
	}

	ctx = malloc_function(arg, sizeof(lwres_context_t));
	if (ctx == NULL)
		return (LWRES_R_NOMEMORY);

	/*
	 * Set up the context.
	 */
	ctx->malloc = malloc_function;
	ctx->free = free_function;
	ctx->arg = arg;
	ctx->sock = -1;

	ctx->timeout = LWRES_DEFAULT_TIMEOUT;
	ctx->serial = time(NULL); /* XXXMLG or BEW */

	/*
	 * Init resolv.conf bits.
	 */
	lwres_conf_init(ctx);

	*contextp = ctx;
	return (LWRES_R_SUCCESS);
}

void
lwres_context_destroy(lwres_context_t **contextp) {
	lwres_context_t *ctx;

	REQUIRE(contextp != NULL && *contextp != NULL);

	ctx = *contextp;
	*contextp = NULL;

	if (ctx->sock != -1) {
		close(ctx->sock);
		ctx->sock = -1;
	}

	CTXFREE(ctx, sizeof(lwres_context_t));
}

lwres_uint32_t
lwres_context_nextserial(lwres_context_t *ctx) {
	REQUIRE(ctx != NULL);

	return (ctx->serial++);
}

void
lwres_context_initserial(lwres_context_t *ctx, lwres_uint32_t serial) {
	REQUIRE(ctx != NULL);

	ctx->serial = serial;
}

void
lwres_context_freemem(lwres_context_t *ctx, void *mem, size_t len) {
	REQUIRE(mem != NULL);
	REQUIRE(len != 0U);

	CTXFREE(mem, len);
}

void *
lwres_context_allocmem(lwres_context_t *ctx, size_t len) {
	REQUIRE(len != 0U);

	return (CTXMALLOC(len));
}

static void *
lwres_malloc(void *arg, size_t len) {
	void *mem;

	UNUSED(arg);

	mem = malloc(len);
	if (mem == NULL)
		return (NULL);

	memset(mem, 0xe5, len);

	return (mem);
}

static void
lwres_free(void *arg, void *mem, size_t len) {
	UNUSED(arg);

	memset(mem, 0xa9, len);
	free(mem);
}

static lwres_result_t
context_connect(lwres_context_t *ctx) {
	int s;
	int ret;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	LWRES_SOCKADDR_LEN_T salen;
	int domain;

	if (ctx->confdata.lwnext != 0) {
		memcpy(&ctx->address, &ctx->confdata.lwservers[0],
		       sizeof(lwres_addr_t));
		LWRES_LINK_INIT(&ctx->address, link);
	} else {
		/* The default is the IPv4 loopback address 127.0.0.1. */
		memset(&ctx->address, 0, sizeof(ctx->address));
		ctx->address.family = LWRES_ADDRTYPE_V4;
		ctx->address.length = 4;
		ctx->address.address[0] = 127;
		ctx->address.address[1] = 0;
		ctx->address.address[2] = 0;
		ctx->address.address[3] = 1;
	}

	if (ctx->address.family == LWRES_ADDRTYPE_V4) {
		memcpy(&sin.sin_addr, ctx->address.address,
		       sizeof(sin.sin_addr));
		sin.sin_port = htons(lwres_udp_port);
		sin.sin_family = AF_INET;
		sa = (struct sockaddr *)&sin;
		salen = sizeof(sin);
		domain = PF_INET;
	} else if (ctx->address.family == LWRES_ADDRTYPE_V6) {
		memcpy(&sin6.sin6_addr, ctx->address.address,
		       sizeof(sin6.sin6_addr));
		sin6.sin6_port = htons(lwres_udp_port);
		sin6.sin6_family = AF_INET6;
		sa = (struct sockaddr *)&sin6;
		salen = sizeof(sin6);
		domain = PF_INET6;
	} else
		return (LWRES_R_IOERROR);

	s = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		return (LWRES_R_IOERROR);

	ret = connect(s, sa, salen);
	if (ret != 0) {
		close(s);
		return (LWRES_R_IOERROR);
	}

	MAKE_NONBLOCKING(s, ret);
	if (ret < 0)
		return (LWRES_R_IOERROR);

	ctx->sock = s;

	return (LWRES_R_SUCCESS);
}

int
lwres_context_getsocket(lwres_context_t *ctx) {
	return (ctx->sock);
}

lwres_result_t
lwres_context_send(lwres_context_t *ctx,
		   void *sendbase, int sendlen) {
	int ret;
	lwres_result_t lwresult;

	if (ctx->sock == -1) {
		lwresult = context_connect(ctx);
		if (lwresult != LWRES_R_SUCCESS)
			return (lwresult);
	}

	ret = sendto(ctx->sock, sendbase, sendlen, 0, NULL, 0);
	if (ret < 0)
		return (LWRES_R_IOERROR);
	if (ret != sendlen)
		return (LWRES_R_IOERROR);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_context_recv(lwres_context_t *ctx,
		   void *recvbase, int recvlen,
		   int *recvd_len)
{
	LWRES_SOCKADDR_LEN_T fromlen;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	int ret;

	if (ctx->address.family == LWRES_ADDRTYPE_V4) {
		sa = (struct sockaddr *)&sin;
		fromlen = sizeof(sin);
	} else {
		sa = (struct sockaddr *)&sin6;
		fromlen = sizeof(sin6);
	}

	/*
	 * The address of fromlen is cast to void * to shut up compiler
	 * warnings, namely on systems that have the sixth parameter
	 * prototyped as a signed int when LWRES_SOCKADDR_LEN_T is
	 * defined as unsigned.
	 */
	ret = recvfrom(ctx->sock, recvbase, recvlen, 0, sa, (void *)&fromlen);

	if (ret < 0)
		return (LWRES_R_IOERROR);

	if (ret == recvlen)
		return (LWRES_R_TOOLARGE);

	/*
	 * If we got something other than what we expect, have the caller
	 * wait for another packet.  This can happen if an old result
	 * comes in, or if someone is sending us random stuff.
	 */
	if (ctx->address.family == LWRES_ADDRTYPE_V4) {
		if (fromlen != sizeof(sin)
		    || memcmp(&sin.sin_addr, ctx->address.address,
			      sizeof(sin.sin_addr)) != 0
		    || sin.sin_port != htons(lwres_udp_port))
			return (LWRES_R_RETRY);
	} else {
		if (fromlen != sizeof(sin6)
		    || memcmp(&sin6.sin6_addr, ctx->address.address,
			      sizeof(sin6.sin6_addr)) != 0
		    || sin6.sin6_port != htons(lwres_udp_port))
			return (LWRES_R_RETRY);
	}

	if (recvd_len != NULL)
		*recvd_len = ret;

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_context_sendrecv(lwres_context_t *ctx,
		       void *sendbase, int sendlen,
		       void *recvbase, int recvlen,
		       int *recvd_len)
{
	lwres_result_t result;
	int ret2;
	fd_set readfds;
	struct timeval timeout;

	/*
	 * Type of tv_sec is long, so make sure the unsigned long timeout
	 * does not overflow it.
	 */
	if (ctx->timeout <= (unsigned int)LONG_MAX)
		timeout.tv_sec = (long)ctx->timeout;
	else
		timeout.tv_sec = LONG_MAX;

	timeout.tv_usec = 0;

	result = lwres_context_send(ctx, sendbase, sendlen);
	if (result != LWRES_R_SUCCESS)
		return (result);
 again:
	FD_ZERO(&readfds);
	FD_SET(ctx->sock, &readfds);
	ret2 = select(ctx->sock + 1, &readfds, NULL, NULL, &timeout);
	
	/*
	 * What happened with select?
	 */
	if (ret2 < 0)
		return (LWRES_R_IOERROR);
	if (ret2 == 0)
		return (LWRES_R_TIMEOUT);

	result = lwres_context_recv(ctx, recvbase, recvlen, recvd_len);
	if (result == LWRES_R_RETRY)
		goto again;
	
	return (result);
}
