# $FreeBSD: src/gnu/usr.bin/binutils/ar/Makefile,v 1.4.2.3 2002/09/01 23:39:13 obrien Exp $
# $DragonFly: src/gnu/usr.bin/binutils/ar/Attic/Makefile,v 1.2 2003/06/17 04:25:44 dillon Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	ar
SRCS=	ar.c not-ranlib.c
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
NOSHARED?= yes
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
