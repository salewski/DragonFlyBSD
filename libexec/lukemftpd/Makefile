#	@(#)Makefile	8.2 (Berkeley) 4/4/94
# $FreeBSD: src/libexec/lukemftpd/Makefile,v 1.5.2.2 2002/08/30 06:50:58 obrien Exp $
# $DragonFly: src/libexec/lukemftpd/Attic/Makefile,v 1.3 2004/01/31 06:56:40 dillon Exp $

LUKEMFTPD=	${.CURDIR}/../../contrib/lukemftpd
.PATH: ${LUKEMFTPD}/src

PROG=	lukemftpd
MAN=	lukemftpd.8 ftpd.conf.5 ftpusers.5
MLINKS=	ftpusers.5 ftpchroot.5
SRCS=	cmds.c conf.c ftpd.c ftpcmd.y popen.c

WFORMAT= 0
CFLAGS+= -I${.CURDIR} -I${LUKEMFTPD} -I${LUKEMFTPD}/src
YFLAGS=

LDADD=	-lmd -lcrypt -lutil
DPADD=	${LIBMD} ${LIBCRYPT} ${LIBUTIL}

# XXX Kluge! Conversation mechanism needs to be fixed.
LDADD+=	-lopie
DPADD+=	${LIBOPIE}

LSDIR=	../../bin/ls
.PATH: ${.CURDIR}/${LSDIR}
SRCS+=	ls-unmain.c cmp.c print.c util.c
CFLAGS+=-I${.CURDIR}/${LSDIR}
DPADD+=	${LIBM}
LDADD+=	-lm

CLEANFILES+=	ls-unmain.c
ls-unmain.c: ls.c
	sed -e 's/^main(/ls_main(/g' -e 's,extern.h,${LSDIR}/extern.h,' \
	    ${.ALLSRC} > ${.TARGET}

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

ftpd.o ftpcmd.o: version.h

lukemftpd.8: ftpd.8
	ln -sf ${.ALLSRC} ${.TARGET}

CLEANFILES+= lukemftpd.8

.include <bsd.prog.mk>
