# $FreeBSD: src/gnu/usr.bin/texinfo/info/Makefile,v 1.10.2.4 2002/08/07 16:53:35 ru Exp $
# $DragonFly: src/gnu/usr.bin/texinfo/info/Makefile,v 1.2 2003/06/17 04:25:49 dillon Exp $

PROG=	info
MAN=	info.1 info.5 texinfo.5

SRCS=	dir.c display.c doc.c dribble.c echo-area.c filesys.c \
	footnotes.c gc.c indices.c info-utils.c info.c infodoc.c \
	infomap.c m-x.c man.c nodemenu.c nodes.c search.c session.c \
	signals.c terminal.c tilde.c variables.c window.c

CFLAGS+= -DINFODIR=\"${INFODIR}:/usr/local/info:/usr/X11R6/info:.\"

DPADD=	${LIBTERMCAP} ${LIBTXI}
LDADD=	-ltermcap ${LIBTXI}

.include <bsd.prog.mk>

.PATH:	${TXIDIR}/info ${TXIDIR}/doc
