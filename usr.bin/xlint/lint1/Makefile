#	$NetBSD: Makefile,v 1.3 1995/07/04 01:53:05 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/lint1/Makefile,v 1.7.2.1 2000/12/07 16:33:35 obrien Exp $
# $DragonFly: src/usr.bin/xlint/lint1/Makefile,v 1.2 2003/06/17 04:29:34 dillon Exp $

PROG=	lint1
SRCS=	cgram.y scan.l mem1.c mem.c err.c main1.c decl.c tree.c func.c \
	init.c emit.c emit1.c y.tab.h
NOMAN=
LDADD+=	-ll
DPADD+=	${LIBL}
CFLAGS+= -I. -I${.CURDIR}
LINTFLAGS=-aehpz

BINDIR=	/usr/libexec

# XXX: -O causes the gcc to die on the i386, when compiling tree.o
CFLAGS+= -DXXX_BROKEN_GCC

.include <bsd.prog.mk>
