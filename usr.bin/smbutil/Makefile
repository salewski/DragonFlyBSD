# $FreeBSD: src/usr.bin/smbutil/Makefile,v 1.1.2.3 2001/12/20 16:28:30 sheldonh Exp $
# $DragonFly: src/usr.bin/smbutil/Makefile,v 1.2 2003/06/17 04:29:31 dillon Exp $

PROG=	smbutil
SRCS=	smbutil.c dumptree.c login.c lookup.c view.c print.c

DPADD=	${LIBSMB}
LDADD=	-lsmb

CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/smbutil

.include <bsd.prog.mk>
