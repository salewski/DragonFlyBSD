# $FreeBSD: src/sys/modules/amr/Makefile,v 1.4.2.2 2002/10/28 21:10:06 emoore Exp $
# $DragonFly: src/sys/dev/raid/amr/Makefile,v 1.2 2003/06/17 04:28:43 dillon Exp $

.PATH:		${.CURDIR}/../../dev/amr
KMOD		= amr
SRCS		= amr.c amr_pci.c amr_disk.c device_if.h bus_if.h pci_if.h

# SCSI passthrough support for non-disk devices
#CFLAGS		+= -DAMR_SCSI_PASSTHROUGH
SRCS		+= amr_cam.c opt_cam.h opt_scsi.h

# Enable a questionable optimisation for newer adapters
#CFLAGS		+= -DAMR_QUARTZ_GOFASTER

# Debugging
#CFLAGS		+= -DAMR_DEBUG=3

.include <bsd.kmod.mk>
