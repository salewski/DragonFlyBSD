# $FreeBSD: src/gnu/usr.bin/binutils/as/Makefile,v 1.10.2.2 2002/09/01 23:39:13 obrien Exp $
# $DragonFly: src/gnu/usr.bin/binutils/as/Attic/Makefile,v 1.2 2003/06/17 04:25:44 dillon Exp $

.include "${.CURDIR}/../Makefile.inc0"

GASES=	${BINUTIL_ARCH}
FORMATS=freebsd

# Only build the cross tools when not cross-compiling.
.if ${TARGET_ARCH} == ${MACHINE_ARCH}
.if defined(CROSS_ARCH)
.for _g in ${CROSS_ARCH}
.if (${GASES:R:M${_g:R}} == "")
GASES+= $(_g)
.endif
.endfor
.endif
.if defined(CROSS_FORMATS)
.for _f in ${CROSS_FORMATS}
.if (${FORMATS:R:M${_f:R}} == "")
FORMATS+= $(_f)
.endif
.endfor
.endif
.endif

.for _g in ${GASES}
.for _f in ${FORMATS}
.if exists(${.CURDIR}/${_g}-${_f})
SUBDIR+=	${_g}-${_f}
.endif
.endfor
.endfor

.include <bsd.subdir.mk>
