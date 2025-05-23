/*
 * Copyright (c) 2001 Dima Dorfman.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/gen/getpeereid.c,v 1.4.2.2 2002/12/23 10:25:36 maxim Exp $
 * $DragonFly: src/lib/libcr/gen/Attic/getpeereid.c,v 1.2 2003/06/17 04:26:42 dillon Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ucred.h>
#include <sys/un.h>

#include <errno.h>
#include <unistd.h>

int
getpeereid(int s, uid_t *euid, gid_t *egid)
{
	struct xucred xuc;
	socklen_t xuclen;
	int error;

	xuclen = sizeof(xuc);
	error = getsockopt(s, 0, LOCAL_PEERCRED, &xuc, &xuclen);
	if (error != 0)
		return (error);
	if (xuc.cr_version != XUCRED_VERSION)
		return (EINVAL);
	*euid = xuc.cr_uid;
	*egid = xuc.cr_gid;
	return (0);
}
