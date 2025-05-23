# Copyright 1998 Juniper Networks, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	$FreeBSD: src/lib/libpam/libpam/Makefile,v 1.12.2.12 2002/07/22 14:21:49 ru Exp $
#	$DragonFly: src/lib/libpam/libpam/Attic/Makefile,v 1.6 2004/12/20 18:02:19 dillon Exp $

PAMDIR=		${.CURDIR}/../../../contrib/libpam
MODOBJDIR=	../modules

.PATH:		${PAMDIR}/libpam ${PAMDIR}/libpam_misc ${PAMDIR}/doc/man

LIB=		pam
CFLAGS+=	-I${PAMDIR}/libpam/include -I${.CURDIR}
CFLAGS+=	-DDEFAULT_MODULE_PATH=\"${SHLIBDIR}/\"
NOPROFILE=	yes

# Files from ${PAMDIR}/libpam:
SRCS=		pam_account.c pam_auth.c pam_data.c pam_delay.c \
		pam_dispatch.c pam_end.c pam_env.c pam_handlers.c \
		pam_item.c pam_log.c pam_misc.c pam_password.c \
		pam_second.c pam_session.c pam_start.c \
		pam_strerror.c
HDRS1=		_pam_compat.h _pam_macros.h _pam_types.h \
		pam_appl.h pam_malloc.h pam_modules.h
MAN+=		pam_authenticate.3 pam_chauthtok.3 pam_fail_delay.3 \
		pam_open_session.3 pam_setcred.3 pam_start.3 \
		pam_strerror.3
MLINKS+=	pam_open_session.3 pam_close_session.3 \
		pam_start.3 pam_end.3
MAN+=		pam.8
MLINKS+=	pam.8 pam.conf.5 pam.8 pam.d.5

# Files from ${PAMDIR}/libpam_misc:
SRCS+=		help_env.c misc_conv.c xstrdup.c
HDRS2=		pam_misc.h

# Files from ${.CURDIR}:
SRCS+=		pam_debug_log.c pam_get_pass.c pam_prompt.c pam_std_option.c
SRCS+=		openpam_borrow_cred.c openpam_restore_cred.c
HDRS3=		security/pam_mod_misc.h
MAN+=		pam_set_item.3
MLINKS+=	pam_set_item.3 pam_get_item.3

# Static PAM modules:
STATIC_MODULES+= ${MODOBJDIR}/pam_cleartext_pass_ok/libpam_cleartext_pass_ok.a
STATIC_MODULES+= ${MODOBJDIR}/pam_deny/libpam_deny.a
.if defined(MAKE_KERBEROS5__) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
STATIC_MODULES+= ${MODOBJDIR}/pam_kerberos5/libpam_kerberos5.a
.endif
STATIC_MODULES+= ${MODOBJDIR}/pam_opie/libpam_opie.a
STATIC_MODULES+= ${MODOBJDIR}/pam_permit/libpam_permit.a
STATIC_MODULES+= ${MODOBJDIR}/pam_radius/libpam_radius.a
STATIC_MODULES+= ${MODOBJDIR}/pam_skey/libpam_skey.a
STATIC_MODULES+= ${MODOBJDIR}/pam_tacplus/libpam_tacplus.a
STATIC_MODULES+= ${MODOBJDIR}/pam_unix/libpam_unix.a

STATICOBJS+=	pam_static_modules.o
CLEANFILES+=	pam_static.o

# doesn't work, bsd.lib.mk already has an _EXTRADEPEND for the .So files.
#_EXTRADEPEND:	pam_static.c
#	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
#	    ${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*} \
#	    ${.ALLSRC}

CLEANFILES+=	_pam_static_modules.o

pam_static_modules.o:	pam_static.o _pam_static_modules.o
	${LD} -o ${.TARGET} -r ${.ALLSRC}

_pam_static_modules.o:	${STATIC_MODULES}
	${LD} -o ${.TARGET} -r --whole-archive ${.ALLSRC}

# Headers
INCS=		${HDRS1:S;^;${PAMDIR}/libpam/include/security/;} \
		${HDRS2} ${HDRS3}
INCSDIR=	${INCLUDEDIR}/security

.include <bsd.lib.mk>
