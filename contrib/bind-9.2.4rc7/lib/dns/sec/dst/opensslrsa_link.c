/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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
 * Principal Author: Brian Wellington
 * $Id: opensslrsa_link.c,v 1.12.2.6 2004/03/09 06:11:42 marka Exp $
 */
#ifdef OPENSSL

#include <config.h>

#include <isc/entropy.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>

static isc_result_t opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data);

static isc_result_t
opensslrsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	UNUSED(key);

	if (dctx->key->key_alg == DST_ALG_RSAMD5) {
		isc_md5_t *md5ctx;

		md5ctx = isc_mem_get(dctx->mctx, sizeof(isc_md5_t));
		isc_md5_init(md5ctx);
		dctx->opaque = md5ctx;
	} else {
		isc_sha1_t *sha1ctx;

		sha1ctx = isc_mem_get(dctx->mctx, sizeof(isc_sha1_t));
		isc_sha1_init(sha1ctx);
		dctx->opaque = sha1ctx;
	}

	return (ISC_R_SUCCESS);
}

static void
opensslrsa_destroyctx(dst_context_t *dctx) {
	if (dctx->key->key_alg == DST_ALG_RSAMD5) {
		isc_md5_t *md5ctx = dctx->opaque;

		if (md5ctx != NULL) {
			isc_md5_invalidate(md5ctx);
			isc_mem_put(dctx->mctx, md5ctx, sizeof(isc_md5_t));
		}
	} else {
		isc_sha1_t *sha1ctx = dctx->opaque;

		if (sha1ctx != NULL) {
			isc_sha1_invalidate(sha1ctx);
			isc_mem_put(dctx->mctx, sha1ctx, sizeof(isc_sha1_t));
		}
	}
	dctx->opaque = NULL;
}

