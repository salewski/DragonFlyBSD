/*	$NetBSD: src/lib/libc/citrus/citrus_stdenc.h,v 1.3 2003/07/10 08:50:44 tshiozak Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/citrus_stdenc.h,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

/*-
 * Copyright (c)2003 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef _CITRUS_STDENC_H_
#define _CITRUS_STDENC_H_

struct _citrus_stdenc;
struct _citrus_stdenc_ops;
struct _citrus_stdenc_traits;

#include "citrus_stdenc_local.h"

__BEGIN_DECLS
int _citrus_stdenc_open(struct _citrus_stdenc * __restrict * __restrict,
			char const * __restrict,
			const void * __restrict, size_t);
void _citrus_stdenc_close(struct _citrus_stdenc *);
__END_DECLS

static __inline int
_citrus_stdenc_init_state(struct _citrus_stdenc * __restrict ce,
			  void * __restrict ps)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_init_state);
	return (*ce->ce_ops->eo_init_state)(ce, ps);
}

static __inline int
_citrus_stdenc_mbtocs(struct _citrus_stdenc * __restrict ce,
		      _citrus_csid_t * __restrict csid,
		      _citrus_index_t * __restrict idx,
		      const char ** __restrict s, size_t n,
		      void * __restrict ps, size_t * __restrict nresult)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_mbtocs);
	return (*ce->ce_ops->eo_mbtocs)(ce, csid, idx, s, n, ps, nresult);
}

static __inline int
_citrus_stdenc_cstomb(struct _citrus_stdenc * __restrict ce,
		      char * __restrict s, size_t n,
		      _citrus_csid_t csid, _citrus_index_t idx,
		      void * __restrict ps, size_t * __restrict nresult)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_cstomb);
	return (*ce->ce_ops->eo_cstomb)(ce, s, n, csid, idx, ps, nresult);
}

static __inline int
_citrus_stdenc_mbtowc(struct _citrus_stdenc * __restrict ce,
		      _citrus_wc_t * __restrict wc,
		      const char ** __restrict s, size_t n,
		      void * __restrict ps, size_t * __restrict nresult)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_mbtocs);
	return (*ce->ce_ops->eo_mbtowc)(ce, wc, s, n, ps, nresult);
}

static __inline int
_citrus_stdenc_wctomb(struct _citrus_stdenc * __restrict ce,
		      char * __restrict s, size_t n, _citrus_wc_t wc,
		      void * __restrict ps, size_t * __restrict nresult)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_cstomb);
	return (*ce->ce_ops->eo_wctomb)(ce, s, n, wc, ps, nresult);
}

static __inline int
_citrus_stdenc_put_state_reset(struct _citrus_stdenc * __restrict ce,
			       char * __restrict s, size_t n,
			       void * __restrict ps,
			       size_t * __restrict nresult)
{
	_DIAGASSERT(ce && ce->ce_ops && ce->ce_ops->eo_put_state_reset);
	return (*ce->ce_ops->eo_put_state_reset)(ce, s, n, ps, nresult);
}

static __inline size_t
_citrus_stdenc_get_state_size(struct _citrus_stdenc *ce)
{
	_DIAGASSERT(ce && ce->ce_traits);
	return ce->ce_traits->et_state_size;
}

static __inline size_t
_citrus_stdenc_get_mb_cur_max(struct _citrus_stdenc *ce)
{
	_DIAGASSERT(ce && ce->ce_traits);
	return ce->ce_traits->et_mb_cur_max;
}

#endif
