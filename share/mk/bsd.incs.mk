# $FreeBSD: src/share/mk/bsd.incs.mk,v 1.3.2.1 2002/07/22 14:21:51 ru Exp $
# $DragonFly: src/share/mk/bsd.incs.mk,v 1.4 2004/04/05 05:30:13 dillon Exp $

.if !target(__<bsd.init.mk>__)
.error bsd.incs.mk cannot be included directly.
.endif

INCSGROUPS?=	INCS

.if !target(buildincludes)
.for group in ${INCSGROUPS}
buildincludes: ${${group}}
.endfor
.endif

all: buildincludes

.if !target(installincludes)
.for group in ${INCSGROUPS}
.if defined(${group}) && !empty(${group})

${group}OWN?=	${BINOWN}
${group}GRP?=	${BINGRP}
${group}MODE?=	${NOBINMODE}
${group}DIR?=	${INCLUDEDIR}

_${group}INCS=
.for header in ${${group}}
.if defined(${group}OWN_${header:T}) || defined(${group}GRP_${header:T}) || \
    defined(${group}MODE_${header:T}) || defined(${group}DIR_${header:T}) || \
    defined(${group}NAME_${header:T})
${group}OWN_${header:T}?=	${${group}OWN}
${group}GRP_${header:T}?=	${${group}GRP}
${group}MODE_${header:T}?=	${${group}MODE}
${group}DIR_${header:T}?=	${${group}DIR}
.if defined(${group}NAME)
${group}NAME_${header:T}?=	${${group}NAME}
.else
${group}NAME_${header:T}?=	${header:T}
.endif
installincludes: _${group}INS_${header:T}
_${group}INS_${header:T}: ${header}
	${INSTALL} -C -o ${${group}OWN_${.ALLSRC:T}} \
	    -g ${${group}GRP_${.ALLSRC:T}} -m ${${group}MODE_${.ALLSRC:T}} \
	    ${.ALLSRC} \
	    ${DESTDIR}${${group}DIR_${.ALLSRC:T}}/${${group}NAME_${.ALLSRC:T}}
.else
_${group}INCS+= ${header}
.endif
.endfor
.if !empty(_${group}INCS)
installincludes: _${group}INS
_${group}INS: ${_${group}INCS}
.if defined(${group}NAME)
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}/${${group}NAME}
.else
	${INSTALL} -C -o ${${group}OWN} -g ${${group}GRP} -m ${${group}MODE} \
	    ${.ALLSRC} ${DESTDIR}${${group}DIR}
.endif
.endif

.endif defined(${group}) && !empty(${group})
.endfor

.if defined(INCSLINKS) && !empty(INCSLINKS)
installincludes:
	@set ${INCSLINKS}; \
	while test $$# -ge 2; do \
		l=$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		${ECHO} $$t -\> $$l; \
		ln -fhs $$l $$t; \
	done; true
.endif
.endif !target(installincludes)

# include files are not installed when building bootstrap programs
#
.if !defined(BOOTSTRAPPING)
realinstall: installincludes
.ORDER: beforeinstall installincludes
.endif
