#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/sail/Makefile,v 1.4.2.3 2002/08/07 16:31:42 ru Exp $
# $DragonFly: src/games/sail/Makefile,v 1.2 2003/06/17 04:25:25 dillon Exp $

PROG=	sail
SRCS=	main.c pl_main.c pl_1.c pl_2.c pl_3.c pl_4.c pl_5.c pl_6.c pl_7.c \
	dr_main.c dr_1.c dr_2.c dr_3.c dr_4.c dr_5.c lo_main.c \
	assorted.c game.c globals.c misc.c parties.c sync.c version.c
MAN=	sail.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame

beforeinstall:
.if !exists(${DESTDIR}/var/games/saillog)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/saillog
.endif

.include <bsd.prog.mk>
