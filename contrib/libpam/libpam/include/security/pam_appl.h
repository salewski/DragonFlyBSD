/*
 * <security/pam_appl.h>
 * 
 * This header file collects definitions for the PAM API --- that is,
 * public interface between the PAM library and an application program
 * that wishes to use it.
 *
 * Note, the copyright information is at end of file.
 * $FreeBSD: src/contrib/libpam/libpam/include/security/pam_appl.h,v 1.1.1.1.6.2 2001/06/11 15:28:14 markm Exp $
 * $DragonFly: src/contrib/libpam/libpam/include/security/Attic/pam_appl.h,v 1.2 2003/06/17 04:24:03 dillon Exp $
 *
 * Created: 15-Jan-96 by TYT
 * Last modified: 1996/3/5 by AGM
 *
 * $Log: pam_appl.h,v $
 * Revision 1.5  1996/11/10 19:56:11  morgan
 * minor prototype change
 *
 * Revision 1.4  1996/03/16 22:38:17  morgan
 * made all of the pam_start input arguments constant
 *
 * Revision 1.3  1996/03/16 20:22:59  morgan
 * changed name comment at top of file.
 *
 * Revision 1.2  1996/03/09 20:39:06  morgan
 * added RCS information
 *
 *
 * $Id: pam_appl.h,v 1.5 1996/11/10 19:56:11 morgan Exp $
 *
 */

#ifndef _SECURITY_PAM_APPL_H
#define _SECURITY_PAM_APPL_H

#include <security/_pam_types.h>      /* Linux-PAM common defined types */

/* -------------- The Linux-PAM Framework layer API ------------- */

extern int pam_start(const char *service_name, const char *user,
		     const struct pam_conv *pam_conversation,
		     pam_handle_t **pamh);
extern int pam_end(pam_handle_t *pamh, int pam_status);

/* Authentication API's */

extern int pam_authenticate(pam_handle_t *pamh, int flags);
extern int pam_setcred(pam_handle_t *pamh, int flags);

/* Account Management API's */

extern int pam_acct_mgmt(pam_handle_t *pamh, int flags);

/* Session Management API's */

extern int pam_open_session(pam_handle_t *pamh, int flags);
extern int pam_close_session(pam_handle_t *pamh, int flags);

/* Password Management API's */

extern int pam_chauthtok(pam_handle_t *pamh, int flags);

/* take care of any compatibility issues */
#include <security/_pam_compat.h>

/*
 * Copyright Theodore Ts'o, 1996.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#endif /* _SECURITY_PAM_APPL_H */
