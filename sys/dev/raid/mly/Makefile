# $FreeBSD: src/sys/modules/mly/Makefile,v 1.1.2.1 2000/10/27 10:07:02 msmith Exp $
# $DragonFly: src/sys/dev/raid/mly/Makefile,v 1.2 2003/06/17 04:28:45 dillon Exp $

.PATH:		${.CURDIR}/../../dev/mly
KMOD		=  mly
SRCS		=  mly.c mly_pci.c mly_cam.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

#CFLAGS+=	-DMLY_DEBUG=1

.include <bsd.kmod.mk>
