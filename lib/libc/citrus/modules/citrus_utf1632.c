/*	$NetBSD: src/lib/libc/citrus/modules/citrus_utf1632.c,v 1.3 2003/06/27 12:55:13 yamt Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/modules/citrus_utf1632.c,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

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
#include <sys/endian.h>
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
#include "citrus_module.h"
#include "citrus_stdenc.h"
#include "citrus_bcs.h"

#include "citrus_utf1632.h"


/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

typedef struct {
	u_int8_t		ch[4];
	int			chlen;
	int			current_endian;
} _UTF1632State;

typedef struct {
	int		preffered_endian;
	unsigned int	cur_max;
#define _ENDIAN_UNKNOWN	0
#define _ENDIAN_BIG	1
#define _ENDIAN_LITTLE	2
	u_int32_t	mode;
#define _MODE_UTF32		0x00000001U
#define _MODE_FORCE_ENDIAN	0x00000002U
} _UTF1632EncodingInfo;

#define _FUNCNAME(m)			_citrus_UTF1632_##m
#define _ENCODING_INFO			_UTF1632EncodingInfo
#define _ENCODING_STATE			_UTF1632State
#define _ENCODING_MB_CUR_MAX(_ei_)	((_ei_)->cur_max)
#define _ENCODING_IS_STATE_DEPENDENT	0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0


static __inline void
/*ARGSUSED*/
_citrus_UTF1632_init_state(_UTF1632EncodingInfo *ei, _UTF1632State *s)
{
	memset(s, 0, sizeof(*s));
}

static int
_citrus_UTF1632_mbrtowc_priv(_UTF1632EncodingInfo *ei, wchar_t *pwc,
			     const char **s, size_t n, _UTF1632State *psenc,
			     size_t *nresult)
{
	int chlenbak, endian, needlen;
	wchar_t wc;
	size_t result;
	const char *s0;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(psenc != NULL);

	s0 = *s;

	if (s0 == NULL) {
		_citrus_UTF1632_init_state(ei, psenc);
		*nresult = 0; /* state independent */
		return (0);
	}

	result = 0;
	chlenbak = psenc->chlen;

refetch:
	if ((ei->mode & _MODE_UTF32) != 0 || chlenbak>=2)
		needlen = 4;
	else
		needlen = 2;

	while (chlenbak < needlen) {
		if (n==0)
			goto restart;
		psenc->ch[chlenbak++] = *s0++;
		n--;
		result++;
	}

	/* judge endian marker */
	if ((ei->mode & _MODE_UTF32) == 0) {
		/* UTF16 */
		if (psenc->ch[0]==0xFE && psenc->ch[1]==0xFF) {
			psenc->current_endian = _ENDIAN_BIG;
			chlenbak = 0;
			goto refetch;
		} else if (psenc->ch[0]==0xFF && psenc->ch[1]==0xFE) {
			psenc->current_endian = _ENDIAN_LITTLE;
			chlenbak = 0;
			goto refetch;
		}
	} else {
		/* UTF32 */
		if (psenc->ch[0]==0x00 && psenc->ch[1]==0x00 &&
		    psenc->ch[2]==0xFE && psenc->ch[3]==0xFF) {
			psenc->current_endian = _ENDIAN_BIG;
			chlenbak = 0;
			goto refetch;
		} else if (psenc->ch[0]==0xFF && psenc->ch[1]==0xFE &&
			   psenc->ch[2]==0x00 && psenc->ch[3]==0x00) {
			psenc->current_endian = _ENDIAN_LITTLE;
			chlenbak = 0;
			goto refetch;
		}
	}
	if ((ei->mode & _MODE_FORCE_ENDIAN) != 0 ||
	    psenc->current_endian == _ENDIAN_UNKNOWN)
		endian = ei->preffered_endian;
	else
		endian = psenc->current_endian;

	/* get wc */
	if ((ei->mode & _MODE_UTF32) == 0) {
		/* UTF16 */
		if (needlen==2) {
			switch (endian) {
			case _ENDIAN_LITTLE:
				wc = (psenc->ch[0] |
				      ((wchar_t)psenc->ch[1] << 8));
				break;
			case _ENDIAN_BIG:
				wc = (psenc->ch[1] |
				      ((wchar_t)psenc->ch[0] << 8));
				break;
			}
			if (wc >= 0xD800 && wc <= 0xDBFF) {
				/* surrogate high */
				needlen=4;
				goto refetch;
			}
		} else {
			/* surrogate low */
			wc -= 0xD800; /* wc : surrogate high (see above) */
			wc <<= 10;
			switch (endian) {
			case _ENDIAN_LITTLE:
				if (psenc->ch[2]<0xDC || psenc->ch[2]>0xDF)
					goto ilseq;
				wc |= psenc->ch[2];
				wc |= (wchar_t)(psenc->ch[3] & 3) << 8;
				break;
			case _ENDIAN_BIG:
				if (psenc->ch[3]<0xDC || psenc->ch[3]>0xDF)
					goto ilseq;
				wc |= psenc->ch[3];
				wc |= (wchar_t)(psenc->ch[2] & 3) << 8;
				break;
			}
			wc += 0x10000;
		}
	} else {
		/* UTF32 */
		switch (endian) {
		case _ENDIAN_LITTLE:
			wc = (psenc->ch[0] |
			      ((wchar_t)psenc->ch[1] << 8) |
			      ((wchar_t)psenc->ch[2] << 16) |
			      ((wchar_t)psenc->ch[3] << 24));
			break;
		case _ENDIAN_BIG:
			wc = (psenc->ch[3] |
			      ((wchar_t)psenc->ch[2] << 8) |
			      ((wchar_t)psenc->ch[1] << 16) |
			      ((wchar_t)psenc->ch[0] << 24));
			break;
		}
	}


	*pwc = wc;
	psenc->chlen = 0;
	*nresult = result;
	*s = s0;

	return (0);

ilseq:
	*nresult = (size_t)-1;
	psenc->chlen = 0;
	return (EILSEQ);

restart:
	*nresult = (size_t)-2;
	psenc->chlen = chlenbak;
	*s = s0;
	return (0);
}

