# $FreeBSD: src/usr.sbin/zic/zic/Makefile,v 1.5.2.1 2001/04/25 12:11:16 ru Exp $
# $DragonFly: src/usr.sbin/zic/zic/Makefile,v 1.2 2003/06/17 04:30:05 dillon Exp $

.PATH:	${.CURDIR}/..

PROG=	zic

SRCS=	zic.c ialloc.c scheck.c
MAN=	${.CURDIR}/../zic.8
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime
CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -DHAVE_STRERROR -DHAVE_UNISTD_H

.include <bsd.prog.mk>
