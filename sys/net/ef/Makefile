# $FreeBSD: src/sys/modules/if_ef/Makefile,v 1.2.2.2 2001/04/25 11:24:03 ru Exp $
# $DragonFly: src/sys/net/ef/Makefile,v 1.3 2005/02/18 11:41:42 corecode Exp $

.PATH:  ${.CURDIR}/../../net

KMOD=   if_ef
SRCS=   if_ef.c opt_ipx.h opt_inet.h opt_ef.h
MAN=	ef.4

# If you need only limited number of frames comment out unneeded ones
# this will reduce number of visible devices
ETHER_II=
ETHER_8023=
ETHER_8022=
ETHER_SNAP=

CFLAGS+= ${PROTOS}

.if defined(EFDEBUG)
CFLAGS+= -DEF_DEBUG
.endif

.if !defined(BUILDING_WITH_KERNEL)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h
.endif

opt_ef.h:
.for frame in ETHER_II ETHER_8023 ETHER_8022 ETHER_SNAP
.if defined(${frame})
	echo "#define ${frame} 1" >> opt_ef.h
.endif
.endfor

load: all
	/sbin/kldload ${.OBJDIR}/${KMOD}

unload:
	-/sbin/kldunload ${KMOD}

.include <bsd.kmod.mk>
