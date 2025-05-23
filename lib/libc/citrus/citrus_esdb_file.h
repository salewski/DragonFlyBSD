/*	$NetBSD: src/lib/libc/citrus/citrus_esdb_file.h,v 1.1 2003/06/25 09:51:32 tshiozak Exp $	*/
/*	$DragonFly: src/lib/libc/citrus/citrus_esdb_file.h,v 1.1 2005/03/11 23:33:53 joerg Exp $ */


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

#ifndef _CITRUS_ESDB_FILE_H_
#define _CITRUS_ESDB_FILE_H_

#define _CITRUS_ESDB_MAGIC	"ESDB\0\0\0\0"

#define _CITRUS_ESDB_SYM_VERSION	"version"
#define _CITRUS_ESDB_SYM_ENCODING	"encoding"
#define _CITRUS_ESDB_SYM_VARIABLE	"variable"
#define _CITRUS_ESDB_SYM_NUM_CHARSETS	"num_charsets"
#define _CITRUS_ESDB_SYM_INVALID	"invalid"
#define _CITRUS_ESDB_SYM_CSNAME_PREFIX	"csname_"
#define _CITRUS_ESDB_SYM_CSID_PREFIX	"csid_"

#define _CITRUS_ESDB_VERSION 0x00000001

#endif
