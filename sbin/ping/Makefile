#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ping/Makefile,v 1.12.2.4 2001/12/19 04:49:11 dd Exp $
# $DragonFly: src/sbin/ping/Makefile,v 1.3 2005/02/13 02:11:26 swildner Exp $

PROG=	ping
MAN=	ping.8
BINMODE=4555
WARNS?=	6
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
