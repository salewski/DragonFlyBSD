# $FreeBSD: src/gnu/usr.bin/groff/contrib/mm/Makefile,v 1.1.2.6 2003/02/14 15:22:13 ru Exp $
# $DragonFly: src/gnu/usr.bin/groff/contrib/mm/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

SCRIPTS=	mmroff.pl
MAN=		mmroff.1

TMACDIR?=	${SHAREDIR}/tmac

FILES=		m.tmac mm.tmac mse.tmac mmse.tmac
FILESDIR_m.tmac=${TMACDIR}
FILESDIR_mse.tmac=${TMACDIR}
FILES+=		0.MT 5.MT 4.MT ms.cov se_ms.cov
FILESDIR=	${TMACDIR}/mm
LOCALE=		locale se_locale

MAN+=		groff_mm.7 groff_mmse.7
MLINKS=		groff_mm.7 mm.7 groff_mmse.7 mmse.7

CLEANFILES+=	${MAN}

.include <bsd.prog.mk>

.PATH: ${DIST_DIR}/mm

beforeinstall:
.for locale in ${LOCALE}
.if !exists(${DESTDIR}${TMACDIR}/mm/${locale})
	${INSTALL} -o ${FILESOWN} -g ${FILESGRP} -m ${FILESMODE} \
		/dev/null ${DESTDIR}${TMACDIR}/mm/${locale}
.endif
.endfor
