# $DragonFly: src/sys/net/pf/Makefile,v 1.3 2005/02/18 11:41:42 corecode Exp $

KMOD=	pf
SRCS=	if_pflog.c pf.c pf_if.c pf_ioctl.c pf_norm.c pf_osfp.c pf_subr.c
SRCS+=	pf_table.c
SRCS+=	use_pflog.h use_pfsync.h opt_inet.h opt_inet6.h
SRCS+=	opt_icmp_bandlim.h
NOMAN=

use_pflog.h:
	echo "#define NPFLOG 1" > ${.TARGET}

.if !defined(BUILDING_WITH_KERNEL)
use_pfsync.h:
	echo "#define NPFSYNC 0" > ${.TARGET}

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif	# !BUILDING_WITH_KERNEL

.include <bsd.kmod.mk>
