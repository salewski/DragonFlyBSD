# $FreeBSD: src/sys/modules/drm/mga/Makefile,v 1.4.2.1 2003/04/26 07:31:06 anholt Exp $
# $DragonFly: src/sys/dev/drm/mga/Makefile,v 1.2 2003/06/17 04:28:43 dillon Exp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
NOMAN	= YES
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c \
	mga_irq.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
