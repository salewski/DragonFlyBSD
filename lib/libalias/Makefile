# $FreeBSD: src/lib/libalias/Makefile,v 1.13.2.5 2002/07/24 03:21:24 luigi Exp $
# $DragonFly: src/lib/libalias/Makefile,v 1.2 2003/06/17 04:26:41 dillon Exp $

LIB=		alias
SHLIB_MAJOR=	4
SHLIB_MINOR=	0
CFLAGS+=	-Wall -Wmissing-prototypes
.if defined(IPFW2)
CFLAGS+=	-DIPFW2
.endif
SRCS=		alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
		alias_nbt.c alias_pptp.c alias_proxy.c alias_smedia.c \
		alias_util.c
INCS=		alias.h
MAN=		libalias.3

.include <bsd.lib.mk>
