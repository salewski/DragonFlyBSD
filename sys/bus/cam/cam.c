/*
 * Generic utility routines for the Common Access Method layer.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cam/cam.c,v 1.3 1999/08/28 00:40:38 peter Exp $
 * $DragonFly: src/sys/bus/cam/cam.c,v 1.6 2004/02/16 19:43:27 dillon Exp $
 */
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#else
#include <stdlib.h>
#include <stdio.h>
#endif

#include "cam.h"
#include "cam_ccb.h"
#include "scsi/scsi_all.h"
#include <sys/sbuf.h>

#ifdef _KERNEL
#include <sys/libkern.h>
#include "cam_xpt.h"
#endif

#ifdef _KERNEL

SYSCTL_NODE(_kern, OID_AUTO, cam, CTLFLAG_RD, 0, "CAM Subsystem");

#endif

void
cam_strvis(u_int8_t *dst, const u_int8_t *src, int srclen, int dstlen)
{

	/* Trim leading/trailing spaces, nulls. */
	while (srclen > 0 && src[0] == ' ')
		src++, srclen--;
	while (srclen > 0
	    && (src[srclen-1] == ' ' || src[srclen-1] == '\0'))
		srclen--;

	while (srclen > 0 && dstlen > 1) {
		u_int8_t *cur_pos = dst;

		if (*src < 0x20 || *src >= 0x80) {
			/* SCSI-II Specifies that these should never occur. */
			/* non-printable character */
			if (dstlen > 4) {
				*cur_pos++ = '\\';
				*cur_pos++ = ((*src & 0300) >> 6) + '0';
				*cur_pos++ = ((*src & 0070) >> 3) + '0';
				*cur_pos++ = ((*src & 0007) >> 0) + '0';
			} else {
				*cur_pos++ = '?';
			}
		} else {
			/* normal character */
			*cur_pos++ = *src;
		}
		src++;
		srclen--;
		dstlen -= cur_pos - dst;
		dst = cur_pos;
	}
	*dst = '\0';
}

/*
 * Compare string with pattern, returning 0 on match.
 * Short pattern matches trailing blanks in name,
 * wildcard '*' in pattern matches rest of name,
 * wildcard '?' matches a single non-space character.
 */
int
cam_strmatch(const u_int8_t *str, const u_int8_t *pattern, int str_len)
{

	while (*pattern != '\0'&& str_len > 0) {  

		if (*pattern == '*') {
			return (0);
		}
		if ((*pattern != *str)
		 && (*pattern != '?' || *str == ' ')) {
			return (1);
		}
		pattern++;
		str++;
		str_len--;
	}
	while (str_len > 0 && *str++ == ' ')
		str_len--;

	return (str_len);
}

caddr_t
cam_quirkmatch(caddr_t target, caddr_t quirk_table, int num_entries,
	       int entry_size, cam_quirkmatch_t *comp_func)
{
	for (; num_entries > 0; num_entries--, quirk_table += entry_size) {
		if ((*comp_func)(target, quirk_table) == 0)
			return (quirk_table);
	}
	return (NULL);
}

/*
 * Common calculate geometry fuction
 *
 * Caller should set ccg->volume_size and block_size.
 * The extended parameter should be zero if extended translation
 * should not be used.
 */
void
cam_calc_geometry(struct ccb_calc_geometry *ccg, int extended)
{
	uint32_t size_mb, secs_per_cylinder;

	size_mb = ccg->volume_size / ((1024L * 1024L) / ccg->block_size);
	if (size_mb > 1024 && extended) {
		ccg->heads = 255;
		ccg->secs_per_track = 63;
	} else {
		ccg->heads = 64;
		ccg->secs_per_track = 32;
	}
	secs_per_cylinder = ccg->heads * ccg->secs_per_track;
	ccg->cylinders = ccg->volume_size / secs_per_cylinder;
	ccg->ccb_h.status = CAM_REQ_CMP;
}

