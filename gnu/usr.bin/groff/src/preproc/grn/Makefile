# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.1.2.2 2002/07/19 08:34:09 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/src/preproc/grn/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

PROG_CXX=	grn
SRCS=		hdb.cc hpoint.cc hgraph.cc main.cc
DPADD=	 	${LIBGROFF} ${LIBM}
LDADD=	 	${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
