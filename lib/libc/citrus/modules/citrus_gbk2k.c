/* $NetBSD: src/lib/libc/citrus/modules/citrus_gbk2k.c,v 1.4 2003/06/26 12:09:57 tshiozak Exp $ */
/* $DragonFly: src/lib/libc/citrus/modules/citrus_gbk2k.c,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_bcs.h"
#include "citrus_module.h"
#include "citrus_ctype.h"
#include "citrus_stdenc.h"
#include "citrus_gbk2k.h"


/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

typedef struct _GBK2KState {
	char ch[4];
	int chlen;
} _GBK2KState;

typedef struct {
	int ei_mode;
} _GBK2KEncodingInfo;
#define _MODE_2BYTE	0x0001

typedef struct {
	_GBK2KEncodingInfo	ei;
	struct {
		/* for future multi-locale facility */
		_GBK2KState	s_mblen;
		_GBK2KState	s_mbrlen;
		_GBK2KState	s_mbrtowc;
		_GBK2KState	s_mbtowc;
		_GBK2KState	s_mbsrtowcs;
		_GBK2KState	s_wcrtomb;
		_GBK2KState	s_wcsrtombs;
		_GBK2KState	s_wctomb;
	} states;
} _GBK2KCTypeInfo;

#define _CEI_TO_EI(_cei_)		(&(_cei_)->ei)
#define _CEI_TO_STATE(_cei_, _func_)	(_cei_)->states.s_##_func_

#define _FUNCNAME(m)			_citrus_GBK2K_##m
#define _ENCODING_INFO			_GBK2KEncodingInfo
#define _CTYPE_INFO			_GBK2KCTypeInfo
#define _ENCODING_STATE			_GBK2KState
#define _ENCODING_MB_CUR_MAX(_ei_)	4
#define _ENCODING_IS_STATE_DEPENDENT	0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0

