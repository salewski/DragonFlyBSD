# $NetBSD: src/share/i18n/csmapper/EBCDIC/Makefile.inc,v 1.3 2003/10/21 10:01:21 lukem Exp $
# $DragonFly: src/share/i18n/csmapper/EBCDIC/Makefile.inc,v 1.1 2005/03/10 16:19:18 joerg Exp $

.PATH: ${.CURDIR}/EBCDIC

SRCS_mapper.dir+=	mapper.dir.EBCDIC
SRCS_charset.pivot+=	charset.pivot.EBCDIC
CLEANFILES+=		mapper.dir.EBCDIC charset.pivot.EBCDIC
PART_EBCDIC!=		sed '/^\#/d;/^$$/d;' ${.CURDIR}/EBCDIC/EBCDIC.part

mapper.dir.EBCDIC: ${.CURDIR}/EBCDIC/EBCDIC.part
	${_MKTARGET_CREATE}
	(\
	    echo "# EBCDIC";\
	    for i in ${PART_EBCDIC}; do \
		printf '%-32s%-16s%s\n' EBCDIC-$$i'/UCS' \
		    mapper_std EBCDIC/EBCDIC-$$i'%UCS.mps';\
		printf '%-32s%-16s%s\n' UCS/EBCDIC-$$i \
		    mapper_std EBCDIC/EBCDIC-$$i'%UCS.mps';\
	    done;\
	    echo;\
	) > ${.TARGET}

charset.pivot.EBCDIC: ${.CURDIR}/EBCDIC/EBCDIC.part
	${_MKTARGET_CREATE}
	(\
	    echo "# EBCDIC";\
	    for i in ${PART_EBCDIC}; do \
		printf "%-32s%-32s%d\n" EBCDIC-$$i UCS 1;\
		printf "%-32s%-32s%d\n" UCS EBCDIC-$$i 1;\
	    done;\
	    echo;\
	) > ${.TARGET}

.for i in ${PART_EBCDIC}
FILES+=					EBCDIC-${i:S/:/@/}%UCS.mps
FILESDIR_EBCDIC-${i:S/:/@/}%UCS.mps=	${BINDIR}/EBCDIC
CLEANFILES+=				EBCDIC-${i:S/:/@/}%UCS.mps
FILES+=					UCS%EBCDIC-${i:S/:/@/}.mps
FILESDIR_UCS%EBCDIC-${i:S/:/@/}.mps=	${BINDIR}/EBCDIC
CLEANFILES+=				UCS%EBCDIC-${i:S/:/@/}.mps
.endfor
