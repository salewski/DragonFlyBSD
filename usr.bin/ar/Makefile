#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/ar/Makefile,v 1.9.2.1 2001/04/25 11:29:15 ru Exp $
# $DragonFly: src/usr.bin/ar/Attic/Makefile,v 1.5 2005/01/13 18:57:56 okumoto Exp $

PROG=	ar
CFLAGS+=-I${.CURDIR} -Wall
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c
MAN=	ar.1aout ar.5
BINDIR=	/usr/libexec/binutils212/aout
WARNS?= 6

.include <bsd.prog.mk>
