# $FreeBSD: src/libexec/rtld-elf/Makefile,v 1.10.2.6 2002/06/22 17:03:13 jdp Exp $
# $DragonFly: src/libexec/rtld-elf/Makefile,v 1.6 2005/02/04 00:33:49 joerg Exp $

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c lockdflt.c map_object.c malloc.c \
		xmalloc.c debug.c reloc.c
MAN=		rtld.1
CFLAGS+=	-Wall -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
INSTALLFLAGS=	-fschg -C -b
MLINKS=		rtld.1 ld-elf.so.1.1

.if exists(${.CURDIR}/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.endif

.ifdef LDSCRIPT
OBJS+=		dyn_hack.so
LDFLAGS+=	-Wl,-T,${LDSCRIPT} -Wl,-E -Wl,-Bstatic
DPADD=		${LIBC}
LDADD=		-lc
.else
CFLAGS+=	-fpic -DPIC
LDFLAGS+=	-shared -Wl,-Bsymbolic
DPADD=		${LIBC_PIC}
LDADD=		-lc_pic
.endif

dyn_hack.so:
	${CC} -shared -nostdlib -o dyn_hack.so -x c /dev/null

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
