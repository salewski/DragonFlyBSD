/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: aclconf.h,v 1.12.2.1 2004/03/09 06:09:21 marka Exp $ */

#ifndef NS_ACLCONF_H
#define NS_ACLCONF_H 1

#include <isc/lang.h>

#include <isccfg/cfg.h>

#include <dns/types.h>

typedef struct ns_aclconfctx {
	ISC_LIST(dns_acl_t) named_acl_cache;
} ns_aclconfctx_t;

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

void
ns_aclconfctx_init(ns_aclconfctx_t *ctx);
/*
 * Initialize an ACL configuration context.
 */

void
ns_aclconfctx_destroy(ns_aclconfctx_t *ctx);
/*
 * Destroy an ACL configuration context.
 */

isc_result_t
ns_acl_fromconfig(cfg_obj_t *caml,
		  cfg_obj_t *cctx,
		  ns_aclconfctx_t *ctx,
		  isc_mem_t *mctx,
		  dns_acl_t **target);
/*
 * Construct a new dns_acl_t from configuration data in 'caml' and
 * 'cctx'.  Memory is allocated through 'mctx'.
 *
 * Any named ACLs referred to within 'caml' will be be converted
 * inte nested dns_acl_t objects.  Multiple references to the same
 * named ACLs will be converted into shared references to a single
 * nested dns_acl_t object when the referring objects were created
 * passing the same ACL configuration context 'ctx'.
 *
 * On success, attach '*target' to the new dns_acl_t object.
 */

ISC_LANG_ENDDECLS

#endif /* NS_ACLCONF_H */
