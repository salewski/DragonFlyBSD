# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.16 2004/08/12 20:06:01 ru Exp $
# $DragonFly: src/usr.sbin/pkg_install/version/Attic/Makefile,v 1.4 2005/03/08 19:11:30 joerg Exp $

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

test:
	sh ${.CURDIR}/test-pkg_version.sh

.include <bsd.prog.mk>
