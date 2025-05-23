/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
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

/* $Id: rdata.c,v 1.147.2.12 2004/03/09 06:11:05 marka Exp $ */

#include <config.h>
#include <ctype.h>

#include <isc/base64.h>
#include <isc/hex.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/cert.h>
#include <dns/compress.h>
#include <dns/keyflags.h>
#include <dns/rcode.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/secproto.h>
#include <dns/time.h>
#include <dns/ttl.h>

#define RETERR(x) \
	do { \
		isc_result_t _r = (x); \
		if (_r != ISC_R_SUCCESS) \
			return (_r); \
	} while (0)
#define RETTOK(x) \
	do { \
		isc_result_t _r = (x); \
		if (_r != ISC_R_SUCCESS) { \
			isc_lex_ungettoken(lexer, &token); \
			return (_r); \
		} \
	} while (0)

#define ARGS_FROMTEXT	int rdclass, dns_rdatatype_t type, \
			isc_lex_t *lexer, dns_name_t *origin, \
			isc_boolean_t downcase, isc_buffer_t *target, \
			dns_rdatacallbacks_t *callbacks

#define ARGS_TOTEXT	dns_rdata_t *rdata, dns_rdata_textctx_t *tctx, \
			isc_buffer_t *target

#define ARGS_FROMWIRE	int rdclass, dns_rdatatype_t type, \
			isc_buffer_t *source, dns_decompress_t *dctx, \
			isc_boolean_t downcase, isc_buffer_t *target

#define ARGS_TOWIRE	dns_rdata_t *rdata, dns_compress_t *cctx, \
			isc_buffer_t *target

#define ARGS_COMPARE	const dns_rdata_t *rdata1, const dns_rdata_t *rdata2

#define ARGS_FROMSTRUCT	int rdclass, dns_rdatatype_t type, \
			void *source, isc_buffer_t *target

#define ARGS_TOSTRUCT	dns_rdata_t *rdata, void *target, isc_mem_t *mctx

#define ARGS_FREESTRUCT void *source

#define ARGS_ADDLDATA	dns_rdata_t *rdata, dns_additionaldatafunc_t add, \
			void *arg

#define ARGS_DIGEST	dns_rdata_t *rdata, dns_digestfunc_t digest, void *arg

/*
 * Context structure for the totext_ functions.
 * Contains formatting options for rdata-to-text
 * conversion.
 */
typedef struct dns_rdata_textctx {
	dns_name_t *origin;	/* Current origin, or NULL. */
	unsigned int flags;	/* DNS_STYLEFLAG_* */
	unsigned int width;	/* Width of rdata column. */
  	const char *linebreak;	/* Line break string. */
} dns_rdata_textctx_t;

static isc_result_t
txt_totext(isc_region_t *source, isc_buffer_t *target);

static isc_result_t
txt_fromtext(isc_textregion_t *source, isc_buffer_t *target);

static isc_result_t
txt_fromwire(isc_buffer_t *source, isc_buffer_t *target);

static isc_boolean_t
name_prefix(dns_name_t *name, dns_name_t *origin, dns_name_t *target);

static unsigned int
name_length(dns_name_t *name);

static isc_result_t
str_totext(const char *source, isc_buffer_t *target);

static isc_result_t
inet_totext(int af, isc_region_t *src, isc_buffer_t *target);

static isc_boolean_t
buffer_empty(isc_buffer_t *source);

static void
buffer_fromregion(isc_buffer_t *buffer, isc_region_t *region);

static isc_result_t
uint32_tobuffer(isc_uint32_t, isc_buffer_t *target);

static isc_result_t
uint16_tobuffer(isc_uint32_t, isc_buffer_t *target);

static isc_result_t
uint8_tobuffer(isc_uint32_t, isc_buffer_t *target);

static isc_result_t
name_tobuffer(dns_name_t *name, isc_buffer_t *target);

static isc_uint32_t
uint32_fromregion(isc_region_t *region);

static isc_uint16_t
uint16_fromregion(isc_region_t *region);

static isc_uint8_t
uint8_fromregion(isc_region_t *region);

static isc_result_t
mem_tobuffer(isc_buffer_t *target, void *base, unsigned int length);

static int
compare_region(isc_region_t *r1, isc_region_t *r2);

static int
hexvalue(char value);

static int
decvalue(char value);

static isc_result_t
btoa_totext(unsigned char *inbuf, int inbuflen, isc_buffer_t *target);

static isc_result_t
atob_tobuffer(isc_lex_t *lexer, isc_buffer_t *target);

static void
default_fromtext_callback(dns_rdatacallbacks_t *callbacks, const char *, ...)
     ISC_FORMAT_PRINTF(2, 3);

static void
fromtext_error(void (*callback)(dns_rdatacallbacks_t *, const char *, ...),
	       dns_rdatacallbacks_t *callbacks, const char *name,
	       unsigned long line, isc_token_t *token, isc_result_t result);

static void
fromtext_warneof(isc_lex_t *lexer, dns_rdatacallbacks_t *callbacks);

static isc_result_t
rdata_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	     isc_buffer_t *target);

static inline int
getquad(const void *src, struct in_addr *dst,
	isc_lex_t *lexer, dns_rdatacallbacks_t *callbacks)
{
	int result;
	struct in_addr *tmp;

	result = inet_aton(src, dst);
	if (result == 1 && callbacks != NULL &&
	    inet_pton(AF_INET, src, &tmp) != 1) {
		const char *name = isc_lex_getsourcename(lexer);
		if (name == NULL)
			name = "UNKNOWN";
		(*callbacks->warn)(callbacks, "%s:%lu: warning \"%s\" "
				   "is not a decimal dotted quad", name,
				   isc_lex_getsourceline(lexer), src);
	}
	return (result);
}

static inline isc_result_t
name_duporclone(dns_name_t *source, isc_mem_t *mctx, dns_name_t *target) {

	if (mctx != NULL)
		return (dns_name_dup(source, mctx, target));
	dns_name_clone(source, target);
	return (ISC_R_SUCCESS);
}

static inline void *
mem_maybedup(isc_mem_t *mctx, void *source, size_t length) {
	void *new;

	if (mctx == NULL)
		return (source);
	new = isc_mem_allocate(mctx, length);
	if (new != NULL)
		memcpy(new, source, length);

	return (new);
}

static const char hexdigits[] = "0123456789abcdef";
static const char decdigits[] = "0123456789";

#include "code.h"

#define META 0x0001
#define RESERVED 0x0002

#define RCODENAMES \
	/* standard rcodes */ \
	{ dns_rcode_noerror, "NOERROR", 0}, \
	{ dns_rcode_formerr, "FORMERR", 0}, \
	{ dns_rcode_servfail, "SERVFAIL", 0}, \
	{ dns_rcode_nxdomain, "NXDOMAIN", 0}, \
	{ dns_rcode_notimp, "NOTIMP", 0}, \
	{ dns_rcode_refused, "REFUSED", 0}, \
	{ dns_rcode_yxdomain, "YXDOMAIN", 0}, \
	{ dns_rcode_yxrrset, "YXRRSET", 0}, \
	{ dns_rcode_nxrrset, "NXRRSET", 0}, \
	{ dns_rcode_notauth, "NOTAUTH", 0}, \
	{ dns_rcode_notzone, "NOTZONE", 0},

