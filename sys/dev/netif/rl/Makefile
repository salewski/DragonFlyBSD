# $FreeBSD: src/sys/modules/rl/Makefile,v 1.6 2000/01/28 11:26:34 bde Exp $
# $DragonFly: src/sys/dev/netif/rl/Makefile,v 1.2 2003/06/17 04:28:46 dillon Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_rl
SRCS	= if_rl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
