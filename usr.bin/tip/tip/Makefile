#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/tip/tip/Makefile,v 1.10.6.3 2002/07/17 13:25:19 ru Exp $
# $DragonFly: src/usr.bin/tip/tip/Makefile,v 1.4 2004/09/09 07:24:41 dillon Exp $
#
# Files are:
#	/etc/remote		remote host description file
#	/etc/phones		phone number file, owned by ${OWNER} and
#				mode 6??
#	/var/log/aculog		ACU accounting file, owned by ${OWNER} and
#				mode 6?? {if ACULOG defined}

.if exists(${.OBJDIR}/../libacu)
LIBACU=${.OBJDIR}/../libacu/libacu.a
.else
LIBACU=${.CURDIR}/../libacu/libacu.a
.endif

PROG=	tip
DPADD=	${LIBACU} ${LIBUTIL}
LDADD=	${LIBACU} -lutil
LINKS=  ${BINDIR}/tip ${BINDIR}/cu
MLINKS= tip.1 cu.1
MAN=	tip.1 modems.5
SRCS=	acu.c acutab.c cmds.c cmdtab.c cu.c hunt.c log.c partab.c \
	remote.c tip.c tipout.c value.c vars.c

BINDIR?=	/usr/bin
BINOWN=		uucp
BINGRP=		dialer

# XXX: there is some concern that `tip' in its current state shouldn't run
#      SUID.  If it believed it should, the mode above may still no be proper.
#BINMODE?=	4510
#INSTALLFLAGS=	-fschg

# XXX Our LINKS rules should really handle this case
#
beforeinstall:
	-chflags noschg ${DESTDIR}${BINDIR}/cu

.include <bsd.prog.mk>
