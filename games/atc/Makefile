#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/atc/Makefile,v 1.14.2.3 2002/02/18 16:02:41 ru Exp $
# $DragonFly: src/games/atc/Makefile,v 1.2 2003/06/17 04:25:22 dillon Exp $

PROG=	atc
CFLAGS+=-DBSD -I${.CURDIR} -I.
SRCS=	extern.c grammar.y graphics.c input.c lex.l list.c log.c \
	main.c tunable.c update.c y.tab.h
MAN=	atc.6
DPADD=	${LIBL} ${LIBM} ${LIBCURSES} ${LIBTERMCAP}
LDADD=	-ll -lm -lcurses -ltermcap
FILES=  Game_List Killer crossover default easy game_2
FILESDIR=	${SHAREDIR}/games/atc
HIDEGAME=hidegame

.PATH: ${.CURDIR}/games

.include <bsd.prog.mk>
