# $FreeBSD: src/sys/boot/ofw/libofw/Makefile,v 1.8 2002/11/10 19:17:36 jake Exp $
# $DragonFly: src/sys/boot/ofw/libofw/Makefile,v 1.1 2003/11/10 06:08:37 dillon Exp $

LIB=		ofw
INTERNALLIB=	true

SRCS=	devicename.c elf_freebsd.c ofw_console.c ofw_copy.c ofw_disk.c \
	ofw_memory.c ofw_module.c ofw_net.c ofw_reboot.c \
	ofw_time.c openfirm.c

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

CFLAGS+=	-ffreestanding
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-msoft-float
.endif

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
