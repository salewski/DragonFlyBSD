# $FreeBSD: src/secure/Makefile,v 1.16.2.1 2002/07/25 09:33:13 ru Exp $
# $DragonFly: src/secure/Makefile,v 1.3 2004/01/31 06:56:41 dillon Exp $

# lib must be first, or it will not work.  This is because we reference
# the lib's in the directory where they are built from the binaries we
# want to build.

SUBDIR= lib libexec usr.bin usr.sbin

SDIR= ${.CURDIR}/..

CODAI=	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend all install

CODAD=	${MAKE} ${MFLAGS} cleandir; \
	${MAKE} ${MFLAGS} obj; \
	${MAKE} ${MFLAGS} depend all distribute

# These are the programs which depend on secure libs
sprog:
	cd ${SDIR}/bin/ed; ${CODAI}
	cd ${SDIR}/sbin/init; ${CODAI}

bootstrap:
	( cd include; ${MAKE} ${MFLAGS} install )
	( cd lib; ${MAKE} ${MFLAGS} depend all install )
	${MAKE} ${MFLAGS} cleandir
	${MAKE} ${MFLAGS} obj
	${MAKE} ${MFLAGS} depend all install sprog

.include <bsd.subdir.mk>
