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

/* $Id: tkeyconf.c,v 1.19.2.1 2004/03/09 06:09:20 marka Exp $ */

#include <config.h>

#include <isc/buffer.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/mem.h>

#include <isccfg/cfg.h>

#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/name.h>
#include <dns/tkey.h>

#include <dst/gssapi.h>

#include <named/tkeyconf.h>

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto failure; \
	} while (0)


isc_result_t
ns_tkeyctx_fromconfig(cfg_obj_t *options, isc_mem_t *mctx, isc_entropy_t *ectx,
		       dns_tkeyctx_t **tctxp)
{
	isc_result_t result;
	dns_tkeyctx_t *tctx = NULL;
	char *s;
	isc_uint32_t n;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_buffer_t b;
	cfg_obj_t *obj;

	result = dns_tkeyctx_create(mctx, ectx, &tctx);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = NULL;
	result = cfg_map_get(options, "tkey-dhkey", &obj);
	if (result == ISC_R_SUCCESS) {
		s = cfg_obj_asstring(cfg_tuple_get(obj, "name"));
		n = cfg_obj_asuint32(cfg_tuple_get(obj, "keyid"));
		isc_buffer_init(&b, s, strlen(s));
		isc_buffer_add(&b, strlen(s));
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		RETERR(dns_name_fromtext(name, &b, dns_rootname,
					 ISC_FALSE, NULL));
		RETERR(dst_key_fromfile(name, (dns_keytag_t) n, DNS_KEYALG_DH,
					DST_TYPE_PUBLIC|DST_TYPE_PRIVATE,
					NULL, mctx, &tctx->dhkey));
	}

	obj = NULL;
	result = cfg_map_get(options, "tkey-domain", &obj);
	if (result == ISC_R_SUCCESS) {
		s = cfg_obj_asstring(obj);
		isc_buffer_init(&b, s, strlen(s));
		isc_buffer_add(&b, strlen(s));
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		RETERR(dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE,
					 NULL));
		tctx->domain = isc_mem_get(mctx, sizeof(dns_name_t));
		if (tctx->domain == NULL) {
			result = ISC_R_NOMEMORY;
			goto failure;
		}
		dns_name_init(tctx->domain, NULL);
		RETERR(dns_name_dup(name, mctx, tctx->domain));
	}

	obj = NULL;
	result = cfg_map_get(options, "tkey-gssapi-credential", &obj);
	if (result == ISC_R_SUCCESS) {
		s = cfg_obj_asstring(obj);
		isc_buffer_init(&b, s, strlen(s));
		isc_buffer_add(&b, strlen(s));
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		RETERR(dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE,
					 NULL));
		RETERR(dst_gssapi_acquirecred(name, ISC_FALSE,
					      &tctx->gsscred));
	}

	*tctxp = tctx;
	return (ISC_R_SUCCESS);

 failure:
	dns_tkeyctx_destroy(&tctx);
	return (result);
}

