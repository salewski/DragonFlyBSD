#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_null/Makefile,v 1.5.6.1 2001/04/25 10:58:38 ru Exp $
# $DragonFly: src/sbin/mount_null/Makefile,v 1.3 2005/04/02 22:00:18 dillon Exp $

WARNS?= 6
PROG=	mount_null
SRCS=	mount_null.c getmntopts.c
MAN=	mount_null.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
