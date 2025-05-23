/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
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
 * Copyright (c) 2000 Andrew Miklic
 *
 * $FreeBSD: src/sys/dev/syscons/scgfbrndr.c,v 1.14.2.1 2001/11/01 08:33:15 obrien Exp $
 * $DragonFly: src/sys/dev/misc/syscons/scgfbrndr.c,v 1.3 2003/08/07 21:16:59 dillon Exp $
 */

#include "opt_syscons.h"
#include "opt_gfb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fbio.h>
#include <sys/consio.h>

#include <machine/bus.h>

#include <dev/fb/fbreg.h>
#include "syscons.h"

#ifndef SC_RENDER_DEBUG
#define SC_RENDER_DEBUG		0
#endif

static vr_clear_t		gfb_clear;
static vr_draw_border_t		gfb_border;
static vr_draw_t		gfb_draw;
static vr_set_cursor_t		gfb_cursor_shape;
static vr_draw_cursor_t		gfb_cursor;
static vr_blink_cursor_t	gfb_blink;
#ifndef SC_NO_CUTPASTE
static vr_draw_mouse_t		gfb_mouse;
#else
#define gfb_mouse		(vr_draw_mouse_t *)gfb_nop
#endif

static void			gfb_nop(scr_stat *scp, ...);

sc_rndr_sw_t txtrndrsw = {
	gfb_clear,
	gfb_border,
	gfb_draw,	
	gfb_cursor_shape,
	gfb_cursor,
	gfb_blink,
	(vr_set_mouse_t *)gfb_nop,
	gfb_mouse,
};

#ifdef SC_PIXEL_MODE
sc_rndr_sw_t gfbrndrsw = {
	gfb_clear,
	gfb_border,
	gfb_draw,
	gfb_cursor_shape,
	gfb_cursor,
	gfb_blink,
	(vr_set_mouse_t *)gfb_nop,
	gfb_mouse,
};
#endif /* SC_PIXEL_MODE */

#ifndef SC_NO_MODE_CHANGE
sc_rndr_sw_t grrndrsw = {
	(vr_clear_t *)gfb_nop,
	gfb_border,
	(vr_draw_t *)gfb_nop,
	(vr_set_cursor_t *)gfb_nop,
	(vr_draw_cursor_t *)gfb_nop,
	(vr_blink_cursor_t *)gfb_nop,
	(vr_set_mouse_t *)gfb_nop,
	(vr_draw_mouse_t *)gfb_nop,
};
#endif /* SC_NO_MODE_CHANGE */

#ifndef SC_NO_CUTPASTE

static u_char mouse_pointer[16] = {
	0x00, 0x40, 0x60, 0x70, 0x78, 0x7c, 0x7e, 0x68,
	0x0c, 0x0c, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00
};
#endif

static void
gfb_nop(scr_stat *scp, ...)
{
}

/* text mode renderer */

static void
gfb_clear(scr_stat *scp, int c, int attr)
{
	(*vidsw[scp->sc->adapter]->clear)(scp->sc->adp);
}

static void
gfb_border(scr_stat *scp, int color)
{
	(*vidsw[scp->sc->adapter]->set_border)(scp->sc->adp, color);
}

static void
gfb_draw(scr_stat *scp, int from, int count, int flip)
{
	char c;
	char a;
	int i, n;
	video_adapter_t *adp;

	adp = scp->sc->adp;

	/*
	   Determine if we need to scroll based on the offset
	   and the number of characters to be displayed...
	 */
	if (from + count > scp->xsize*scp->ysize) {

		/*
		   Calculate the number of characters past the end of the
		   visible screen...
		*/
		count = (from + count) -
		    (adp->va_info.vi_width * adp->va_info.vi_height);

		/*
		   Calculate the number of rows past the end of the visible
		   screen...
		*/
		n = (count / adp->va_info.vi_width) + 1;

		/* Scroll to make room for new text rows... */
		(*vidsw[scp->sc->adapter]->copy)(adp, n, 0, n);
#ifdef 0
		(*vidsw[scp->sc->adapter]->clear)(adp, n);
#endif

		/* Display new text rows... */
		(*vidsw[scp->sc->adapter]->puts)(adp, from,
		    (u_int16_t *)sc_vtb_pointer(&scp->vtb, from), count);
	}

	/*
	   We don't need to scroll, so we can just put the characters
	   all-at-once...
	*/
	else {

		/*
		   Determine the method by which we are to display characters
		   (are we going to print forwards or backwards?
		   do we need to do a character-by-character copy, then?)...
		*/
		if (flip)
			for (i = count; i-- > 0; ++from) {
				c = sc_vtb_getc(&scp->vtb, from);
				a = sc_vtb_geta(&scp->vtb, from) >> 8;
				(*vidsw[scp->sc->adapter]->putc)(adp, from, c,
				    a);
			}
		else {
			(*vidsw[scp->sc->adapter]->puts)(adp, from,
			    (u_int16_t *)sc_vtb_pointer(&scp->vtb, from),
			    count);
		}
	}
}

