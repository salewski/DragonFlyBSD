# $FreeBSD: src/sys/modules/ciss/Makefile,v 1.1.2.1 2001/12/12 06:38:16 ps Exp $
# $DragonFly: src/sys/dev/raid/ciss/Makefile,v 1.2 2003/06/17 04:28:43 dillon Exp $

KMOD		=  ciss
.PATH:		${.CURDIR}/../../dev/${KMOD}
SRCS		=  ciss.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

CFLAGS		+=-DCISS_DEBUG=0

.include <bsd.kmod.mk>
