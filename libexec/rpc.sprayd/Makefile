# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.4.2.1 2001/04/25 10:40:59 ru Exp $
# $DragonFly: src/libexec/rpc.sprayd/Makefile,v 1.2 2003/06/17 04:27:07 dillon Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

