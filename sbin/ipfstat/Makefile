#$FreeBSD: src/sbin/ipfstat/Makefile,v 1.4.2.7 2002/04/27 18:13:13 darrenr Exp $
# $DragonFly: src/sbin/ipfstat/Makefile,v 1.5 2004/07/28 00:22:36 hmp Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c printstate.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DSTATETOP
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

DPADD+=	${LIBNCURSES} ${LIBKVM}
LDADD+=	-lncurses -lkvm

.include <bsd.prog.mk>
