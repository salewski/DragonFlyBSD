# $FreeBSD: src/sys/modules/kue/Makefile,v 1.2 2000/01/28 11:26:30 bde Exp $
# $DragonFly: src/sys/dev/netif/kue/Makefile,v 1.2 2003/06/17 04:28:44 dillon Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/dev/usb
KMOD	= if_kue
SRCS	= if_kue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS	+= miibus_if.h

.include <bsd.kmod.mk>
