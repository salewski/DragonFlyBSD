# $FreeBSD: src/gnu/usr.bin/binutils/libopcodes/Makefile,v 1.6.2.3 2002/09/01 23:39:17 obrien Exp $
# $DragonFly: src/gnu/usr.bin/binutils/libopcodes/Attic/Makefile,v 1.2 2003/06/17 04:25:45 dillon Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=	opcodes
SRCS+=	dis-buf.c disassemble.c
CFLAGS+= -I${SRCDIR}/opcodes -I${SRCDIR}/bfd
INTERNALLIB=	true

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if	defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH: ${BINUTILSDISTDIR}/bfd ${BINUTILSDISTDIR}/opcodes
CFLAGS+= -I${BINUTILSDISTDIR}/opcodes -I${BINUTILSDISTDIR}/bfd
CFLAGS+= -I${BINUTILSDISTDIR}/include
.endif

.include <bsd.lib.mk>