#define ERCODENAMES \
	/* extended rcodes */ \
	{ dns_rcode_badvers, "BADVERS", 0}, \
	{ 0, NULL, 0 }

#define TSIGRCODENAMES \
	/* extended rcodes */ \
	{ dns_tsigerror_badsig, "BADSIG", 0}, \
	{ dns_tsigerror_badkey, "BADKEY", 0}, \
	{ dns_tsigerror_badtime, "BADTIME", 0}, \
	{ dns_tsigerror_badmode, "BADMODE", 0}, \
	{ dns_tsigerror_badname, "BADNAME", 0}, \
	{ dns_tsigerror_badalg, "BADALG", 0}, \
	{ 0, NULL, 0 }

/* RFC2538 section 2.1 */

#define CERTNAMES \
	{ 1, "PKIX", 0}, \
	{ 2, "SPKI", 0}, \
	{ 3, "PGP", 0}, \
	{ 253, "URI", 0}, \
	{ 254, "OID", 0}, \
	{ 0, NULL, 0}

/* RFC2535 section 7 */

#define SECALGNAMES \
	{ 1, "RSAMD5", 0 }, \
	{ 2, "DH", 0 }, \
	{ 3, "DSA", 0 }, \
	{ 4, "ECC", 0 }, \
	{ 252, "INDIRECT", 0 }, \
	{ 253, "PRIVATEDNS", 0 }, \
	{ 254, "PRIVATEOID", 0 }, \
	{ 0, NULL, 0}

/* RFC2535 section 7.1 */

#define SECPROTONAMES \
	{   0,    "NONE", 0 }, \
	{   1,    "TLS", 0 }, \
	{   2,    "EMAIL", 0 }, \
	{   3,    "DNSSEC", 0 }, \
	{   4,    "IPSEC", 0 }, \
	{ 255,    "ALL", 0 }, \
	{ 0, NULL, 0}

struct tbl {
	unsigned int	value;
	const char	*name;
	int		flags;
};

static struct tbl rcodes[] = { RCODENAMES ERCODENAMES };
static struct tbl tsigrcodes[] = { RCODENAMES TSIGRCODENAMES };
static struct tbl certs[] = { CERTNAMES };
static struct tbl secalgs[] = { SECALGNAMES };
static struct tbl secprotos[] = { SECPROTONAMES };

static struct keyflag {
	const char *name;
	unsigned int value;
	unsigned int mask;
} keyflags[] = {
	{ "NOCONF", 0x4000, 0xC000 },
	{ "NOAUTH", 0x8000, 0xC000 },
	{ "NOKEY",  0xC000, 0xC000 },
	{ "FLAG2",  0x2000, 0x2000 },
	{ "EXTEND", 0x1000, 0x1000 },
	{ "FLAG4",  0x0800, 0x0800 },
	{ "FLAG5",  0x0400, 0x0400 },
	{ "USER",   0x0000, 0x0300 },
	{ "ZONE",   0x0100, 0x0300 },
	{ "HOST",   0x0200, 0x0300 },
	{ "NTYP3",  0x0300, 0x0300 },
	{ "FLAG8",  0x0080, 0x0080 },
	{ "FLAG9",  0x0040, 0x0040 },
	{ "FLAG10", 0x0020, 0x0020 },
	{ "FLAG11", 0x0010, 0x0010 },
	{ "SIG0",   0x0000, 0x000F },
	{ "SIG1",   0x0001, 0x000F },
	{ "SIG2",   0x0002, 0x000F },
	{ "SIG3",   0x0003, 0x000F },
	{ "SIG4",   0x0004, 0x000F },
	{ "SIG5",   0x0005, 0x000F },
	{ "SIG6",   0x0006, 0x000F },
	{ "SIG7",   0x0007, 0x000F },
	{ "SIG8",   0x0008, 0x000F },
	{ "SIG9",   0x0009, 0x000F },
	{ "SIG10",  0x000A, 0x000F },
	{ "SIG11",  0x000B, 0x000F },
	{ "SIG12",  0x000C, 0x000F },
	{ "SIG13",  0x000D, 0x000F },
	{ "SIG14",  0x000E, 0x000F },
	{ "SIG15",  0x000F, 0x000F },
	{ NULL,     0, 0 }
};

/***
 *** Initialization
 ***/

void
dns_rdata_init(dns_rdata_t *rdata) {

	REQUIRE(rdata != NULL);

	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = 0;
	rdata->type = 0;
	rdata->flags = 0;
	ISC_LINK_INIT(rdata, link);
	/* ISC_LIST_INIT(rdata->list); */
}

#if 0
#define DNS_RDATA_INITIALIZED(rdata) \
	((rdata)->data == NULL && (rdata)->length == 0 && \
	 (rdata)->rdclass == 0 && (rdata)->type == 0 && (rdata)->flags == 0 && \
	 !ISC_LINK_LINKED((rdata), link))
#else
#ifdef ISC_LIST_CHECKINIT
#define DNS_RDATA_INITIALIZED(rdata) \
	(!ISC_LINK_LINKED((rdata), link))
#else
#define DNS_RDATA_INITIALIZED(rdata) ISC_TRUE
#endif
#endif
#define DNS_RDATA_VALIDFLAGS(rdata) \
	(((rdata)->flags & ~DNS_RDATA_UPDATE) == 0)

void
dns_rdata_reset(dns_rdata_t *rdata) {

	REQUIRE(rdata != NULL);

	REQUIRE(!ISC_LINK_LINKED(rdata, link));
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = 0;
	rdata->type = 0;
	rdata->flags = 0;
}

/***
 ***
 ***/

void
dns_rdata_clone(const dns_rdata_t *src, dns_rdata_t *target) {

	REQUIRE(src != NULL);
	REQUIRE(target != NULL);

	REQUIRE(DNS_RDATA_INITIALIZED(target));

	REQUIRE(DNS_RDATA_VALIDFLAGS(src));
	REQUIRE(DNS_RDATA_VALIDFLAGS(target));

	target->data = src->data;
	target->length = src->length;
	target->rdclass = src->rdclass;
	target->type = src->type;
	target->flags = src->flags;
}


/***
 *** Comparisons
 ***/

int
dns_rdata_compare(const dns_rdata_t *rdata1, const dns_rdata_t *rdata2) {
	int result = 0;
	isc_boolean_t use_default = ISC_FALSE;

	REQUIRE(rdata1 != NULL);
	REQUIRE(rdata2 != NULL);
	REQUIRE(rdata1->data != NULL);
	REQUIRE(rdata2->data != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata1));
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata2));

	if (rdata1->rdclass != rdata2->rdclass)
		return (rdata1->rdclass < rdata2->rdclass ? -1 : 1);

	if (rdata1->type != rdata2->type)
		return (rdata1->type < rdata2->type ? -1 : 1);

	COMPARESWITCH

	if (use_default) {
		isc_region_t r1;
		isc_region_t r2;

		dns_rdata_toregion(rdata1, &r1);
		dns_rdata_toregion(rdata2, &r2);
		result = compare_region(&r1, &r2);
	}
	return (result);
}

/***
 *** Conversions
 ***/

void
dns_rdata_fromregion(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, isc_region_t *r)
{

	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_INITIALIZED(rdata));
	REQUIRE(r != NULL);

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	rdata->data = r->base;
	rdata->length = r->length;
	rdata->rdclass = rdclass;
	rdata->type = type;
	rdata->flags = 0;
}

