/* r128_state.c -- State support for r128 -*- linux-c -*-
 * Created: Thu Jan 27 02:53:43 2000 by gareth@valinux.com
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD: src/sys/dev/drm/r128_state.c,v 1.6.2.1 2003/04/26 07:05:29 anholt Exp $
 * $DragonFly: src/sys/dev/drm/r128/Attic/r128_state.c,v 1.4 2005/02/17 13:59:36 joerg Exp $
 */

#include "r128.h"
#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "r128_drm.h"
#include "r128_drv.h"


/* ================================================================
 * CCE hardware state programming functions
 */

static void r128_emit_clip_rects( drm_r128_private_t *dev_priv,
				  drm_clip_rect_t *boxes, int count )
{
	u32 aux_sc_cntl = 0x00000000;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 17 );

	if ( count >= 1 ) {
		OUT_RING( CCE_PACKET0( R128_AUX1_SC_LEFT, 3 ) );
		OUT_RING( boxes[0].x1 );
		OUT_RING( boxes[0].x2 - 1 );
		OUT_RING( boxes[0].y1 );
		OUT_RING( boxes[0].y2 - 1 );

		aux_sc_cntl |= (R128_AUX1_SC_EN | R128_AUX1_SC_MODE_OR);
	}
	if ( count >= 2 ) {
		OUT_RING( CCE_PACKET0( R128_AUX2_SC_LEFT, 3 ) );
		OUT_RING( boxes[1].x1 );
		OUT_RING( boxes[1].x2 - 1 );
		OUT_RING( boxes[1].y1 );
		OUT_RING( boxes[1].y2 - 1 );

		aux_sc_cntl |= (R128_AUX2_SC_EN | R128_AUX2_SC_MODE_OR);
	}
	if ( count >= 3 ) {
		OUT_RING( CCE_PACKET0( R128_AUX3_SC_LEFT, 3 ) );
		OUT_RING( boxes[2].x1 );
		OUT_RING( boxes[2].x2 - 1 );
		OUT_RING( boxes[2].y1 );
		OUT_RING( boxes[2].y2 - 1 );

		aux_sc_cntl |= (R128_AUX3_SC_EN | R128_AUX3_SC_MODE_OR);
	}

	OUT_RING( CCE_PACKET0( R128_AUX_SC_CNTL, 0 ) );
	OUT_RING( aux_sc_cntl );

	ADVANCE_RING();
}

static __inline__ void r128_emit_core( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_SCALE_3D_CNTL, 0 ) );
	OUT_RING( ctx->scale_3d_cntl );

	ADVANCE_RING();
}

static __inline__ void r128_emit_context( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 13 );

	OUT_RING( CCE_PACKET0( R128_DST_PITCH_OFFSET_C, 11 ) );
	OUT_RING( ctx->dst_pitch_offset_c );
	OUT_RING( ctx->dp_gui_master_cntl_c );
	OUT_RING( ctx->sc_top_left_c );
	OUT_RING( ctx->sc_bottom_right_c );
	OUT_RING( ctx->z_offset_c );
	OUT_RING( ctx->z_pitch_c );
	OUT_RING( ctx->z_sten_cntl_c );
	OUT_RING( ctx->tex_cntl_c );
	OUT_RING( ctx->misc_3d_state_cntl_reg );
	OUT_RING( ctx->texture_clr_cmp_clr_c );
	OUT_RING( ctx->texture_clr_cmp_msk_c );
	OUT_RING( ctx->fog_color_c );

	ADVANCE_RING();
}

static __inline__ void r128_emit_setup( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 3 );

	OUT_RING( CCE_PACKET1( R128_SETUP_CNTL, R128_PM4_VC_FPU_SETUP ) );
	OUT_RING( ctx->setup_cntl );
	OUT_RING( ctx->pm4_vc_fpu_setup );

	ADVANCE_RING();
}

static __inline__ void r128_emit_masks( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 5 );

	OUT_RING( CCE_PACKET0( R128_DP_WRITE_MASK, 0 ) );
	OUT_RING( ctx->dp_write_mask );

	OUT_RING( CCE_PACKET0( R128_STEN_REF_MASK_C, 1 ) );
	OUT_RING( ctx->sten_ref_mask_c );
	OUT_RING( ctx->plane_3d_mask_c );

	ADVANCE_RING();
}

static __inline__ void r128_emit_window( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_WINDOW_XY_OFFSET, 0 ) );
	OUT_RING( ctx->window_xy_offset );

	ADVANCE_RING();
}

static __inline__ void r128_emit_tex0( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_context_regs_t *ctx = &sarea_priv->context_state;
	drm_r128_texture_regs_t *tex = &sarea_priv->tex_state[0];
	int i;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 7 + R128_MAX_TEXTURE_LEVELS );

	OUT_RING( CCE_PACKET0( R128_PRIM_TEX_CNTL_C,
			       2 + R128_MAX_TEXTURE_LEVELS ) );
	OUT_RING( tex->tex_cntl );
	OUT_RING( tex->tex_combine_cntl );
	OUT_RING( ctx->tex_size_pitch_c );
	for ( i = 0 ; i < R128_MAX_TEXTURE_LEVELS ; i++ ) {
		OUT_RING( tex->tex_offset[i] );
	}

	OUT_RING( CCE_PACKET0( R128_CONSTANT_COLOR_C, 1 ) );
	OUT_RING( ctx->constant_color_c );
	OUT_RING( tex->tex_border_color );

	ADVANCE_RING();
}

