#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/quad/Makefile.inc,v 1.8 1999/08/28 00:00:27 peter Exp $
# $DragonFly: src/lib/libcr/quad/Attic/Makefile.inc,v 1.3 2003/07/24 21:41:54 dillon Exp $

# Quad support, if needed
.PATH: ${.CURDIR}/../libcr/${MACHINE_ARCH}/quad ${.CURDIR}/../libcr/quad

.if ${MACHINE_ARCH} == "i386"

SRCS+=	cmpdi2.c divdi3.c moddi3.c qdivrem.c ucmpdi2.c udivdi3.c umoddi3.c

.else

SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c fixdfdi.c \
	fixsfdi.c fixunsdfdi.c fixunssfdi.c floatdidf.c floatdisf.c \
	floatunsdidf.c iordi3.c lshldi3.c lshrdi3.c moddi3.c muldi3.c \
	negdi2.c notdi2.c qdivrem.c subdi3.c ucmpdi2.c udivdi3.c umoddi3.c \
	xordi3.c

.endif
