# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/eqn/Makefile,v 1.1.2.2 2002/07/19 08:34:09 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/src/preproc/eqn/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

PROG_CXX=	eqn
SRCS=		eqn.cc eqn_tab.h main.cc lex.cc \
		box.cc limit.cc list.cc over.cc text.cc \
		script.cc mark.cc other.cc delim.cc sqrt.cc pile.cc special.cc
CFLAGS+=	-I${DIST_DIR} -I.
DPADD=		${LIBGROFF}
LDADD=		${LIBGROFF}
SCRIPTS=	neqn
MAN=		eqn.1 neqn.1
CLEANFILES=	eqn.cc eqn_tab.h ${SCRIPTS} ${MAN} y.tab.c y.tab.h

eqn_tab.h: eqn.cc

.include <bsd.prog.mk>