static __inline__ void r128_emit_tex1( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_texture_regs_t *tex = &sarea_priv->tex_state[1];
	int i;
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __func__ );

	BEGIN_RING( 5 + R128_MAX_TEXTURE_LEVELS );

	OUT_RING( CCE_PACKET0( R128_SEC_TEX_CNTL_C,
			       1 + R128_MAX_TEXTURE_LEVELS ) );
	OUT_RING( tex->tex_cntl );
	OUT_RING( tex->tex_combine_cntl );
	for ( i = 0 ; i < R128_MAX_TEXTURE_LEVELS ; i++ ) {
		OUT_RING( tex->tex_offset[i] );
	}

	OUT_RING( CCE_PACKET0( R128_SEC_TEXTURE_BORDER_COLOR_C, 0 ) );
	OUT_RING( tex->tex_border_color );

	ADVANCE_RING();
}

static __inline__ void r128_emit_state( drm_r128_private_t *dev_priv )
{
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty;

	DRM_DEBUG( "%s: dirty=0x%08x\n", __func__, dirty );

	if ( dirty & R128_UPLOAD_CORE ) {
		r128_emit_core( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_CORE;
	}

	if ( dirty & R128_UPLOAD_CONTEXT ) {
		r128_emit_context( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_CONTEXT;
	}

	if ( dirty & R128_UPLOAD_SETUP ) {
		r128_emit_setup( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_SETUP;
	}

	if ( dirty & R128_UPLOAD_MASKS ) {
		r128_emit_masks( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_MASKS;
	}

	if ( dirty & R128_UPLOAD_WINDOW ) {
		r128_emit_window( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_WINDOW;
	}

	if ( dirty & R128_UPLOAD_TEX0 ) {
		r128_emit_tex0( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_TEX0;
	}

	if ( dirty & R128_UPLOAD_TEX1 ) {
		r128_emit_tex1( dev_priv );
		sarea_priv->dirty &= ~R128_UPLOAD_TEX1;
	}

	/* Turn off the texture cache flushing */
	sarea_priv->context_state.tex_cntl_c &= ~R128_TEX_CACHE_FLUSH;

	sarea_priv->dirty &= ~R128_REQUIRE_QUIESCENCE;
}


#if R128_PERFORMANCE_BOXES
/* ================================================================
 * Performance monitoring functions
 */

static void r128_clear_box( drm_r128_private_t *dev_priv,
			    int x, int y, int w, int h,
			    int r, int g, int b )
{
	u32 pitch, offset;
	u32 fb_bpp, color;
	RING_LOCALS;

	switch ( dev_priv->fb_bpp ) {
	case 16:
		fb_bpp = R128_GMC_DST_16BPP;
		color = (((r & 0xf8) << 8) |
			 ((g & 0xfc) << 3) |
			 ((b & 0xf8) >> 3));
		break;
	case 24:
		fb_bpp = R128_GMC_DST_24BPP;
		color = ((r << 16) | (g << 8) | b);
		break;
	case 32:
		fb_bpp = R128_GMC_DST_32BPP;
		color = (((0xff) << 24) | (r << 16) | (g <<  8) | b);
		break;
	default:
		return;
	}

	offset = dev_priv->back_offset;
	pitch = dev_priv->back_pitch >> 3;

	BEGIN_RING( 6 );

	OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
	OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
		  R128_GMC_BRUSH_SOLID_COLOR |
		  fb_bpp |
		  R128_GMC_SRC_DATATYPE_COLOR |
		  R128_ROP3_P |
		  R128_GMC_CLR_CMP_CNTL_DIS |
		  R128_GMC_AUX_CLIP_DIS );

	OUT_RING( (pitch << 21) | (offset >> 5) );
	OUT_RING( color );

	OUT_RING( (x << 16) | y );
	OUT_RING( (w << 16) | h );

	ADVANCE_RING();
}

static void r128_cce_performance_boxes( drm_r128_private_t *dev_priv )
{
	if ( atomic_read( &dev_priv->idle_count ) == 0 ) {
		r128_clear_box( dev_priv, 64, 4, 8, 8, 0, 255, 0 );
	} else {
		atomic_set( &dev_priv->idle_count, 0 );
	}
}

#endif


/* ================================================================
 * CCE command dispatch functions
 */

static void r128_print_dirty( const char *msg, unsigned int flags )
{
	DRM_INFO( "%s: (0x%x) %s%s%s%s%s%s%s%s%s\n",
		  msg,
		  flags,
		  (flags & R128_UPLOAD_CORE)        ? "core, " : "",
		  (flags & R128_UPLOAD_CONTEXT)     ? "context, " : "",
		  (flags & R128_UPLOAD_SETUP)       ? "setup, " : "",
		  (flags & R128_UPLOAD_TEX0)        ? "tex0, " : "",
		  (flags & R128_UPLOAD_TEX1)        ? "tex1, " : "",
		  (flags & R128_UPLOAD_MASKS)       ? "masks, " : "",
		  (flags & R128_UPLOAD_WINDOW)      ? "window, " : "",
		  (flags & R128_UPLOAD_CLIPRECTS)   ? "cliprects, " : "",
		  (flags & R128_REQUIRE_QUIESCENCE) ? "quiescence, " : "" );
}

static void r128_cce_dispatch_clear( drm_device_t *dev,
				     drm_r128_clear_t *clear )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	unsigned int flags = clear->flags;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __func__ );

	if ( dev_priv->page_flipping && dev_priv->current_page == 1 ) {
		unsigned int tmp = flags;

		flags &= ~(R128_FRONT | R128_BACK);
		if ( tmp & R128_FRONT ) flags |= R128_BACK;
		if ( tmp & R128_BACK )  flags |= R128_FRONT;
	}

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG( "dispatch clear %d,%d-%d,%d flags 0x%x\n",
			   pbox[i].x1, pbox[i].y1, pbox[i].x2,
			   pbox[i].y2, flags );

		if ( flags & (R128_FRONT | R128_BACK) ) {
			BEGIN_RING( 2 );

			OUT_RING( CCE_PACKET0( R128_DP_WRITE_MASK, 0 ) );
			OUT_RING( clear->color_mask );

			ADVANCE_RING();
		}

		if ( flags & R128_FRONT ) {
			BEGIN_RING( 6 );

			OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
				  R128_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  R128_GMC_SRC_DATATYPE_COLOR |
				  R128_ROP3_P |
				  R128_GMC_CLR_CMP_CNTL_DIS |
				  R128_GMC_AUX_CLIP_DIS );

			OUT_RING( dev_priv->front_pitch_offset_c );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}

		if ( flags & R128_BACK ) {
			BEGIN_RING( 6 );

			OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
				  R128_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  R128_GMC_SRC_DATATYPE_COLOR |
				  R128_ROP3_P |
				  R128_GMC_CLR_CMP_CNTL_DIS |
				  R128_GMC_AUX_CLIP_DIS );

			OUT_RING( dev_priv->back_pitch_offset_c );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}

		if ( flags & R128_DEPTH ) {
			BEGIN_RING( 6 );

			OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
				  R128_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->depth_fmt << 8) |
				  R128_GMC_SRC_DATATYPE_COLOR |
				  R128_ROP3_P |
				  R128_GMC_CLR_CMP_CNTL_DIS |
				  R128_GMC_AUX_CLIP_DIS |
				  R128_GMC_WR_MSK_DIS );

			OUT_RING( dev_priv->depth_pitch_offset_c );
			OUT_RING( clear->clear_depth );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}
	}
}