static isc_result_t
opensslrsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	if (dctx->key->key_alg == DST_ALG_RSAMD5) {
		isc_md5_t *md5ctx = dctx->opaque;
		isc_md5_update(md5ctx, data->base, data->length);
	} else {
		isc_sha1_t *sha1ctx = dctx->opaque;
		isc_sha1_update(sha1ctx, data->base, data->length);
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key = dctx->key;
	RSA *rsa = key->opaque;
	isc_region_t r;
	/* note: ISC_SHA1_DIGESTLENGTH > ISC_MD5_DIGESTLENGTH */
	unsigned char digest[ISC_SHA1_DIGESTLENGTH];
	unsigned int siglen;
	int status;
	int type;
	unsigned int digestlen;

	isc_buffer_availableregion(sig, &r);

	if (r.length < (unsigned int) RSA_size(rsa))
		return (ISC_R_NOSPACE);

	if (dctx->key->key_alg == DST_ALG_RSAMD5) {
		isc_md5_t *md5ctx = dctx->opaque;
		isc_md5_final(md5ctx, digest);
		type = NID_md5;
		digestlen = ISC_MD5_DIGESTLENGTH;
	} else {
		isc_sha1_t *sha1ctx = dctx->opaque;
		isc_sha1_final(sha1ctx, digest);
		type = NID_sha1;
		digestlen = ISC_SHA1_DIGESTLENGTH;
	}

	status = RSA_sign(type, digest, digestlen, r.base, &siglen, rsa);
	if (status == 0) {
		ERR_clear_error();
		return (DST_R_SIGNFAILURE);
	}

	isc_buffer_add(sig, siglen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	dst_key_t *key = dctx->key;
	RSA *rsa = key->opaque;
	/* note: ISC_SHA1_DIGESTLENGTH > ISC_MD5_DIGESTLENGTH */
	unsigned char digest[ISC_SHA1_DIGESTLENGTH];
	int status = 0;
	int type;
	unsigned int digestlen;

	if (dctx->key->key_alg == DST_ALG_RSAMD5) {
		isc_md5_t *md5ctx = dctx->opaque;
		isc_md5_final(md5ctx, digest);
		type = NID_md5;
		digestlen = ISC_MD5_DIGESTLENGTH;
	} else {
		isc_sha1_t *sha1ctx = dctx->opaque;
		isc_sha1_final(sha1ctx, digest);
		type = NID_sha1;
		digestlen = ISC_SHA1_DIGESTLENGTH;
	}

	if (sig->length < (unsigned int) RSA_size(rsa))
		return (DST_R_VERIFYFAILURE);

	status = RSA_verify(type, digest, digestlen, sig->base,
			    RSA_size(rsa), rsa);
	if (status == 0) {
		ERR_clear_error();
		return (DST_R_VERIFYFAILURE);
	}

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
opensslrsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	int status;
	RSA *rsa1, *rsa2;

	rsa1 = (RSA *) key1->opaque;
	rsa2 = (RSA *) key2->opaque;

	if (rsa1 == NULL && rsa2 == NULL)
		return (ISC_TRUE);
	else if (rsa1 == NULL || rsa2 == NULL)
		return (ISC_FALSE);

	status = BN_cmp(rsa1->n, rsa2->n) ||
		 BN_cmp(rsa1->e, rsa2->e);

	if (status != 0)
		return (ISC_FALSE);

	if (rsa1->d != NULL || rsa2->d != NULL) {
		if (rsa1->d == NULL || rsa2->d == NULL)
			return (ISC_FALSE);
		status = BN_cmp(rsa1->d, rsa2->d) ||
			 BN_cmp(rsa1->p, rsa2->p) ||
			 BN_cmp(rsa1->q, rsa2->q);

		if (status != 0)
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

static isc_result_t
opensslrsa_generate(dst_key_t *key, int exp) {
	RSA *rsa;
	unsigned long e;

	if (exp == 0)
		e = RSA_3;
	else
		e = RSA_F4;
	rsa = RSA_generate_key(key->key_size, e, NULL, NULL);
	if (rsa == NULL) {
		ERR_clear_error();
		return (DST_R_OPENSSLFAILURE);
	}

	rsa->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE);
	rsa->flags |= RSA_FLAG_BLINDING;

	key->opaque = rsa;

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
opensslrsa_isprivate(const dst_key_t *key) {
	RSA *rsa = (RSA *) key->opaque;
	return (ISC_TF(rsa != NULL && rsa->d != NULL));
}

static isc_boolean_t
opensslrsa_issymmetric(void) {
        return (ISC_FALSE);
}

static void
opensslrsa_destroy(dst_key_t *key) {
	RSA *rsa = key->opaque;
	RSA_free(rsa);
	key->opaque = NULL;
}


static isc_result_t
opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	RSA *rsa;
	isc_region_t r;
	unsigned int e_bytes;
	unsigned int mod_bytes;

	REQUIRE(key->opaque != NULL);

	rsa = (RSA *) key->opaque;

	isc_buffer_availableregion(data, &r);

	e_bytes = BN_num_bytes(rsa->e);
	mod_bytes = BN_num_bytes(rsa->n);

	if (e_bytes < 256) {	/* key exponent is <= 2040 bits */
		if (r.length < 1)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint8(data, (isc_uint8_t) e_bytes);
	} else {
		if (r.length < 3)
			return (ISC_R_NOSPACE);
		isc_buffer_putuint8(data, 0);
		isc_buffer_putuint16(data, (isc_uint16_t) e_bytes);
	}

	if (r.length < e_bytes + mod_bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_availableregion(data, &r);

	BN_bn2bin(rsa->e, r.base);
	r.base += e_bytes;
	BN_bn2bin(rsa->n, r.base);

	isc_buffer_add(data, e_bytes + mod_bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	RSA *rsa;
	isc_region_t r;
	unsigned int e_bytes;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	rsa = RSA_new();
	if (rsa == NULL)
		return (ISC_R_NOMEMORY);
	rsa->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE);
	rsa->flags |= RSA_FLAG_BLINDING;

	if (r.length < 1) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	e_bytes = *r.base++;
	r.length--;

	if (e_bytes == 0) {
		if (r.length < 2) {
			RSA_free(rsa);
			return (DST_R_INVALIDPUBLICKEY);
		}
		e_bytes = ((*r.base++) << 8);
		e_bytes += *r.base++;
		r.length -= 2;
	}

	if (r.length < e_bytes) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	rsa->e = BN_bin2bn(r.base, e_bytes, NULL);
	r.base += e_bytes;
	r.length -= e_bytes;

	rsa->n = BN_bin2bn(r.base, r.length, NULL);

	key->key_size = BN_num_bits(rsa->n);

	isc_buffer_forward(data, r.length);

	key->opaque = (void *) rsa;

	return (ISC_R_SUCCESS);
}


static isc_result_t
opensslrsa_tofile(const dst_key_t *key, const char *directory) {
	int i;
	RSA *rsa;
	dst_private_t priv;
	unsigned char *bufs[8];
	isc_result_t result;

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	rsa = (RSA *) key->opaque;

	for (i = 0; i < 8; i++) {
		bufs[i] = isc_mem_get(key->mctx, BN_num_bytes(rsa->n));
		if (bufs[i] == NULL) {
			result = ISC_R_NOMEMORY;
			goto fail;
		}
	}

	i = 0;

	priv.elements[i].tag = TAG_RSA_MODULUS;
	priv.elements[i].length = BN_num_bytes(rsa->n);
	BN_bn2bin(rsa->n, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PUBLICEXPONENT;
	priv.elements[i].length = BN_num_bytes(rsa->e);
	BN_bn2bin(rsa->e, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PRIVATEEXPONENT;
	priv.elements[i].length = BN_num_bytes(rsa->d);
	BN_bn2bin(rsa->d, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PRIME1;
	priv.elements[i].length = BN_num_bytes(rsa->p);
	BN_bn2bin(rsa->p, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PRIME2;
	priv.elements[i].length = BN_num_bytes(rsa->q);
	BN_bn2bin(rsa->q, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_EXPONENT1;
	priv.elements[i].length = BN_num_bytes(rsa->dmp1);
	BN_bn2bin(rsa->dmp1, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_EXPONENT2;
	priv.elements[i].length = BN_num_bytes(rsa->dmq1);
	BN_bn2bin(rsa->dmq1, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_COEFFICIENT;
	priv.elements[i].length = BN_num_bytes(rsa->iqmp);
	BN_bn2bin(rsa->iqmp, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.nelements = i;
	result =  dst__privstruct_writefile(key, &priv, directory);
 fail:
	for (i = 0; i < 8; i++) {
		if (bufs[i] == NULL)
			break;
		isc_mem_put(key->mctx, bufs[i], BN_num_bytes(rsa->n));
	}
	return (result);
}

static isc_result_t
opensslrsa_fromfile(dst_key_t *key, const char *filename) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	RSA *rsa = NULL;
	isc_mem_t *mctx = key->mctx;
#define DST_RET(a) {ret = a; goto err;}

	/* read private key file */
	ret = dst__privstruct_parsefile(key, filename, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	rsa = RSA_new();
	if (rsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	rsa->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE);
	rsa->flags |= RSA_FLAG_BLINDING;
	key->opaque = rsa;

	for (i = 0; i < priv.nelements; i++) {
		BIGNUM *bn;
		bn = BN_bin2bn(priv.elements[i].data,
			       priv.elements[i].length, NULL);
		if (bn == NULL)
			DST_RET(ISC_R_NOMEMORY);

		switch (priv.elements[i].tag) {
			case TAG_RSA_MODULUS:
				rsa->n = bn;
				break;
			case TAG_RSA_PUBLICEXPONENT:
				rsa->e = bn;
				break;
			case TAG_RSA_PRIVATEEXPONENT:
				rsa->d = bn;
				break;
			case TAG_RSA_PRIME1:
				rsa->p = bn;
				break;
			case TAG_RSA_PRIME2:
				rsa->q = bn;
				break;
			case TAG_RSA_EXPONENT1:
				rsa->dmp1 = bn;
				break;
			case TAG_RSA_EXPONENT2:
				rsa->dmq1 = bn;
				break;
			case TAG_RSA_COEFFICIENT:
				rsa->iqmp = bn;
				break;
		}
	}
	dst__privstruct_free(&priv, mctx);

	key->key_size = BN_num_bits(rsa->n);

	return (ISC_R_SUCCESS);

 err:
	opensslrsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t opensslrsa_functions = {
	opensslrsa_createctx,
	opensslrsa_destroyctx,
	opensslrsa_adddata,
	opensslrsa_sign,
	opensslrsa_verify,
	NULL, /* computesecret */
	opensslrsa_compare,
	NULL, /* paramcompare */
	opensslrsa_generate,
	opensslrsa_isprivate,
	opensslrsa_issymmetric,
	opensslrsa_destroy,
	opensslrsa_todns,
	opensslrsa_fromdns,
	opensslrsa_tofile,
	opensslrsa_fromfile,
};

isc_result_t
dst__opensslrsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL && *funcp == NULL);
	*funcp = &opensslrsa_functions;
	return (ISC_R_SUCCESS);
}

void
dst__opensslrsa_destroy(void) {
}

#endif /* OPENSSL */
