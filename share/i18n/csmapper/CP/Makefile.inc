# $NetBSD: src/share/i18n/csmapper/CP/Makefile.inc,v 1.3 2003/10/21 10:01:20 lukem Exp $
# $DragonFly: src/share/i18n/csmapper/CP/Makefile.inc,v 1.1 2005/03/10 16:19:18 joerg Exp $

.PATH: ${.CURDIR}/CP

SRCS_mapper.dir+=	mapper.dir.CP
SRCS_charset.pivot+=	charset.pivot.CP
CLEANFILES+=		mapper.dir.CP charset.pivot.CP
PART_CP!=		sed '/^\#/d;/^$$/d;' ${.CURDIR}/CP/CP.part

mapper.dir.CP: ${.CURDIR}/CP/CP.part ${.CURDIR}/CP/mapper.dir.CP.src
	${_MKTARGET_CREATE}
	(\
	    echo "# CP";\
	    for i in ${PART_CP}; do \
		printf '%-32s%-16s%s\n' CP$$i'/UCS' \
		    mapper_std CP/CP$$i'%UCS.mps';\
		printf '%-32s%-16s%s\n' UCS/CP$$i \
		    mapper_std CP/UCS%CP$$i'.mps';\
	    done;\
	    cat ${.CURDIR}/CP/mapper.dir.CP.src;\
	    echo;\
	) > ${.TARGET}

charset.pivot.CP: ${.CURDIR}/CP/CP.part ${.CURDIR}/CP/charset.pivot.CP.src
	${_MKTARGET_CREATE}
	(\
	    echo "# CP";\
	    for i in ${PART_CP}; do \
		printf "%-32s%-32s%d\n" CP$$i UCS 1;\
		printf "%-32s%-32s%d\n" UCS CP$$i 1;\
	    done;\
	    cat ${.CURDIR}/CP/charset.pivot.CP.src;\
	    echo;\
	) > ${.TARGET}

.for i in ${PART_CP}
FILES+=		CP${i:S/:/@/}%UCS.mps UCS%CP${i:S/:/@/}.mps
CLEANFILES+=	CP${i:S/:/@/}%UCS.mps UCS%CP${i:S/:/@/}.mps
FILESDIR_CP${i:S/:/@/}%UCS.mps=	${BINDIR}/CP
FILESDIR_UCS%CP${i:S/:/@/}.mps=	${BINDIR}/CP
.endfor
