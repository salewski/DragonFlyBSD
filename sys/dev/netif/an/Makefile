# $FreeBSD: src/sys/modules/an/Makefile,v 1.2 2000/01/28 11:26:26 bde Exp $
# $DragonFly: src/sys/dev/netif/an/Makefile,v 1.4 2005/02/18 11:41:42 corecode Exp $

.PATH:	${.CURDIR}/../../dev/an
KMOD	= if_an
SRCS	= if_an.c if_an_pccard.c if_an_pci.c if_an_isa.c
SRCS	+= opt_inet.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h

.if !defined(BUILDING_WITH_KERNEL)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
