# $FreeBSD: src/usr.sbin/pcvt/vgaio/Makefile,v 1.12.2.2 2001/07/19 01:45:31 kris Exp $
# $DragonFly: src/usr.sbin/pcvt/vgaio/Attic/Makefile,v 1.3 2004/03/31 20:22:14 cpressey Exp $

.if !exists(${.CURDIR}/../Makefile.inc)
error:
	@echo
	@echo "                      You MUST link/copy"
	@echo
	@echo "       ../Makefile.inc.NetBSD    or   ../Makefile.inc.FreeBSD"
	@echo
	@echo "             to ../Makefile.inc before you can proceed !"
	@echo
.else

PROG=	vgaio
MAN=	vgaio.8

SRCS=	vgaio.y lex.l y.tab.h
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I

DPADD=	${LIBM} ${LIBL}
LDADD=	-lm -ll

CFLAGS+= -I${.OBJDIR} -I${.CURDIR}

CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "vgaio is not installed automatically ...."

.include <bsd.prog.mk>

.endif
