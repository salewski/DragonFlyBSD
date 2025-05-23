/*	$NetBSD: src/lib/libc/citrus/modules/citrus_mskanji.c,v 1.9 2005/02/10 19:03:51 tnozaki Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/modules/citrus_mskanji.c,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

/*-
 * Copyright (c)2002 Citrus Project,
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
 *    ja_JP.SJIS locale table for BSD4.4/rune
 *    version 1.0
 *    (C) Sin'ichiro MIYATANI / Phase One, Inc
 *    May 12, 1995
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Phase One, Inc.
 * 4. The name of Phase One, Inc. may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE  
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL  
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS  
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT  
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY  
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.  
 */  


#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_stdenc.h"
#include "citrus_mskanji.h"


/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

typedef struct _MSKanjiState {
	char ch[2];
	int chlen;
} _MSKanjiState;

typedef struct {
	int dummy;
} _MSKanjiEncodingInfo;

typedef struct {
	_MSKanjiEncodingInfo	ei;
	struct {
		/* for future multi-locale facility */
		_MSKanjiState	s_mblen;
		_MSKanjiState	s_mbrlen;
		_MSKanjiState	s_mbrtowc;
		_MSKanjiState	s_mbtowc;
		_MSKanjiState	s_mbsrtowcs;
		_MSKanjiState	s_wcrtomb;
		_MSKanjiState	s_wcsrtombs;
		_MSKanjiState	s_wctomb;
	} states;
} _MSKanjiCTypeInfo;

#define _CEI_TO_EI(_cei_)		(&(_cei_)->ei)
#define _CEI_TO_STATE(_cei_, _func_)	(_cei_)->states.s_##_func_

#define _FUNCNAME(m)			_citrus_MSKanji_##m
#define _ENCODING_INFO			_MSKanjiEncodingInfo
#define _CTYPE_INFO			_MSKanjiCTypeInfo
#define _ENCODING_STATE			_MSKanjiState
#define _ENCODING_MB_CUR_MAX(_ei_)	2
#define _ENCODING_IS_STATE_DEPENDENT	0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0


static int
_mskanji1(int c)
{

	if ((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xef))
		return 1;
	else
		return 0;
}

static int
_mskanji2(int c)
{

	if ((c >= 0x40 && c <= 0x7e) || (c >= 0x80 && c <= 0xfc))
		return 1;
	else
		return 0;
}

