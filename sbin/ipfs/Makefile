# $FreeBSD: src/sbin/ipfs/Makefile,v 1.1.2.1 2001/11/14 14:21:03 guido Exp $
# $DragonFly: src/sbin/ipfs/Makefile,v 1.3 2004/01/31 06:56:40 dillon Exp $

.PATH:  ${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=   ipfs
MAN=    ipfs.8
SRCS=   ipfs.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
