# $FreeBSD: src/sbin/ipmon/Makefile,v 1.6.2.3 2002/04/27 18:13:36 darrenr Exp $
# $DragonFly: src/sbin/ipmon/Makefile,v 1.5 2004/02/04 01:02:14 rob Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN=	ipl.4 ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+= -I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
CFLAGS+=-D__FreeBSD__

.include <bsd.prog.mk>
