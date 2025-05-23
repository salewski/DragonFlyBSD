/*
 * Copyright (c) 2000 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_l1lib.c - general useful L1 procedures
 *	------------------------------------------
 *
 *	$Id: i4b_l1lib.c,v 1.3 2000/05/29 15:41:41 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_l1lib.c,v 1.3.2.1 2001/08/10 14:08:36 obrien Exp $
 * $DragonFly: src/sys/net/i4b/layer1/i4b_l1lib.c,v 1.5 2003/08/07 21:54:31 dillon Exp $
 *
 *      last edit-date: [Mon May 29 15:24:21 2000]
 *
 *---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>


#include <net/i4b/include/machine/i4b_ioctl.h>
#include <net/i4b/include/machine/i4b_trace.h>

#include "i4b_l1.h"

#define TEL_IDLE_MIN (BCH_MAX_DATALEN/2)

/*---------------------------------------------------------------------------*
 *	telephony silence detection
 *---------------------------------------------------------------------------*/
int
i4b_l1_bchan_tel_silence(unsigned char *data, int len)
{
	int i = 0;
	int j = 0;

	/* count idle bytes */
	
	for(;i < len; i++)
	{
		if((*data >= 0xaa) && (*data <= 0xac))
			j++;
		data++;
	}

#ifdef NOTDEF
	printf("i4b_l1_bchan_tel_silence: got %d silence bytes in frame\n", j);
#endif
	
	if(j < (TEL_IDLE_MIN))
		return(0);
	else
		return(1);

}
