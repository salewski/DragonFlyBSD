# $FreeBSD: src/gnu/usr.bin/man/man/Makefile,v 1.27.2.4 2002/07/17 13:25:17 ru Exp $
# $DragonFly: src/gnu/usr.bin/man/man/Makefile,v 1.3 2004/11/17 19:15:54 dillon Exp $
#

MAN_LIBZ=YES

PROG=	man
SRCS=	man.c manpath.c glob.c
BINOWN= man
BINMODE=4555
.if !defined(NOFSCHG)
INSTALLFLAGS=	-fschg
.endif

CFLAGS+= -I${LIBDESTDIR}
DPADD=	${LIBMAN}
LDADD=	${LIBMAN}

.if defined(MAN_LIBZ) && !empty(MAN_LIBZ)
DPADD+=	${LIBZ}
LDADD+=	-lz
CFLAGS+=-DHAVE_LIBZ=1
.endif

CFLAGS+= -I${.CURDIR}/../lib -DSTDC_HEADERS -DPOSIX -DHAS_TROFF
CFLAGS+= -DDO_COMPRESS -DSETREUID -DCATMODE=0644
CLEANFILES+=	man.1

.PATH:	${.CURDIR}/../manpath

man.1: ${.CURDIR}/man.man
	@${ECHO} echo Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
