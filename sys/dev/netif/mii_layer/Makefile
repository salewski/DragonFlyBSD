# $FreeBSD: src/sys/modules/mii/Makefile,v 1.11.2.6 2001/06/18 18:49:12 wpaul Exp $
# $DragonFly: src/sys/dev/netif/mii_layer/Makefile,v 1.3 2004/02/09 16:43:15 hmp Exp $

.PATH:	${.CURDIR}/../../dev/mii
KMOD	= miibus
SRCS	= mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h pci_if.h
SRCS	+= miibus_if.h device_if.h miibus_if.c e1000phy.c exphy.c nsphy.c
SRCS	+= mlphy.c tlphy.c rlphy.c amphy.c dcphy.c pnphy.c nsgphy.c
SRCS	+= pnaphy.c brgphy.c xmphy.c inphy.c lxtphy.c qsphy.c acphy.c

.include "./Makefile.miidevs"

.include <bsd.kmod.mk>
