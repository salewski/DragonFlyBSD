# $FreeBSD: src/gnu/usr.bin/groff/tmac/Makefile,v 1.21.2.8 2003/02/14 15:22:14 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/tmac/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

TMACOWN?=	${BINOWN}
TMACGRP?=	${BINGRP}
TMACMODE?=	${NOBINMODE}
TMACDIR?=	${SHAREDIR}/tmac
MDOCDIR?=	${TMACDIR}/mdoc

MAN=		groff_ms.7 groff_man.7 groff_me.7 groff_mdoc.7
MAN+=		groff_trace.7 groff_www.7
MLINKS=		groff_ms.7 ms.7
MLINKS+=	groff_man.7 man.7
MLINKS+=	groff_me.7 me.7
MLINKS+=	groff_mdoc.7 mdoc.7 groff_mdoc.7 mdoc.samples.7

CLEANFILES=	${MAN} ${MDOCFILES:S/$/-s/} ${STRIPFILES:S/$/-s/} ${SPECIALFILES:S/$/-s/}

NORMALFILES=	mandoc.tmac andoc.tmac an-old.tmac \
		me.tmac \
		mdoc.tmac \
		pic.tmac \
		a4.tmac \
		ec.tmac \
		safer.tmac \
		trace.tmac \
		ps.tmac psold.tmac pspic.tmac psatk.tmac \
		dvi.tmac \
		tty.tmac tty-char.tmac \
		latin1.tmac cp1047.tmac \
		X.tmac Xps.tmac \
		lj4.tmac \
		lbp.tmac \
		html.tmac www.tmac \
		eqnrc \
		troffrc troffrc-end \
		hyphen.us
SPECIALFILES=	an.tmac man.tmac s.tmac ms.tmac
STRIPFILES=	e.tmac doc.tmac mdoc.local
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms \
		ru.KOI8-R

all: ${MDOCFILES:S/$/-s/} ${STRIPFILES:S/$/-s/} ${SPECIALFILES:S/$/-s/}

.for f in ${MDOCFILES} ${STRIPFILES}
$f-s: $f
	sed -f ${DIST_DIR}/strip.sed ${.ALLSRC} > ${.TARGET}
.endfor

.for f in ${SPECIALFILES}
$f-s: $f
	sed -e "s;@TMAC_AN_PREFIX@;${tmac_an_prefix};g" \
	    -e "s;@TMAC_S_PREFIX@;${tmac_s_prefix};g" \
		${.ALLSRC} > ${.TARGET}
.endfor

beforeinstall:
	cd ${DIST_DIR}; \
	    ${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${NORMALFILES} ${DESTDIR}${TMACDIR}
	cd ${.CURDIR}; \
	    ${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    hyphen.ru ${DESTDIR}${TMACDIR}
	cd ${.OBJDIR}
.for f in ${STRIPFILES} ${SPECIALFILES}
	${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    $f-s ${DESTDIR}${TMACDIR}/$f
.endfor
.for f in ${MDOCFILES}
	${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    $f-s ${DESTDIR}${MDOCDIR}/$f
.endfor
.if !exists(${DESTDIR}${TMACDIR}/man.local)
	${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${DIST_DIR}/man.local ${DESTDIR}${TMACDIR}
.endif

.include <bsd.prog.mk>
