#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fortune/fortune/Makefile,v 1.3.8.1 2001/04/25 09:28:54 ru Exp $
# $DragonFly: src/games/fortune/fortune/Makefile,v 1.2 2003/06/17 04:25:24 dillon Exp $

PROG=	fortune
MAN=	fortune.6
CFLAGS+=-DDEBUG -Wall -I${.CURDIR}/../strfile
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat

.include "${.CURDIR}/../../Makefile.inc"
.include <bsd.prog.mk>
