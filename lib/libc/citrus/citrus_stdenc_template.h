/*	$NetBSD: src/lib/libc/citrus/citrus_stdenc_template.h,v 1.2 2003/06/26 12:09:57 tshiozak Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/citrus_stdenc_template.h,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

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
 */

/*
 * CAUTION: THIS IS NOT STANDALONE FILE
 *
 * function templates of iconv standard encoding handler for each encodings.
 *
 */

/*
 * macros
 */

#undef _TO_EI
#undef _CE_TO_EI
#undef _TO_STATE
#define _TO_EI(_cl_)	((_ENCODING_INFO*)(_cl_))
#define _CE_TO_EI(_ce_)	(_TO_EI((_ce_)->ce_closure))
#define _TO_STATE(_ps_)	((_ENCODING_STATE*)(_ps_))

/* ----------------------------------------------------------------------
 * templates for public functions
 */

int
_FUNCNAME(stdenc_getops)(struct _citrus_stdenc_ops *ops, size_t lenops,
			 uint32_t expected_version)
{
	if (expected_version<_CITRUS_STDENC_ABI_VERSION || lenops<sizeof(*ops))
		return (EINVAL);

	memcpy(ops, &_FUNCNAME(stdenc_ops), sizeof(_FUNCNAME(stdenc_ops)));

	return (0);
}

static int
_FUNCNAME(stdenc_init)(struct _citrus_stdenc * __restrict ce,
		       const void * __restrict var, size_t lenvar,
		       struct _citrus_stdenc_traits * __restrict et)
{
	int ret;
	_ENCODING_INFO *ei;

	ei = NULL;
	if (sizeof(_ENCODING_INFO) > 0) {
		ei = calloc(1, sizeof(_ENCODING_INFO));
		if (ei == NULL) {
			return errno;
		}
	}

	ret = _FUNCNAME(encoding_module_init)(ei, var, lenvar);
	if (ret) {
		free((void *)ei);
		return ret;
	}

	ce->ce_closure = ei;
	et->et_state_size = sizeof(_ENCODING_STATE);
	et->et_mb_cur_max = _ENCODING_MB_CUR_MAX(_CE_TO_EI(ce));

	return 0;
}

static void
_FUNCNAME(stdenc_uninit)(struct _citrus_stdenc * __restrict ce)
{
	if (ce) {
		_FUNCNAME(encoding_module_uninit)(_CE_TO_EI(ce));
		free(ce->ce_closure);
	}
}

static int
_FUNCNAME(stdenc_init_state)(struct _citrus_stdenc * __restrict ce,
			     void * __restrict ps)
{
	_FUNCNAME(init_state)(_CE_TO_EI(ce), _TO_STATE(ps));

	return 0;
}

static int
_FUNCNAME(stdenc_mbtocs)(struct _citrus_stdenc * __restrict ce,
			 _citrus_csid_t * __restrict csid,
			 _citrus_index_t * __restrict idx,
			 const char ** __restrict s, size_t n,
			 void * __restrict ps, size_t * __restrict nresult)
{
	int ret;
	wchar_t wc;

	_DIAGASSERT(nresult != NULL);

	ret = _FUNCNAME(mbrtowc_priv)(_CE_TO_EI(ce), &wc, s, n,
				      _TO_STATE(ps), nresult);

	if (!ret && *nresult != (size_t)-2)
		_FUNCNAME(stdenc_wctocs)(_CE_TO_EI(ce), csid, idx, wc);

	return ret;
}

static int
_FUNCNAME(stdenc_cstomb)(struct _citrus_stdenc * __restrict ce,
			 char * __restrict s, size_t n,
			 _citrus_csid_t csid, _citrus_index_t idx,
			 void * __restrict ps, size_t * __restrict nresult)
{
	int ret;
	wchar_t wc;

	_DIAGASSERT(nresult != NULL);

	wc = 0;

	if (csid != _CITRUS_CSID_INVALID) {
		ret = _FUNCNAME(stdenc_cstowc)(_CE_TO_EI(ce), &wc, csid, idx);
		if (ret)
			return ret;
	}

	return _FUNCNAME(wcrtomb_priv)(_CE_TO_EI(ce), s, n, wc, _TO_STATE(ps),
				       nresult);
}

static int
_FUNCNAME(stdenc_mbtowc)(struct _citrus_stdenc * __restrict ce,
			 _citrus_wc_t * __restrict wc,
			 const char ** __restrict s, size_t n,
			 void * __restrict ps, size_t * __restrict nresult)
{
	return _FUNCNAME(mbrtowc_priv)(_CE_TO_EI(ce), wc, s, n,
				       _TO_STATE(ps), nresult);
}

static int
_FUNCNAME(stdenc_wctomb)(struct _citrus_stdenc * __restrict ce,
			  char * __restrict s, size_t n, _citrus_wc_t wc,
			  void * __restrict ps, size_t * __restrict nresult)
{
	return _FUNCNAME(wcrtomb_priv)(_CE_TO_EI(ce), s, n, wc, _TO_STATE(ps),
				       nresult);
}

static int
_FUNCNAME(stdenc_put_state_reset)(struct _citrus_stdenc * __restrict ce,
				  char * __restrict s, size_t n,
				  void * __restrict ps,
				  size_t * __restrict nresult)
{
#if _ENCODING_IS_STATE_DEPENDENT
	return _FUNCNAME(put_state_reset)(_CE_TO_EI(ce), s, n, _TO_STATE(ps),
					  nresult);
#else
	*nresult = 0;
	return 0;
#endif
}
