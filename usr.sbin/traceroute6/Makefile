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
# $FreeBSD: src/usr.sbin/traceroute6/Makefile,v 1.2.2.3 2002/07/23 07:48:27 ru Exp $
# $DragonFly: src/usr.sbin/traceroute6/Makefile,v 1.2 2003/06/17 04:30:03 dillon Exp $

PROG=	traceroute6
MAN=	traceroute6.8
BINOWN=	root
BINMODE=4555

CFLAGS+=-DINET6 -DIPSEC -DHAVE_POLL

DPADD=	${LIBIPSEC}
LDADD=	-lipsec

.include <bsd.prog.mk>