static void r128_cce_dispatch_swap( drm_device_t *dev )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __func__ );

#if R128_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	r128_cce_performance_boxes( dev_priv );
#endif

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		BEGIN_RING( 7 );

		OUT_RING( CCE_PACKET3( R128_CNTL_BITBLT_MULTI, 5 ) );
		OUT_RING( R128_GMC_SRC_PITCH_OFFSET_CNTL |
			  R128_GMC_DST_PITCH_OFFSET_CNTL |
			  R128_GMC_BRUSH_NONE |
			  (dev_priv->color_fmt << 8) |
			  R128_GMC_SRC_DATATYPE_COLOR |
			  R128_ROP3_S |
			  R128_DP_SRC_SOURCE_MEMORY |
			  R128_GMC_CLR_CMP_CNTL_DIS |
			  R128_GMC_AUX_CLIP_DIS |
			  R128_GMC_WR_MSK_DIS );

		OUT_RING( dev_priv->back_pitch_offset_c );
		OUT_RING( dev_priv->front_pitch_offset_c );

		OUT_RING( (x << 16) | y );
		OUT_RING( (x << 16) | y );
		OUT_RING( (w << 16) | h );

		ADVANCE_RING();
	}

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_LAST_FRAME_REG, 0 ) );
	OUT_RING( dev_priv->sarea_priv->last_frame );

	ADVANCE_RING();
}

static void r128_cce_dispatch_flip( drm_device_t *dev )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "page=%d\n", dev_priv->current_page );

#if R128_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	r128_cce_performance_boxes( dev_priv );
#endif

	BEGIN_RING( 4 );

	R128_WAIT_UNTIL_PAGE_FLIPPED();
	OUT_RING( CCE_PACKET0( R128_CRTC_OFFSET, 0 ) );

	if ( dev_priv->current_page == 0 ) {
		OUT_RING( dev_priv->back_offset );
		dev_priv->current_page = 1;
	} else {
		OUT_RING( dev_priv->front_offset );
		dev_priv->current_page = 0;
	}

	ADVANCE_RING();

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_LAST_FRAME_REG, 0 ) );
	OUT_RING( dev_priv->sarea_priv->last_frame );

	ADVANCE_RING();
}

static void r128_cce_dispatch_vertex( drm_device_t *dev,
				      drm_buf_t *buf )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_buf_priv_t *buf_priv = buf->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int format = sarea_priv->vc_format;
	int offset = buf->bus_address;
	int size = buf->used;
	int prim = buf_priv->prim;
	int i = 0;
	RING_LOCALS;
	DRM_DEBUG( "buf=%d nbox=%d\n", buf->idx, sarea_priv->nbox );

	if ( 0 )
		r128_print_dirty( "dispatch_vertex", sarea_priv->dirty );

	if ( buf->used ) {
		buf_priv->dispatched = 1;

		if ( sarea_priv->dirty & ~R128_UPLOAD_CLIPRECTS ) {
			r128_emit_state( dev_priv );
		}

		do {
			/* Emit the next set of up to three cliprects */
			if ( i < sarea_priv->nbox ) {
				r128_emit_clip_rects( dev_priv,
						      &sarea_priv->boxes[i],
						      sarea_priv->nbox - i );
			}

			/* Emit the vertex buffer rendering commands */
			BEGIN_RING( 5 );

			OUT_RING( CCE_PACKET3( R128_3D_RNDR_GEN_INDX_PRIM, 3 ) );
			OUT_RING( offset );
			OUT_RING( size );
			OUT_RING( format );
			OUT_RING( prim | R128_CCE_VC_CNTL_PRIM_WALK_LIST |
				  (size << R128_CCE_VC_CNTL_NUM_SHIFT) );

			ADVANCE_RING();

			i += 3;
		} while ( i < sarea_priv->nbox );
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the vertex buffer age */
		BEGIN_RING( 2 );

		OUT_RING( CCE_PACKET0( R128_LAST_DISPATCH_REG, 0 ) );
		OUT_RING( buf_priv->age );

		ADVANCE_RING();

		buf->pending = 1;
		buf->used = 0;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;

	sarea_priv->dirty &= ~R128_UPLOAD_CLIPRECTS;
	sarea_priv->nbox = 0;
}

