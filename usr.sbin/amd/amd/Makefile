# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/amd/Makefile,v 1.17.2.1 2001/04/25 12:09:12 ru Exp $
# $DragonFly: src/usr.sbin/amd/amd/Makefile,v 1.2 2003/06/17 04:29:52 dillon Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/amd

PROG=		amd
MAN=		amd.8

SRCS=		conf_parse.y conf_tok.l
SRCS+=		am_ops.c amd.c amfs_auto.c amfs_direct.c amfs_error.c
SRCS+=		amfs_host.c amfs_inherit.c amfs_link.c amfs_linkx.c amfs_nfsl.c
SRCS+=		amfs_nfsx.c amfs_program.c amfs_root.c amfs_toplvl.c
SRCS+=		amfs_union.c amq_subr.c amq_svc.c autil.c clock.c conf.c
SRCS+=		get_args.c info_file.c info_ndbm.c info_nis.c info_passwd.c 
SRCS+=		info_union.c map.c mapc.c mntfs.c nfs_prot_svc.c nfs_start.c 
SRCS+=		nfs_subr.c ops_cdfs.c ops_lofs.c ops_mfs.c ops_nfs.c ops_nfs3.c 
SRCS+=		ops_nullfs.c ops_pcfs.c ops_tfs.c ops_ufs.c ops_umapfs.c 
SRCS+=		ops_unionfs.c opts.c restart.c rpc_fwd.c sched.c
SRCS+=		srvr_amfs_auto.c srvr_nfs.c

CFLAGS+=	-I${.CURDIR}/../../../contrib/amd/amd

DPADD=		${LIBAMU} ${LIBRPCSVC}
LDADD=		${LIBAMU} -lrpcsvc

CLEANFILES+=	conf_parse.c conf_parse.h conf_tok.c

conf_tok.o:	conf_parse.h

#		These are generated at compile time
SRCS+=		mount_xdr.c
CLEANFILES+=	mount_xdr.c

mount_xdr.c: ${MOUNT_X}
	${RPCCOM} -c -DWANT_NFS3 ${MOUNT_X} -o ${.TARGET}

.include <bsd.prog.mk>
