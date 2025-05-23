/*	$NetBSD: src/lib/libc/citrus/citrus_stdenc_local.h,v 1.2 2003/06/26 12:09:57 tshiozak Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/citrus_stdenc_local.h,v 1.1 2005/03/11 23:33:53 joerg Exp $ */

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

#ifndef _CITRUS_STDENC_LOCAL_H_
#define _CITRUS_STDENC_LOCAL_H_

#define _CITRUS_STDENC_GETOPS_FUNC_BASE(n)			\
int n(struct _citrus_stdenc_ops *, size_t, uint32_t)
#define _CITRUS_STDENC_GETOPS_FUNC(_e_)					\
_CITRUS_STDENC_GETOPS_FUNC_BASE(_citrus_##_e_##_stdenc_getops)
typedef _CITRUS_STDENC_GETOPS_FUNC_BASE((*_citrus_stdenc_getops_t));


#define _CITRUS_STDENC_DECLS(_e_)					\
static int	_citrus_##_e_##_stdenc_init				\
	(struct _citrus_stdenc * __restrict, const void * __restrict,	\
	 size_t, struct _citrus_stdenc_traits * __restrict);		\
static void	_citrus_##_e_##_stdenc_uninit(struct _citrus_stdenc *);	\
static int	_citrus_##_e_##_stdenc_init_state			\
	(struct _citrus_stdenc * __restrict, void * __restrict);	\
static int	_citrus_##_e_##_stdenc_mbtocs				\
	(struct _citrus_stdenc * __restrict,				\
	 _citrus_csid_t * __restrict, _citrus_index_t * __restrict,	\
	 const char ** __restrict, size_t,				\
	 void * __restrict, size_t * __restrict);			\
static int	_citrus_##_e_##_stdenc_cstomb				\
	(struct _citrus_stdenc * __restrict, char * __restrict,		\
	 size_t, _citrus_csid_t, _citrus_index_t,			\
	 void * __restrict, size_t * __restrict);			\
static int	_citrus_##_e_##_stdenc_mbtowc				\
	(struct _citrus_stdenc * __restrict,				\
	 _citrus_wc_t * __restrict,					\
	 const char ** __restrict, size_t,				\
	 void * __restrict, size_t * __restrict);			\
static int	_citrus_##_e_##_stdenc_wctomb				\
	(struct _citrus_stdenc * __restrict, char * __restrict, size_t,	\
	 _citrus_wc_t, void * __restrict, size_t * __restrict);		\
static int	_citrus_##_e_##_stdenc_put_state_reset			\
	(struct _citrus_stdenc * __restrict, char * __restrict, size_t,	\
	 void * __restrict, size_t * __restrict)

#define _CITRUS_STDENC_DEF_OPS(_e_)					\
struct _citrus_stdenc_ops _citrus_##_e_##_stdenc_ops = {		\
	/* eo_abi_version */	_CITRUS_STDENC_ABI_VERSION,		\
	/* eo_init */		&_citrus_##_e_##_stdenc_init,		\
	/* eo_uninit */		&_citrus_##_e_##_stdenc_uninit,		\
	/* eo_init_state */	&_citrus_##_e_##_stdenc_init_state,	\
	/* eo_mbtocs */		&_citrus_##_e_##_stdenc_mbtocs,		\
	/* eo_cstomb */		&_citrus_##_e_##_stdenc_cstomb,		\
	/* eo_mbtowc */		&_citrus_##_e_##_stdenc_mbtowc,		\
	/* eo_wctomb */		&_citrus_##_e_##_stdenc_wctomb,		\
	/* eo_put_state_reset */&_citrus_##_e_##_stdenc_put_state_reset \
}

typedef int	(*_citrus_stdenc_init_t)
	(struct _citrus_stdenc * __reatrict, const void * __restrict , size_t,
	 struct _citrus_stdenc_traits * __restrict);
typedef void	(*_citrus_stdenc_uninit_t)(struct _citrus_stdenc * __restrict);
typedef int	(*_citrus_stdenc_init_state_t)
	(struct _citrus_stdenc * __restrict, void * __restrict);
typedef int	(*_citrus_stdenc_mbtocs_t)
	(struct _citrus_stdenc * __restrict,
	 _citrus_csid_t * __restrict, _citrus_index_t * __restrict,
	 const char ** __restrict, size_t,
	 void * __restrict, size_t * __restrict);
typedef int	(*_citrus_stdenc_cstomb_t)
	(struct _citrus_stdenc *__restrict, char * __restrict, size_t,
	 _citrus_csid_t, _citrus_index_t, void * __restrict,
	 size_t * __restrict);
typedef int	(*_citrus_stdenc_mbtowc_t)
	(struct _citrus_stdenc * __restrict,
	 _citrus_wc_t * __restrict,
	 const char ** __restrict, size_t,
	 void * __restrict, size_t * __restrict);
typedef int	(*_citrus_stdenc_wctomb_t)
	(struct _citrus_stdenc *__restrict, char * __restrict, size_t,
	 _citrus_wc_t, void * __restrict, size_t * __restrict);
typedef int	(*_citrus_stdenc_put_state_reset_t)
	(struct _citrus_stdenc *__restrict, char * __restrict, size_t,
	 void * __restrict, size_t * __restrict);

/*
 * ABI version change log
 *   0x00000001
 *     initial version
 */
#define _CITRUS_STDENC_ABI_VERSION	0x00000001
struct _citrus_stdenc_ops {
	uint32_t			eo_abi_version;
	/* version 0x00000001 */
	_citrus_stdenc_init_t		eo_init;
	_citrus_stdenc_uninit_t		eo_uninit;
	_citrus_stdenc_init_state_t	eo_init_state;
	_citrus_stdenc_mbtocs_t		eo_mbtocs;
	_citrus_stdenc_cstomb_t		eo_cstomb;
	_citrus_stdenc_mbtowc_t		eo_mbtowc;
	_citrus_stdenc_wctomb_t		eo_wctomb;
	_citrus_stdenc_put_state_reset_t eo_put_state_reset;
};

struct _citrus_stdenc_traits {
	/* version 0x00000001 */
	size_t				et_state_size;
	size_t				et_mb_cur_max;
};

struct _citrus_stdenc {
	/* public */
	/* version 0x00000001 */
	struct _citrus_stdenc_ops	*ce_ops;
	void				*ce_closure;
	/* private */
	_citrus_module_t		ce_module;
	struct _citrus_stdenc_traits	*ce_traits;
};

#define _CITRUS_DEFAULT_STDENC_NAME		"NONE"

#endif
