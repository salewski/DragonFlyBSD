# $FreeBSD: src/gnu/usr.bin/man/lib/Makefile,v 1.11.6.1 2002/07/19 18:46:24 ru Exp $
# $DragonFly: src/gnu/usr.bin/man/lib/Makefile,v 1.2 2003/06/17 04:25:46 dillon Exp $

LIB=	man
INTERNALLIB= true

CFLAGS+= -DSTDC_HEADERS
CLEANFILES+= config.h
SRCS=	gripes.c util.c

# Kludge to create config.h for other modules.  The library itself doesn't
# depend on config.h.
SRCS+=	config.h

config.h: ${.CURDIR}/config.h_dist ${.CURDIR}/../Makefile.inc
	sed -e 's,%apropos%,${apropos},' -e 's,%whatis%,${whatis},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%nroff%,${nroff},' -e 's,%tbl%,${tbl},' \
	    -e 's,%col%,${col},' -e 's,%pic%,${pic},' \
	    -e 's,%eqn%,${eqn},' -e 's,%neqn%,${neqn},' \
	    -e 's,%vgrind%,${vgrind},' -e 's,%refer%,${refer},' \
	    -e 's,%grap%,${grap},' -e 's,%zcat%,${zcat},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	    -e 's,%compext%,${compext},' \
	    ${.CURDIR}/config.h_dist > ${.TARGET}

.include <bsd.lib.mk>
