# $NetBSD: src/share/i18n/csmapper/GB/Makefile.inc,v 1.3 2003/10/21 10:01:21 lukem Exp $
# $DragonFly: src/share/i18n/csmapper/GB/Makefile.inc,v 1.1 2005/03/10 16:19:35 joerg Exp $

.PATH: ${.CURDIR}/GB

SRCS_mapper.dir+=	mapper.dir.GB
SRCS_charset.pivot+=	charset.pivot.GB
CLEANFILES+=		mapper.dir.GB charset.pivot.GB
MAPS_GB=		GB2312%UCS.mps \
			UCS%GB2312.mps

mapper.dir.GB: ${.CURDIR}/GB/mapper.dir.GB.src
	${_MKTARGET_CREATE}
	(echo "# GB" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.GB: ${.CURDIR}/GB/charset.pivot.GB.src
	${_MKTARGET_CREATE}
	(echo "# GB" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_GB}
CLEANFILES+= ${MAPS_GB}
.for i in ${MAPS_GB}
FILESDIR_$i= ${BINDIR}/GB
.endfor
