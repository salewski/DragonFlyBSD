# $FreeBSD: src/sys/boot/Makefile,v 1.18 2003/08/16 02:48:20 obrien Exp $
# $DragonFly: src/sys/boot/Makefile,v 1.4 2005/02/19 23:19:51 swildner Exp $

.if ${MACHINE_ARCH} == "amd64"
.MAKEFLAGS:  MACHINE_ARCH=i386 MACHINE=i386 REALLY_AMD64=true
.endif

.if !defined(NOFORTH) && ${MACHINE_ARCH} != "powerpc"
# Build the add-in FORTH interpreter
SUBDIR+=		ficl
.endif

# Build OpenFirmware library
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
SUBDIR+=		ofw
.endif

# Pick the machine-dependant subdir based on the target architecture.
SUBDIR+=		${MACHINE}

# Build EFI executable on ia64
.if ${MACHINE_ARCH} == "ia64"
SUBDIR+=		efi
.endif

.include <bsd.subdir.mk>