void
dns_rdata_toregion(const dns_rdata_t *rdata, isc_region_t *r) {

	REQUIRE(rdata != NULL);
	REQUIRE(r != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	r->base = rdata->data;
	r->length = rdata->length;
}

isc_result_t
dns_rdata_fromwire(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_buffer_t *source,
		   dns_decompress_t *dctx, isc_boolean_t downcase,
		   isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_region_t region;
	isc_buffer_t ss;
	isc_buffer_t st;
	isc_boolean_t use_default = ISC_FALSE;
	isc_uint32_t activelength;

	REQUIRE(dctx != NULL);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}

	if (type == 0)
		return (DNS_R_FORMERR);

	ss = *source;
	st = *target;

	activelength = isc_buffer_activelength(source);
	INSIST(activelength < 65536);

	FROMWIRESWITCH

	if (use_default) {
		if (activelength > isc_buffer_availablelength(target))
			result = ISC_R_NOSPACE;
		else {
			isc_buffer_putmem(target, isc_buffer_current(source),
					  activelength);
			isc_buffer_forward(source, activelength);
			result = ISC_R_SUCCESS;
		}
	}

	/*
	 * We should have consumed all of our buffer.
	 */
	if (result == ISC_R_SUCCESS && !buffer_empty(source))
		result = DNS_R_EXTRADATA;

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = isc_buffer_usedlength(target) -
				isc_buffer_usedlength(&st);
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}

	if (result != ISC_R_SUCCESS) {
		*source = ss;
		*target = st;
	}
	return (result);
}

isc_result_t
dns_rdata_towire(dns_rdata_t *rdata, dns_compress_t *cctx,
		 isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;
	isc_region_t tr;
	isc_buffer_t st;

	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Some DynDNS meta-RRs have empty rdata.
	 */
	if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
		INSIST(rdata->length == 0);
		return (ISC_R_SUCCESS);
	}

	st = *target;

	TOWIRESWITCH

	if (use_default) {
		isc_buffer_availableregion(target, &tr);
		if (tr.length < rdata->length)
			return (ISC_R_NOSPACE);
		memcpy(tr.base, rdata->data, rdata->length);
		isc_buffer_add(target, rdata->length);
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS) {
		*target = st;
		INSIST(target->used < 65536);
		dns_compress_rollback(cctx, (isc_uint16_t)target->used);
	}
	return (result);
}

/*
 * If the binary data in 'src' is valid uncompressed wire format
 * rdata of class 'rdclass' and type 'type', return ISC_R_SUCCESS
 * and copy the validated rdata to 'dest'.  Otherwise return an error.
 */
static isc_result_t
rdata_validate(isc_buffer_t *src, isc_buffer_t *dest, dns_rdataclass_t rdclass,
	    dns_rdatatype_t type)
{
	dns_decompress_t dctx;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;

	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_NONE);
	isc_buffer_setactive(src, isc_buffer_usedlength(src));
	result = dns_rdata_fromwire(&rdata, rdclass, type, src,
				    &dctx, ISC_FALSE, dest);
	dns_decompress_invalidate(&dctx);

	return (result);
}

static isc_result_t
unknown_fromtext(dns_rdataclass_t rdclass, dns_rdatatype_t type,
		 isc_lex_t *lexer, isc_mem_t *mctx, isc_buffer_t *target)
{
	isc_result_t result;
	isc_buffer_t *buf = NULL;
	isc_token_t token;

	if (type == 0 || dns_rdatatype_ismeta(type))
		return (DNS_R_METATYPE);

	result = isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
					ISC_FALSE);
	if (result == ISC_R_SUCCESS && token.value.as_ulong > 65535U)
		return (ISC_R_RANGE);
	result = isc_buffer_allocate(mctx, &buf, token.value.as_ulong);
	if (result != ISC_R_SUCCESS)
		return (result);
	
	result = isc_hex_tobuffer(lexer, buf,
				  (unsigned int)token.value.as_ulong);
	if (result != ISC_R_SUCCESS)
	       goto failure;
	if (isc_buffer_usedlength(buf) != token.value.as_ulong) {
		result = ISC_R_UNEXPECTEDEND;
		goto failure;
	}

	if (dns_rdatatype_isknown(type)) {
		result = rdata_validate(buf, target, rdclass, type);
	} else {
		isc_region_t r;
		isc_buffer_usedregion(buf, &r);
		result = isc_buffer_copyregion(target, &r);
	}
	if (result != ISC_R_SUCCESS)
		goto failure;

	isc_buffer_free(&buf);
	return (ISC_R_SUCCESS);

 failure:
	isc_buffer_free(&buf);
	return (result);
}

isc_result_t
dns_rdata_fromtext(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_lex_t *lexer,
		   dns_name_t *origin, isc_boolean_t downcase, isc_mem_t *mctx,
		   isc_buffer_t *target, dns_rdatacallbacks_t *callbacks)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_region_t region;
	isc_buffer_t st;
	isc_token_t token;
	unsigned int options = ISC_LEXOPT_EOL | ISC_LEXOPT_EOF |
			       ISC_LEXOPT_DNSMULTILINE | ISC_LEXOPT_ESCAPE;
	char *name;
	unsigned long line;
	void (*callback)(dns_rdatacallbacks_t *, const char *, ...);
	isc_result_t tresult;

	REQUIRE(origin == NULL || dns_name_isabsolute(origin) == ISC_TRUE);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}
	if (callbacks != NULL) {
		REQUIRE(callbacks->warn != NULL);
		REQUIRE(callbacks->error != NULL);
	}

	st = *target;

	if (callbacks != NULL)
		callback = callbacks->error;
	else
		callback = default_fromtext_callback;

	result = isc_lex_getmastertoken(lexer, &token, isc_tokentype_qstring,
					ISC_FALSE);
	if (result != ISC_R_SUCCESS) {
		name = isc_lex_getsourcename(lexer);
		line = isc_lex_getsourceline(lexer);
		fromtext_error(callback, callbacks, name, line,
			       &token, result);
		return (result);
	}

	if (strcmp((char *)token.value.as_pointer, "\\#") == 0)
		result = unknown_fromtext(rdclass, type, lexer, mctx, target);
	else {
		isc_lex_ungettoken(lexer, &token);

		FROMTEXTSWITCH
	}

	/*
	 * Consume to end of line / file.
	 * If not at end of line initially set error code.
	 * Call callback via fromtext_error once if there was an error.
	 */
	do {
		name = isc_lex_getsourcename(lexer);
		line = isc_lex_getsourceline(lexer);
		tresult = isc_lex_gettoken(lexer, options, &token);
		if (tresult != ISC_R_SUCCESS) {
			if (result == ISC_R_SUCCESS)
				result = tresult;
			if (callback != NULL)
				fromtext_error(callback, callbacks, name,
					       line, NULL, result);
			break;
		} else if (token.type != isc_tokentype_eol &&
			   token.type != isc_tokentype_eof) {
			if (result == ISC_R_SUCCESS)
				result = DNS_R_EXTRATOKEN;
			if (callback != NULL) {
				fromtext_error(callback, callbacks, name,
					       line, &token, result);
				callback = NULL;
			}
		} else if (result != ISC_R_SUCCESS && callback != NULL) {
			fromtext_error(callback, callbacks, name, line,
				       &token, result);
			break;
		} else {
			if (token.type == isc_tokentype_eof)
				fromtext_warneof(lexer, callbacks);
			break;
		}
	} while (1);

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = isc_buffer_usedlength(target) -
				isc_buffer_usedlength(&st);
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}
	if (result != ISC_R_SUCCESS) {
		*target = st;
	}
	return (result);
}