static __inline void
/*ARGSUSED*/
_citrus_MSKanji_init_state(_MSKanjiEncodingInfo * __restrict ei,
			   _MSKanjiState * __restrict s)
{
	memset(s, 0, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_MSKanji_pack_state(_MSKanjiEncodingInfo * __restrict ei,
			   void * __restrict pspriv,
			   const _MSKanjiState * __restrict s)
{
	memcpy(pspriv, (const void *)s, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_MSKanji_unpack_state(_MSKanjiEncodingInfo * __restrict ei,
			     _MSKanjiState * __restrict s,
			     const void * __restrict pspriv)
{
	memcpy((void *)s, pspriv, sizeof(*s));
}

static int
/*ARGSUSED*/
_citrus_MSKanji_mbrtowc_priv(_MSKanjiEncodingInfo * __restrict ei,
			     wchar_t * __restrict pwc,
			     const char ** __restrict s, size_t n,
			     _MSKanjiState * __restrict psenc,
			     size_t * __restrict nresult)
{
	wchar_t wchar;
	int len;
	int chlenbak;
	const char *s0;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(psenc != NULL);

	s0 = *s;

	if (s0 == NULL) {
		_citrus_MSKanji_init_state(ei, psenc);
		*nresult = 0; /* state independent */
		return (0);
	}

	chlenbak = psenc->chlen;

	/* make sure we have the first byte in the buffer */
	switch (psenc->chlen) {
	case 0:
		if (n < 1)
			goto restart;
		psenc->ch[0] = *s0++;
		psenc->chlen = 1;
		n--;
		break;
	case 1:
		break;
	default:
		/* illegal state */
		goto encoding_error;
	}

	len = _mskanji1(psenc->ch[0] & 0xff) ? 2 : 1;
	while (psenc->chlen < len) {
		if (n < 1)
			goto restart;
		psenc->ch[psenc->chlen] = *s0++;
		psenc->chlen++;
		n--;
	}

	*s = s0;

	switch (len) {
	case 1:
		wchar = psenc->ch[0] & 0xff;
		break;
	case 2:
		if (!_mskanji2(psenc->ch[1] & 0xff))
			goto encoding_error;
		wchar = ((psenc->ch[0] & 0xff) << 8) | (psenc->ch[1] & 0xff);
		break;
	default:
		/* illegal state */
		goto encoding_error;
	}

	psenc->chlen = 0;

	if (pwc)
		*pwc = wchar;

	if (!wchar)
		*nresult = 0;
	else
		*nresult = len - chlenbak;

	return (0);

encoding_error:
	psenc->chlen = 0;
	*nresult = (size_t)-1;
	return (EILSEQ);

restart:
	*nresult = (size_t)-2;
	*s = s0;
	return (0);
}


static int
_citrus_MSKanji_wcrtomb_priv(_MSKanjiEncodingInfo * __restrict ei,
			     char * __restrict s, size_t n, wchar_t wc,
			     _MSKanjiState * __restrict psenc,
			     size_t * __restrict nresult)
{
	int ret;

	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(psenc != NULL);
	_DIAGASSERT(s != NULL);

	/* check invalid sequence */
	if (wc & ~0xffff) {
		ret = EILSEQ;
		goto err;
	}

	if (wc & 0xff00) {
		if (n < 2) {
			ret = E2BIG;
			goto err;
		}

		s[0] = (wc >> 8) & 0xff;
		s[1] = wc & 0xff;
		if (!_mskanji1(s[0] & 0xff) || !_mskanji2(s[1] & 0xff)) {
			ret = EILSEQ;
			goto err;
		}

		*nresult = 2;
		return 0;
	} else {
		if (n < 1) {
			ret = E2BIG;
			goto err;
		}

		s[0] = wc & 0xff;
		if (_mskanji1(s[0] & 0xff)) {
			ret = EILSEQ;
			goto err;
		}

		*nresult = 1;
		return 0;
	}

err:
	*nresult = (size_t)-1;
	return ret;
}


static __inline int
/*ARGSUSED*/
_citrus_MSKanji_stdenc_wctocs(_MSKanjiEncodingInfo * __restrict ei,
			      _csid_t * __restrict csid,
			      _index_t * __restrict idx, wchar_t wc)
{
	_index_t row, col;

	_DIAGASSERT(csid != NULL && idx != NULL);

	if ((_wc_t)wc < 0x80) {
		/* ISO-646 */
		*csid = 0;
		*idx = (_index_t)wc;
	} else if ((_wc_t)wc < 0x100) {
		/* KANA */
		*csid = 1;
		*idx = (_index_t)wc & 0x7F;
	} else if ((0x8140 <= (_wc_t)wc && (_wc_t)wc <= 0x9FFC) ||
		   (0xE040 <= (_wc_t)wc && (_wc_t)wc <= 0xFCFC)) {
		/* Kanji (containing Gaiji zone) */
		/*
		 * 94^2 zone (contains a part of Gaiji (0xED40 - 0xEEFC)):
		 * 0x8140 - 0x817E -> 0x2121 - 0x215F
		 * 0x8180 - 0x819E -> 0x2160 - 0x217E
		 * 0x819F - 0x81FC -> 0x2221 - 0x227E
		 *
		 * 0x8240 - 0x827E -> 0x2321 - 0x235F
		 *  ...
		 * 0x9F9F - 0x9FFc -> 0x5E21 - 0x5E7E
		 *
		 * 0xE040 - 0xE07E -> 0x5F21 - 0x5F5F
		 *  ...
		 * 0xEF9F - 0xEFFC -> 0x7E21 - 0x7E7E
		 *
		 * extended Gaiji zone:
		 * 0xF040 - 0xFCFC
		 */
		*csid = 2;
		row = ((_wc_t)wc >> 8) - 0x81;
		if (row >= 0x5F)
			row -= 0x40;
		row = row * 2 + 0x21;
		col = (wc & 0xFF) - 0x1F;
		if (col >= 0x61)
			col -= 1;
		if (col > 0x7E) {
			row += 1;
			col -= 0x5E;
		}
		*idx = ((_index_t)row << 8) | col;
	} else
		return EILSEQ;

	return 0;
}

static __inline int
/*ARGSUSED*/
_citrus_MSKanji_stdenc_cstowc(_MSKanjiEncodingInfo * __restrict ei,
			      wchar_t * __restrict wc,
			      _csid_t csid, _index_t idx)
{
	u_int32_t row, col;

	_DIAGASSERT(wc != NULL);

	switch (csid) {
	case 0:
		/* ISO-646 */
		if (idx >= 0x80)
			return EILSEQ;
		*wc = (wchar_t)idx;
		break;
	case 1:
		/* kana */
		if (idx >= 0x80)
			return EILSEQ;
		*wc = (wchar_t)idx + 0x80;
		break;
	case 2:
		/* kanji */
		row = (idx >> 8);
		col = idx & 0x7F;
		if (row<0x21 || row>0x97 || col<0x21 || col>0x7E)
			return EILSEQ;
		row -= 0x21; col -= 0x21;
		if ((row & 1)==0) {
			col += 0x40;
			if (col>=0x7F)
				col += 1;
		} else
			col += 0x9F;
		if (row<0x3E)
			row = row/2 + 0x81;
		else
			row = row/2 + 0xc1;
		*wc = ((wchar_t)row << 8) | col;
		break;
	default:
		return EILSEQ;
	}

	return 0;
}

static int
/*ARGSUSED*/
_citrus_MSKanji_encoding_module_init(_MSKanjiEncodingInfo *  __restrict ei,
				     const void * __restrict var,
				     size_t lenvar)
{
	_DIAGASSERT(ei != NULL);

	return (0);
}

static void
_citrus_MSKanji_encoding_module_uninit(_MSKanjiEncodingInfo *ei)
{
}

/* ----------------------------------------------------------------------
 * public interface for ctype
 */

_CITRUS_CTYPE_DECLS(MSKanji);
_CITRUS_CTYPE_DEF_OPS(MSKanji);

#include "citrus_ctype_template.h"

/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(MSKanji);
_CITRUS_STDENC_DEF_OPS(MSKanji);

#include "citrus_stdenc_template.h"