static void r128_cce_dispatch_indirect( drm_device_t *dev,
					drm_buf_t *buf,
					int start, int end )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_buf_priv_t *buf_priv = buf->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "indirect: buf=%d s=0x%x e=0x%x\n",
		   buf->idx, start, end );

	if ( start != end ) {
		int offset = buf->bus_address + start;
		int dwords = (end - start + 3) / sizeof(u32);

		/* Indirect buffer data must be an even number of
		 * dwords, so if we've been given an odd number we must
		 * pad the data with a Type-2 CCE packet.
		 */
		if ( dwords & 1 ) {
			u32 *data = (u32 *)
				((char *)dev_priv->buffers->handle
				 + buf->offset + start);
			data[dwords++] = cpu_to_le32( R128_CCE_PACKET2 );
		}

		buf_priv->dispatched = 1;

		/* Fire off the indirect buffer */
		BEGIN_RING( 3 );

		OUT_RING( CCE_PACKET0( R128_PM4_IW_INDOFF, 1 ) );
		OUT_RING( offset );
		OUT_RING( dwords );

		ADVANCE_RING();
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the indirect buffer age */
		BEGIN_RING( 2 );

		OUT_RING( CCE_PACKET0( R128_LAST_DISPATCH_REG, 0 ) );
		OUT_RING( buf_priv->age );

		ADVANCE_RING();

		buf->pending = 1;
		buf->used = 0;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;
}

static void r128_cce_dispatch_indices( drm_device_t *dev,
				       drm_buf_t *buf,
				       int start, int end,
				       int count )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_buf_priv_t *buf_priv = buf->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int format = sarea_priv->vc_format;
	int offset = dev_priv->buffers->offset - dev_priv->cce_buffers_offset;
	int prim = buf_priv->prim;
	u32 *data;
	int dwords;
	int i = 0;
	RING_LOCALS;
	DRM_DEBUG( "indices: s=%d e=%d c=%d\n", start, end, count );

	if ( 0 )
		r128_print_dirty( "dispatch_indices", sarea_priv->dirty );

	if ( start != end ) {
		buf_priv->dispatched = 1;

		if ( sarea_priv->dirty & ~R128_UPLOAD_CLIPRECTS ) {
			r128_emit_state( dev_priv );
		}

		dwords = (end - start + 3) / sizeof(u32);

		data = (u32 *)((char *)dev_priv->buffers->handle
			       + buf->offset + start);

		data[0] = cpu_to_le32( CCE_PACKET3( R128_3D_RNDR_GEN_INDX_PRIM,
						    dwords-2 ) );

		data[1] = cpu_to_le32( offset );
		data[2] = cpu_to_le32( R128_MAX_VB_VERTS );
		data[3] = cpu_to_le32( format );
		data[4] = cpu_to_le32( (prim | R128_CCE_VC_CNTL_PRIM_WALK_IND |
					(count << 16)) );

		if ( count & 0x1 ) {
#ifdef __LITTLE_ENDIAN
			data[dwords-1] &= 0x0000ffff;
#else
			data[dwords-1] &= 0xffff0000;
#endif
		}

		do {
			/* Emit the next set of up to three cliprects */
			if ( i < sarea_priv->nbox ) {
				r128_emit_clip_rects( dev_priv,
						      &sarea_priv->boxes[i],
						      sarea_priv->nbox - i );
			}

			r128_cce_dispatch_indirect( dev, buf, start, end );

			i += 3;
		} while ( i < sarea_priv->nbox );
	}

	if ( buf_priv->discard ) {
		buf_priv->age = dev_priv->sarea_priv->last_dispatch;

		/* Emit the vertex buffer age */
		BEGIN_RING( 2 );

		OUT_RING( CCE_PACKET0( R128_LAST_DISPATCH_REG, 0 ) );
		OUT_RING( buf_priv->age );

		ADVANCE_RING();

		buf->pending = 1;
		/* FIXME: Check dispatched field */
		buf_priv->dispatched = 0;
	}

	dev_priv->sarea_priv->last_dispatch++;

	sarea_priv->dirty &= ~R128_UPLOAD_CLIPRECTS;
	sarea_priv->nbox = 0;
}

