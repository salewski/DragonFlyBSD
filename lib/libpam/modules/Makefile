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
#	$FreeBSD: src/lib/libpam/modules/Makefile,v 1.8.2.8 2002/07/03 21:35:18 des Exp $
#	$DragonFly: src/lib/libpam/modules/Attic/Makefile,v 1.3 2003/08/05 07:45:41 asmodai Exp $

SUBDIR+=	pam_cleartext_pass_ok
SUBDIR+=	pam_deny
.if defined(MAKE_KERBEROS5) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
SUBDIR+=	pam_krb5
.endif
SUBDIR+=	pam_opie
SUBDIR+=	pam_opieaccess
SUBDIR+=	pam_permit
SUBDIR+=	pam_radius
SUBDIR+=	pam_skey
.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
SUBDIR+=	pam_ssh
.endif
SUBDIR+=	pam_tacplus
SUBDIR+=	pam_unix

.include <bsd.subdir.mk>