static isc_result_t
rdata_totext(dns_rdata_t *rdata, dns_rdata_textctx_t *tctx,
	     isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;
	char buf[sizeof("65536")];
	isc_region_t sr;

	REQUIRE(rdata != NULL);
	REQUIRE(tctx->origin == NULL ||
		dns_name_isabsolute(tctx->origin) == ISC_TRUE);

	/*
	 * Some DynDNS meta-RRs have empty rdata.
	 */
	if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
		INSIST(rdata->length == 0);
		return (ISC_R_SUCCESS);
	}

	TOTEXTSWITCH

	if (use_default) {
		sprintf(buf, "\\# ");
		result = str_totext(buf, target);
		dns_rdata_toregion(rdata, &sr);
		INSIST(sr.length < 65536);
		sprintf(buf, "%u", sr.length);
		result = str_totext(buf, target);
		if (sr.length != 0 && result == ISC_R_SUCCESS) {
			if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
				result = str_totext(" ( ", target);
			else
				result = str_totext(" ", target);
			if (result == ISC_R_SUCCESS)
				result = isc_hex_totext(&sr, tctx->width - 2,
							tctx->linebreak,
							target);
			if (result == ISC_R_SUCCESS &&
			    (tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
				result = str_totext(" )", target);
		}
	}

	return (result);
}

isc_result_t
dns_rdata_totext(dns_rdata_t *rdata, dns_name_t *origin, isc_buffer_t *target)
{
	dns_rdata_textctx_t tctx;

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Set up formatting options for single-line output.
	 */
	tctx.origin = origin;
	tctx.flags = 0;
	tctx.width = 60;
	tctx.linebreak = " ";
	return (rdata_totext(rdata, &tctx, target));
}

isc_result_t
dns_rdata_tofmttext(dns_rdata_t *rdata, dns_name_t *origin,
		    unsigned int flags, unsigned int width,
		    char *linebreak, isc_buffer_t *target)
{
	dns_rdata_textctx_t tctx;

	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	/*
	 * Set up formatting options for formatted output.
	 */
	tctx.origin = origin;
	tctx.flags = flags;
	if ((flags & DNS_STYLEFLAG_MULTILINE) != 0) {
		tctx.width = width;
		tctx.linebreak = linebreak;
	} else {
		tctx.width = 60; /* Used for hex word length only. */
		tctx.linebreak = " ";
	}
	return (rdata_totext(rdata, &tctx, target));
}

isc_result_t
dns_rdata_fromstruct(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, void *source,
		     isc_buffer_t *target)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_buffer_t st;
	isc_region_t region;
	isc_boolean_t use_default = ISC_FALSE;

	REQUIRE(source != NULL);
	if (rdata != NULL) {
		REQUIRE(DNS_RDATA_INITIALIZED(rdata));
		REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));
	}

	st = *target;

	FROMSTRUCTSWITCH

	if (use_default)
		(void)NULL;

	if (rdata != NULL && result == ISC_R_SUCCESS) {
		region.base = isc_buffer_used(&st);
		region.length = isc_buffer_usedlength(target) -
				isc_buffer_usedlength(&st);
		dns_rdata_fromregion(rdata, rdclass, type, &region);
	}
	if (result != ISC_R_SUCCESS)
		*target = st;
	return (result);
}

