#	@(#)Makefile	5.4 (Berkeley) 5/7/91
#	$FreeBSD: src/lib/libskey/Makefile,v 1.15.2.1 2001/04/25 10:04:40 ru Exp $
#	$DragonFly: src/lib/libskey/Makefile,v 1.3 2005/02/28 13:14:08 joerg Exp $

LIB=	skey
SRCS=	skeyaccess.c put.c skey_crypt.c skey_getpass.c skeylogin.c skeysubr.c
INCS=	skey.h
MAN=	skey.1
MAN+=	skey.3
MLINKS=	skey.3 skeylookup.3 skey.3 skeyverify.3 skey.3 skeychallenge.3 \
	skey.3 skeyinfo.3 skey.3 skeyaccess.3 skey.3 skey_getpass.3 \
	skey.3 skey_crypt.3
MAN+=	skey.access.5

CFLAGS+=-DPERMIT_CONSOLE -D_SKEY_INTERNAL -I${.CURDIR}
CFLAGS+=-W -Wall

DPADD+= ${LIBCRYPT} ${LIBMD}
LDADD+=	-lcrypt -lmd

WARNS?=	6

.include <bsd.lib.mk>
