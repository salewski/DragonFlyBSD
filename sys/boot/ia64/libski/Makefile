# $FreeBSD: src/sys/boot/ia64/libski/Makefile,v 1.10 2003/07/02 11:47:33 ru Exp $
# $DragonFly: src/sys/boot/ia64/libski/Makefile,v 1.1 2003/11/10 06:08:37 dillon Exp $

LIB=		ski
INTERNALLIB=	true

SRCS=	skiconsole.c time.c copy.c devicename.c module.c exit.c
SRCS+=	delay.c skifs.c elf_freebsd.c bootinfo.c ssc.c
SRCS+=	acpi_stub.c efi_stub.c pal_stub.S sal_stub.c

CFLAGS+=	-ffreestanding -fpic -g
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

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