static int
_citrus_UTF1632_wcrtomb_priv(_UTF1632EncodingInfo *ei, char *s, size_t n,
			     wchar_t wc, _UTF1632State *psenc,
			     size_t *nresult)
{
	int ret;
	wchar_t wc2;

	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(s != NULL);

	wc2 = 0;
	if ((ei->mode & _MODE_UTF32)==0) {
		/* UTF16 */
		if (wc>0xFFFF) {
			/* surrogate */
			if (wc>0x10FFFF) {
				ret = EILSEQ;
				goto err;
			}
			if (n < 4) {
				ret = E2BIG;
				goto err;
			}
			wc -= 0x10000;
			wc2 = (wc & 0x3FF) | 0xDC00;
			wc = (wc>>10) | 0xD800;
			*nresult = (size_t)4;
		} else {
			if (n < 2) {
				ret = E2BIG;
				goto err;
			}
			*nresult = (size_t)2;
		}

surrogate:
		switch (ei->preffered_endian) {
		case _ENDIAN_BIG:
			s[1] = wc;
			s[0] = (wc >>= 8);
			break;
		case _ENDIAN_LITTLE:
			s[0] = wc;
			s[1] = (wc >>= 8);
			break;
		}
		if (wc2!=0) {
			wc = wc2;
			wc2 = 0;
			s += 2;
			goto surrogate;
		}
	} else {
		/* UTF32 */
		if (n < 4) {
			ret = E2BIG;
			goto err;
		}
		switch (ei->preffered_endian) {
		case _ENDIAN_BIG:
			s[3] = wc;
			s[2] = (wc >>= 8);
			s[1] = (wc >>= 8);
			s[0] = (wc >>= 8);
			break;
		case _ENDIAN_LITTLE:
			s[0] = wc;
			s[1] = (wc >>= 8);
			s[2] = (wc >>= 8);
			s[3] = (wc >>= 8);
			break;
		}
		*nresult = (size_t)4;
	}

	return 0;

err:
	*nresult = (size_t)-1;
	return ret;
}

static void
parse_variable(_UTF1632EncodingInfo * __restrict ei,
	       const void * __restrict var, size_t lenvar)
{
#define MATCH(x, act)						\
do {								\
	if (lenvar >= (sizeof(#x)-1) &&				\
	    _bcs_strncasecmp(p, #x, sizeof(#x)-1) == 0) {	\
		act;						\
		lenvar -= sizeof(#x)-1;				\
		p += sizeof(#x)-1;				\
	}							\
} while (/*CONSTCOND*/0)
	const char *p;
	p = var;
	while (lenvar>0) {
		switch (*p) {
		case 'B':
		case 'b':
			MATCH(big, ei->preffered_endian = _ENDIAN_BIG);
			break;
		case 'L':
		case 'l':
			MATCH(little, ei->preffered_endian = _ENDIAN_LITTLE);
			break;
		case 'F':
		case 'f':
			MATCH(force, ei->mode |= _MODE_FORCE_ENDIAN);
			break;
		case 'U':
		case 'u':
			MATCH(utf32, ei->mode |= _MODE_UTF32);
			break;
		}
		p++;
		lenvar--;
	}
}

static int
/*ARGSUSED*/
_citrus_UTF1632_encoding_module_init(_UTF1632EncodingInfo * __restrict ei,
				     const void * __restrict var,
				     size_t lenvar)
{
	_DIAGASSERT(ei != NULL);

	memset((void *)ei, 0, sizeof(*ei));

	parse_variable(ei, var, lenvar);

	if ((ei->mode&_MODE_UTF32)==0)
		ei->cur_max = 6; /* endian + surrogate */
	else
		ei->cur_max = 8; /* endian + normal */

	if (ei->preffered_endian == _ENDIAN_UNKNOWN) {
#if BYTE_ORDER == BIG_ENDIAN
		ei->preffered_endian = _ENDIAN_BIG;
#else
		ei->preffered_endian = _ENDIAN_LITTLE;
#endif
	}

	return (0);
}

static void
/*ARGSUSED*/
_citrus_UTF1632_encoding_module_uninit(_UTF1632EncodingInfo *ei)
{
}

static __inline int
/*ARGSUSED*/
_citrus_UTF1632_stdenc_wctocs(_UTF1632EncodingInfo * __restrict ei,
			      _csid_t * __restrict csid,
			      _index_t * __restrict idx,
			      _wc_t wc)
{

	_DIAGASSERT(csid != NULL && idx != NULL);

	*csid = 0;
	*idx = (_index_t)wc;

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_UTF1632_stdenc_cstowc(_UTF1632EncodingInfo * __restrict ei,
			      _wc_t * __restrict wc,
			      _csid_t csid, _index_t idx)
{

	_DIAGASSERT(wc != NULL);

	if (csid != 0)
		return (EILSEQ);

	*wc = (_wc_t)idx;

	return (0);
}


/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(UTF1632);
_CITRUS_STDENC_DEF_OPS(UTF1632);

#include "citrus_stdenc_template.h"
