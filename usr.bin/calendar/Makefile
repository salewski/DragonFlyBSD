#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/calendar/Makefile,v 1.12.8.6 2003/04/06 20:04:56 dwmalone Exp $
# $DragonFly: src/usr.bin/calendar/Makefile,v 1.2 2003/06/17 04:29:25 dillon Exp $

PROG=	calendar
SRCS=   calendar.c io.c day.c ostern.c paskha.c
INTER=          de_DE.ISO8859-1 fr_FR.ISO8859-1 hr_HR.ISO8859-2 ru_RU.KOI8-R
DE_LINKS=       de_DE.ISO_8859-1 de_DE.ISO8859-15 de_DE.ISO_8859-15
FR_LINKS=       fr_FR.ISO8859-15
HR_LINKS=       hr_HR.ISO_8859-2
TEXTMODE?=	444

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/calendars/calendar.* ${DESTDIR}${SHAREDIR}/calendar
.for lang in ${INTER}
	mkdir -p ${DESTDIR}${SHAREDIR}/calendar/${lang}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
		${.CURDIR}/calendars/${lang}/calendar.* \
		${DESTDIR}${SHAREDIR}/calendar/${lang} 
.endfor
.for link in ${DE_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s de_DE.ISO8859-1 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor
.for link in ${FR_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s fr_FR.ISO8859-1 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor
.for link in ${HR_LINKS}
	rm -rf ${DESTDIR}${SHAREDIR}/calendar/${link}
	ln -s hr_HR.ISO8859-2 ${DESTDIR}${SHAREDIR}/calendar/${link}
.endfor

.include <bsd.prog.mk>
