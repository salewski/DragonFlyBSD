# $FreeBSD: src/usr.sbin/pcvt/keycap/Makefile,v 1.9.6.3 2002/08/07 16:32:02 ru Exp $
# $DragonFly: src/usr.sbin/pcvt/keycap/Attic/Makefile,v 1.2 2003/06/17 04:29:59 dillon Exp $

LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN	=	keycap.3
MLINKS+=	keycap.3 kgetent.3 \
		keycap.3 kgetnum.3 \
		keycap.3 kgetflag.3 \
		keycap.3 kgetstr.3
MAN	+=	man5/keycap.5
#CLEANFILES+=	keycap.0 man5/keycap.0

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
