#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/robots/Makefile,v 1.5.2.3 2002/08/07 16:31:42 ru Exp $
# $DragonFly: src/games/robots/Makefile,v 1.2 2003/06/17 04:25:24 dillon Exp $

PROG=	robots
CFLAGS+=-DMAX_PER_UID=5
SRCS=	extern.c init_field.c main.c make_level.c move.c move_robs.c \
	play_level.c query.c rnd_pos.c score.c flush_in.c
MAN=	robots.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame

beforeinstall:
.if !exists(${DESTDIR}/var/games/robots_roll)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/robots_roll
.endif

.include <bsd.prog.mk>
