# $FreeBSD: src/usr.sbin/pppd/Makefile,v 1.19.2.2 2001/08/19 03:32:43 kris Exp $
# $DragonFly: src/usr.sbin/pppd/Makefile,v 1.5 2004/01/31 06:56:46 dillon Exp $

CFLAGS+= -DHAVE_PATHS_H

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c ipxcp.c upap.c chap.c ccp.c \
	demand.c auth.c options.c sys-bsd.c
MAN=	pppd.8
BINMODE=4550
BINOWN=	root
BINGRP=	dialer

LDADD=	-lcrypt -lutil -lmd
DPADD=	${LIBCRYPT} ${LIBUTIL} ${LIBMD}

# Support SPX/IPX - not quite ready
#CFLAGS+=-DIPX_CHANGE
#SRCS+=	ipxcp.c

# Callback Control Protocol
CFLAGS+=-DCBCP_SUPPORT
SRCS+=	cbcp.c

# Filter support
CFLAGS+=-DPPP_FILTER
LDADD+=	-lpcap
DPADD+=	${LIBPCAP}

# MS-CHAP support.  Requires the DES library.
.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT)  && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
CFLAGS+=-DCHAPMS
SRCS+=	chap_ms.c
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

.if defined(RELEASE_CRUNCH)
# We must create these objects because crunchgen will link them,
# and we don't want any unused symbols to spoil the final link.
SRCS+= chap_ms.c
chap_ms.o:
	>null_${.PREFIX}.c
	cc -c -o ${.TARGET} null_${.PREFIX}.c
.endif

.include <bsd.prog.mk>
