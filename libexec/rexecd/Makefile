#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/rexecd/Makefile,v 1.10.2.1 2001/04/25 10:40:55 ru Exp $
# $DragonFly: src/libexec/rexecd/Makefile,v 1.2 2003/06/17 04:27:07 dillon Exp $

PROG=	rexecd
MAN=	rexecd.8
CFLAGS+= -DSKEY

DPADD=  ${LIBSKEY} ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
LDADD=	-lskey -lmd -lcrypt -lutil

.include <bsd.prog.mk>
