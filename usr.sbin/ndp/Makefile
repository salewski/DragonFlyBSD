# Copyright (c) 1996 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modifications, are permitted provided that the above copyright notice
# and this paragraph are duplicated in all such forms and that any
# documentation, advertising materials, and other materials related to
# such distribution and use acknowledge that the software was developed
# by the WIDE Project, Japan. The name of the Project may not be used to
# endorse or promote products derived from this software without
# specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS''
# AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
# LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE.
# $FreeBSD: src/usr.sbin/ndp/Makefile,v 1.1.2.1 2001/04/25 12:10:20 ru Exp $
# $DragonFly: src/usr.sbin/ndp/Makefile,v 1.3 2004/06/26 16:55:04 dillon Exp $

.PATH:	${.CURDIR}/../../contrib/tcpdump-3.8.3

PROG=	ndp
SRCS=	ndp.c gmt2local.c
MAN=	ndp.8

CFLAGS+=-DINET6
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../tcpdump/tcpdump -I${.CURDIR}/../../contrib/tcpdump-3.8.3
CFLAGS+= -DHAVE_CONFIG_H -D_U_="__attribute__((unused))"


.include <bsd.prog.mk>
