#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/locate/locate/Makefile,v 1.11.2.4 2002/08/07 16:32:01 ru Exp $
# $DragonFly: src/usr.bin/locate/locate/Makefile,v 1.2 2003/06/17 04:29:28 dillon Exp $

PROG=	locate
SRCS=	util.c locate.c
CFLAGS+= -I${.CURDIR} -DMMAP # -DDEBUG (print time) -O2 (10% faster)
SCRIPTS=updatedb.sh mklocatedb.sh concatdb.sh
MAN=	locate.1 locate.updatedb.8

SCRIPTSDIR=	${LIBEXECDIR}
.for script in ${SCRIPTS}
SCRIPTSNAME_${script}=	locate.${script:R}
.endfor
MLINKS+= locate.updatedb.8 updatedb.8

# only /usr/src/etc/Makefile install files in /etc
#	${INSTALL} -o root -g wheel -m 644 \
#		${.CURDIR}/locate.rc ${DESTDIR}/etc

.include "../../Makefile.inc"
.include <bsd.prog.mk>
