# $FreeBSD: src/sys/modules/drm/tdfx/Makefile,v 1.2.4.1 2003/04/26 07:31:06 anholt Exp $
# $DragonFly: src/sys/dev/drm/tdfx/Makefile,v 1.2 2003/06/17 04:28:43 dillon Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= tdfx
NOMAN	= YES
SRCS	= tdfx_drv.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
