# $FreeBSD: src/sys/modules/syscons/Makefile,v 1.11.2.2 2003/05/15 02:02:39 murray Exp $
# $DragonFly: src/sys/dev/misc/syscons/Makefile,v 1.2 2003/06/17 04:28:47 dillon Exp $

SUBDIR =
SUBDIR += blank
SUBDIR += daemon
SUBDIR += dragon
SUBDIR += fade
SUBDIR += fire
SUBDIR += green
SUBDIR += logo
SUBDIR += rain
SUBDIR += snake
SUBDIR += star
SUBDIR += warp

.if ${MACHINE_ARCH} == "i386"
SUBDIR += apm
.endif

.include <bsd.subdir.mk>
