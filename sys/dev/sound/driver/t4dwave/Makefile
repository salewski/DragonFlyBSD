# $FreeBSD: src/sys/modules/sound/driver/t4dwave/Makefile,v 1.1.2.1 2000/09/23 19:15:08 cg Exp $
# $DragonFly: src/sys/dev/sound/driver/t4dwave/Makefile,v 1.2 2003/06/17 04:28:46 dillon Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_t4dwave
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= t4dwave.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>