static int r128_cce_dispatch_blit( DRMFILE filp,
				   drm_device_t *dev,
				   drm_r128_blit_t *blit )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_r128_buf_priv_t *buf_priv;
	u32 *data;
	int dword_shift, dwords;
	RING_LOCALS;
	DRM_DEBUG( "\n" );

	/* The compiler won't optimize away a division by a variable,
	 * even if the only legal values are powers of two.  Thus, we'll
	 * use a shift instead.
	 */
	switch ( blit->format ) {
	case R128_DATATYPE_ARGB8888:
		dword_shift = 0;
		break;
	case R128_DATATYPE_ARGB1555:
	case R128_DATATYPE_RGB565:
	case R128_DATATYPE_ARGB4444:
		dword_shift = 1;
		break;
	case R128_DATATYPE_CI8:
	case R128_DATATYPE_RGB8:
		dword_shift = 2;
		break;
	default:
		DRM_ERROR( "invalid blit format %d\n", blit->format );
		return DRM_ERR(EINVAL);
	}

	/* Flush the pixel cache, and mark the contents as Read Invalid.
	 * This ensures no pixel data gets mixed up with the texture
	 * data from the host data blit, otherwise part of the texture
	 * image may be corrupted.
	 */
	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_PC_GUI_CTLSTAT, 0 ) );
	OUT_RING( R128_PC_RI_GUI | R128_PC_FLUSH_GUI );

	ADVANCE_RING();

	/* Dispatch the indirect buffer.
	 */
	buf = dma->buflist[blit->idx];
	buf_priv = buf->dev_private;

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", blit->idx );
		return DRM_ERR(EINVAL);
	}

	buf_priv->discard = 1;

	dwords = (blit->width * blit->height) >> dword_shift;

	data = (u32 *)((char *)dev_priv->buffers->handle + buf->offset);

	data[0] = cpu_to_le32( CCE_PACKET3( R128_CNTL_HOSTDATA_BLT, dwords + 6 ) );
	data[1] = cpu_to_le32( (R128_GMC_DST_PITCH_OFFSET_CNTL |
				R128_GMC_BRUSH_NONE |
				(blit->format << 8) |
				R128_GMC_SRC_DATATYPE_COLOR |
				R128_ROP3_S |
				R128_DP_SRC_SOURCE_HOST_DATA |
				R128_GMC_CLR_CMP_CNTL_DIS |
				R128_GMC_AUX_CLIP_DIS |
				R128_GMC_WR_MSK_DIS) );

	data[2] = cpu_to_le32( (blit->pitch << 21) | (blit->offset >> 5) );
	data[3] = cpu_to_le32( 0xffffffff );
	data[4] = cpu_to_le32( 0xffffffff );
	data[5] = cpu_to_le32( (blit->y << 16) | blit->x );
	data[6] = cpu_to_le32( (blit->height << 16) | blit->width );
	data[7] = cpu_to_le32( dwords );

	buf->used = (dwords + 8) * sizeof(u32);

	r128_cce_dispatch_indirect( dev, buf, 0, buf->used );

	/* Flush the pixel cache after the blit completes.  This ensures
	 * the texture data is written out to memory before rendering
	 * continues.
	 */
	BEGIN_RING( 2 );

	OUT_RING( CCE_PACKET0( R128_PC_GUI_CTLSTAT, 0 ) );
	OUT_RING( R128_PC_FLUSH_GUI );

	ADVANCE_RING();

	return 0;
}


/* ================================================================
 * Tiled depth buffer management
 *
 * FIXME: These should all set the destination write mask for when we
 * have hardware stencil support.
 */

static int r128_cce_dispatch_write_span( drm_device_t *dev,
					 drm_r128_depth_t *depth )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	int count, x, y;
	u32 *buffer;
	u8 *mask;
	int i, buffer_size, mask_size;
	RING_LOCALS;
	DRM_DEBUG( "\n" );

	count = depth->n;
	if ( DRM_COPY_FROM_USER( &x, depth->x, sizeof(x) ) ) {
		return DRM_ERR(EFAULT);
	}
	if ( DRM_COPY_FROM_USER( &y, depth->y, sizeof(y) ) ) {
		return DRM_ERR(EFAULT);
	}

	buffer_size = depth->n * sizeof(u32);
	buffer = DRM_MALLOC( buffer_size );
	if ( buffer == NULL )
		return DRM_ERR(ENOMEM);
	if ( DRM_COPY_FROM_USER( buffer, depth->buffer, buffer_size ) ) {
		DRM_FREE( buffer, buffer_size);
		return DRM_ERR(EFAULT);
	}

	mask_size = depth->n * sizeof(u8);
	if ( depth->mask ) {
		mask = DRM_MALLOC( mask_size );
		if ( mask == NULL ) {
			DRM_FREE( buffer, buffer_size );
			return DRM_ERR(ENOMEM);
		}
		if ( DRM_COPY_FROM_USER( mask, depth->mask, mask_size ) ) {
			DRM_FREE( buffer, buffer_size );
			DRM_FREE( mask, mask_size );
			return DRM_ERR(EFAULT);
		}

		for ( i = 0 ; i < count ; i++, x++ ) {
			if ( mask[i] ) {
				BEGIN_RING( 6 );

				OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
				OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
					  R128_GMC_BRUSH_SOLID_COLOR |
					  (dev_priv->depth_fmt << 8) |
					  R128_GMC_SRC_DATATYPE_COLOR |
					  R128_ROP3_P |
					  R128_GMC_CLR_CMP_CNTL_DIS |
					  R128_GMC_WR_MSK_DIS );

				OUT_RING( dev_priv->depth_pitch_offset_c );
				OUT_RING( buffer[i] );

				OUT_RING( (x << 16) | y );
				OUT_RING( (1 << 16) | 1 );

				ADVANCE_RING();
			}
		}

		DRM_FREE( mask, mask_size );
	} else {
		for ( i = 0 ; i < count ; i++, x++ ) {
			BEGIN_RING( 6 );

			OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
				  R128_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->depth_fmt << 8) |
				  R128_GMC_SRC_DATATYPE_COLOR |
				  R128_ROP3_P |
				  R128_GMC_CLR_CMP_CNTL_DIS |
				  R128_GMC_WR_MSK_DIS );

			OUT_RING( dev_priv->depth_pitch_offset_c );
			OUT_RING( buffer[i] );

			OUT_RING( (x << 16) | y );
			OUT_RING( (1 << 16) | 1 );

			ADVANCE_RING();
		}
	}

	DRM_FREE( buffer, buffer_size );

	return 0;
}

