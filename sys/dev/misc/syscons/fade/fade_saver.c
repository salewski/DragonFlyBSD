/*-
 * Copyright (c) 1995-1998 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/modules/syscons/fade/fade_saver.c,v 1.18 1999/08/28 00:47:47 peter Exp $
 * $DragonFly: src/sys/dev/misc/syscons/fade/fade_saver.c,v 1.3 2003/08/15 08:32:30 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/consio.h>
#include <sys/fbio.h>

#include <dev/video/fb/fbreg.h>
#include <dev/video/fb/splashreg.h>
#include "../syscons.h"

static u_char palette[256*3];
static int blanked;

static int
fade_saver(video_adapter_t *adp, int blank)
{
	static int count = 0;
	u_char pal[256*3];
	int i;

	if (blank) {
		blanked = TRUE;
		if (ISPALAVAIL(adp->va_flags)) {
			if (count <= 0)
				save_palette(adp, palette);
			if (count < 256) {
				pal[0] = pal[1] = pal[2] = 0;
				for (i = 3; i < 256*3; i++) {
					if (palette[i] - count > 60)
						pal[i] = palette[i] - count;
					else
						pal[i] = 60;
				}
				load_palette(adp, pal);
				count++;
			}
		} else {
	    		(*vidsw[adp->va_index]->blank_display)(adp,
							       V_DISPLAY_BLANK);
		}
	} else {
		if (ISPALAVAIL(adp->va_flags)) {
			load_palette(adp, palette);
			count = 0;
		} else {
	    		(*vidsw[adp->va_index]->blank_display)(adp,
							       V_DISPLAY_ON);
		}
		blanked = FALSE;
	}
	return 0;
}

static int
fade_init(video_adapter_t *adp)
{
	if (!ISPALAVAIL(adp->va_flags)
	    && (*vidsw[adp->va_index]->blank_display)(adp, V_DISPLAY_ON) != 0)
		return ENODEV;
	blanked = FALSE;
	return 0;
}

static int
fade_term(video_adapter_t *adp)
{
	return 0;
}

static scrn_saver_t fade_module = {
	"fade_saver", fade_init, fade_term, fade_saver, NULL,
};

SAVER_MODULE(fade_saver, fade_module);
