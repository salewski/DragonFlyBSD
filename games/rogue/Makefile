#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/rogue/Makefile,v 1.4.2.5 2002/08/07 16:31:42 ru Exp $
# $DragonFly: src/games/rogue/Makefile,v 1.2 2003/06/17 04:25:24 dillon Exp $

PROG=	rogue
CFLAGS+=-DUNIX -fwritable-strings
SRCS=	hit.c init.c inventory.c level.c machdep.c main.c \
	message.c monster.c move.c object.c pack.c play.c random.c ring.c \
	room.c save.c score.c spec_hit.c throw.c trap.c use.c zap.c
DPADD=	${LIBCURSES} ${LIBTERMCAP}
LDADD=	-lcurses -ltermcap
HIDEGAME=hidegame
MAN=	rogue.6

beforeinstall:
.if !exists(${DESTDIR}/var/games/rogue.scores)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/rogue.scores
.endif

.include <bsd.prog.mk>
