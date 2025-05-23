#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/regex/Makefile.inc,v 1.5.2.1 2001/04/25 10:04:08 ru Exp $
# $DragonFly: src/lib/libc/regex/Makefile.inc,v 1.2 2003/06/17 04:26:44 dillon Exp $

# regex sources
.PATH: ${.CURDIR}/../libc/regex

CFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

.if ${LIB} == "c"
MAN+=	regex.3
MAN+=	re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
MLINKS+=regexec.3 regfree.3
.endif
