# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/pic/Makefile,v 1.1.2.2 2002/07/19 08:34:10 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/src/preproc/pic/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

PROG_CXX=	pic
SRCS=		lex.cc main.cc object.cc pic.cc pic_tab.h \
		common.cc troff.cc tex.cc
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN} pic.cc pic_tab.h y.tab.c y.tab.h

pic_tab.h: pic.cc

.include <bsd.prog.mk>
