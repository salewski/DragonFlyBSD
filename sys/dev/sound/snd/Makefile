# $FreeBSD: src/sys/modules/sound/snd/Makefile,v 1.1.2.3 2001/08/01 05:37:31 cg Exp $
# $DragonFly: src/sys/dev/sound/snd/Attic/Makefile,v 1.2 2003/06/17 04:28:46 dillon Exp $

.PATH: ${.CURDIR}/../../../dev/sound

KMOD		=	snd
SRCS		=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= 	ac97_if.h channel_if.h feeder_if.h mixer_if.h
SRCS		+=	driver.c
KMODDEPS 	+= 	snd_pcm
KMODDEPS 	+= 	snd_ad1816 snd_als4000 snd_cmi snd_cs4281 snd_csa snd_ds1
KMODDEPS	+=	snd_emu10k1 snd_es137x snd_fm801 snd_ich
KMODDEPS	+=	snd_maestro snd_maestro3 snd_mss snd_neomagic
KMODDEPS	+=      snd_solo snd_t4dwave snd_via82c686 snd_vibes
KMODDEPS	+=      snd_sbc snd_sb16 snd_sb8 snd_ess

.include <bsd.kmod.mk>
