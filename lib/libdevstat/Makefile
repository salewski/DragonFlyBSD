# $FreeBSD: src/lib/libdevstat/Makefile,v 1.4.2.1 2001/04/25 10:04:27 ru Exp $
# $DragonFly: src/lib/libdevstat/Makefile,v 1.3 2004/01/31 06:56:39 dillon Exp $

LIB=	devstat
SRCS=	devstat.c
INCS=	devstat.h

MAN=	devstat.3

MLINKS+=devstat.3 getnumdevs.3 
MLINKS+=devstat.3 getgeneration.3 
MLINKS+=devstat.3 getversion.3
MLINKS+=devstat.3 checkversion.3 
MLINKS+=devstat.3 getdevs.3 
MLINKS+=devstat.3 selectdevs.3
MLINKS+=devstat.3 buildmatch.3 
MLINKS+=devstat.3 compute_stats.3
MLINKS+=devstat.3 compute_etime.3

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../sys

.include <bsd.lib.mk>
