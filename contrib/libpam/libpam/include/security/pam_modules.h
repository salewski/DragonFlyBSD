/*
 * <security/pam_modules.h>
 * 
 * $Id: pam_modules.h,v 1.8 1997/01/04 20:14:42 morgan Exp morgan $
 * $FreeBSD: src/contrib/libpam/libpam/include/security/pam_modules.h,v 1.2.6.2 2001/06/11 15:28:14 markm Exp $
 * $DragonFly: src/contrib/libpam/libpam/include/security/Attic/pam_modules.h,v 1.2 2003/06/17 04:24:03 dillon Exp $
 *
 * This header file documents the PAM SPI --- that is, interface
 * between the PAM library and a PAM service library which is called
 * by the PAM library.
 *
 * Note, the copyright information is at end of file.
 *
 * $Log: pam_modules.h,v $
 * Revision 1.8  1997/01/04 20:14:42  morgan
 * moved PAM_DATA_SILENT to _pam_types.h so applications can use it too
 *
 * Revision 1.7  1996/11/10 19:57:08  morgan
 * pam_get_user prototype.
 *
 * Revision 1.6  1996/09/05 06:18:45  morgan
 * added some data error_status masks, changed prototype for cleanup()
 *
 * Revision 1.5  1996/06/02 07:58:37  morgan
 * altered the way in which modules obtain static prototypes for
 * functions
 *
 */

#ifndef _SECURITY_PAM_MODULES_H
#define _SECURITY_PAM_MODULES_H

/*
 * Define either PAM_STATIC or PAM_DYNAMIC, based on whether PIC
 * compilation is being used.
 */
#if !defined(PIC) && !defined(PAM_STATIC)
#define PAM_STATIC
#endif
#ifndef PAM_STATIC
#define PAM_DYNAMIC
#endif

#ifdef PAM_STATIC
#include <linker_set.h>
#endif

#include <security/_pam_types.h>      /* Linux-PAM common defined types */

/* these defines are used by pam_set_item() and pam_get_item() and are
 * in addition to those found in <security/_pam_types.h> */

#define PAM_AUTHTOK     6	/* The authentication token (password) */
#define PAM_OLDAUTHTOK  7	/* The old authentication token */

/* -------------- The Linux-PAM Module PI ------------- */

extern int pam_set_data(pam_handle_t *pamh, const char *module_data_name,
			void *data,
			void (*cleanup)(pam_handle_t *pamh, void *data,
				       int error_status));
extern int pam_get_data(const pam_handle_t *pamh,
			const char *module_data_name, const void **data);

extern int pam_get_user(pam_handle_t *pamh, const char **user
			, const char *prompt);

#ifdef PAM_STATIC

#define PAM_EXTERN static

struct pam_module {
    const char *name;		/* Name of the module */

    /* These are function pointers to the module's key functions.  */

    int (*pam_sm_authenticate)(pam_handle_t *pamh, int flags,
			       int argc, const char **argv);
    int (*pam_sm_setcred)(pam_handle_t *pamh, int flags,
			  int argc, const char **argv);
    int (*pam_sm_acct_mgmt)(pam_handle_t *pamh, int flags,
			    int argc, const char **argv);
    int (*pam_sm_open_session)(pam_handle_t *pamh, int flags,
			       int argc, const char **argv);
    int (*pam_sm_close_session)(pam_handle_t *pamh, int flags,
				int argc, const char **argv);
    int (*pam_sm_chauthtok)(pam_handle_t *pamh, int flags,
			    int argc, const char **argv);
};

#ifdef PAM_SM_AUTH
#define PAM_SM_AUTH_ENTRY	pam_sm_authenticate
#define PAM_SM_SETCRED_ENTRY	pam_sm_setcred
#else
#define PAM_SM_AUTH_ENTRY	NULL
#define PAM_SM_SETCRED_ENTRY	NULL
#endif

#ifdef PAM_SM_ACCOUNT
#define PAM_SM_ACCOUNT_ENTRY	pam_sm_acct_mgmt
#else
#define PAM_SM_ACCOUNT_ENTRY	NULL
#endif

