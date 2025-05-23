#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/lib/libc/compat-43/Makefile.inc,v 1.9.2.1 2001/04/25 10:04:04 ru Exp $
# $DragonFly: src/lib/libcr/compat-43/Attic/Makefile.inc,v 1.3 2003/07/24 21:41:45 dillon Exp $

# compat-43 sources
.PATH: ${.CURDIR}/../libcr/${MACHINE_ARCH}/compat-43 ${.CURDIR}/../libcr/compat-43

SRCS+=	creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c sigcompat.c

.if ${LIB} == "c"
MAN+=	creat.2 killpg.2 sigblock.2 sigpause.2 sigsetmask.2 sigvec.2
MAN+=	gethostid.3 setruid.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=setruid.3 setrgid.3
.endif
