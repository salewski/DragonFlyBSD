#	Makefile for pccardd
# $FreeBSD: src/usr.sbin/pccard/pccardd/Makefile,v 1.4.6.2 2001/04/25 12:10:25 ru Exp $
# $DragonFly: src/usr.sbin/pccard/pccardd/Attic/Makefile,v 1.2 2003/06/17 04:29:59 dillon Exp $

PROG=	pccardd
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c
MAN=	pccard.conf.5 pccardd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil 
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
