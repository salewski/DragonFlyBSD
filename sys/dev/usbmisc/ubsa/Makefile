# $FreeBSD: src/sys/modules/ubsa/Makefile,v 1.1.2.2 2003/01/14 20:04:22 kan Exp $
# $DragonFly: src/sys/dev/usbmisc/ubsa/Makefile,v 1.4 2004/08/13 17:51:06 dillon Exp $

.PATH: ${.CURDIR}/../ucom

KMOD=	ubsa
SRCS=	ubsa.c ucomvar.h opt_usb.h device_if.h bus_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
