# $FreeBSD: src/sys/modules/sound/driver/csa/Makefile,v 1.1.2.2 2001/02/27 04:31:27 cg Exp $
# $DragonFly: src/sys/dev/sound/driver/csa/Makefile,v 1.2 2003/06/17 04:28:46 dillon Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_csa
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= csa.c csapcm.c
KMODDEPS 	= snd_pcm

.include <bsd.kmod.mk>
