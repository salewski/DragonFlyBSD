#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/battlestar/Makefile,v 1.5.2.3 2002/08/07 16:31:41 ru Exp $
# $DragonFly: src/games/battlestar/Makefile,v 1.2 2003/06/17 04:25:22 dillon Exp $

PROG=	battlestar
SRCS=	battlestar.c com1.c com2.c com3.c com4.c com5.c com6.c com7.c \
	init.c cypher.c getcom.c parse.c room.c save.c fly.c misc.c \
	globals.c dayfile.c nightfile.c dayobjs.c nightobjs.c words.c
MAN=	battlestar.6
DPADD=	${LIBCURSES} ${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-lcurses -ltermcap -lcompat
HIDEGAME=hidegame
#CFLAGS+= -Wall -W

beforeinstall:
.if !exists(${DESTDIR}/var/games/battlestar.log)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 664 /dev/null \
	    ${DESTDIR}/var/games/battlestar.log
.endif

.include <bsd.prog.mk>
