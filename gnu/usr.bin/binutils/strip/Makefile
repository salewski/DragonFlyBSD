# $FreeBSD: src/gnu/usr.bin/binutils/strip/Makefile,v 1.6.2.3 2002/09/01 23:39:18 obrien Exp $
# $DragonFly: src/gnu/usr.bin/binutils/strip/Attic/Makefile,v 1.2 2003/06/17 04:25:45 dillon Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	strip
SRCS=	objcopy.c is-strip.c
WARNS=	2
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}
CLEANFILES+=	maybe_stripped

all: maybe_stripped

maybe_stripped: strip
	cp strip maybe_stripped
.if defined(STRIP)
.if ${STRIP:M-s} != ""
	-strip maybe_stripped
.endif
.endif

realinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    maybe_stripped ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