static int r128_cce_dispatch_write_pixels( drm_device_t *dev,
					   drm_r128_depth_t *depth )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	int count, *x, *y;
	u32 *buffer;
	u8 *mask;
	int i, xbuf_size, ybuf_size, buffer_size, mask_size;
	RING_LOCALS;
	DRM_DEBUG( "\n" );

	count = depth->n;

	xbuf_size = count * sizeof(*x);
	ybuf_size = count * sizeof(*y);
	x = DRM_MALLOC( xbuf_size );
	if ( x == NULL ) {
		return DRM_ERR(ENOMEM);
	}
	y = DRM_MALLOC( ybuf_size );
	if ( y == NULL ) {
		DRM_FREE( x, xbuf_size );
		return DRM_ERR(ENOMEM);
	}
	if ( DRM_COPY_FROM_USER( x, depth->x, xbuf_size ) ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		return DRM_ERR(EFAULT);
	}
	if ( DRM_COPY_FROM_USER( y, depth->y, xbuf_size ) ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		return DRM_ERR(EFAULT);
	}

	buffer_size = depth->n * sizeof(u32);
	buffer = DRM_MALLOC( buffer_size );
	if ( buffer == NULL ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		return DRM_ERR(ENOMEM);
	}
	if ( DRM_COPY_FROM_USER( buffer, depth->buffer, buffer_size ) ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		DRM_FREE( buffer, buffer_size );
		return DRM_ERR(EFAULT);
	}

	if ( depth->mask ) {
		mask_size = depth->n * sizeof(u8);
		mask = DRM_MALLOC( mask_size );
		if ( mask == NULL ) {
			DRM_FREE( x, xbuf_size );
			DRM_FREE( y, ybuf_size );
			DRM_FREE( buffer, buffer_size );
			return DRM_ERR(ENOMEM);
		}
		if ( DRM_COPY_FROM_USER( mask, depth->mask, mask_size ) ) {
			DRM_FREE( x, xbuf_size );
			DRM_FREE( y, ybuf_size );
			DRM_FREE( buffer, buffer_size );
			DRM_FREE( mask, mask_size );
			return DRM_ERR(EFAULT);
		}

		for ( i = 0 ; i < count ; i++ ) {
			if ( mask[i] ) {
				BEGIN_RING( 6 );

				OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
				OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
					  R128_GMC_BRUSH_SOLID_COLOR |
					  (dev_priv->depth_fmt << 8) |
					  R128_GMC_SRC_DATATYPE_COLOR |
					  R128_ROP3_P |
					  R128_GMC_CLR_CMP_CNTL_DIS |
					  R128_GMC_WR_MSK_DIS );

				OUT_RING( dev_priv->depth_pitch_offset_c );
				OUT_RING( buffer[i] );

				OUT_RING( (x[i] << 16) | y[i] );
				OUT_RING( (1 << 16) | 1 );

				ADVANCE_RING();
			}
		}

		DRM_FREE( mask, mask_size );
	} else {
		for ( i = 0 ; i < count ; i++ ) {
			BEGIN_RING( 6 );

			OUT_RING( CCE_PACKET3( R128_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( R128_GMC_DST_PITCH_OFFSET_CNTL |
				  R128_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->depth_fmt << 8) |
				  R128_GMC_SRC_DATATYPE_COLOR |
				  R128_ROP3_P |
				  R128_GMC_CLR_CMP_CNTL_DIS |
				  R128_GMC_WR_MSK_DIS );

			OUT_RING( dev_priv->depth_pitch_offset_c );
			OUT_RING( buffer[i] );

			OUT_RING( (x[i] << 16) | y[i] );
			OUT_RING( (1 << 16) | 1 );

			ADVANCE_RING();
		}
	}

	DRM_FREE( x, xbuf_size );
	DRM_FREE( y, ybuf_size );
	DRM_FREE( buffer, buffer_size );

	return 0;
}

static int r128_cce_dispatch_read_span( drm_device_t *dev,
					drm_r128_depth_t *depth )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	int count, x, y;
	RING_LOCALS;
	DRM_DEBUG( "\n" );

	count = depth->n;
	if ( DRM_COPY_FROM_USER( &x, depth->x, sizeof(x) ) ) {
		return DRM_ERR(EFAULT);
	}
	if ( DRM_COPY_FROM_USER( &y, depth->y, sizeof(y) ) ) {
		return DRM_ERR(EFAULT);
	}

	BEGIN_RING( 7 );

	OUT_RING( CCE_PACKET3( R128_CNTL_BITBLT_MULTI, 5 ) );
	OUT_RING( R128_GMC_SRC_PITCH_OFFSET_CNTL |
		  R128_GMC_DST_PITCH_OFFSET_CNTL |
		  R128_GMC_BRUSH_NONE |
		  (dev_priv->depth_fmt << 8) |
		  R128_GMC_SRC_DATATYPE_COLOR |
		  R128_ROP3_S |
		  R128_DP_SRC_SOURCE_MEMORY |
		  R128_GMC_CLR_CMP_CNTL_DIS |
		  R128_GMC_WR_MSK_DIS );

	OUT_RING( dev_priv->depth_pitch_offset_c );
	OUT_RING( dev_priv->span_pitch_offset_c );

	OUT_RING( (x << 16) | y );
	OUT_RING( (0 << 16) | 0 );
	OUT_RING( (count << 16) | 1 );

	ADVANCE_RING();

	return 0;
}