static void 
gfb_cursor_shape(scr_stat *scp, int base, int height, int blink)
{
	if (base < 0 || base >= scp->font_size)
		return;
	/* the caller may set height <= 0 in order to disable the cursor */
#if 0
	scp->cursor_base = base;
	scp->cursor_height = height;
#endif
	(*vidsw[scp->sc->adapter]->set_hw_cursor_shape)(scp->sc->adp,
	    base, height, scp->font_size, blink);
}

static int pxlblinkrate = 0;

#ifdef 0
static void
gfb_cursor(scr_stat *scp, int at, int blink, int on, int flip)
{
	video_adapter_t *adp;

	if (scp->cursor_height <= 0)	/* the text cursor is disabled */
		return;

	adp = scp->sc->adp;
	if(blink) {
		scp->status |= VR_CURSOR_BLINK;
		if (on) {
			scp->status |= VR_CURSOR_ON;
			(*vidsw[adp->va_index]->set_hw_cursor)(adp,
			    at%scp->xsize,
			    at/scp->xsize);
		} else {
			if (scp->status & VR_CURSOR_ON)
				(*vidsw[adp->va_index]->set_hw_cursor)(adp, -1,
				    -1);
			scp->status &= ~VR_CURSOR_ON;
		}
	} else {
		scp->status &= ~VR_CURSOR_BLINK;
		if(on) {
			scp->status |= VR_CURSOR_ON;
			scp->cursor_saveunder_char = sc_vtb_getc(&scp->scr, at);
			scp->cursor_saveunder_attr = sc_vtb_geta(&scp->scr, at);
			(*vidsw[scp->sc->adapter]->putc)(scp->sc->adp, at,
			    scp->cursor_saveunder_char,
			    scp->cursor_saveunder_attr);
		} else {
			if (scp->status & VR_CURSOR_ON)
				(*vidsw[scp->sc->adapter]->putc)(scp->sc->adp,
				    at, scp->cursor_saveunder_char,
				    scp->cursor_saveunder_attr);
			scp->status &= ~VR_CURSOR_ON;
		}
	}
}
#endif

static void 
gfb_cursor(scr_stat *scp, int at, int blink, int on, int flip)
{
	video_adapter_t *adp;

	adp = scp->sc->adp;
	if (scp->cursor_height <= 0)	/* the text cursor is disabled */
		return;

	if (on) {
		if (!blink) {
			scp->status |= VR_CURSOR_ON;
			(*vidsw[adp->va_index]->set_hw_cursor)(adp,
			    at%scp->xsize, at/scp->xsize);
		} else if (++pxlblinkrate & 4) {
			pxlblinkrate = 0;
			scp->status ^= VR_CURSOR_ON;
			if(scp->status & VR_CURSOR_ON)
				(*vidsw[adp->va_index]->set_hw_cursor)(adp,
				    at%scp->xsize, at/scp->xsize);
			else
				(*vidsw[adp->va_index]->set_hw_cursor)(adp, -1,
				    -1);
		}
	} else {
		if (scp->status & VR_CURSOR_ON)
			(*vidsw[adp->va_index]->set_hw_cursor)(adp,
			    at%scp->xsize, at/scp->xsize);
		scp->status &= ~VR_CURSOR_ON;
	}
	if (blink)
		scp->status |= VR_CURSOR_BLINK;
	else
		scp->status &= ~VR_CURSOR_BLINK;
}

static void
gfb_blink(scr_stat *scp, int at, int flip)
{
	if (!(scp->status & VR_CURSOR_BLINK))
		return;
	if (!(++pxlblinkrate & 4))
		return;
	pxlblinkrate = 0;
	scp->status ^= VR_CURSOR_ON;
	gfb_cursor(scp, at, scp->status & VR_CURSOR_BLINK,
	    scp->status & VR_CURSOR_ON, flip);
}

#ifndef SC_NO_CUTPASTE

static void 
gfb_mouse(scr_stat *scp, int x, int y, int on)
{
	int i, pos;

	if (on) {

		/* Display the mouse pointer image... */
		(*vidsw[scp->sc->adapter]->putm)(scp->sc->adp, x, y,
		    mouse_pointer, 0xffffffff, 16);
	} else {

		/*
		   Erase the mouse cursor image by redrawing the text
		   underneath it...
		*/
		return;
		pos = x*scp->xsize + y;
		i = (y < scp->xsize - 1) ? 2 : 1;
		(*scp->rndr->draw)(scp, pos, i, FALSE);
		if (x < scp->ysize - 1)
			(*scp->rndr->draw)(scp, pos + scp->xsize, i, FALSE);
	}
}

#endif /* SC_NO_CUTPASTE */
