#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: src/sbin/md5/Makefile,v 1.9 2004/06/11 16:07:02 eik Exp $
# $DragonFly: src/sbin/md5/Makefile,v 1.4 2005/03/09 16:18:55 corecode Exp $

PROG=	md5

LINKS=	${BINDIR}/md5 ${BINDIR}/rmd160 \
	${BINDIR}/md5 ${BINDIR}/sha1

MLINKS=	md5.1 rmd160.1 \
	md5.1 sha1.1

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
