# $FreeBSD: src/kerberos5/lib/libroken/Makefile,v 1.16 2004/02/05 18:51:49 ru Exp $
# $DragonFly: src/kerberos5/lib/libroken/Makefile,v 1.5 2005/01/16 14:25:46 eirikn Exp $

LIB=	roken
INCS=	roken.h roken-common.h

SRCS=	base64.c \
	bswap.c \
	concat.c \
	copyhostent.c \
	ecalloc.c \
	emalloc.c \
	environment.c \
	eread.c \
	erealloc.c \
	esetenv.c \
	estrdup.c \
	ewrite.c \
	get_default_username.c \
	get_window_size.c \
	getaddrinfo_hostspec.c \
	getarg.c \
	getnameinfo_verified.c \
	hostent_find_fqdn.c \
	issuid.c \
	k_getpwnam.c \
	k_getpwuid.c \
	mini_inetd.c \
	ndbm_wrap.c \
	net_read.c \
	net_write.c \
	parse_bytes.c \
	parse_time.c \
	parse_units.c \
	resolve.c \
	roken.h \
	roken_gethostby.c \
	rtbl.c \
	simple_exec.c \
	snprintf.c \
	socket.c \
	strcollect.c \
	strlwr.c \
	strndup.c \
	strnlen.c \
	strsep_copy.c \
	strupr.c \
	timeval.c \
	tm2time.c \
	unvis.c \
	verify.c \
	vis.c \
	warnerr.c \
	write_pid.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I.

CLEANFILES= roken.h

roken.h:
	${MAKEROKEN} > ${.TARGET}

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/roken
