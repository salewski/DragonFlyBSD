/* drm_drawable.h -- IOCTLs for drawables -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD: src/sys/dev/drm/drm_drawable.h,v 1.2.2.1 2003/04/26 07:05:28 anholt Exp $
 * $DragonFly: src/sys/dev/drm/Attic/drm_drawable.h,v 1.2 2003/06/17 04:28:24 dillon Exp $
 */

#include "dev/drm/drmP.h"

int DRM(adddraw)( DRM_IOCTL_ARGS )
{
	drm_draw_t draw;

	draw.handle = 0;	/* NOOP */
	DRM_DEBUG("%d\n", draw.handle);
	
	DRM_COPY_TO_USER_IOCTL( (drm_draw_t *)data, draw, sizeof(draw) );

	return 0;
}

int DRM(rmdraw)( DRM_IOCTL_ARGS )
{
	return 0;		/* NOOP */
}