static int r128_cce_dispatch_read_pixels( drm_device_t *dev,
					  drm_r128_depth_t *depth )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	int count, *x, *y;
	int i, xbuf_size, ybuf_size;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __func__ );

	count = depth->n;
	if ( count > dev_priv->depth_pitch ) {
		count = dev_priv->depth_pitch;
	}

	xbuf_size = count * sizeof(*x);
	ybuf_size = count * sizeof(*y);
	x = DRM_MALLOC( xbuf_size );
	if ( x == NULL ) {
		return DRM_ERR(ENOMEM);
	}
	y = DRM_MALLOC( ybuf_size );
	if ( y == NULL ) {
		DRM_FREE( x, xbuf_size );
		return DRM_ERR(ENOMEM);
	}
	if ( DRM_COPY_FROM_USER( x, depth->x, xbuf_size ) ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		return DRM_ERR(EFAULT);
	}
	if ( DRM_COPY_FROM_USER( y, depth->y, ybuf_size ) ) {
		DRM_FREE( x, xbuf_size );
		DRM_FREE( y, ybuf_size );
		return DRM_ERR(EFAULT);
	}

	for ( i = 0 ; i < count ; i++ ) {
		BEGIN_RING( 7 );

		OUT_RING( CCE_PACKET3( R128_CNTL_BITBLT_MULTI, 5 ) );
		OUT_RING( R128_GMC_SRC_PITCH_OFFSET_CNTL |
			  R128_GMC_DST_PITCH_OFFSET_CNTL |
			  R128_GMC_BRUSH_NONE |
			  (dev_priv->depth_fmt << 8) |
			  R128_GMC_SRC_DATATYPE_COLOR |
			  R128_ROP3_S |
			  R128_DP_SRC_SOURCE_MEMORY |
			  R128_GMC_CLR_CMP_CNTL_DIS |
			  R128_GMC_WR_MSK_DIS );

		OUT_RING( dev_priv->depth_pitch_offset_c );
		OUT_RING( dev_priv->span_pitch_offset_c );

		OUT_RING( (x[i] << 16) | y[i] );
		OUT_RING( (i << 16) | 0 );
		OUT_RING( (1 << 16) | 1 );

		ADVANCE_RING();
	}

	DRM_FREE( x, xbuf_size );
	DRM_FREE( y, ybuf_size );

	return 0;
}


/* ================================================================
 * Polygon stipple
 */

static void r128_cce_dispatch_stipple( drm_device_t *dev, u32 *stipple )
{
	drm_r128_private_t *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __func__ );

	BEGIN_RING( 33 );

	OUT_RING( CCE_PACKET0( R128_BRUSH_DATA0, 31 ) );
	for ( i = 0 ; i < 32 ; i++ ) {
		OUT_RING( stipple[i] );
	}

	ADVANCE_RING();
}


/* ================================================================
 * IOCTL functions
 */

