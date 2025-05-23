# $NetBSD: src/share/i18n/esdb/UTF/Makefile.inc,v 1.3 2003/10/21 10:01:21 lukem Exp $
# $DragonFly: src/share/i18n/esdb/UTF/Makefile.inc,v 1.1 2005/03/10 16:20:15 joerg Exp $

.PATH: ${.CURDIR}/UTF

CODE:=		UTF
.include "${.CURDIR}/Makefile.part"

UTF-16-mod=	UTF1632
UTF-16-var=	utf16
UTF-16BE-mod=	UTF1632
UTF-16BE-var=	utf16,big,force
UTF-16LE-mod=	UTF1632
UTF-16LE-var=	utf16,little,force
UTF-32-mod=	UTF1632
UTF-32-var=	utf32
UTF-32BE-mod=	UTF1632
UTF-32BE-var=	utf32,big,force
UTF-32LE-mod=	UTF1632
UTF-32LE-var=	utf32,little,force
UTF-8-mod=	UTF8
UTF-8-var=	utf8

.for i in ${UTF_PART}
UTF-$i.src: UTF.src
	${_MKTARGET_CREATE}
	sed 's/UTF-x/UTF-$i/;s/UTF-mod/${UTF-$i-mod}/;s/UTF-var/${UTF-$i-var}/' $> > $@
CLEANFILES+= UTF-$i.src
.endfor