isc_result_t
dns_rdata_tostruct(dns_rdata_t *rdata, void *target, isc_mem_t *mctx) {
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;

	REQUIRE(rdata != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	TOSTRUCTSWITCH

	if (use_default)
		(void)NULL;

	return (result);
}

void
dns_rdata_freestruct(void *source) {
	dns_rdatacommon_t *common = source;
	REQUIRE(source != NULL);

	FREESTRUCTSWITCH
}

isc_result_t
dns_rdata_additionaldata(dns_rdata_t *rdata, dns_additionaldatafunc_t add,
			 void *arg)
{
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;

	/*
	 * Call 'add' for each name and type from 'rdata' which is subject to
	 * additional section processing.
	 */

	REQUIRE(rdata != NULL);
	REQUIRE(add != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	ADDITIONALDATASWITCH

	/* No additional processing for unknown types */
	if (use_default)
		result = ISC_R_SUCCESS;

	return (result);
}

isc_result_t
dns_rdata_digest(dns_rdata_t *rdata, dns_digestfunc_t digest, void *arg) {
	isc_result_t result = ISC_R_NOTIMPLEMENTED;
	isc_boolean_t use_default = ISC_FALSE;
	isc_region_t r;

	/*
	 * Send 'rdata' in DNSSEC canonical form to 'digest'.
	 */

	REQUIRE(rdata != NULL);
	REQUIRE(digest != NULL);
	REQUIRE(DNS_RDATA_VALIDFLAGS(rdata));

	DIGESTSWITCH

	if (use_default) {
		dns_rdata_toregion(rdata, &r);
		result = (digest)(arg, &r);
	}

	return (result);
}

unsigned int
dns_rdatatype_attributes(dns_rdatatype_t type)
{
	if (type < (sizeof(typeattr)/sizeof(typeattr[0])))
		return (typeattr[type].flags);
	return (DNS_RDATATYPEATTR_UNKNOWN);
}

#define NUMBERSIZE sizeof("037777777777") /* 2^32-1 octal + NUL */

static isc_result_t
dns_mnemonic_fromtext(unsigned int *valuep, isc_textregion_t *source,
		      struct tbl *table, unsigned int max)
{
	int i;

	if (isdigit(source->base[0] & 0xff) &&
	    source->length <= NUMBERSIZE - 1) {
		unsigned int n;
		char *e;
		char buffer[NUMBERSIZE];
		/*
		 * We have a potential number.  Try to parse it with strtoul().
		 * strtoul() requires null termination, so we must make
		 * a copy.
		 */
		strncpy(buffer, source->base, NUMBERSIZE);
		INSIST(buffer[source->length] == '\0');

		n = strtoul(buffer, &e, 10);
		if (*e == 0) {
			if (n > max)
				return (ISC_R_RANGE);
			*valuep = n;
			return (ISC_R_SUCCESS);
		}
		/*
		 * It was not a number after all; fall through.
		 */
	}

	for (i = 0; table[i].name != NULL; i++) {
		unsigned int n;
		n = strlen(table[i].name);
		if (n == source->length &&
		    strncasecmp(source->base, table[i].name, n) == 0) {
			*valuep = table[i].value;
			return (ISC_R_SUCCESS);
		}
	}
	return (DNS_R_UNKNOWN);
}

static isc_result_t
dns_mnemonic_totext(unsigned int value, isc_buffer_t *target,
		    struct tbl *table)
{
	int i = 0;
	char buf[sizeof "4294967296"];
	while (table[i].name != NULL) {
		if (table[i].value == value) {
			return (str_totext(table[i].name, target));
		}
		i++;
	}
	sprintf(buf, "%u", value);
	return (str_totext(buf, target));
}


/*
 * This uses lots of hard coded values, but how often do we actually
 * add classes?
 */
isc_result_t
dns_rdataclass_fromtext(dns_rdataclass_t *classp, isc_textregion_t *source) {
#define COMPARE(string, rdclass) \
	if (((sizeof(string) - 1) == source->length) \
	    && (strncasecmp(source->base, string, source->length) == 0)) { \
		*classp = rdclass; \
		return (ISC_R_SUCCESS); \
	}

	switch (tolower((unsigned char)source->base[0])) {
	case 'a':
		COMPARE("any", dns_rdataclass_any);
		break;
	case 'c':
		/*
		 * RFC1035 says the mnemonic for the CHAOS class is CH,
		 * but historical BIND practice is to call it CHAOS.
		 * We will accept both forms, but only generate CH.
		 */
		COMPARE("ch", dns_rdataclass_chaos);
		COMPARE("chaos", dns_rdataclass_chaos);

		if (source->length > 5 &&
		    source->length < (5 + sizeof("65000")) &&
		    strncasecmp("class", source->base, 5) == 0) {
			char buf[sizeof("65000")];
			char *endp;
			unsigned int val;

			strncpy(buf, source->base + 5, source->length - 5);
			buf[source->length - 5] = '\0';
			val = strtoul(buf, &endp, 10);
			if (*endp == '\0' && val <= 0xffff) {
				*classp = (dns_rdataclass_t)val;
				return (ISC_R_SUCCESS);
			}
		}
		break;
	case 'h':
		COMPARE("hs", dns_rdataclass_hs);
		COMPARE("hesiod", dns_rdataclass_hs);
		break;
	case 'i':
		COMPARE("in", dns_rdataclass_in);
		break;
	case 'n':
		COMPARE("none", dns_rdataclass_none);
		break;
	case 'r':
		COMPARE("reserved0", dns_rdataclass_reserved0);
		break;
	}

#undef COMPARE

	return (DNS_R_UNKNOWN);
}

isc_result_t
dns_rdataclass_totext(dns_rdataclass_t rdclass, isc_buffer_t *target) {
	char buf[sizeof("CLASS65535")];

	switch (rdclass) {
	case dns_rdataclass_any:
		return (str_totext("ANY", target));
	case dns_rdataclass_chaos:
		return (str_totext("CH", target));
	case dns_rdataclass_hs:
		return (str_totext("HS", target));
	case dns_rdataclass_in:
		return (str_totext("IN", target));
	case dns_rdataclass_none:
		return (str_totext("NONE", target));
	case dns_rdataclass_reserved0:
		return (str_totext("RESERVED0", target));
	default:
		sprintf(buf, "CLASS%u", rdclass);
		return (str_totext(buf, target));
	}
}

void
dns_rdataclass_format(dns_rdataclass_t rdclass,
		      char *array, unsigned int size)
{
	isc_result_t result;
	isc_buffer_t buf;

	isc_buffer_init(&buf, array, size);
	result = dns_rdataclass_totext(rdclass, &buf);
	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}
	if (result != ISC_R_SUCCESS) {
		snprintf(array, size, "<unknown>");
		array[size - 1] = '\0';
	}
}

isc_result_t
dns_rdatatype_fromtext(dns_rdatatype_t *typep, isc_textregion_t *source) {
	unsigned int hash;
	unsigned int n;
	unsigned char a, b;

	n = source->length;

	if (n == 0)
		return (DNS_R_UNKNOWN);

	a = tolower((unsigned char)source->base[0]);
	b = tolower((unsigned char)source->base[n - 1]);

	hash = ((a + n) * b) % 256;

	/*
	 * This switch block is inlined via #define, and will use "return"
	 * to return a result to the caller if it is a valid (known)
	 * rdatatype name.
	 */
	RDATATYPE_FROMTEXT_SW(hash, source->base, n, typep);

	if (source->length > 4 && source->length < (4 + sizeof("65000")) &&
	    strncasecmp("type", source->base, 4) == 0) {
		char buf[sizeof("65000")];
		char *endp;
		unsigned int val;

		strncpy(buf, source->base + 4, source->length - 4);
		buf[source->length - 4] = '\0';
		val = strtoul(buf, &endp, 10);
		if (*endp == '\0' && val <= 0xffff) {
			*typep = (dns_rdatatype_t)val;
			return (ISC_R_SUCCESS);
		}
	}

	return (DNS_R_UNKNOWN);
}

isc_result_t
dns_rdatatype_totext(dns_rdatatype_t type, isc_buffer_t *target) {
	char buf[sizeof("TYPE65536")];

	if (type < (sizeof(typeattr)/sizeof(typeattr[0])))
		return (str_totext(typeattr[type].name, target));
	snprintf(buf, sizeof buf, "TYPE%u", type);
	return (str_totext(buf, target));
}

void
dns_rdatatype_format(dns_rdatatype_t rdtype,
		     char *array, unsigned int size)
{
	isc_result_t result;
	isc_buffer_t buf;

	isc_buffer_init(&buf, array, size);
	result = dns_rdatatype_totext(rdtype, &buf);
	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}
	if (result != ISC_R_SUCCESS) {
		snprintf(array, size, "<unknown>");
		array[size - 1] = '\0';
	}
}


/* XXXRTH  Should we use a hash table here? */

