# Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# $FreeBSD: src/lib/libipsec/Makefile,v 1.2.2.3 2001/04/25 10:04:30 ru Exp $
# $DragonFly: src/lib/libipsec/Makefile,v 1.2 2003/06/17 04:26:49 dillon Exp $

LIB=	ipsec
SHLIB_MAJOR= 1
SHLIB_MINOR= 0
CFLAGS+=-I. -I${.CURDIR}
CFLAGS+=-DIPSEC_DEBUG -DIPSEC
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

#.PATH:	${.CURDIR}/../../sys/netkey
#SRCS=	pfkey.c pfkey_dump.c
SRCS+=	ipsec_strerror.c policy_parse.y policy_token.l
SRCS+=	ipsec_dump_policy.c ipsec_get_policylen.c
#SRCS+=	key_debug.c
CLEANFILES+=	y.tab.c y.tab.h
YFLAGS+=-d -p __libipsecyy
LFLAGS+=-P__libipsecyy

MAN=	ipsec_set_policy.3 ipsec_strerror.3
MLINKS+=ipsec_set_policy.3 ipsec_get_policylen.3 \
	ipsec_set_policy.3 ipsec_dump_policy.3

SRCS+= y.tab.h
y.tab.h:	policy_parse.y

.include <bsd.lib.mk>
