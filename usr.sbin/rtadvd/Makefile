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
# $FreeBSD: src/usr.sbin/rtadvd/Makefile,v 1.1.2.2 2001/04/25 12:10:48 ru Exp $
# $DragonFly: src/usr.sbin/rtadvd/Makefile,v 1.2 2003/06/17 04:30:02 dillon Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CFLAGS+=-DINET6
LDADD+=	-lcompat
DPADD+=	${LIBCOMPAT}

MAN=	rtadvd.conf.5 rtadvd.8

.include <bsd.prog.mk>
