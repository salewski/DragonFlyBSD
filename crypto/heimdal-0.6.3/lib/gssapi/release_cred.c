/*
 * Copyright (c) 1997-2003 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "gssapi_locl.h"

RCSID("$Id: release_cred.c,v 1.8.2.1 2003/10/07 01:08:21 lha Exp $");

OM_uint32 gss_release_cred
           (OM_uint32 * minor_status,
            gss_cred_id_t * cred_handle
           )
{
    *minor_status = 0;

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
        return GSS_S_COMPLETE;
    }

    GSSAPI_KRB5_INIT ();

    if ((*cred_handle)->principal != NULL)
        krb5_free_principal(gssapi_krb5_context, (*cred_handle)->principal);
    if ((*cred_handle)->keytab != NULL)
	krb5_kt_close(gssapi_krb5_context, (*cred_handle)->keytab);
    if ((*cred_handle)->ccache != NULL) {
	const krb5_cc_ops *ops;
	ops = krb5_cc_get_ops(gssapi_krb5_context, (*cred_handle)->ccache);
	if (ops == &krb5_mcc_ops)
	    krb5_cc_destroy(gssapi_krb5_context, (*cred_handle)->ccache);
	else 
	    krb5_cc_close(gssapi_krb5_context, (*cred_handle)->ccache);
    }
    gss_release_oid_set(NULL, &(*cred_handle)->mechanisms);
    free(*cred_handle);
    *cred_handle = GSS_C_NO_CREDENTIAL;
    return GSS_S_COMPLETE;
}

