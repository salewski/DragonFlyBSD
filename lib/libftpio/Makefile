# $FreeBSD: src/lib/libftpio/Makefile,v 1.9.2.2 2001/04/25 10:04:30 ru Exp $
# $DragonFly: src/lib/libftpio/Makefile,v 1.3 2004/08/16 12:59:21 joerg Exp $

LIB=		ftpio
SHLIB_MAJOR=	5

SRCS=		ftpio.c ftperr.c
INCS=		ftpio.h
CFLAGS+=	-I${.CURDIR} -Wall
CFLAGS+=	-DINET6
MAN=		ftpio.3
CLEANFILES=	ftperr.c
WARNS?=		6

ftperr.c:	ftp.errors
	@echo '#include <stdio.h>' > ${.TARGET}
	@echo '#include "ftpio.h"' >> ${.TARGET}
	@echo "struct ftperr ftpErrList[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "  { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "};" >> ${.TARGET}
	@echo -n "int const ftpErrListLength = " >> ${.TARGET}
	@echo "sizeof(ftpErrList) / sizeof(*ftpErrList);" >> ${.TARGET}

.include <bsd.lib.mk>
