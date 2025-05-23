/* tdfx.h -- 3dfx DRM template customization -*- linux-c -*-
 * Created: Wed Feb 14 12:32:32 2001 by gareth@valinux.com
 *
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
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD: src/sys/dev/drm/tdfx.h,v 1.1.4.1 2003/04/26 07:05:30 anholt Exp $
 * $DragonFly: src/sys/dev/drm/tdfx/Attic/tdfx.h,v 1.2 2003/06/17 04:28:24 dillon Exp $
 */

#ifndef __TDFX_H__
#define __TDFX_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) tdfx_##x

/* General customization:
 */
#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	1

#endif
