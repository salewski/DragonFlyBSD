#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/worms/Makefile,v 1.3.2.1 2001/04/25 09:29:04 ru Exp $
# $DragonFly: src/games/worms/Makefile,v 1.3 2005/03/15 20:53:41 dillon Exp $

PROG=	worms
MAN=	worms.6
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermcap
WARNS?=	6

.include <bsd.prog.mk>