#ifdef PAM_SM_SESSION
#define PAM_SM_OPEN_SESSION_ENTRY	pam_sm_open_session
#define PAM_SM_CLOSE_SESSION_ENTRY	pam_sm_close_session
#else
#define PAM_SM_OPEN_SESSION_ENTRY	NULL
#define PAM_SM_CLOSE_SESSION_ENTRY	NULL
#endif

#ifdef PAM_SM_PASSWORD
#define PAM_SM_PASSWORD_ENTRY	pam_sm_chauthtok
#else
#define PAM_SM_PASSWORD_ENTRY	NULL
#endif

#define PAM_MODULE_ENTRY(name)			\
    static struct pam_module _pam_modstruct = {	\
	name,					\
	PAM_SM_AUTH_ENTRY,			\
	PAM_SM_SETCRED_ENTRY,			\
	PAM_SM_ACCOUNT_ENTRY,			\
	PAM_SM_OPEN_SESSION_ENTRY,		\
	PAM_SM_CLOSE_SESSION_ENTRY,		\
	PAM_SM_PASSWORD_ENTRY			\
    };						\
    DATA_SET(_pam_static_modules, _pam_modstruct)

#else /* !PAM_STATIC */

#define PAM_EXTERN extern
#define PAM_MODULE_ENTRY(name)

#endif /* PAM_STATIC */
	
/* Lots of files include pam_modules.h that don't need these
 * declared.  However, when they are declared static, they
 * need to be defined later.  So we have to protect C files
 * that include these without wanting these functions defined.. */

#if (defined(PAM_STATIC) && defined(PAM_SM_AUTH)) || !defined(PAM_STATIC)

/* Authentication API's */
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv);
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags,
			      int argc, const char **argv);

#endif /*(defined(PAM_STATIC) && defined(PAM_SM_AUTH))
	 || !defined(PAM_STATIC)*/

#if (defined(PAM_STATIC) && defined(PAM_SM_ACCOUNT)) || !defined(PAM_STATIC)

/* Account Management API's */
PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
				int argc, const char **argv);

#endif /*(defined(PAM_STATIC) && defined(PAM_SM_ACCOUNT))
	 || !defined(PAM_STATIC)*/

#if (defined(PAM_STATIC) && defined(PAM_SM_SESSION)) || !defined(PAM_STATIC)

/* Session Management API's */
PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags,
				   int argc, const char **argv);

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags,
				    int argc, const char **argv);

#endif /*(defined(PAM_STATIC) && defined(PAM_SM_SESSION))
	 || !defined(PAM_STATIC)*/

#if (defined(PAM_STATIC) && defined(PAM_SM_PASSWORD)) || !defined(PAM_STATIC)

/* Password Management API's */
PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags,
				int argc, const char **argv);

#endif /*(defined(PAM_STATIC) && defined(PAM_SM_PASSWORD))
	 || !defined(PAM_STATIC)*/

/* The following two flags are for use across the Linux-PAM/module
 * interface only. The Application is not permitted to use these
 * tokens.
 *
 * The password service should only perform preliminary checks.  No
 * passwords should be updated. */
#define PAM_PRELIM_CHECK		0x4000

/* The password service should update passwords Note: PAM_PRELIM_CHECK
 * and PAM_UPDATE_AUTHTOK can not both be set simultaneously! */
#define PAM_UPDATE_AUTHTOK		0x2000


/*
 * here are some proposed error status definitions for the
 * 'error_status' argument used by the cleanup function associated
 * with data items they should be logically OR'd with the error_status
 * of the latest return from libpam -- new with .52 and positive
 * impression from Sun although not official as of 1996/9/4 there are
 * others in _pam_types.h -- they are for common module/app use.
 */

#define PAM_DATA_REPLACE   0x20000000     /* used when replacing a data item */

/* take care of any compatibility issues */
#include <security/_pam_compat.h>

/* Copyright (C) Theodore Ts'o, 1996.
 * Copyright (C) Andrew Morgan, 1996-8.
 *                                                All rights reserved.
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
 * the GNU General Public License, in which case the provisions of the
 * GNU GPL are required INSTEAD OF the above restrictions.  (This
 * clause is necessary due to a potential bad interaction between the
 * GNU GPL and the restrictions contained in a BSD-style copyright.)
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
 * OF THE POSSIBILITY OF SUCH DAMAGE.  */

#endif /* _SECURITY_PAM_MODULES_H */

