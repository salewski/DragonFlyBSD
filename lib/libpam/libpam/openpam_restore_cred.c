/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $P4: //depot/projects/openpam/lib/openpam_restore_cred.c#2 $
 * $FreeBSD: src/lib/libpam/libpam/openpam_restore_cred.c,v 1.1.2.3 2002/07/03 21:45:44 des Exp $
 * $DragonFly: src/lib/libpam/libpam/Attic/openpam_restore_cred.c,v 1.2 2003/06/17 04:26:50 dillon Exp $
 */

#include <sys/param.h>

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include <security/pam_appl.h>

#include <security/pam_mod_misc.h>

/*
 * OpenPAM extension
 *
 * Restore credentials
 */

int
openpam_restore_cred(pam_handle_t *pamh)
{
	struct pam_saved_cred *scred;
	int r;

	r = pam_get_data(pamh, PAM_SAVED_CRED, (const void **)&scred);
	if (r != PAM_SUCCESS)
		return (r);
	if (scred == NULL)
		return (PAM_SYSTEM_ERR);
	if (seteuid(scred->euid) == -1 ||
	    setgroups(scred->ngroups, scred->groups) == -1 ||
	    setegid(scred->egid) == -1)
		return (PAM_SYSTEM_ERR);
	pam_set_data(pamh, PAM_SAVED_CRED, NULL, NULL);
	return (PAM_SUCCESS);
}

/*
 * Error codes:
 *
 *	=pam_get_data
 *	PAM_SYSTEM_ERR
 */

/**
 * The =openpam_restore_cred function restores the credentials saved by
 * =openpam_borrow_cred.
 *
 * >setegid
 * >seteuid
 * >setgroups
 */
