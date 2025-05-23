/*	$NecBSD: bsfunc.h,v 1.1 1997/07/18 09:19:03 kmatsuda Exp $	*/
/*	$NetBSD$	*/
/*	$DragonFly: src/sys/dev/disk/i386/bs/Attic/bsfunc.h,v 1.5 2004/09/18 18:47:20 dillon Exp $ */
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

/**************************************************
 * FUNC
 **************************************************/
/* timeout */
void bstimeout (void *);

/* ctrl setup */
void bs_setup_ctrl (struct targ_info *, u_int, u_int);
struct targ_info *bs_init_target_info (struct bs_softc *, int);

/* msg op */
int bs_send_msg (struct targ_info *, u_int, struct msgbase *, int);
struct bsccb *bs_request_sense (struct targ_info *);

/* sync msg op */
int bs_start_syncmsg (struct targ_info *, struct bsccb *, int);
int bs_send_syncmsg (struct targ_info *);
int bs_analyze_syncmsg (struct targ_info *, struct bsccb *);

/* reset device */
void bs_scsibus_start (struct bs_softc *);
void bs_reset_nexus (struct bs_softc *);
struct bsccb *bs_force_abort (struct targ_info *);
void bs_reset_device (struct targ_info *);

/* ccb */
struct bsccb *bs_make_internal_ccb (struct targ_info *, u_int, u_int8_t *, u_int, u_int8_t *, u_int, u_int, int);
struct bsccb *bs_make_msg_ccb (struct targ_info *, u_int, struct bsccb *, struct msgbase *, u_int);

/* misc funcs */
void bs_printf (struct targ_info *, char *, char *);
void bs_panic (struct bs_softc *, u_char *);

/* misc debug */
u_int bsr (u_int);
u_int bsw (u_int, int);
void bs_debug_print_all (struct bs_softc *);
void bs_debug_print (struct bs_softc *, struct targ_info *);

/**************************************************
 * TARG FLAGS
 *************************************************/
static BS_INLINE int bs_check_sat (struct targ_info *);
static BS_INLINE int bs_check_smit (struct targ_info *);
static BS_INLINE int bs_check_disc (struct targ_info *);
static BS_INLINE int bs_check_link (struct targ_info *, struct bsccb *);
static BS_INLINE u_int8_t bs_identify_msg (struct targ_info *);
static BS_INLINE void bs_targ_flags (struct targ_info *, struct bsccb *);

static BS_INLINE int
bs_check_disc(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSDISC);
}

static BS_INLINE int
bs_check_sat(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSSAT);
}

static BS_INLINE int
bs_check_smit(ti)
	struct targ_info *ti;
{

	return (ti->ti_flags & BSSMIT);
}

static BS_INLINE int
bs_check_link(ti, cb)
	struct targ_info *ti;
	struct bsccb *cb;
{
	struct bsccb *nextcb;

	return ((ti->ti_flags & BSLINK) &&
		(nextcb = TAILQ_NEXT(cb, ccb_chain)) &&
		(nextcb->bsccb_flags & BSLINK));
}

static BS_INLINE u_int8_t
bs_identify_msg(ti)
	struct targ_info *ti;
{

	return ((bs_check_disc(ti) ? 0xc0 : 0x80) | ti->ti_lun);
}

static BS_INLINE void
bs_targ_flags(ti, cb)
	struct targ_info *ti;
	struct bsccb *cb;
{
	u_int cmf = (u_int) bshw_cmd[cb->cmd[0]];

	cb->bsccb_flags |= ((cmf & (BSSAT | BSSMIT | BSLINK)) | BSDISC);
	cb->bsccb_flags &= ti->ti_mflags;

	if (cb->datalen < DEV_BSIZE)
		cb->bsccb_flags &= ~BSSMIT;
	if (cb->bsccb_flags & BSFORCEIOPOLL)
		cb->bsccb_flags &= ~(BSLINK | BSSMIT | BSSAT | BSDISC);
}

/**************************************************
 * QUEUE OP
 **************************************************/
static BS_INLINE void bs_hostque_init (struct bs_softc *);
static BS_INLINE void bs_hostque_head (struct bs_softc *, struct targ_info *);
static BS_INLINE void bs_hostque_tail (struct bs_softc *, struct targ_info *);
static BS_INLINE void bs_hostque_delete (struct bs_softc *, struct targ_info *);

static BS_INLINE void
bs_hostque_init(bsc)
	struct bs_softc *bsc;
{

	TAILQ_INIT(&bsc->sc_sttab);
	TAILQ_INIT(&bsc->sc_titab);
}

static BS_INLINE void
bs_hostque_head(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED) {
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain);
	} else {
		ti->ti_flags |= BSQUEUED;
	}
	TAILQ_INSERT_HEAD(&bsc->sc_sttab, ti, ti_wchain);
}

static BS_INLINE void
bs_hostque_tail(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED) {
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain);
	} else {
		ti->ti_flags |= BSQUEUED;
	}
	TAILQ_INSERT_TAIL(&bsc->sc_sttab, ti, ti_wchain);
}

static BS_INLINE void
bs_hostque_delete(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	if (ti->ti_flags & BSQUEUED)
	{
		ti->ti_flags &= ~BSQUEUED;
		TAILQ_REMOVE(&bsc->sc_sttab, ti, ti_wchain);
	}
}

/*************************************************************
 * TIMEOUT
 ************************************************************/
static BS_INLINE void bs_start_timeout (struct bs_softc *);
static BS_INLINE void bs_terminate_timeout (struct bs_softc *);

static BS_INLINE void
bs_start_timeout(bsc)
	struct bs_softc *bsc;
{

	if ((bsc->sc_flags & BSSTARTTIMEOUT) == 0)
	{
		bsc->sc_flags |= BSSTARTTIMEOUT;
		callout_reset(&bsc->timeout_ch, BS_TIMEOUT_INTERVAL,
				bstimeout, bsc);
	}
}

static BS_INLINE void
bs_terminate_timeout(bsc)
	struct bs_softc *bsc;
{

	if (bsc->sc_flags & BSSTARTTIMEOUT)
	{
		callout_stop(&bsc->timeout_ch);
		bsc->sc_flags &= ~BSSTARTTIMEOUT;
	}
}
