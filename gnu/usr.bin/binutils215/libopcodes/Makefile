# $DragonFly: src/gnu/usr.bin/binutils215/libopcodes/Attic/Makefile,v 1.1 2004/12/20 13:14:45 asmodai Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=	opcodes
SRCS+=	dis-buf.c disassemble.c dis-init.c
CFLAGS+= -I${SRCDIR}/opcodes -I${SRCDIR}/bfd -I${.OBJDIR}/../libbfd
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
