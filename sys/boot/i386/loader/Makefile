# $FreeBSD: src/sys/boot/i386/loader/Makefile,v 1.66 2003/06/26 03:51:57 peter Exp $
# $DragonFly: src/sys/boot/i386/loader/Attic/Makefile,v 1.7 2004/10/14 09:41:01 dillon Exp $
#

PROG=		loader
STRIP=
NEWVERSWHAT=	"bootstrap loader" i386
BINDIR?=	/boot
INSTALLFLAGS=	-b

# Notes: the 'loader' client is brought in with a virtual base of (typically)
# 0xa000 and a code offset of 0x0.  The btxldr is loaded at 0x200000 (?).  
#
# MEM_BTX_USR_ARG is not used in the boot2->loader stage, only in the
# boot1->boot2 stage (XXX what about the use in cdboot and pxeboot?)
#
ORG= 0x0

# architecture-specific loader code
SRCS=		main.c conf.c

CFLAGS+=	-ffreestanding
# Enable PXE TFTP or NFS support, not both.
.if defined(LOADER_TFTP_SUPPORT)
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.else
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/i386
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif
.endif

.if defined(LOADER_BZIP2_SUPPORT)
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif
.if !defined(LOADER_NO_GZIP_SUPPORT)
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I.

CLEANFILES+=	vers.c vers.o ${PROG}.list ${PROG}.bin ${PROG}.sym ${PROG}.help

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext ${ORG}

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..

# where to get libstand from
#XXX need a better way to do this
LIBSTAND=	${.CURDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	-lstand
.endif
.endif
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
CFLAGS+=	-I${.CURDIR}/../btx/lib

# BTX is expecting ELF components
CFLAGS+=	-elf

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

vers.o:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} ${CFLAGS} -c vers.c

${PROG}: ${PROG}.bin ${BTXLDR} ${BTXKERN} ${BTXCRT}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} ${PROG}.bin
#	/usr/bin/kzip ${.TARGET}
#	mv ${.TARGET}.kz ${.TARGET}

${PROG}.bin: ${PROG}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

${PROG}.help: help.common help.i386
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth 
FILES=	${PROG}.help loader.4th support.4th loader.conf loader-bootp.conf
FILES+= screen.4th frames.4th beastie.4th
FILESDIR_loader.conf=	/boot/defaults
FILESDIR_loader-bootp.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=	${.CURDIR}/loader.rc
.endif

.include "../Makefile.inc"

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

${PROG}.sym: ${OBJS} ${LIBI386} ${LIBSTAND} ${LIBFICL} vers.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} ${OBJS} vers.o \
		${LIBFICL} ${LIBI386} ${LIBSTAND}

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine
