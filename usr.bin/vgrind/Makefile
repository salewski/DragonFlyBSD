#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD: src/usr.bin/vgrind/Makefile,v 1.7.6.3 2002/02/18 16:16:57 ru Exp $
# $DragonFly: src/usr.bin/vgrind/Makefile,v 1.2 2003/06/17 04:29:33 dillon Exp $

PROG=	vfontedpr
SRCS=	regexp.c vfontedpr.c
SCRIPTS=vgrind.sh
FILES=	vgrindefs.src vgrindefs.src.db tmac.vgrind
FILESNAME_vgrindefs.src=	vgrindefs
FILESNAME_vgrindefs.src.db=	vgrindefs.db
FILESDIR=	${SHAREDIR}/misc
FILESDIR_tmac.vgrind=	${SHAREDIR}/tmac
MAN=	vgrind.1 vgrindefs.5

BINDIR=	/usr/libexec
SCRIPTSDIR=/usr/bin

EXTRA=	vgrindefs.src.db
CLEANFILES+=	${EXTRA}

all: ${EXTRA}

${EXTRA}: ${.CURDIR}/vgrindefs.src
	cap_mkdb -f vgrindefs.src ${.CURDIR}/vgrindefs.src

.include <bsd.prog.mk>
