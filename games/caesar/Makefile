#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/caesar/Makefile,v 1.4.6.2 2001/04/25 14:03:58 ru Exp $
# $DragonFly: src/games/caesar/Makefile,v 1.3 2005/03/01 22:47:10 joerg Exp $

PROG=	caesar
DPADD=	${LIBM}
LDADD=	-lm
SCRIPTS=rot13.sh
MAN=	caesar.6
MLINKS=	caesar.6 rot13.6
WARNS?=	6

.include <bsd.prog.mk>
