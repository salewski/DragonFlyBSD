# $FreeBSD: src/share/syscons/fonts/Makefile,v 1.24.2.7 2002/08/07 16:31:52 ru Exp $
# $DragonFly: src/share/syscons/fonts/Makefile,v 1.2 2003/06/17 04:37:02 dillon Exp $

FONTS=	cp437-8x8.fnt cp437-8x14.fnt cp437-8x16.fnt \
	cp437-thin-8x8.fnt cp437-thin-8x16.fnt \
	cp850-8x8.fnt cp850-8x14.fnt cp850-8x16.fnt \
	cp850-thin-8x8.fnt cp850-thin-8x16.fnt \
	cp865-8x8.fnt cp865-8x14.fnt cp865-8x16.fnt \
	cp865-thin-8x8.fnt cp865-thin-8x16.fnt \
	cp866-8x8.fnt cp866-8x14.fnt cp866-8x16.fnt \
	cp866b-8x16.fnt cp866c-8x16.fnt \
	cp866u-8x16.fnt cp866u-8x14.fnt cp866u-8x8.fnt \
	cp1251-8x8.fnt cp1251-8x14.fnt cp1251-8x16.fnt \
	iso-8x8.fnt iso-8x14.fnt iso-8x16.fnt \
	iso02-8x8.fnt iso02-8x14.fnt iso02-8x16.fnt \
	iso04-8x8.fnt iso04-8x14.fnt iso04-8x16.fnt iso04-wide-8x16.fnt \
	iso04-vga9-8x8.fnt iso04-vga9-8x14.fnt iso04-vga9-8x16.fnt \
	iso04-vga9-wide-8x16.fnt \
	iso05-8x8.fnt iso05-8x14.fnt iso05-8x16.fnt \
	iso07-8x8.fnt iso07-8x14.fnt iso07-8x16.fnt \
	iso08-8x8.fnt iso08-8x14.fnt iso08-8x16.fnt \
	iso09-8x16.fnt \
	iso15-8x8.fnt iso15-8x14.fnt iso15-8x16.fnt \
	iso-thin-8x16.fnt iso15-thin-8x16.fnt \
	koi8-r-8x8.fnt koi8-r-8x14.fnt koi8-r-8x16.fnt \
	koi8-rb-8x16.fnt koi8-rc-8x16.fnt \
	koi8-u-8x8.fnt koi8-u-8x14.fnt koi8-u-8x16.fnt \
	swiss-8x16.fnt swiss-8x8.fnt \
	INDEX.fonts

FONTDIR = ${SHAREDIR}/syscons/fonts

NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FONTS} \
		${DESTDIR}${FONTDIR}

.include <bsd.prog.mk>
