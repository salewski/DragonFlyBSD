#	@(#)Makefile	5.6 (Berkeley) 3/5/91
# $FreeBSD: src/usr.bin/keyinit/Makefile,v 1.6.6.1 2001/04/25 11:29:24 ru Exp $
# $DragonFly: src/usr.bin/keyinit/Makefile,v 1.3 2005/01/16 21:14:51 cpressey Exp $

PROG=	keyinit
WARNS?=	6
SRCS=	skeyinit.c

CFLAGS+= -D_SKEY_INTERNAL

BINMODE=4555

DPADD=	${LIBSKEY} ${LIBCRYPT} ${LIBMD}
LDADD=	-lskey -lcrypt -lmd

.include <bsd.prog.mk>
