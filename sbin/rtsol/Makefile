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
# $FreeBSD: src/sbin/rtsol/Makefile,v 1.1.2.2 2001/07/03 11:01:20 ume Exp $
# $DragonFly: src/sbin/rtsol/Makefile,v 1.3 2005/02/15 00:26:00 cpressey Exp $

SRCDIR=	${.CURDIR}/../../usr.sbin/rtsold

PROG=	rtsol
WARNS?=	6
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c rtsock.c

CFLAGS+=-DINET6 -DHAVE_GETIFADDRS

NOMAN=	yes

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
