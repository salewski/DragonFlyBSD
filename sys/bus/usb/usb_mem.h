/*
 * $NetBSD: usb_mem.h,v 1.18 2002/05/28 17:45:17 augustss Exp $
 * $FreeBSD: src/sys/dev/usb/usb_mem.h,v 1.20 2003/09/01 01:07:24 jmg Exp $
 * $DragonFly: src/sys/bus/usb/usb_mem.h,v 1.4 2004/02/11 15:17:26 joerg Exp $
 */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

typedef struct usb_dma_block {
	bus_dma_tag_t tag;
	bus_dmamap_t map;
#if defined(__FreeBSD__) || defined(__DragonFly__)
	void *kaddr;
#else
        caddr_t kaddr;
#endif
        bus_dma_segment_t segs[1];
        int nsegs;
        size_t size;
        size_t align;
	int fullblock;
	LIST_ENTRY(usb_dma_block) next;
} usb_dma_block_t;

#if defined(__FreeBSD__)  || defined(__DragonFly__)
#define DMAADDR(dma, o) ((dma)->block->segs[0].ds_addr + (dma)->offs + (o))
#else
#define DMAADDR(dma, o) (((char *)(dma)->block->map->dm_segs[0].ds_addr) + (dma)->offs + (o))
#endif
#define KERNADDR(dma, o) \
	((void *)((char *)((dma)->block->kaddr) + (dma)->offs + (o)))

usbd_status	usb_allocmem(usbd_bus_handle,size_t,size_t, usb_dma_t *);
void		usb_freemem(usbd_bus_handle, usb_dma_t *);
