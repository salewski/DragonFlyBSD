/*-
 * Copyright (c) 2000 Brian Somers <brian@Awfulhak.org>
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
 * @(#) pmap_check.h 1.3 93/11/21 16:18:53
 *
 * $FreeBSD: src/usr.sbin/portmap/pmap_check.h,v 1.3 2000/01/15 23:08:28 brian Exp $
 * $DragonFly: src/usr.sbin/portmap/pmap_check.h,v 1.3 2003/11/03 19:31:40 eirikn Exp $
 */

extern int from_local(struct sockaddr_in *);
extern void check_startup(void);
extern int check_default(struct sockaddr_in *, u_long, u_long);
extern int check_setunset(struct sockaddr_in *, u_long, u_long, u_long);
extern int check_privileged_port(struct sockaddr_in *, u_long, u_long,
	u_long);
extern int check_callit(struct sockaddr_in *, u_long, u_long, u_long);

extern int verboselog;
extern int allow_severity;
extern int deny_severity;
