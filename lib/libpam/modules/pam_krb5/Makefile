# Copyright 2001 FreeBSD, Inc.
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
#	$FreeBSD: src/lib/libpam/modules/pam_krb5/Makefile,v 1.2.2.5 2002/07/23 09:29:53 ru Exp $
#	$DragonFly: src/lib/libpam/modules/pam_krb5/Attic/Makefile,v 1.2 2003/06/17 04:26:50 dillon Exp $


LIB=		pam_krb5
SRCS=		pam_krb5_auth.c pam_krb5_pass.c pam_krb5_acct.c \
		pam_krb5_sess.c support.c compat_heimdal.c
CFLAGS+=	-Wall
CFLAGS+=	-I${PAMDIR}/libpam/include
CFLAGS+=	-I${.CURDIR}/../../libpam
DPADD=		${LIBKRB5} ${LIBGSSAPI} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBCOM_ERR} ${LIBROKEN}
LDADD=		-lkrb5 -lgssapi -lasn1 -lcrypto -lcrypt -lcom_err \
		-lroken 
MAN=		pam_krb5.8

.include <bsd.lib.mk>
