# $DragonFly: src/gnu/usr.bin/binutils215/as/Attic/Makefile,v 1.1 2004/12/20 13:14:44 asmodai Exp $

.include "${.CURDIR}/../Makefile.inc0"

GASES=	${BINUTIL_ARCH}
FORMATS=dragonfly

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
