# $FreeBSD: src/gnu/usr.bin/binutils/libiberty/Makefile,v 1.3.2.6 2002/09/01 23:39:16 obrien Exp $
# $DragonFly: src/gnu/usr.bin/binutils/libiberty/Attic/Makefile,v 1.2 2003/06/17 04:25:45 dillon Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/libiberty

LIB=	iberty
SRCS=	argv.c choose-temp.c concat.c cp-demangle.c cplus-dem.c \
	dyn-string.c getopt.c getopt1.c getpwd.c getruntime.c \
	hex.c floatformat.c hashtab.c lbasename.c make-temp-file.c \
	objalloc.c obstack.c safe-ctype.c xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c
.if defined(BOOTSTRAPPING)
SRCS+=	basename.c
.endif
WARNS=	1
CFLAGS+=	-DHAVE_CONFIG_H
INTERNALLIB=	true

.include <bsd.lib.mk>
