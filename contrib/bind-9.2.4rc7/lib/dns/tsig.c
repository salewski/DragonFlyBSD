/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/*
 * $Id: tsig.c,v 1.112.2.4 2004/03/09 06:11:09 marka Exp $
 */

#include <config.h>
#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/refcount.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/rbt.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/result.h>
#include <dns/tsig.h>

#include <dst/result.h>

#define TSIG_MAGIC		ISC_MAGIC('T', 'S', 'I', 'G')
#define VALID_TSIG_KEY(x)	ISC_MAGIC_VALID(x, TSIG_MAGIC)

#define is_response(msg) (msg->flags & DNS_MESSAGEFLAG_QR)
#define algname_is_allocated(algname) \
	((algname) != dns_tsig_hmacmd5_name && \
	 (algname) != dns_tsig_gssapi_name && \
	 (algname) != dns_tsig_gssapims_name)

#define BADTIMELEN 6

static unsigned char hmacmd5_ndata[] = "\010hmac-md5\007sig-alg\003reg\003int";
static unsigned char hmacmd5_offsets[] = { 0, 9, 17, 21, 25 };

static dns_name_t hmacmd5 = {
	DNS_NAME_MAGIC,
	hmacmd5_ndata, 26, 5,
	DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE,
	hmacmd5_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

dns_name_t *dns_tsig_hmacmd5_name = &hmacmd5;

static unsigned char gsstsig_ndata[] = "\010gss-tsig";
static unsigned char gsstsig_offsets[] = { 0, 9 };

static dns_name_t gsstsig = {
	DNS_NAME_MAGIC,
	gsstsig_ndata, 10, 2,
	DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE,
	gsstsig_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

dns_name_t *dns_tsig_gssapi_name = &gsstsig;

/* It's nice of Microsoft to conform to their own standard. */
static unsigned char gsstsigms_ndata[] = "\003gss\011microsoft\003com";
static unsigned char gsstsigms_offsets[] = { 0, 4, 14, 18 };

static dns_name_t gsstsigms = {
	DNS_NAME_MAGIC,
	gsstsigms_ndata, 19, 4,
	DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE,
	gsstsigms_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

dns_name_t *dns_tsig_gssapims_name = &gsstsigms;

static isc_result_t
tsig_verify_tcp(isc_buffer_t *source, dns_message_t *msg);

static void
tsig_log(dns_tsigkey_t *key, int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(3, 4);

static void
tsig_log(dns_tsigkey_t *key, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];
	char namestr[DNS_NAME_FORMATSIZE];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;
	if (key != NULL)
		dns_name_format(&key->name, namestr, sizeof(namestr));
	else
		strcpy(namestr, "<null>");
	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DNSSEC, DNS_LOGMODULE_TSIG,
		      level, "tsig key '%s': %s", namestr, message);
}

isc_result_t
dns_tsigkey_createfromkey(dns_name_t *name, dns_name_t *algorithm,
			  dst_key_t *dstkey, isc_boolean_t generated,
			  dns_name_t *creator, isc_stdtime_t inception,
			  isc_stdtime_t expire, isc_mem_t *mctx,
			  dns_tsig_keyring_t *ring, dns_tsigkey_t **key)
{
	dns_tsigkey_t *tkey;
	isc_result_t ret;
	unsigned int refs = 0;

	REQUIRE(key == NULL || *key == NULL);
	REQUIRE(name != NULL);
	REQUIRE(algorithm != NULL);
	REQUIRE(mctx != NULL);

	tkey = (dns_tsigkey_t *) isc_mem_get(mctx, sizeof(dns_tsigkey_t));
	if (tkey == NULL)
		return (ISC_R_NOMEMORY);

	dns_name_init(&tkey->name, NULL);
	ret = dns_name_dup(name, mctx, &tkey->name);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_key;
	dns_name_downcase(&tkey->name, &tkey->name, NULL);

	if (dns_name_equal(algorithm, DNS_TSIG_HMACMD5_NAME)) {
		tkey->algorithm = DNS_TSIG_HMACMD5_NAME;
		if (dstkey != NULL && dst_key_alg(dstkey) != DST_ALG_HMACMD5) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_GSSAPI_NAME)) {
		tkey->algorithm = DNS_TSIG_GSSAPI_NAME;
		if (dstkey != NULL && dst_key_alg(dstkey) != DST_ALG_GSSAPI) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else if (dns_name_equal(algorithm, DNS_TSIG_GSSAPIMS_NAME)) {
		tkey->algorithm = DNS_TSIG_GSSAPIMS_NAME;
		if (dstkey != NULL && dst_key_alg(dstkey) != DST_ALG_GSSAPI) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
	} else {
		if (key != NULL) {
			ret = DNS_R_BADALG;
			goto cleanup_name;
		}
		tkey->algorithm = isc_mem_get(mctx, sizeof(dns_name_t));
		if (tkey->algorithm == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_name;
		}
		dns_name_init(tkey->algorithm, NULL);
		ret = dns_name_dup(algorithm, mctx, tkey->algorithm);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_algorithm;
		dns_name_downcase(tkey->algorithm, tkey->algorithm, NULL);
	}

	if (creator != NULL) {
		tkey->creator = isc_mem_get(mctx, sizeof(dns_name_t));
		if (tkey->creator == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_algorithm;
		}
		dns_name_init(tkey->creator, NULL);
		ret = dns_name_dup(creator, mctx, tkey->creator);
		if (ret != ISC_R_SUCCESS) {
			isc_mem_put(mctx, tkey->creator, sizeof(dns_name_t));
			goto cleanup_algorithm;
		}
	} else
		tkey->creator = NULL;

	tkey->key = dstkey;
	tkey->ring = ring;

	if (ring != NULL) {
		RWLOCK(&ring->lock, isc_rwlocktype_write);
		ret = dns_rbt_addname(ring->keys, name, tkey);
		if (ret != ISC_R_SUCCESS) {
			RWUNLOCK(&ring->lock, isc_rwlocktype_write);
			goto cleanup_algorithm;
		}
		refs++;
		RWUNLOCK(&ring->lock, isc_rwlocktype_write);
	}

	if (key != NULL)
		refs++;
	isc_refcount_init(&tkey->refs, refs);
	tkey->generated = generated;
	tkey->inception = inception;
	tkey->expire = expire;
	tkey->mctx = mctx;

	tkey->magic = TSIG_MAGIC;

	if (dstkey != NULL && dst_key_size(dstkey) < 64) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DNSSEC,
			      DNS_LOGMODULE_TSIG, ISC_LOG_INFO,
			      "the key '%s' is too short to be secure",
			      namestr);
	}
	if (key != NULL)
		*key = tkey;

	return (ISC_R_SUCCESS);

 cleanup_algorithm:
	if (algname_is_allocated(tkey->algorithm)) {
		if (dns_name_dynamic(tkey->algorithm))
			dns_name_free(tkey->algorithm, mctx);
		isc_mem_put(mctx, tkey->algorithm, sizeof(dns_name_t));
	}
 cleanup_name:
	dns_name_free(&tkey->name, mctx);
 cleanup_key:
	isc_mem_put(mctx, tkey, sizeof(dns_tsigkey_t));

	return (ret);
}