int r128_cce_clear( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_r128_clear_t clear;
	DRM_DEBUG( "\n" );

	LOCK_TEST_WITH_RETURN( dev, filp );

	DRM_COPY_FROM_USER_IOCTL( clear, (drm_r128_clear_t *) data,
			     sizeof(clear) );

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	if ( sarea_priv->nbox > R128_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = R128_NR_SAREA_CLIPRECTS;

	r128_cce_dispatch_clear( dev, &clear );

	/* Make sure we restore the 3D state next time.
	 */
	dev_priv->sarea_priv->dirty |= R128_UPLOAD_CONTEXT | R128_UPLOAD_MASKS;

	return 0;
}

int r128_cce_swap( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_sarea_t *sarea_priv = dev_priv->sarea_priv;
	DRM_DEBUG( "%s\n", __func__ );

	LOCK_TEST_WITH_RETURN( dev, filp );

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	if ( sarea_priv->nbox > R128_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = R128_NR_SAREA_CLIPRECTS;

	if ( !dev_priv->page_flipping ) {
		r128_cce_dispatch_swap( dev );
		dev_priv->sarea_priv->dirty |= (R128_UPLOAD_CONTEXT |
						R128_UPLOAD_MASKS);
	} else {
		r128_cce_dispatch_flip( dev );
	}

	return 0;
}

int r128_cce_vertex( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_r128_buf_priv_t *buf_priv;
	drm_r128_vertex_t vertex;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __func__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( vertex, (drm_r128_vertex_t *) data,
			     sizeof(vertex) );

	DRM_DEBUG( "pid=%d index=%d count=%d discard=%d\n",
		   DRM_CURRENTPID,
		   vertex.idx, vertex.count, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}
	if ( vertex.prim < 0 ||
	     vertex.prim > R128_CCE_VC_CNTL_PRIM_TYPE_TRI_TYPE2 ) {
		DRM_ERROR( "buffer prim %d\n", vertex.prim );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[vertex.idx];
	buf_priv = buf->dev_private;

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return DRM_ERR(EINVAL);
	}

	buf->used = vertex.count;
	buf_priv->prim = vertex.prim;
	buf_priv->discard = vertex.discard;

	r128_cce_dispatch_vertex( dev, buf );

	return 0;
}

int r128_cce_indices( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_r128_buf_priv_t *buf_priv;
	drm_r128_indices_t elts;
	int count;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __func__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( elts, (drm_r128_indices_t *) data,
			     sizeof(elts) );

	DRM_DEBUG( "pid=%d buf=%d s=%d e=%d d=%d\n", DRM_CURRENTPID,
		   elts.idx, elts.start, elts.end, elts.discard );

	if ( elts.idx < 0 || elts.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   elts.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}
	if ( elts.prim < 0 ||
	     elts.prim > R128_CCE_VC_CNTL_PRIM_TYPE_TRI_TYPE2 ) {
		DRM_ERROR( "buffer prim %d\n", elts.prim );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[elts.idx];
	buf_priv = buf->dev_private;

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", elts.idx );
		return DRM_ERR(EINVAL);
	}

	count = (elts.end - elts.start) / sizeof(u16);
	elts.start -= R128_INDEX_PRIM_OFFSET;

	if ( elts.start & 0x7 ) {
		DRM_ERROR( "misaligned buffer 0x%x\n", elts.start );
		return DRM_ERR(EINVAL);
	}
	if ( elts.start < buf->used ) {
		DRM_ERROR( "no header 0x%x - 0x%x\n", elts.start, buf->used );
		return DRM_ERR(EINVAL);
	}

	buf->used = elts.end;
	buf_priv->prim = elts.prim;
	buf_priv->discard = elts.discard;

	r128_cce_dispatch_indices( dev, buf, elts.start, elts.end, count );

	return 0;
}

int r128_cce_blit( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_device_dma_t *dma = dev->dma;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_blit_t blit;

	LOCK_TEST_WITH_RETURN( dev, filp );

	DRM_COPY_FROM_USER_IOCTL( blit, (drm_r128_blit_t *) data,
			     sizeof(blit) );

	DRM_DEBUG( "pid=%d index=%d\n", DRM_CURRENTPID, blit.idx );

	if ( blit.idx < 0 || blit.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   blit.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	return r128_cce_dispatch_blit( filp, dev, &blit );
}

int r128_cce_depth( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_depth_t depth;

	LOCK_TEST_WITH_RETURN( dev, filp );

	DRM_COPY_FROM_USER_IOCTL( depth, (drm_r128_depth_t *) data,
			     sizeof(depth) );

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	switch ( depth.func ) {
	case R128_WRITE_SPAN:
		return r128_cce_dispatch_write_span( dev, &depth );
	case R128_WRITE_PIXELS:
		return r128_cce_dispatch_write_pixels( dev, &depth );
	case R128_READ_SPAN:
		return r128_cce_dispatch_read_span( dev, &depth );
	case R128_READ_PIXELS:
		return r128_cce_dispatch_read_pixels( dev, &depth );
	}

	return DRM_ERR(EINVAL);
}

int r128_cce_stipple( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_stipple_t stipple;
	u32 mask[32];

	LOCK_TEST_WITH_RETURN( dev, filp );

	DRM_COPY_FROM_USER_IOCTL( stipple, (drm_r128_stipple_t *) data,
			     sizeof(stipple) );

	if ( DRM_COPY_FROM_USER( &mask, stipple.mask,
			     32 * sizeof(u32) ) )
		return DRM_ERR( EFAULT );

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	r128_cce_dispatch_stipple( dev, mask );

	return 0;
}

int r128_cce_indirect( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_r128_buf_priv_t *buf_priv;
	drm_r128_indirect_t indirect;
#if 0
	RING_LOCALS;
#endif

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __func__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( indirect, (drm_r128_indirect_t *) data,
			     sizeof(indirect) );

	DRM_DEBUG( "indirect: idx=%d s=%d e=%d d=%d\n",
		   indirect.idx, indirect.start,
		   indirect.end, indirect.discard );

	if ( indirect.idx < 0 || indirect.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   indirect.idx, dma->buf_count - 1 );
		return DRM_ERR(EINVAL);
	}

	buf = dma->buflist[indirect.idx];
	buf_priv = buf->dev_private;

	if ( buf->filp != filp ) {
		DRM_ERROR( "process %d using buffer owned by %p\n",
			   DRM_CURRENTPID, buf->filp );
		return DRM_ERR(EINVAL);
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", indirect.idx );
		return DRM_ERR(EINVAL);
	}

	if ( indirect.start < buf->used ) {
		DRM_ERROR( "reusing indirect: start=0x%x actual=0x%x\n",
			   indirect.start, buf->used );
		return DRM_ERR(EINVAL);
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf->used = indirect.end;
	buf_priv->discard = indirect.discard;

#if 0
	/* Wait for the 3D stream to idle before the indirect buffer
	 * containing 2D acceleration commands is processed.
	 */
	BEGIN_RING( 2 );
	RADEON_WAIT_UNTIL_3D_IDLE();
	ADVANCE_RING();
#endif

	/* Dispatch the indirect buffer full of commands from the
	 * X server.  This is insecure and is thus only available to
	 * privileged clients.
	 */
	r128_cce_dispatch_indirect( dev, buf, indirect.start, indirect.end );

	return 0;
}

int r128_getparam( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_r128_private_t *dev_priv = dev->dev_private;
	drm_r128_getparam_t param;
	int value;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __func__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( param, (drm_r128_getparam_t *)data,
			     sizeof(param) );

	DRM_DEBUG( "pid=%d\n", DRM_CURRENTPID );

	switch( param.param ) {
	case R128_PARAM_IRQ_NR:
		value = dev->irq;
		break;
	default:
		return DRM_ERR(EINVAL);
	}

	if ( DRM_COPY_TO_USER( param.value, &value, sizeof(int) ) ) {
		DRM_ERROR( "copy_to_user\n" );
		return DRM_ERR(EFAULT);
	}
	
	return 0;
}
