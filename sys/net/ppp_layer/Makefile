# $FreeBSD: src/sys/modules/if_ppp/Makefile,v 1.21 1999/11/28 18:52:51 bde Exp $
# $DragonFly: src/sys/net/ppp_layer/Makefile,v 1.5 2004/08/13 17:51:09 dillon Exp $

.PATH:  ${.CURDIR}/../ppp ${.CURDIR}/..
KMOD=   if_ppp
SRCS=	if_ppp.c ppp_tty.c slcompress.c \
	use_ppp.h opt_inet.h opt_ipx.h opt_ppp.h
NOMAN=

NPPP?=		2
PPP_BSDCOMP?=	1	# 0/1
PPP_DEFLATE?=	1	# 0/1
PPP_FILTER?=	1	# 0/1 - requires bpf to be configured in kernel
PPP_INET?=	1	# 0/1 - requires INET to be configured in kernel
PPP_IPX?=	0	# 0/1 - requires IPX to be configured in kernel

CFLAGS+= ${PROTOS}

.if ${PPP_BSDCOMP} > 0
SRCS+=	bsd_comp.c
.endif
.if ${PPP_DEFLATE} > 0
SRCS+=	ppp_deflate.c zlib.c
.endif

CLEANFILES=	use_ppp.h

use_ppp.h:
	echo "#define NPPP ${NPPP}" > use_ppp.h

opt_inet.h:
	touch opt_inet.h
.if ${PPP_INET} > 0
	echo "#define INET 1" > opt_inet.h
.endif

opt_ipx.h:
	touch opt_ipx.h
.if ${PPP_IPX} > 0
	echo "#define IPX ${PPP_IPX}" > opt_ipx.h
.endif

opt_ppp.h:
	touch opt_ppp.h
.if ${PPP_BSDCOMP} > 0
	echo "#define PPP_BSDCOMP ${PPP_BSDCOMP}" > opt_ppp.h
.endif
.if ${PPP_DEFLATE} > 0
	echo "#define PPP_DEFLATE ${PPP_DEFLATE}" >> opt_ppp.h
.endif
.if ${PPP_FILTER} > 0
	echo "#define PPP_FILTER ${PPP_FILTER}" >> opt_ppp.h
.endif

.include <bsd.kmod.mk>