isc_result_t
dns_tsigkey_create(dns_name_t *name, dns_name_t *algorithm,
		   unsigned char *secret, int length, isc_boolean_t generated,
		   dns_name_t *creator, isc_stdtime_t inception,
		   isc_stdtime_t expire, isc_mem_t *mctx,
		   dns_tsig_keyring_t *ring, dns_tsigkey_t **key)
{
	dst_key_t *dstkey = NULL;
	isc_result_t result;

	REQUIRE(length >= 0);
	if (length > 0)
		REQUIRE(secret != NULL);

	if (!dns_name_equal(algorithm, DNS_TSIG_HMACMD5_NAME) && length > 0)
		return (DNS_R_BADALG);

	if (secret != NULL) {
		isc_buffer_t b;

		isc_buffer_init(&b, secret, length);
		isc_buffer_add(&b, length);
		result = dst_key_frombuffer(name, DST_ALG_HMACMD5,
					    DNS_KEYOWNER_ENTITY,
					    DNS_KEYPROTO_DNSSEC,
					    dns_rdataclass_in,
					    &b, mctx, &dstkey);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	result = dns_tsigkey_createfromkey(name, algorithm, dstkey,
					   generated, creator,
					   inception, expire, mctx, ring, key);
	if (result != ISC_R_SUCCESS && dstkey != NULL)
		dst_key_free(&dstkey);
	return (result);
}

void
dns_tsigkey_attach(dns_tsigkey_t *source, dns_tsigkey_t **targetp) {
	REQUIRE(VALID_TSIG_KEY(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	isc_refcount_increment(&source->refs, NULL);
	*targetp = source;
}

static void
tsigkey_free(dns_tsigkey_t *key) {
	REQUIRE(VALID_TSIG_KEY(key));

	key->magic = 0;
	dns_name_free(&key->name, key->mctx);
	if (algname_is_allocated(key->algorithm)) {
		dns_name_free(key->algorithm, key->mctx);
		isc_mem_put(key->mctx, key->algorithm, sizeof(dns_name_t));
	}
	if (key->key != NULL)
		dst_key_free(&key->key);
	if (key->creator != NULL) {
		dns_name_free(key->creator, key->mctx);
		isc_mem_put(key->mctx, key->creator, sizeof(dns_name_t));
	}
	isc_refcount_destroy(&key->refs);
	isc_mem_put(key->mctx, key, sizeof(dns_tsigkey_t));
}

void
dns_tsigkey_detach(dns_tsigkey_t **keyp) {
	dns_tsigkey_t *key;
	unsigned int refs;

	REQUIRE(keyp != NULL);
	REQUIRE(VALID_TSIG_KEY(*keyp));

	key = *keyp;
	isc_refcount_decrement(&key->refs, &refs);

	if (refs == 0)
		tsigkey_free(key);

	*keyp = NULL;
}

void
dns_tsigkey_setdeleted(dns_tsigkey_t *key) {
	REQUIRE(VALID_TSIG_KEY(key));
	REQUIRE(key->ring != NULL);

	RWLOCK(&key->ring->lock, isc_rwlocktype_write);
	(void)dns_rbt_deletename(key->ring->keys, &key->name, ISC_FALSE);
	RWUNLOCK(&key->ring->lock, isc_rwlocktype_write);
}

static void
buffer_putuint48(isc_buffer_t *b, isc_uint64_t val) {
	isc_uint16_t valhi;
	isc_uint32_t vallo;

	valhi = (isc_uint16_t)(val >> 32);
	vallo = (isc_uint32_t)(val & 0xFFFFFFFF);
	isc_buffer_putuint16(b, valhi);
	isc_buffer_putuint32(b, vallo);
}

isc_result_t
dns_tsig_sign(dns_message_t *msg) {
	dns_tsigkey_t *key;
	dns_rdata_any_tsig_t tsig, querytsig;
	unsigned char data[128];
	isc_buffer_t databuf, sigbuf;
	isc_buffer_t *dynbuf;
	dns_name_t *owner;
	dns_rdata_t *rdata;
	dns_rdatalist_t *datalist;
	dns_rdataset_t *dataset;
	isc_region_t r;
	isc_stdtime_t now;
	isc_mem_t *mctx;
	dst_context_t *ctx = NULL;
	isc_result_t ret;
	unsigned char badtimedata[BADTIMELEN];
	unsigned int sigsize = 0;

	REQUIRE(msg != NULL);
	REQUIRE(VALID_TSIG_KEY(dns_message_gettsigkey(msg)));

	/*
	 * If this is a response, there should be a query tsig.
	 */
	if (is_response(msg) && msg->querytsig == NULL)
		return (DNS_R_EXPECTEDTSIG);

	dynbuf = NULL;

	mctx = msg->mctx;
	key = dns_message_gettsigkey(msg);

	tsig.mctx = mctx;
	tsig.common.rdclass = dns_rdataclass_any;
	tsig.common.rdtype = dns_rdatatype_tsig;
	ISC_LINK_INIT(&tsig.common, link);
	dns_name_init(&tsig.algorithm, NULL);
	dns_name_clone(key->algorithm, &tsig.algorithm);

	isc_stdtime_get(&now);
	tsig.timesigned = now + msg->timeadjust;
	tsig.fudge = DNS_TSIG_FUDGE;

	tsig.originalid = msg->id;

	isc_buffer_init(&databuf, data, sizeof(data));

	if (is_response(msg))
		tsig.error = msg->querytsigstatus;
	else
		tsig.error = dns_rcode_noerror;

	if (tsig.error != dns_tsigerror_badtime) {
		tsig.otherlen = 0;
		tsig.other = NULL;
	} else {
		isc_buffer_t otherbuf;

		tsig.otherlen = BADTIMELEN;
		tsig.other = badtimedata;
		isc_buffer_init(&otherbuf, tsig.other, tsig.otherlen);
		buffer_putuint48(&otherbuf, tsig.timesigned);
	}

	if (key->key != NULL && tsig.error != dns_tsigerror_badsig) {
		unsigned char header[DNS_MESSAGE_HEADERLEN];
		isc_buffer_t headerbuf;

		ret = dst_context_create(key->key, mctx, &ctx);
		if (ret != ISC_R_SUCCESS)
			return (ret);

		/*
		 * If this is a response, digest the query signature.
		 */
		if (is_response(msg)) {
			dns_rdata_t querytsigrdata = DNS_RDATA_INIT;

			ret = dns_rdataset_first(msg->querytsig);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			dns_rdataset_current(msg->querytsig, &querytsigrdata);
			ret = dns_rdata_tostruct(&querytsigrdata, &querytsig,
						 NULL);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			isc_buffer_putuint16(&databuf, querytsig.siglen);
			if (isc_buffer_availablelength(&databuf) <
			    querytsig.siglen)
			{
				ret = ISC_R_NOSPACE;
				goto cleanup_context;
			}
			isc_buffer_putmem(&databuf, querytsig.signature,
					  querytsig.siglen);
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}

		/*
		 * Digest the header.
		 */
		isc_buffer_init(&headerbuf, header, sizeof(header));
		dns_message_renderheader(msg, &headerbuf);
		isc_buffer_usedregion(&headerbuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the remainder of the message.
		 */
		isc_buffer_usedregion(msg->buffer, &r);
		isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (msg->tcp_continuation == 0) {
			/*
			 * Digest the name, class, ttl, alg.
			 */
			dns_name_toregion(&key->name, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			isc_buffer_clear(&databuf);
			isc_buffer_putuint16(&databuf, dns_rdataclass_any);
			isc_buffer_putuint32(&databuf, 0); /* ttl */
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			dns_name_toregion(&tsig.algorithm, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

		}
		/* Digest the timesigned and fudge */
		isc_buffer_clear(&databuf);
		if (tsig.error == dns_tsigerror_badtime)
			tsig.timesigned = querytsig.timesigned;
		buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (msg->tcp_continuation == 0) {
			/*
			 * Digest the error and other data length.
			 */
			isc_buffer_clear(&databuf);
			isc_buffer_putuint16(&databuf, tsig.error);
			isc_buffer_putuint16(&databuf, tsig.otherlen);

			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;

			/*
			 * Digest the error and other data.
			 */
			if (tsig.otherlen > 0) {
				r.length = tsig.otherlen;
				r.base = tsig.other;
				ret = dst_context_adddata(ctx, &r);
				if (ret != ISC_R_SUCCESS)
					goto cleanup_context;
			}
		}

		ret = dst_key_sigsize(key->key, &sigsize);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;
		tsig.signature = (unsigned char *) isc_mem_get(mctx, sigsize);
		if (tsig.signature == NULL) {
			ret = ISC_R_NOMEMORY;
			goto cleanup_context;
		}

		isc_buffer_init(&sigbuf, tsig.signature, sigsize);
		ret = dst_context_sign(ctx, &sigbuf);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_signature;
		dst_context_destroy(&ctx);
		tsig.siglen = isc_buffer_usedlength(&sigbuf);
	} else {
		tsig.siglen = 0;
		tsig.signature = NULL;
	}

	rdata = NULL;
	ret = dns_message_gettemprdata(msg, &rdata);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_signature;
	ret = isc_buffer_allocate(msg->mctx, &dynbuf, 512);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_signature;
	ret = dns_rdata_fromstruct(rdata, dns_rdataclass_any,
				   dns_rdatatype_tsig, &tsig, dynbuf);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_dynbuf;

	dns_message_takebuffer(msg, &dynbuf);

	if (tsig.signature != NULL) {
		isc_mem_put(mctx, tsig.signature, sigsize);
		tsig.signature = NULL;
	}

	owner = NULL;
	ret = dns_message_gettempname(msg, &owner);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_dynbuf;
	dns_name_init(owner, NULL);
	ret = dns_name_dup(&key->name, msg->mctx, owner);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_owner;

	datalist = NULL;
	ret = dns_message_gettemprdatalist(msg, &datalist);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_owner;
	datalist->rdclass = dns_rdataclass_any;
	datalist->type = dns_rdatatype_tsig;
	datalist->covers = 0;
	datalist->ttl = 0;
	ISC_LIST_INIT(datalist->rdata);
	ISC_LIST_APPEND(datalist->rdata, rdata, link);
	dataset = NULL;
	ret = dns_message_gettemprdataset(msg, &dataset);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_owner;
	dns_rdataset_init(dataset);
	dns_rdatalist_tordataset(datalist, dataset);
	msg->tsig = dataset;
	msg->tsigname = owner;

	return (ISC_R_SUCCESS);

cleanup_owner:
	if (owner != NULL)
		dns_message_puttempname(msg, &owner);
cleanup_dynbuf:
	if (dynbuf != NULL)
		isc_buffer_free(&dynbuf);
cleanup_signature:
	if (tsig.signature != NULL)
		isc_mem_put(mctx, tsig.signature, sigsize);
cleanup_context:
	if (ctx != NULL)
		dst_context_destroy(&ctx);
	return (ret);
}

isc_result_t
dns_tsig_verify(isc_buffer_t *source, dns_message_t *msg,
		dns_tsig_keyring_t *ring1, dns_tsig_keyring_t *ring2)
{
	dns_rdata_any_tsig_t tsig, querytsig;
	isc_region_t r, source_r, header_r, sig_r;
	isc_buffer_t databuf;
	unsigned char data[32];
	dns_name_t *keyname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_stdtime_t now;
	isc_result_t ret;
	dns_tsigkey_t *tsigkey;
	dst_key_t *key = NULL;
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	dst_context_t *ctx = NULL;
	isc_mem_t *mctx;
	isc_uint16_t addcount, id;

	REQUIRE(source != NULL);
	REQUIRE(DNS_MESSAGE_VALID(msg));
	tsigkey = dns_message_gettsigkey(msg);
	REQUIRE(tsigkey == NULL || VALID_TSIG_KEY(tsigkey));

	msg->verify_attempted = 1;

	if (msg->tcp_continuation)
		return (tsig_verify_tcp(source, msg));

	/*
	 * There should be a TSIG record...
	 */
	if (msg->tsig == NULL)
		return (DNS_R_EXPECTEDTSIG);

	/*
	 * If this is a response and there's no key or query TSIG, there
	 * shouldn't be one on the response.
	 */
	if (is_response(msg) &&
	    (tsigkey == NULL || msg->querytsig == NULL))
		return (DNS_R_UNEXPECTEDTSIG);

	mctx = msg->mctx;

	/*
	 * If we're here, we know the message is well formed and contains a
	 * TSIG record.
	 */

	keyname = msg->tsigname;
	ret = dns_rdataset_first(msg->tsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdataset_current(msg->tsig, &rdata);
	ret = dns_rdata_tostruct(&rdata, &tsig, NULL);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdata_reset(&rdata);
	if (is_response(msg)) {
		ret = dns_rdataset_first(msg->querytsig);
		if (ret != ISC_R_SUCCESS)
			return (ret);
		dns_rdataset_current(msg->querytsig, &rdata);
		ret = dns_rdata_tostruct(&rdata, &querytsig, NULL);
		if (ret != ISC_R_SUCCESS)
			return (ret);
	}

	/*
	 * Do the key name and algorithm match that of the query?
	 */
	if (is_response(msg) &&
	    (!dns_name_equal(keyname, &tsigkey->name) ||
	     !dns_name_equal(&tsig.algorithm, &querytsig.algorithm)))
	{
		msg->tsigstatus = dns_tsigerror_badkey;
		tsig_log(msg->tsigkey, 2,
			 "key name and algorithm do not match");
		return (DNS_R_TSIGVERIFYFAILURE);
	}

	/*
	 * Get the current time.
	 */
	isc_stdtime_get(&now);

	/*
	 * Find dns_tsigkey_t based on keyname.
	 */
	if (tsigkey == NULL) {
		ret = ISC_R_NOTFOUND;
		if (ring1 != NULL)
			ret = dns_tsigkey_find(&tsigkey, keyname,
					       &tsig.algorithm, ring1);
		if (ret == ISC_R_NOTFOUND && ring2 != NULL)
			ret = dns_tsigkey_find(&tsigkey, keyname,
					       &tsig.algorithm, ring2);
		if (ret != ISC_R_SUCCESS) {
			msg->tsigstatus = dns_tsigerror_badkey;
			ret = dns_tsigkey_create(keyname, &tsig.algorithm,
						 NULL, 0, ISC_FALSE, NULL,
						 now, now,
						 mctx, NULL, &msg->tsigkey);
			if (ret != ISC_R_SUCCESS)
				return (ret);
			tsig_log(msg->tsigkey, 2, "unknown key");
			return (DNS_R_TSIGVERIFYFAILURE);
		}
		msg->tsigkey = tsigkey;
	}

	key = tsigkey->key;

	/*
	 * Is the time ok?
	 */
	if (now + msg->timeadjust > tsig.timesigned + tsig.fudge) {
		msg->tsigstatus = dns_tsigerror_badtime;
		tsig_log(msg->tsigkey, 2, "signature has expired");
		return (DNS_R_CLOCKSKEW);
	} else if (now + msg->timeadjust < tsig.timesigned - tsig.fudge) {
		msg->tsigstatus = dns_tsigerror_badtime;
		tsig_log(msg->tsigkey, 2, "signature is in the future");
		return (DNS_R_CLOCKSKEW);
	}

	if (tsig.siglen > 0) {
		sig_r.base = tsig.signature;
		sig_r.length = tsig.siglen;

		ret = dst_context_create(key, mctx, &ctx);
		if (ret != ISC_R_SUCCESS)
			return (ret);

		if (is_response(msg)) {
			isc_buffer_init(&databuf, data, sizeof(data));
			isc_buffer_putuint16(&databuf, querytsig.siglen);
			isc_buffer_usedregion(&databuf, &r);
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
			if (querytsig.siglen > 0) {
				r.length = querytsig.siglen;
				r.base = querytsig.signature;
				ret = dst_context_adddata(ctx, &r);
				if (ret != ISC_R_SUCCESS)
					goto cleanup_context;
			}
		}

		/*
		 * Extract the header.
		 */
		isc_buffer_usedregion(source, &r);
		memcpy(header, r.base, DNS_MESSAGE_HEADERLEN);
		isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);

		/*
		 * Decrement the additional field counter.
		 */
		memcpy(&addcount, &header[DNS_MESSAGE_HEADERLEN - 2], 2);
		addcount = htons((isc_uint16_t)(ntohs(addcount) - 1));
		memcpy(&header[DNS_MESSAGE_HEADERLEN - 2], &addcount, 2);

		/*
		 * Put in the original id.
		 */
		id = htons(tsig.originalid);
		memcpy(&header[0], &id, 2);

		/*
		 * Digest the modified header.
		 */
		header_r.base = (unsigned char *) header;
		header_r.length = DNS_MESSAGE_HEADERLEN;
		ret = dst_context_adddata(ctx, &header_r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest all non-TSIG records.
		 */
		isc_buffer_usedregion(source, &source_r);
		r.base = source_r.base + DNS_MESSAGE_HEADERLEN;
		r.length = msg->sigstart - DNS_MESSAGE_HEADERLEN;
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the key name.
		 */
		dns_name_toregion(&tsigkey->name, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		isc_buffer_init(&databuf, data, sizeof(data));
		isc_buffer_putuint16(&databuf, tsig.common.rdclass);
		isc_buffer_putuint32(&databuf, msg->tsig->ttl);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the key algorithm.
		 */
		dns_name_toregion(tsigkey->algorithm, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		isc_buffer_clear(&databuf);
		buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_putuint16(&databuf, tsig.error);
		isc_buffer_putuint16(&databuf, tsig.otherlen);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(ctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		if (tsig.otherlen > 0) {
			r.base = tsig.other;
			r.length = tsig.otherlen;
			ret = dst_context_adddata(ctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}

		ret = dst_context_verify(ctx, &sig_r);
		if (ret == DST_R_VERIFYFAILURE) {
			msg->tsigstatus = dns_tsigerror_badsig;
			ret = DNS_R_TSIGVERIFYFAILURE;
			tsig_log(msg->tsigkey, 2,
				 "signature failed to verify");
			goto cleanup_context;
		} else if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		dst_context_destroy(&ctx);
	} else if (tsig.error != dns_tsigerror_badsig &&
		   tsig.error != dns_tsigerror_badkey)
	{
		msg->tsigstatus = dns_tsigerror_badsig;
		tsig_log(msg->tsigkey, 2, "signature was empty");
		return (DNS_R_TSIGVERIFYFAILURE);
	}

	msg->tsigstatus = dns_rcode_noerror;

	if (tsig.error != dns_rcode_noerror) {
		if (tsig.error == dns_tsigerror_badtime)
			return (DNS_R_CLOCKSKEW);
		else
			return (DNS_R_TSIGERRORSET);
	}

	msg->verified_sig = 1;

	return (ISC_R_SUCCESS);

cleanup_context:
	if (ctx != NULL)
		dst_context_destroy(&ctx);

	return (ret);
}

static isc_result_t
tsig_verify_tcp(isc_buffer_t *source, dns_message_t *msg) {
	dns_rdata_any_tsig_t tsig, querytsig;
	isc_region_t r, source_r, header_r, sig_r;
	isc_buffer_t databuf;
	unsigned char data[32];
	dns_name_t *keyname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_stdtime_t now;
	isc_result_t ret;
	dns_tsigkey_t *tsigkey;
	dst_key_t *key = NULL;
	unsigned char header[DNS_MESSAGE_HEADERLEN];
	isc_uint16_t addcount, id;
	isc_boolean_t has_tsig = ISC_FALSE;
	isc_mem_t *mctx;

	REQUIRE(source != NULL);
	REQUIRE(msg != NULL);
	REQUIRE(dns_message_gettsigkey(msg) != NULL);
	REQUIRE(msg->tcp_continuation == 1);
	REQUIRE(msg->querytsig != NULL);

	if (!is_response(msg))
		return (DNS_R_EXPECTEDRESPONSE);

	mctx = msg->mctx;

	tsigkey = dns_message_gettsigkey(msg);

	/*
	 * Extract and parse the previous TSIG
	 */
	ret = dns_rdataset_first(msg->querytsig);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdataset_current(msg->querytsig, &rdata);
	ret = dns_rdata_tostruct(&rdata, &querytsig, NULL);
	if (ret != ISC_R_SUCCESS)
		return (ret);
	dns_rdata_reset(&rdata);

	/*
	 * If there is a TSIG in this message, do some checks.
	 */
	if (msg->tsig != NULL) {
		has_tsig = ISC_TRUE;

		keyname = msg->tsigname;
		ret = dns_rdataset_first(msg->tsig);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;
		dns_rdataset_current(msg->tsig, &rdata);
		ret = dns_rdata_tostruct(&rdata, &tsig, NULL);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;

		/*
		 * Do the key name and algorithm match that of the query?
		 */
		if (!dns_name_equal(keyname, &tsigkey->name) ||
		    !dns_name_equal(&tsig.algorithm, &querytsig.algorithm))
		{
			msg->tsigstatus = dns_tsigerror_badkey;
			ret = DNS_R_TSIGVERIFYFAILURE;
			tsig_log(msg->tsigkey, 2,
				 "key name and algorithm do not match");
			goto cleanup_querystruct;
		}

		/*
		 * Is the time ok?
		 */
		isc_stdtime_get(&now);

		if (now + msg->timeadjust > tsig.timesigned + tsig.fudge) {
			msg->tsigstatus = dns_tsigerror_badtime;
			tsig_log(msg->tsigkey, 2, "signature has expired");
			ret = DNS_R_CLOCKSKEW;
			goto cleanup_querystruct;
		} else if (now + msg->timeadjust <
			   tsig.timesigned - tsig.fudge)
		{
			msg->tsigstatus = dns_tsigerror_badtime;
			tsig_log(msg->tsigkey, 2,
				 "signature is in the future");
			ret = DNS_R_CLOCKSKEW;
			goto cleanup_querystruct;
		}
	}

	key = tsigkey->key;

	if (msg->tsigctx == NULL) {
		ret = dst_context_create(key, mctx, &msg->tsigctx);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_querystruct;

		/*
		 * Digest the length of the query signature
		 */
		isc_buffer_init(&databuf, data, sizeof(data));
		isc_buffer_putuint16(&databuf, querytsig.siglen);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(msg->tsigctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		/*
		 * Digest the data of the query signature
		 */
		if (querytsig.siglen > 0) {
			r.length = querytsig.siglen;
			r.base = querytsig.signature;
			ret = dst_context_adddata(msg->tsigctx, &r);
			if (ret != ISC_R_SUCCESS)
				goto cleanup_context;
		}
	}

	/*
	 * Extract the header.
	 */
	isc_buffer_usedregion(source, &r);
	memcpy(header, r.base, DNS_MESSAGE_HEADERLEN);
	isc_region_consume(&r, DNS_MESSAGE_HEADERLEN);

	/*
	 * Decrement the additional field counter if necessary.
	 */
	if (has_tsig) {
		memcpy(&addcount, &header[DNS_MESSAGE_HEADERLEN - 2], 2);
		addcount = htons((isc_uint16_t)(ntohs(addcount) - 1));
		memcpy(&header[DNS_MESSAGE_HEADERLEN - 2], &addcount, 2);
	}

	/*
	 * Put in the original id.
	 */
	/* XXX Can TCP transfers be forwarded?  How would that work? */
	if (has_tsig) {
		id = htons(tsig.originalid);
		memcpy(&header[0], &id, 2);
	}

	/*
	 * Digest the modified header.
	 */
	header_r.base = (unsigned char *) header;
	header_r.length = DNS_MESSAGE_HEADERLEN;
	ret = dst_context_adddata(msg->tsigctx, &header_r);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	/*
	 * Digest all non-TSIG records.
	 */
	isc_buffer_usedregion(source, &source_r);
	r.base = source_r.base + DNS_MESSAGE_HEADERLEN;
	if (has_tsig)
		r.length = msg->sigstart - DNS_MESSAGE_HEADERLEN;
	else
		r.length = source_r.length - DNS_MESSAGE_HEADERLEN;
	ret = dst_context_adddata(msg->tsigctx, &r);
	if (ret != ISC_R_SUCCESS)
		goto cleanup_context;

	/*
	 * Digest the time signed and fudge.
	 */
	if (has_tsig) {
		isc_buffer_init(&databuf, data, sizeof(data));
		buffer_putuint48(&databuf, tsig.timesigned);
		isc_buffer_putuint16(&databuf, tsig.fudge);
		isc_buffer_usedregion(&databuf, &r);
		ret = dst_context_adddata(msg->tsigctx, &r);
		if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		sig_r.base = tsig.signature;
		sig_r.length = tsig.siglen;
		if (tsig.siglen == 0) {
			if (tsig.error != dns_rcode_noerror) {
				if (tsig.error == dns_tsigerror_badtime)
					ret = DNS_R_CLOCKSKEW;
				else
					ret = DNS_R_TSIGERRORSET;
			} else {
				tsig_log(msg->tsigkey, 2,
					 "signature is empty");
				ret = DNS_R_TSIGVERIFYFAILURE;
			}
			goto cleanup_context;
		}

		ret = dst_context_verify(msg->tsigctx, &sig_r);
		if (ret == DST_R_VERIFYFAILURE) {
			msg->tsigstatus = dns_tsigerror_badsig;
			tsig_log(msg->tsigkey, 2,
				 "signature failed to verify");
			ret = DNS_R_TSIGVERIFYFAILURE;
			goto cleanup_context;
		}
		else if (ret != ISC_R_SUCCESS)
			goto cleanup_context;

		dst_context_destroy(&msg->tsigctx);
	}

	msg->tsigstatus = dns_rcode_noerror;
	return (ISC_R_SUCCESS);

 cleanup_context:
	dst_context_destroy(&msg->tsigctx);

 cleanup_querystruct:
	dns_rdata_freestruct(&querytsig);

	return (ret);

}

isc_result_t
dns_tsigkey_find(dns_tsigkey_t **tsigkey, dns_name_t *name,
		 dns_name_t *algorithm, dns_tsig_keyring_t *ring)
{
	dns_tsigkey_t *key;
	isc_stdtime_t now;
	isc_result_t result;

	REQUIRE(tsigkey != NULL);
	REQUIRE(*tsigkey == NULL);
	REQUIRE(name != NULL);
	REQUIRE(ring != NULL);

	isc_stdtime_get(&now);
	RWLOCK(&ring->lock, isc_rwlocktype_read);
	key = NULL;
	result = dns_rbt_findname(ring->keys, name, 0, NULL, (void *)&key);
	if (result == DNS_R_PARTIALMATCH || result == ISC_R_NOTFOUND) {
		RWUNLOCK(&ring->lock, isc_rwlocktype_read);
		return (ISC_R_NOTFOUND);
	}
	if (algorithm != NULL && !dns_name_equal(key->algorithm, algorithm)) {
		RWUNLOCK(&ring->lock, isc_rwlocktype_read);
		return (ISC_R_NOTFOUND);
	}
	if (key->inception != key->expire && key->expire < now) {
		/*
		 * The key has expired.
		 */
		RWUNLOCK(&ring->lock, isc_rwlocktype_read);
		RWLOCK(&ring->lock, isc_rwlocktype_write);
		(void) dns_rbt_deletename(ring->keys, name, ISC_FALSE);
		RWUNLOCK(&ring->lock, isc_rwlocktype_write);
		return (ISC_R_NOTFOUND);
	}

	isc_refcount_increment(&key->refs, NULL);
	RWUNLOCK(&ring->lock, isc_rwlocktype_read);
	*tsigkey = key;
	return (ISC_R_SUCCESS);
}

static void
free_tsignode(void *node, void *_unused) {
	dns_tsigkey_t *key;

	UNUSED(_unused);

	REQUIRE(node != NULL);

	key = node;
	dns_tsigkey_detach(&key);
}

isc_result_t
dns_tsigkeyring_create(isc_mem_t *mctx, dns_tsig_keyring_t **ringp) {
	isc_result_t result;
	dns_tsig_keyring_t *ring;

	REQUIRE(mctx != NULL);
	REQUIRE(ringp != NULL);
	REQUIRE(*ringp == NULL);

	ring = isc_mem_get(mctx, sizeof(dns_tsig_keyring_t));
	if (ring == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_rwlock_init(&ring->lock, 0, 0);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_rwlock_init() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	ring->keys = NULL;
	result = dns_rbt_create(mctx, free_tsignode, NULL, &ring->keys);
	if (result != ISC_R_SUCCESS) {
		isc_rwlock_destroy(&ring->lock);
		isc_mem_put(mctx, ring, sizeof(dns_tsig_keyring_t));
		return (result);
	}

	ring->mctx = mctx;

	*ringp = ring;
	return (ISC_R_SUCCESS);
}

void
dns_tsigkeyring_destroy(dns_tsig_keyring_t **ringp) {
	dns_tsig_keyring_t *ring;

	REQUIRE(ringp != NULL);
	REQUIRE(*ringp != NULL);

	ring = *ringp;
	*ringp = NULL;

	dns_rbt_destroy(&ring->keys);
	isc_rwlock_destroy(&ring->lock);
	isc_mem_put(ring->mctx, ring, sizeof(dns_tsig_keyring_t));
}
