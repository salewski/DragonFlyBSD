# $FreeBSD: src/sys/modules/twe/Makefile,v 1.1.2.2 2000/10/27 06:04:54 msmith Exp $
# $DragonFly: src/sys/dev/raid/twe/Makefile,v 1.2 2003/06/17 04:28:47 dillon Exp $

#
# The 3ware controller offers a choice of options related to handling of
# mirror consistency in shutdown situations.
#
# If TWE_SHUTDOWN_NOTIFICATION is defined, the driver will inform the 
# controller on clean shutdown.  On reboot after an unclean shutdown, the 
# controller will perform a complete mirror rebuild.  This is the most
# "safe" configuration, but it imposes a severe performance penalty in the
# event of an unexpected reboot.
#
# In the alternate configuration, the controller assumes that all writes to
# a mirror complete successfully.  In the case of an unclean shutdown,
# mirror corruption may occur if the controller is reset or power is lost
# during a disk write.  This configuration is more suitable if the system
# has reliable power, but may be unstable for other reasons.
#
CFLAGS+=-DTWE_SHUTDOWN_NOTIFICATION

#
# The module can be built to override a compiled-in driver
#
#CFLAGS+=-DTWE_OVERRIDE

.PATH:	${.CURDIR}/../../dev/twe
KMOD=	twe
SRCS=	bus_if.h device_if.h pci_if.h twe.c twe_freebsd.c

#CFLAGS+=-DTWE_DEBUG=0

.include <bsd.kmod.mk>
