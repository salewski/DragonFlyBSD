# $FreeBSD: src/gnu/usr.bin/cvs/cvsbug/Makefile,v 1.11.2.7 2003/01/21 23:06:51 peter Exp $
# $DragonFly: src/gnu/usr.bin/cvs/cvsbug/Attic/Makefile,v 1.4 2004/01/31 06:56:37 dillon Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

SCRIPTS=	cvsbug
MAN=		cvsbug.8

CLEANFILES+=    cvsbug
VERSION!=       sed < ${CVSDIR}/configure \
		-e '/^[ 	]*VERSION=/!d' -e 's/.*=\(.*\)/\1/' -e q

cvsbug: cvsbug.in
	sed -e "s,@VERSION@,${VERSION}-DragonFly,g" ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
