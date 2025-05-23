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

/* $Id: base64.h,v 1.2.2.1 2004/03/09 06:12:27 marka Exp $ */

#ifndef ISCCC_BASE64_H
#define ISCCC_BASE64_H 1

#include <isc/lang.h>
#include <isccc/types.h>

ISC_LANG_BEGINDECLS

/***
 *** Functions
 ***/

isc_result_t
isccc_base64_encode(isccc_region_t *source, int wordlength,
		  const char *wordbreak, isccc_region_t *target);
/*
 * Convert data into base64 encoded text.
 *
 * Notes:
 *	The base64 encoded text in 'target' will be divided into
 *	words of at most 'wordlength' characters, separated by
 * 	the 'wordbreak' string.  No parentheses will surround
 *	the text.
 *
 * Requires:
 *	'source' is a region containing binary data.
 *	'target' is a text region containing available space.
 *	'wordbreak' points to a null-terminated string of
 *		zero or more whitespace characters.
 */

isc_result_t
isccc_base64_decode(const char *cstr, isccc_region_t *target);
/*
 * Decode a null-terminated base64 string.
 *
 * Requires:
 *	'cstr' is non-null.
 *	'target' is a valid region.
 *
 * Returns:
 *	ISC_R_SUCCESS	-- the entire decoded representation of 'cstring'
 *			   fit in 'target'.
 *	ISC_R_BADBASE64 -- 'cstr' is not a valid base64 encoding.
 *	ISC_R_NOSPACE	-- 'target' is not big enough.
 */

ISC_LANG_ENDDECLS

#endif /* ISCCC_BASE64_H */
