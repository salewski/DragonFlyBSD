# $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $
# $FreeBSD: src/sbin/rcorder/Makefile,v 1.3 2002/06/21 15:52:05 obrien Exp $
# $DragonFly: src/sbin/rcorder/Makefile,v 1.2 2004/12/01 19:44:53 liamfoy Exp $

PROG=   rcorder
SRCS=   ealloc.c hash.c rcorder.c
MAN=	rcorder.8

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

WARNS= 6	
# XXX hack for make's hash.[ch]
CFLAGS+= -DORDER -I.

SRCS+=	util.h
CLEANFILES+=	util.h

util.h:
	ln -sf ${.CURDIR}/../../lib/libutil/libutil.h ${.TARGET}

.include <bsd.prog.mk>