static __inline void
/*ARGSUSED*/
_citrus_GBK2K_init_state(_GBK2KEncodingInfo * __restrict ei,
			 _GBK2KState * __restrict s)
{
	memset(s, 0, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_GBK2K_pack_state(_GBK2KEncodingInfo * __restrict ei,
			 void * __restrict pspriv,
			 const _GBK2KState * __restrict s)
{
	memcpy(pspriv, (const void *)s, sizeof(*s));
}

static __inline void
/*ARGSUSED*/
_citrus_GBK2K_unpack_state(_GBK2KEncodingInfo * __restrict ei,
			   _GBK2KState * __restrict s,
			   const void * __restrict pspriv)
{
	memcpy((void *)s, pspriv, sizeof(*s));
}

static  __inline int
_mb_singlebyte(int c)
{
	c &= 0xff;
	return (c <= 0x7f);
}

static __inline int
_mb_leadbyte(int c)
{
	c &= 0xff;
	return (0x81 <= c && c <= 0xfe);
}

static __inline int
_mb_trailbyte(int c)
{
	c &= 0xff;
	return ((0x40 <= c && c <= 0x7e) || (0x80 <= c && c <= 0xfe));
}

static __inline int
_mb_surrogate(int c)
{
	c &= 0xff;
	return (0x30 <= c && c <= 0x39);
}

static __inline int
_mb_count(wchar_t v)
{
	u_int32_t c;

	c = (u_int32_t)v; /* XXX */
	if (!(c & 0xffffff00))
		return (1);
	if (!(c & 0xffff0000))
		return (2);
	return (4);
}

#define	_PSENC		(psenc->ch[psenc->chlen - 1])
#define	_PUSH_PSENC(c)	(psenc->ch[psenc->chlen++] = (c))

static int
_citrus_GBK2K_mbrtowc_priv(_GBK2KEncodingInfo * __restrict ei,
			   wchar_t * __restrict pwc,
			   const char ** __restrict s, size_t n,
			   _GBK2KState * __restrict psenc,
			   size_t * __restrict nresult)
{
	int chlenbak, len;
	const char *s0, *s1;
	wchar_t wc;

	_DIAGASSERT(ei != NULL);
	/* pwc may be NULL */
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(psenc != NULL);

	s0 = *s;

	if (s0 == NULL) {
		/* _citrus_GBK2K_init_state(ei, psenc); */
		psenc->chlen = 0;
		*nresult = 0;
		return (0);
	}

	chlenbak = psenc->chlen;

	switch (psenc->chlen) {
	case 3:
		if (!_mb_leadbyte (_PSENC))
			goto invalid;
	/* FALLTHROUGH */
	case 2:
		if (!_mb_surrogate(_PSENC) || _mb_trailbyte(_PSENC))
			goto invalid;
	/* FALLTHROUGH */
	case 1:
		if (!_mb_leadbyte (_PSENC))
			goto invalid;
	/* FALLTHOROUGH */
	case 0:
		break;
	default:
		goto invalid;
	}

	for (;;) {
		if (n-- < 1)
			goto restart;

		_PUSH_PSENC(*s0++);

		switch (psenc->chlen) {
		case 1:
			if (_mb_singlebyte(_PSENC))
				goto convert;
			if (_mb_leadbyte  (_PSENC))
				continue;
			goto ilseq;
		case 2:
			if (_mb_trailbyte (_PSENC))
				goto convert;
			if ((ei->ei_mode & _MODE_2BYTE) == 0 &&
			    _mb_surrogate (_PSENC))
				continue;
			goto ilseq;
		case 3:
			if (_mb_leadbyte  (_PSENC))
				continue;
			goto ilseq;
		case 4:
			if (_mb_surrogate (_PSENC))
				goto convert;
			goto ilseq;
		}
	}

convert:
	len = psenc->chlen;
	s1  = &psenc->ch[0];
	wc  = 0;
	while (len-- > 0)
		wc = (wc << 8) | (*s1++ & 0xff);

	if (pwc != NULL)
		*pwc = wc;
	*s = s0;
	*nresult = (wc == 0) ? 0 : psenc->chlen - chlenbak; 
	/* _citrus_GBK2K_init_state(ei, psenc); */
	psenc->chlen = 0;

	return (0);

restart:
	*s = s0;
	*nresult = (size_t)-2;

	return (0);

invalid:
	return (EINVAL);

ilseq:
	*nresult = (size_t)-1;
	return (EILSEQ);
}

static int
_citrus_GBK2K_wcrtomb_priv(_GBK2KEncodingInfo * __restrict ei,
			   char * __restrict s, size_t n, wchar_t wc,
			   _GBK2KState * __restrict psenc,
			   size_t * __restrict nresult)
{
	int len, ret;

	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(psenc != NULL);

	if (psenc->chlen != 0) {
		ret = EINVAL;
		goto err;
	}

	len = _mb_count(wc);
	if (n < len) {
		ret = E2BIG;
		goto err;
	}

	switch (len) {
	case 1:
		if (!_mb_singlebyte(_PUSH_PSENC(wc     ))) {
			ret = EILSEQ;
			goto err;
		}
		break;
	case 2:
		if (!_mb_leadbyte  (_PUSH_PSENC(wc >> 8)) ||
		    !_mb_trailbyte (_PUSH_PSENC(wc     ))) {
			ret = EILSEQ;
			goto err;
		}
		break;
	case 4:
		if ((ei->ei_mode & _MODE_2BYTE) != 0 ||
		    !_mb_leadbyte  (_PUSH_PSENC(wc >> 24)) ||
		    !_mb_surrogate (_PUSH_PSENC(wc >> 16)) ||
		    !_mb_leadbyte  (_PUSH_PSENC(wc >>  8)) ||
		    !_mb_surrogate (_PUSH_PSENC(wc      ))) {
			ret = EILSEQ;
			goto err;
		}
		break;
	}

	_DIAGASSERT(len == psenc->chlen);

	memcpy(s, psenc->ch, psenc->chlen);
	*nresult = psenc->chlen;
	/* _citrus_GBK2K_init_state(ei, psenc); */
	psenc->chlen = 0;

	return (0);

err:
	*nresult = (size_t)-1;
	return ret;
}

static __inline int
/*ARGSUSED*/
_citrus_GBK2K_stdenc_wctocs(_GBK2KEncodingInfo * __restrict ei,
			    _csid_t * __restrict csid,
			    _index_t * __restrict idx, wchar_t wc)
{
	u_int8_t ch, cl;

	_DIAGASSERT(csid != NULL && idx != NULL);

	if ((u_int32_t)wc<0x80) {
		/* ISO646 */
		*csid = 0;
		*idx = (_index_t)wc;
	} else if ((u_int32_t)wc>=0x10000) {
		/* GBKUCS : XXX */
		*csid = 3;
		*idx = (_index_t)wc;
	} else {
		ch = (u_int8_t)(wc >> 8);
		cl = (u_int8_t)wc;
		if (ch>=0xA1 && cl>=0xA1) {
			/* EUC G1 */
			*csid = 1;
			*idx = (_index_t)wc & 0x7F7FU;
		} else {
			/* extended area (0x8140-) */
			*csid = 2;
			*idx = (_index_t)wc;
		}
	}
		
	return 0;
}

static __inline int
/*ARGSUSED*/
_citrus_GBK2K_stdenc_cstowc(_GBK2KEncodingInfo * __restrict ei,
			    wchar_t * __restrict wc,
			    _csid_t csid, _index_t idx)
{

	_DIAGASSERT(wc != NULL);

	switch (csid) {
	case 0:
		/* ISO646 */
		*wc = (wchar_t)idx;
		break;
	case 1:
		/* EUC G1 */
		*wc = (wchar_t)idx | 0x8080U;
		break;
	case 2:
		/* extended area */
		*wc = (wchar_t)idx;
		break;
	case 3:
		/* GBKUCS : XXX */
		if ((ei->ei_mode & _MODE_2BYTE) != 0)
			return EINVAL;
		*wc = (wchar_t)idx;
		break;
	default:
		return EILSEQ;
	}

	return 0;
}

static int
/*ARGSUSED*/
_citrus_GBK2K_encoding_module_init(_GBK2KEncodingInfo * __restrict ei,
				   const void * __restrict var, size_t lenvar)
{
	const char *p;

	_DIAGASSERT(ei != NULL);

	p = var;
#define MATCH(x, act)                                           \
do {                                                            \
        if (lenvar >= (sizeof(#x)-1) &&                         \
            _bcs_strncasecmp(p, #x, sizeof(#x)-1) == 0) {       \
                act;                                            \
                lenvar -= sizeof(#x)-1;                         \
                p += sizeof(#x)-1;                              \
        }                                                       \
} while (/*CONSTCOND*/0)
	while (lenvar>0) {
		switch (_bcs_tolower(*p)) {
		case '2':
			MATCH("2byte", ei->ei_mode |= _MODE_2BYTE);
			break;
		}
		p++;
		lenvar--;
	}

	memset((void *)ei, 0, sizeof(*ei));
	return (0);
}

static void
/*ARGSUSED*/
_citrus_GBK2K_encoding_module_uninit(_GBK2KEncodingInfo *ei)
{
}


/* ----------------------------------------------------------------------
 * public interface for ctype
 */

_CITRUS_CTYPE_DECLS(GBK2K);
_CITRUS_CTYPE_DEF_OPS(GBK2K);

#include "citrus_ctype_template.h"

/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(GBK2K);
_CITRUS_STDENC_DEF_OPS(GBK2K);

#include "citrus_stdenc_template.h"
