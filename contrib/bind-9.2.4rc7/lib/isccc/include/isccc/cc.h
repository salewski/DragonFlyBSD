/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: cc.h,v 1.3.2.1 2004/03/09 06:12:27 marka Exp $ */

#ifndef ISCCC_CC_H
#define ISCCC_CC_H 1

#include <isc/lang.h>
#include <isccc/types.h>

ISC_LANG_BEGINDECLS

#define ISCCC_CC_MAXDGRAMPACKET		4096

#define ISCCC_CCMSGTYPE_STRING		0x00
#define ISCCC_CCMSGTYPE_BINARYDATA	0x01
#define ISCCC_CCMSGTYPE_TABLE		0x02
#define ISCCC_CCMSGTYPE_LIST		0x03

isc_result_t
isccc_cc_towire(isccc_sexpr_t *alist, isccc_region_t *target,
	      isccc_region_t *secret);

isc_result_t
isccc_cc_fromwire(isccc_region_t *source, isccc_sexpr_t **alistp,
		isccc_region_t *secret);

isc_result_t
isccc_cc_createmessage(isc_uint32_t version, const char *from, const char *to,
		     isc_uint32_t serial, isccc_time_t now,
		     isccc_time_t expires, isccc_sexpr_t **alistp);

isc_result_t
isccc_cc_createack(isccc_sexpr_t *message, isc_boolean_t ok,
		 isccc_sexpr_t **ackp);

isc_boolean_t
isccc_cc_isack(isccc_sexpr_t *message);

isc_boolean_t
isccc_cc_isreply(isccc_sexpr_t *message);

isc_result_t
isccc_cc_createresponse(isccc_sexpr_t *message, isccc_time_t now,
		      isccc_time_t expires, isccc_sexpr_t **alistp);

isccc_sexpr_t *
isccc_cc_definestring(isccc_sexpr_t *alist, const char *key, const char *str);

isccc_sexpr_t *
isccc_cc_defineuint32(isccc_sexpr_t *alist, const char *key, isc_uint32_t i);

isc_result_t
isccc_cc_lookupstring(isccc_sexpr_t *alist, const char *key, char **strp);

isc_result_t
isccc_cc_lookupuint32(isccc_sexpr_t *alist, const char *key,
		    isc_uint32_t *uintp);

isc_result_t
isccc_cc_createsymtab(isccc_symtab_t **symtabp);

void
isccc_cc_cleansymtab(isccc_symtab_t *symtab, isccc_time_t now);

isc_result_t
isccc_cc_checkdup(isccc_symtab_t *symtab, isccc_sexpr_t *message,
		   isccc_time_t now);

ISC_LANG_ENDDECLS

#endif /* ISCCC_CC_H */
