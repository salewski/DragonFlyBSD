# $Id: Makefile,v 1.6 2003/10/24 15:43:24 simokawa Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.1 2003/10/24 15:44:10 simokawa Exp $
# $DragonFly: src/sys/dev/misc/dcons/dcons_crom/Makefile,v 1.3 2004/10/25 13:53:27 simokawa Exp $

.PATH:	${.CURDIR}/..

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h dcons_os.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

.include <bsd.kmod.mk>
