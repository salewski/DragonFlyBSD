# $FreeBSD: src/kerberos5/lib/libkadm5srv/Makefile,v 1.7 2004/02/05 18:51:49 ru Exp $
# $DragonFly: src/kerberos5/lib/libkadm5srv/Makefile,v 1.3 2005/01/16 14:25:46 eirikn Exp $

LIB=	kadm5srv

SRCS=	acl.c \
	bump_pw_expire.c \
	chpass_s.c \
	common_glue.c \
	context_s.c \
	create_s.c \
	delete_s.c \
	destroy_s.c \
	ent_setup.c \
	error.c \
	flush_s.c \
	free.c \
	get_princs_s.c \
	get_s.c \
	init_s.c \
	kadm5_err.c \
	kadm5_err.h \
	keys.c \
	log.c \
	marshall.c \
	modify_s.c \
	password_quality.c \
	privs_s.c \
	randkey_s.c \
	rename_s.c \
	server_glue.c \
	set_keys.c \
	set_modifier.c

CFLAGS+=-I${KRB5DIR}/lib/kadm5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/kadm5
