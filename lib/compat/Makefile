# $FreeBSD: src/lib/compat/Makefile,v 1.6.2.2 2001/03/23 22:09:37 peter Exp $
# $DragonFly: src/lib/compat/Attic/Makefile,v 1.2 2003/06/17 04:26:40 dillon Exp $

SUBDIR=

.if ${MACHINE_ARCH} == "i386"
# Note that compat21 is *not* for the 2.1.x branch!
.if defined(COMPAT1X) || defined(RELEASEDIR)
SUBDIR+=	compat1x
.endif
.if defined(COMPAT20) || defined(RELEASEDIR)
SUBDIR+=	compat20
.endif
.if defined(COMPAT21) || defined(RELEASEDIR)
SUBDIR+=	compat21
.endif
.if defined(COMPAT22) || defined(RELEASEDIR)
SUBDIR+=	compat22
.endif
.endif

.if exists (${.CURDIR}/compat3x.${MACHINE_ARCH})
.if defined(COMPAT3X) || defined(RELEASEDIR)
SUBDIR+=	compat3x.${MACHINE_ARCH}
.endif
.endif

.if exists (${.CURDIR}/compat4x.${MACHINE_ARCH})
.if defined(COMPAT4X) || defined(RELEASEDIR)
SUBDIR+=	compat4x.${MACHINE_ARCH}
.endif
.endif

.include <bsd.subdir.mk>
