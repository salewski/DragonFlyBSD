# $FreeBSD: src/usr.bin/compile_et/Makefile,v 1.11 1999/09/04 09:52:32 markm Exp $
# $DragonFly: src/usr.bin/compile_et/Makefile,v 1.2 2003/06/17 04:29:25 dillon Exp $

PROG=	compile_et
SRCS=	compile_et.c parse.y lex.l getarg.c
CFLAGS+=-I. -I${.CURDIR}/../../contrib/com_err

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../../contrib/com_err