isc_result_t
dns_rcode_fromtext(dns_rcode_t *rcodep, isc_textregion_t *source) {
	unsigned int value;
	RETERR(dns_mnemonic_fromtext(&value, source, rcodes, 0xffff));
	*rcodep = value;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_rcode_totext(dns_rcode_t rcode, isc_buffer_t *target) {
	return (dns_mnemonic_totext(rcode, target, rcodes));
}

isc_result_t
dns_tsigrcode_fromtext(dns_rcode_t *rcodep, isc_textregion_t *source) {
	unsigned int value;
	RETERR(dns_mnemonic_fromtext(&value, source, tsigrcodes, 0xffff));
	*rcodep = value;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_tsigrcode_totext(dns_rcode_t rcode, isc_buffer_t *target) {
	return (dns_mnemonic_totext(rcode, target, tsigrcodes));
}

isc_result_t
dns_cert_fromtext(dns_cert_t *certp, isc_textregion_t *source) {
	unsigned int value;
	RETERR(dns_mnemonic_fromtext(&value, source, certs, 0xffff));
	*certp = value;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_cert_totext(dns_cert_t cert, isc_buffer_t *target) {
	return (dns_mnemonic_totext(cert, target, certs));
}

isc_result_t
dns_secalg_fromtext(dns_secalg_t *secalgp, isc_textregion_t *source) {
	unsigned int value;
	RETERR(dns_mnemonic_fromtext(&value, source, secalgs, 0xff));
	*secalgp = value;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_secalg_totext(dns_secalg_t secalg, isc_buffer_t *target) {
	return (dns_mnemonic_totext(secalg, target, secalgs));
}

isc_result_t
dns_secproto_fromtext(dns_secproto_t *secprotop, isc_textregion_t *source) {
	unsigned int value;
	RETERR(dns_mnemonic_fromtext(&value, source, secprotos, 0xff));
	*secprotop = value;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_secproto_totext(dns_secproto_t secproto, isc_buffer_t *target) {
	return (dns_mnemonic_totext(secproto, target, secprotos));
}

isc_result_t
dns_keyflags_fromtext(dns_keyflags_t *flagsp, isc_textregion_t *source)
{
	char *text, *end;
	unsigned int value, mask;

	if (isdigit(source->base[0] & 0xff) &&
	    source->length <= NUMBERSIZE - 1) {
		unsigned int n;
		char *e;
		char buffer[NUMBERSIZE];
		/*
		 * We have a potential number.  Try to parse it with strtoul().
		 * strtoul() requires null termination, so we must make
		 * a copy.
		 */
		strncpy(buffer, source->base, NUMBERSIZE);
		INSIST(buffer[source->length] == '\0');

		n = strtoul(buffer, &e, 0); /* Allow hex/octal. */
		if (*e == 0) {
			if (n > 0xffff)
				return (ISC_R_RANGE);
			*flagsp = n;
			return (ISC_R_SUCCESS);
		}
		/* It was not a number after all; fall through. */
	}

	text = source->base;
	end = source->base + source->length;
	value = mask = 0;

	while (text < end) {
		struct keyflag *p;
		unsigned int len;
		char *delim = memchr(text, '|', end - text);
		if (delim != NULL)
			len = delim - text;
		else
			len = end - text;
		for (p = keyflags; p->name != NULL; p++) {
			if (strncasecmp(p->name, text, len) == 0)
				break;
		}
		if (p->name == NULL)
			return (DNS_R_UNKNOWN);
		value |= p->value;
#ifdef notyet
		if ((mask & p->mask) != 0)
			warn("overlapping key flags");
#endif
		mask |= p->mask;
		text += len;
		if (delim != NULL)
			text++;	/* Skip "|" */
	}
	*flagsp = value;
	return (ISC_R_SUCCESS);
}

/*
 * Private function.
 */

static unsigned int
name_length(dns_name_t *name) {
	return (name->length);
}

static isc_result_t
txt_totext(isc_region_t *source, isc_buffer_t *target) {
	unsigned int tl;
	unsigned int n;
	unsigned char *sp;
	char *tp;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	sp = source->base;
	tp = (char *)region.base;
	tl = region.length;

	n = *sp++;

	REQUIRE(n + 1 <= source->length);

	if (tl < 1)
		return (ISC_R_NOSPACE);
	*tp++ = '"';
	tl--;
	while (n--) {
		if (*sp < 0x20 || *sp >= 0x7f) {
			if (tl < 4)
				return (ISC_R_NOSPACE);
			sprintf(tp, "\\%03u", *sp++);
			tp += 4;
			tl -= 4;
			continue;
		}
		if (*sp == 0x22 || *sp == 0x3b || *sp == 0x5c) {
			if (tl < 2)
				return (ISC_R_NOSPACE);
			*tp++ = '\\';
			tl--;
		}
		if (tl < 1)
			return (ISC_R_NOSPACE);
		*tp++ = *sp++;
		tl--;
	}
	if (tl < 1)
		return (ISC_R_NOSPACE);
	*tp++ = '"';
	tl--;
	isc_buffer_add(target, tp - (char *)region.base);
	isc_region_consume(source, *source->base + 1);
	return (ISC_R_SUCCESS);
}

static isc_result_t
txt_fromtext(isc_textregion_t *source, isc_buffer_t *target) {
	isc_region_t tregion;
	isc_boolean_t escape;
	unsigned int n, nrem;
	char *s;
	unsigned char *t;
	int d;
	int c;

	isc_buffer_availableregion(target, &tregion);
	s = source->base;
	n = source->length;
	t = tregion.base;
	nrem = tregion.length;
	escape = ISC_FALSE;
	if (nrem < 1)
		return (ISC_R_NOSPACE);
	/*
	 * Length byte.
	 */
	nrem--;
	t++;
	/*
	 * Maximum text string length.
	 */
	if (nrem > 255)
		nrem = 255;
	while (n-- != 0) {
		c = (*s++) & 0xff;
		if (escape && (d = decvalue((char)c)) != -1) {
			c = d;
			if (n == 0)
				return (DNS_R_SYNTAX);
			n--;
			if ((d = decvalue(*s++)) != -1)
				c = c * 10 + d;
			else
				return (DNS_R_SYNTAX);
			if (n == 0)
				return (DNS_R_SYNTAX);
			n--;
			if ((d = decvalue(*s++)) != -1)
				c = c * 10 + d;
			else
				return (DNS_R_SYNTAX);
			if (c > 255)
				return (DNS_R_SYNTAX);
		} else if (!escape && c == '\\') {
			escape = ISC_TRUE;
			continue;
		}
		escape = ISC_FALSE;
		if (nrem == 0)
			return (ISC_R_NOSPACE);
		*t++ = c;
		nrem--;
	}
	if (escape)
		return (DNS_R_SYNTAX);
	*tregion.base = t - tregion.base - 1;
	isc_buffer_add(target, *tregion.base + 1);
	return (ISC_R_SUCCESS);
}

static isc_result_t
txt_fromwire(isc_buffer_t *source, isc_buffer_t *target) {
	unsigned int n;
	isc_region_t sregion;
	isc_region_t tregion;

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length == 0)
		return(ISC_R_UNEXPECTEDEND);
	n = *sregion.base + 1;
	if (n > sregion.length)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_availableregion(target, &tregion);
	if (n > tregion.length)
		return (ISC_R_NOSPACE);

	memcpy(tregion.base, sregion.base, n);
	isc_buffer_forward(source, n);
	isc_buffer_add(target, n);
	return (ISC_R_SUCCESS);
}

static isc_boolean_t
name_prefix(dns_name_t *name, dns_name_t *origin, dns_name_t *target) {
	int l1, l2;

	if (origin == NULL)
		goto return_false;

	if (dns_name_compare(origin, dns_rootname) == 0)
		goto return_false;

	if (!dns_name_issubdomain(name, origin))
		goto return_false;

	l1 = dns_name_countlabels(name);
	l2 = dns_name_countlabels(origin);

	if (l1 == l2)
		goto return_false;

	dns_name_getlabelsequence(name, 0, l1 - l2, target);
	return (ISC_TRUE);

return_false:
	*target = *name;
	return (ISC_FALSE);
}

static isc_result_t
str_totext(const char *source, isc_buffer_t *target) {
	unsigned int l;
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	l = strlen(source);

	if (l > region.length)
		return (ISC_R_NOSPACE);

	memcpy(region.base, source, l);
	isc_buffer_add(target, l);
	return (ISC_R_SUCCESS);
}

static isc_result_t
inet_totext(int af, isc_region_t *src, isc_buffer_t *target) {
	char tmpbuf[64];

	/* Note - inet_ntop doesn't do size checking on its input. */
	if (inet_ntop(af, src->base, tmpbuf, sizeof(tmpbuf)) == NULL)
		return (ISC_R_NOSPACE);
	if (strlen(tmpbuf) > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);
	isc_buffer_putstr(target, tmpbuf);
	return (ISC_R_SUCCESS);
}

static isc_boolean_t
buffer_empty(isc_buffer_t *source) {
	return((source->current == source->active) ? ISC_TRUE : ISC_FALSE);
}

static void
buffer_fromregion(isc_buffer_t *buffer, isc_region_t *region) {
	isc_buffer_init(buffer, region->base, region->length);
	isc_buffer_add(buffer, region->length);
	isc_buffer_setactive(buffer, region->length);
}

static isc_result_t
uint32_tobuffer(isc_uint32_t value, isc_buffer_t *target) {
	isc_region_t region;

	isc_buffer_availableregion(target, &region);
	if (region.length < 4)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint32(target, value);
	return (ISC_R_SUCCESS);
}

static isc_result_t
uint16_tobuffer(isc_uint32_t value, isc_buffer_t *target) {
	isc_region_t region;

	if (value > 0xffff)
		return (ISC_R_RANGE);
	isc_buffer_availableregion(target, &region);
	if (region.length < 2)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint16(target, (isc_uint16_t)value);
	return (ISC_R_SUCCESS);
}

static isc_result_t
uint8_tobuffer(isc_uint32_t value, isc_buffer_t *target) {
	isc_region_t region;

	if (value > 0xff)
		return (ISC_R_RANGE);
	isc_buffer_availableregion(target, &region);
	if (region.length < 1)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint8(target, (isc_uint8_t)value);
	return (ISC_R_SUCCESS);
}

static isc_result_t
name_tobuffer(dns_name_t *name, isc_buffer_t *target) {
	isc_region_t r;
	dns_name_toregion(name, &r);
	return (isc_buffer_copyregion(target, &r));
}

static isc_uint32_t
uint32_fromregion(isc_region_t *region) {
	unsigned long value;

	REQUIRE(region->length >= 4);
	value = region->base[0] << 24;
	value |= region->base[1] << 16;
	value |= region->base[2] << 8;
	value |= region->base[3];
	return(value);
}

static isc_uint16_t
uint16_fromregion(isc_region_t *region) {

	REQUIRE(region->length >= 2);

	return ((region->base[0] << 8) | region->base[1]);
}

static isc_uint8_t
uint8_fromregion(isc_region_t *region) {

	REQUIRE(region->length >= 1);

	return (region->base[0]);
}

static isc_result_t
mem_tobuffer(isc_buffer_t *target, void *base, unsigned int length) {
	isc_region_t tr;

	isc_buffer_availableregion(target, &tr);
	if (length > tr.length)
		return (ISC_R_NOSPACE);
	memcpy(tr.base, base, length);
	isc_buffer_add(target, length);
	return (ISC_R_SUCCESS);
}

static int
compare_region(isc_region_t *r1, isc_region_t *r2) {
	unsigned int l;
	int result;

	l = (r1->length < r2->length) ? r1->length : r2->length;

	if ((result = memcmp(r1->base, r2->base, l)) != 0)
		return ((result < 0) ? -1 : 1);
	else
		return ((r1->length == r2->length) ? 0 :
			(r1->length < r2->length) ? -1 : 1);
}

static int
hexvalue(char value) {
	char *s;
	unsigned char c;

	c = (unsigned char)value;

	if (!isascii(c))
		return (-1);
	if (isupper(c))
		c = tolower(c);
	if ((s = strchr(hexdigits, value)) == NULL)
		return (-1);
	return (s - hexdigits);
}

static int
decvalue(char value) {
	char *s;

	/*
	 * isascii() is valid for full range of int values, no need to
	 * mask or cast.
	 */
	if (!isascii(value))
		return (-1);
	if ((s = strchr(decdigits, value)) == NULL)
		return (-1);
	return (s - decdigits);
}

static const char atob_digits[86] =
	"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`" \
	"abcdefghijklmnopqrstu";
/*
 * Subroutines to convert between 8 bit binary bytes and printable ASCII.
 * Computes the number of bytes, and three kinds of simple checksums.
 * Incoming bytes are collected into 32-bit words, then printed in base 85:
 *	exp(85,5) > exp(2,32)
 * The ASCII characters used are between '!' and 'u';
 * 'z' encodes 32-bit zero; 'x' is used to mark the end of encoded data.
 *
 * Originally by Paul Rutter (philabs!per) and Joe Orost (petsd!joe) for
 * the atob/btoa programs, released with the compress program, in mod.sources.
 * Modified by Mike Schwartz 8/19/86 for use in BIND.
 * Modified to be re-entrant 3/2/99.
 */


struct state {
	isc_int32_t Ceor;
	isc_int32_t Csum;
	isc_int32_t Crot;
	isc_int32_t word;
	isc_int32_t bcount;
};

#define Ceor state->Ceor
#define Csum state->Csum
#define Crot state->Crot
#define word state->word
#define bcount state->bcount

#define times85(x)	((((((x<<2)+x)<<2)+x)<<2)+x)

static isc_result_t	byte_atob(int c, isc_buffer_t *target,
				  struct state *state);
static isc_result_t	putbyte(int c, isc_buffer_t *, struct state *state);
static isc_result_t	byte_btoa(int c, isc_buffer_t *, struct state *state);

/*
 * Decode ASCII-encoded byte c into binary representation and
 * place into *bufp, advancing bufp.
 */
static isc_result_t
byte_atob(int c, isc_buffer_t *target, struct state *state) {
	char *s;
	if (c == 'z') {
		if (bcount != 0)
			return(DNS_R_SYNTAX);
		else {
			RETERR(putbyte(0, target, state));
			RETERR(putbyte(0, target, state));
			RETERR(putbyte(0, target, state));
			RETERR(putbyte(0, target, state));
		}
	} else if ((s = strchr(atob_digits, c)) != NULL) {
		if (bcount == 0) {
			word = s - atob_digits;
			++bcount;
		} else if (bcount < 4) {
			word = times85(word);
			word += s - atob_digits;
			++bcount;
		} else {
			word = times85(word);
			word += s - atob_digits;
			RETERR(putbyte((word >> 24) & 0xff, target, state));
			RETERR(putbyte((word >> 16) & 0xff, target, state));
			RETERR(putbyte((word >> 8) & 0xff, target, state));
			RETERR(putbyte(word & 0xff, target, state));
			word = 0;
			bcount = 0;
		}
	} else
		return(DNS_R_SYNTAX);
	return(ISC_R_SUCCESS);
}

/*
 * Compute checksum info and place c into target.
 */
static isc_result_t
putbyte(int c, isc_buffer_t *target, struct state *state) {
	isc_region_t tr;

	Ceor ^= c;
	Csum += c;
	Csum += 1;
	if ((Crot & 0x80000000)) {
		Crot <<= 1;
		Crot += 1;
	} else {
		Crot <<= 1;
	}
	Crot += c;
	isc_buffer_availableregion(target, &tr);
	if (tr.length < 1)
		return (ISC_R_NOSPACE);
	tr.base[0] = c;
	isc_buffer_add(target, 1);
	return (ISC_R_SUCCESS);
}

/*
 * Read the ASCII-encoded data from inbuf, of length inbuflen, and convert
 * it into T_UNSPEC (binary data) in outbuf, not to exceed outbuflen bytes;
 * outbuflen must be divisible by 4.  (Note: this is because outbuf is filled
 * in 4 bytes at a time.  If the actual data doesn't end on an even 4-byte
 * boundary, there will be no problem...it will be padded with 0 bytes, and
 * numbytes will indicate the correct number of bytes.  The main point is
 * that since the buffer is filled in 4 bytes at a time, even if there is
 * not a full 4 bytes of data at the end, there has to be room to 0-pad the
 * data, so the buffer must be of size divisible by 4).  Place the number of
 * output bytes in numbytes, and return a failure/success status.
 */

static isc_result_t
atob_tobuffer(isc_lex_t *lexer, isc_buffer_t *target) {
	long oeor, osum, orot;
	struct state statebuf, *state= &statebuf;
	isc_token_t token;
	char c;
	char *e;

	Ceor = Csum = Crot = word = bcount = 0;

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	while (token.value.as_textregion.length != 0) {
		if ((c = token.value.as_textregion.base[0]) == 'x') {
			break;
		} else
			RETERR(byte_atob(c, target, state));
		isc_textregion_consume(&token.value.as_textregion, 1);
	}

	/*
	 * Number of bytes.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if ((token.value.as_ulong % 4) != 0U)
		isc_buffer_subtract(target,  4 - (token.value.as_ulong % 4));

	/*
	 * Checksum.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	oeor = strtol(token.value.as_pointer, &e, 16);
	if (*e != 0)
		return (DNS_R_SYNTAX);

	/*
	 * Checksum.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	osum = strtol(token.value.as_pointer, &e, 16);
	if (*e != 0)
		return (DNS_R_SYNTAX);

	/*
	 * Checksum.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	orot = strtol(token.value.as_pointer, &e, 16);
	if (*e != 0)
		return (DNS_R_SYNTAX);

	if ((oeor != Ceor) || (osum != Csum) || (orot != Crot))
		return(DNS_R_BADCKSUM);
	return (ISC_R_SUCCESS);
}

/*
 * Encode binary byte c into ASCII representation and place into *bufp,
 * advancing bufp.
 */
static isc_result_t
byte_btoa(int c, isc_buffer_t *target, struct state *state) {
	isc_region_t tr;

	isc_buffer_availableregion(target, &tr);
	Ceor ^= c;
	Csum += c;
	Csum += 1;
	if ((Crot & 0x80000000)) {
		Crot <<= 1;
		Crot += 1;
	} else {
		Crot <<= 1;
	}
	Crot += c;

	word <<= 8;
	word |= c;
	if (bcount == 3) {
		if (word == 0) {
			if (tr.length < 1)
				return (ISC_R_NOSPACE);
			tr.base[0] = 'z';
			isc_buffer_add(target, 1);
		} else {
		    register int tmp = 0;
		    register isc_int32_t tmpword = word;

		    if (tmpword < 0) {
			   /*
			    * Because some don't support u_long.
			    */
		    	tmp = 32;
		    	tmpword -= (isc_int32_t)(85 * 85 * 85 * 85 * 32);
		    }
		    if (tmpword < 0) {
		    	tmp = 64;
		    	tmpword -= (isc_int32_t)(85 * 85 * 85 * 85 * 32);
		    }
			if (tr.length < 5)
				return (ISC_R_NOSPACE);
		    	tr.base[0] = atob_digits[(tmpword /
					      (isc_int32_t)(85 * 85 * 85 * 85))
						+ tmp];
			tmpword %= (isc_int32_t)(85 * 85 * 85 * 85);
			tr.base[1] = atob_digits[tmpword / (85 * 85 * 85)];
			tmpword %= (85 * 85 * 85);
			tr.base[2] = atob_digits[tmpword / (85 * 85)];
			tmpword %= (85 * 85);
			tr.base[3] = atob_digits[tmpword / 85];
			tmpword %= 85;
			tr.base[4] = atob_digits[tmpword];
			isc_buffer_add(target, 5);
		}
		bcount = 0;
	} else {
		bcount += 1;
	}
	return (ISC_R_SUCCESS);
}


/*
 * Encode the binary data from inbuf, of length inbuflen, into a
 * target.  Return success/failure status
 */
static isc_result_t
btoa_totext(unsigned char *inbuf, int inbuflen, isc_buffer_t *target) {
	int inc;
	struct state statebuf, *state = &statebuf;
	char buf[sizeof "x 2000000000 ffffffff ffffffff ffffffff"];

	Ceor = Csum = Crot = word = bcount = 0;
	for (inc = 0; inc < inbuflen; inbuf++, inc++)
		RETERR(byte_btoa(*inbuf, target, state));

	while (bcount != 0)
		RETERR(byte_btoa(0, target, state));

	/*
	 * Put byte count and checksum information at end of buffer,
	 * delimited by 'x'
	 */
	sprintf(buf, "x %d %x %x %x", inbuflen, Ceor, Csum, Crot);
	return (str_totext(buf, target));
}


static void
default_fromtext_callback(dns_rdatacallbacks_t *callbacks, const char *fmt,
			  ...)
{
	va_list ap;

	UNUSED(callbacks);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void
fromtext_warneof(isc_lex_t *lexer, dns_rdatacallbacks_t *callbacks) {
	if (isc_lex_isfile(lexer) && callbacks != NULL) {
		const char *name = isc_lex_getsourcename(lexer);
		if (name == NULL)
			name = "UNKNOWN";
		(*callbacks->warn)(callbacks,
				   "%s:%lu: file does not end with newline",
				   name, isc_lex_getsourceline(lexer));
	}
}

static void
fromtext_error(void (*callback)(dns_rdatacallbacks_t *, const char *, ...),
	       dns_rdatacallbacks_t *callbacks, const char *name,
	       unsigned long line, isc_token_t *token, isc_result_t result)
{
	if (name == NULL)
		name = "UNKNOWN";

	if (token != NULL) {
		switch (token->type) {
		case isc_tokentype_eol:
			(*callback)(callbacks, "%s: %s:%lu: near eol: %s",
				    "dns_rdata_fromtext", name, line,
				    dns_result_totext(result));
			break;
		case isc_tokentype_eof:
			(*callback)(callbacks, "%s: %s:%lu: near eof: %s",
				    "dns_rdata_fromtext", name, line,
				    dns_result_totext(result));
			break;
		case isc_tokentype_number:
			(*callback)(callbacks, "%s: %s:%lu: near %lu: %s",
				    "dns_rdata_fromtext", name, line,
				    token->value.as_ulong,
				    dns_result_totext(result));
			break;
		case isc_tokentype_string:
		case isc_tokentype_qstring:
			(*callback)(callbacks, "%s: %s:%lu: near '%s': %s",
				    "dns_rdata_fromtext", name, line,
				    (char *)token->value.as_pointer,
				    dns_result_totext(result));
			break;
		default:
			(*callback)(callbacks, "%s: %s:%lu: %s",
				    "dns_rdata_fromtext", name, line,
				    dns_result_totext(result));
			break;
		}
	} else {
		(*callback)(callbacks, "dns_rdata_fromtext: %s:%lu: %s",
			    name, line, dns_result_totext(result));
	}
}

dns_rdatatype_t
dns_rdata_covers(dns_rdata_t *rdata) {
	return (covers_sig(rdata));
}

isc_boolean_t
dns_rdatatype_ismeta(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_META) != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdatatype_issingleton(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_SINGLETON)
	    != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdatatype_notquestion(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_NOTQUESTION)
	    != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdatatype_questiononly(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_QUESTIONONLY)
	    != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdataclass_ismeta(dns_rdataclass_t rdclass) {

	if (rdclass == dns_rdataclass_reserved0
	    || rdclass == dns_rdataclass_none
	    || rdclass == dns_rdataclass_any)
		return (ISC_TRUE);

	return (ISC_FALSE);  /* Assume it is not a meta class. */
}

isc_boolean_t
dns_rdatatype_isdnssec(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_DNSSEC) != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdatatype_iszonecutauth(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type)
	     & (DNS_RDATATYPEATTR_DNSSEC | DNS_RDATATYPEATTR_ZONECUTAUTH))
	    != 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

isc_boolean_t
dns_rdatatype_isknown(dns_rdatatype_t type) {
	if ((dns_rdatatype_attributes(type) & DNS_RDATATYPEATTR_UNKNOWN)
	    == 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

