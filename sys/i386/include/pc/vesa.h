/*-
 * Copyright (c) 1998 Michael Smith and Kazutaka YOKOTA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/include/pc/vesa.h,v 1.7 1999/12/29 04:33:12 peter Exp $
 * $DragonFly: src/sys/i386/include/pc/Attic/vesa.h,v 1.2 2003/06/17 04:28:36 dillon Exp $
 */

#ifndef _MACHINE_PC_VESA_H
#define _MACHINE_PC_VESA_H

struct vesa_info
{
    /* mandatory fields */
    u_int8_t		v_sig[4] __attribute__ ((packed));	/* VESA */
    u_int16_t		v_version __attribute__ ((packed));	/* ver in BCD */
    u_int32_t		v_oemstr __attribute__ ((packed));	/* OEM string */
    u_int32_t		v_flags __attribute__ ((packed));	/* flags */
#define V_DAC8		(1<<0)
#define V_NONVGA	(1<<1)
#define V_SNOW		(1<<2)
    u_int32_t		v_modetable __attribute__ ((packed));	/* modes */
    u_int16_t		v_memsize __attribute__ ((packed));	/* in 64K */
    /* 2.0 */
    u_int16_t		v_revision __attribute__ ((packed));	/* software rev */
    u_int32_t		v_venderstr __attribute__ ((packed));	/* vender */
    u_int32_t		v_prodstr __attribute__ ((packed));	/* product name */
    u_int32_t		v_revstr __attribute__ ((packed));	/* product rev */
};

struct vesa_mode 
{
    /* mandatory fields */
    u_int16_t		v_modeattr;
#define V_MODESUPP	(1<<0)	/* VESA mode attributes */
#define V_MODEOPTINFO	(1<<1)
#define V_MODEBIOSOUT	(1<<2)
#define V_MODECOLOR	(1<<3)
#define V_MODEGRAPHICS	(1<<4)
#define V_MODENONVGA	(1<<5)
#define V_MODENONBANK	(1<<6)
#define V_MODELFB	(1<<7)
#define V_MODEVESA	(1<<16)	/* Private attributes */
    u_int8_t		v_waattr;
    u_int8_t		v_wbattr;
#define V_WATTREXIST	(1<<0)
#define V_WATTRREAD	(1<<1)
#define V_WATTRWRITE	(1<<2)
    u_int16_t		v_wgran;
    u_int16_t		v_wsize;
    u_int16_t		v_waseg;
    u_int16_t		v_wbseg;
    u_int32_t		v_posfunc;
    u_int16_t		v_bpscanline;
    /* fields optional for 1.0/1.1 implementations */
    u_int16_t		v_width;
    u_int16_t		v_height;
    u_int8_t		v_cwidth;
    u_int8_t		v_cheight;
    u_int8_t		v_planes;
    u_int8_t		v_bpp;
    u_int8_t		v_banks;
    u_int8_t		v_memmodel;
#define V_MMTEXT	0
#define V_MMCGA		1
#define V_MMHGC		2
#define V_MMEGA		3
#define V_MMPACKED	4
#define V_MMSEQU256	5
#define V_MMDIRCOLOR	6
#define V_MMYUV		7
    u_int8_t		v_banksize;
    u_int8_t		v_ipages;
    u_int8_t		v_reserved0;
    /* fields for 1.2+ implementations */
    u_int8_t		v_redmasksize;
    u_int8_t		v_redfieldpos;
    u_int8_t		v_greenmasksize;
    u_int8_t		v_greenfieldpos;
    u_int8_t		v_bluemasksize;
    u_int8_t		v_bluefieldpos;
    u_int8_t		v_resmasksize;
    u_int8_t		v_resfieldpos;
    u_int8_t		v_dircolormode;
    /* 2.0 implementations */
    u_int32_t		v_lfb;
    u_int32_t		v_offscreen;
    u_int16_t		v_offscreensize;
};

#ifdef _KERNEL

#define VESA_MODE(x)	((x) >= M_VESA_BASE)

int vesa_load_ioctl(void);
int vesa_unload_ioctl(void);

#endif

#endif /* !_MACHINE_PC_VESA_H */
