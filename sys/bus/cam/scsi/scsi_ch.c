/*
 * Copyright (c) 1997 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 1999 Kenneth D. Merry.
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
 * $FreeBSD: src/sys/cam/scsi/scsi_ch.c,v 1.20.2.2 2000/10/31 08:09:49 dwmalone Exp $
 * $DragonFly: src/sys/bus/cam/scsi/scsi_ch.c,v 1.10 2004/05/19 22:52:38 dillon Exp $
 */
/*
 * Derived from the NetBSD SCSI changer driver.
 *
 *	$NetBSD: ch.c,v 1.32 1998/01/12 09:49:12 thorpej Exp $
 *
 */
/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe <thorpej@and.com>
 * All rights reserved.
 *
 * Partially based on an autochanger driver written by Stefan Grefen
 * and on an autochanger driver written by the Systems Programming Group
 * at the University of Utah Computer Science Department.
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
 *    must display the following acknowledgements:
 *	This product includes software developed by Jason R. Thorpe
 *	for And Communications, http://www.and.com/
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/chio.h>
#include <sys/errno.h>
#include <sys/devicestat.h>

#include "../cam.h"
#include "../cam_ccb.h"
#include "../cam_extend.h"
#include "../cam_periph.h"
#include "../cam_xpt_periph.h"
#include "../cam_queue.h"
#include "../cam_debug.h"

#include "scsi_all.h"
#include "scsi_message.h"
#include "scsi_ch.h"

/*
 * Timeout definitions for various changer related commands.  They may
 * be too short for some devices (especially the timeout for INITIALIZE
 * ELEMENT STATUS).
 */

static const u_int32_t	CH_TIMEOUT_MODE_SENSE                = 6000;
static const u_int32_t	CH_TIMEOUT_MOVE_MEDIUM               = 100000;
static const u_int32_t	CH_TIMEOUT_EXCHANGE_MEDIUM           = 100000;
static const u_int32_t	CH_TIMEOUT_POSITION_TO_ELEMENT       = 100000;
static const u_int32_t	CH_TIMEOUT_READ_ELEMENT_STATUS       = 10000;
static const u_int32_t	CH_TIMEOUT_SEND_VOLTAG		     = 10000;
static const u_int32_t	CH_TIMEOUT_INITIALIZE_ELEMENT_STATUS = 500000;

typedef enum {
	CH_FLAG_INVALID		= 0x001,
	CH_FLAG_OPEN		= 0x002
} ch_flags;

typedef enum {
	CH_STATE_PROBE,
	CH_STATE_NORMAL
} ch_state;

typedef enum {
	CH_CCB_PROBE,
	CH_CCB_WAITING
} ch_ccb_types;

typedef enum {
	CH_Q_NONE	= 0x00,
	CH_Q_NO_DBD	= 0x01
} ch_quirks;

#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct scsi_mode_sense_data {
	struct scsi_mode_header_6 header;
	struct scsi_mode_blk_desc blk_desc;
	union {
		struct page_element_address_assignment ea;
		struct page_transport_geometry_parameters tg;
		struct page_device_capabilities cap;
	} pages;
};

struct ch_softc {
	ch_flags	flags;
	ch_state	state;
	ch_quirks	quirks;
	union ccb	saved_ccb;
	struct devstat	device_stats;

	int		sc_picker;	/* current picker */

	/*
	 * The following information is obtained from the
	 * element address assignment page.
	 */
	int		sc_firsts[4];	/* firsts, indexed by CHET_* */
	int		sc_counts[4];	/* counts, indexed by CHET_* */

	/*
	 * The following mask defines the legal combinations
	 * of elements for the MOVE MEDIUM command.
	 */
	u_int8_t	sc_movemask[4];

	/*
	 * As above, but for EXCHANGE MEDIUM.
	 */
	u_int8_t	sc_exchangemask[4];

	/*
	 * Quirks; see below.  XXX KDM not implemented yet
	 */
	int		sc_settledelay;	/* delay for settle */
};

#define CHUNIT(x)       (minor((x)))
#define CH_CDEV_MAJOR	17

static	d_open_t	chopen;
static	d_close_t	chclose;
static	d_ioctl_t	chioctl;
static	periph_init_t	chinit;
static  periph_ctor_t	chregister;
static	periph_oninv_t	choninvalidate;
static  periph_dtor_t   chcleanup;
static  periph_start_t  chstart;
static	void		chasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		chdone(struct cam_periph *periph,
			       union ccb *done_ccb);
static	int		cherror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static	int		chmove(struct cam_periph *periph,
			       struct changer_move *cm);
static	int		chexchange(struct cam_periph *periph,
				   struct changer_exchange *ce);
static	int		chposition(struct cam_periph *periph,
				   struct changer_position *cp);
static	int		chgetelemstatus(struct cam_periph *periph,
				struct changer_element_status_request *csr);
static	int		chsetvoltag(struct cam_periph *periph,
				    struct changer_set_voltag_request *csvr);
static	int		chielem(struct cam_periph *periph, 
				unsigned int timeout);
static	int		chgetparams(struct cam_periph *periph);

static struct periph_driver chdriver =
{
	chinit, "ch",
	TAILQ_HEAD_INITIALIZER(chdriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, chdriver);

static struct cdevsw ch_cdevsw = {
	/* name */	"ch",
	/* maj */	CH_CDEV_MAJOR,
	/* flags */	0,
	/* port */	NULL,
	/* clone */     NULL,

	/* open */	chopen,
	/* close */	chclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	chioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};

static struct extend_array *chperiphs;

void
chinit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	chperiphs = cam_extend_new();
	if (chperiphs == NULL) {
		printf("ch: Failed to alloc extend array!\n");
		return;
	}

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

                xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
                csa.ccb_h.func_code = XPT_SASYNC_CB;
                csa.event_enable = AC_FOUND_DEVICE;
                csa.callback = chasync;
                csa.callback_arg = NULL;
                xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
                xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("ch: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

static void
choninvalidate(struct cam_periph *periph)
{
	struct ch_softc *softc;
	struct ccb_setasync csa;

	softc = (struct ch_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path,
		      /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = chasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	softc->flags |= CH_FLAG_INVALID;

	xpt_print_path(periph->path);
	printf("lost device\n");

}

static void
chcleanup(struct cam_periph *periph)
{
	struct ch_softc *softc;

	softc = (struct ch_softc *)periph->softc;

	devstat_remove_entry(&softc->device_stats);
	cam_extend_release(chperiphs, periph->unit_number);
	xpt_print_path(periph->path);
	printf("removing device entry\n");
	cdevsw_remove(&ch_cdevsw, -1, periph->unit_number);
	free(softc, M_DEVBUF);
}

static void
chasync(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;

	switch(code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;

		cgd = (struct ccb_getdev *)arg;

		if (SID_TYPE(&cgd->inq_data)!= T_CHANGER)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(chregister, choninvalidate,
					  chcleanup, chstart, "ch",
					  CAM_PERIPH_BIO, cgd->ccb_h.path,
					  chasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("chasync: Unable to probe new device "
			       "due to status 0x%x\n", status);

		break;

	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
chregister(struct cam_periph *periph, void *arg)
{
	struct ch_softc *softc;
	struct ccb_setasync csa;
	struct ccb_getdev *cgd;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("chregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("chregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = malloc(sizeof(*softc), M_DEVBUF, M_INTWAIT | M_ZERO);
	softc->state = CH_STATE_PROBE;
	periph->softc = softc;
	cam_extend_set(chperiphs, periph->unit_number, periph);
	softc->quirks = CH_Q_NONE;

	/*
	 * Changers don't have a blocksize, and obviously don't support
	 * tagged queueing.
	 */
	devstat_add_entry(&softc->device_stats, "ch",
			  periph->unit_number, 0,
			  DEVSTAT_NO_BLOCKSIZE | DEVSTAT_NO_ORDERED_TAGS,
			  SID_TYPE(&cgd->inq_data)| DEVSTAT_TYPE_IF_SCSI,
			  DEVSTAT_PRIORITY_OTHER);

	/* Register the device */
	cdevsw_add(&ch_cdevsw, -1, periph->unit_number);
	make_dev(&ch_cdevsw, periph->unit_number, UID_ROOT,
		  GID_OPERATOR, 0600, "%s%d", periph->periph_name,
		  periph->unit_number);

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = chasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	/*
	 * Lock this peripheral until we are setup.
	 * This first call can't block
	 */
	(void)cam_periph_lock(periph, 0);
	xpt_schedule(periph, /*priority*/5);

	return(CAM_REQ_CMP);
}

static int
chopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct ch_softc *softc;
	int unit, error;
	int s;

	unit = CHUNIT(dev);
	periph = cam_extend_get(chperiphs, unit);

	if (periph == NULL)
		return(ENXIO);

	softc = (struct ch_softc *)periph->softc;

	s = splsoftcam();
	if (softc->flags & CH_FLAG_INVALID) {
		splx(s);
		return(ENXIO);
	}

	if ((error = cam_periph_lock(periph, PCATCH)) != 0) {
		splx(s);
		return (error);
	}
	
	splx(s);

	if ((softc->flags & CH_FLAG_OPEN) == 0) {
		if (cam_periph_acquire(periph) != CAM_REQ_CMP)
			return(ENXIO);
		softc->flags |= CH_FLAG_OPEN;
	}

	/*
	 * Load information about this changer device into the softc.
	 */
	if ((error = chgetparams(periph)) != 0) {
		softc->flags &= ~CH_FLAG_OPEN;
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return(error);
	}

	cam_periph_unlock(periph);

	return(error);
}

static int
chclose(dev_t dev, int flag, int fmt, struct thread *td)
{
	struct	cam_periph *periph;
	struct	ch_softc *softc;
	int	unit, error;

	error = 0;

	unit = CHUNIT(dev);
	periph = cam_extend_get(chperiphs, unit);
	if (periph == NULL)
		return(ENXIO);

	softc = (struct ch_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, 0)) != 0)
		return(error);

	softc->flags &= ~CH_FLAG_OPEN;

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return(0);
}

static void
chstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct ch_softc *softc;
	int s;

	softc = (struct ch_softc *)periph->softc;

	switch (softc->state) {
	case CH_STATE_NORMAL:
	{
		s = splbio();
		if (periph->immediate_priority <= periph->pinfo.priority){
			start_ccb->ccb_h.ccb_state = CH_CCB_WAITING;

			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			splx(s);
			wakeup(&periph->ccb_list);
		} else
			splx(s);
		break;
	}
	case CH_STATE_PROBE:
	{
		int mode_buffer_len;
		void *mode_buffer;

		/*
		 * Include the block descriptor when calculating the mode
		 * buffer length,
		 */
		mode_buffer_len = sizeof(struct scsi_mode_header_6) +
				  sizeof(struct scsi_mode_blk_desc) +
				 sizeof(struct page_element_address_assignment);

		mode_buffer = malloc(mode_buffer_len, M_TEMP, M_INTWAIT | M_ZERO);
		/*
		 * Get the element address assignment page.
		 */
		scsi_mode_sense(&start_ccb->csio,
				/* retries */ 1,
				/* cbfcnp */ chdone,
				/* tag_action */ MSG_SIMPLE_Q_TAG,
				/* dbd */ (softc->quirks & CH_Q_NO_DBD) ?
					FALSE : TRUE,
				/* page_code */ SMS_PAGE_CTRL_CURRENT,
				/* page */ CH_ELEMENT_ADDR_ASSIGN_PAGE,
				/* param_buf */ (u_int8_t *)mode_buffer,
				/* param_len */ mode_buffer_len,
				/* sense_len */ SSD_FULL_SIZE,
				/* timeout */ CH_TIMEOUT_MODE_SENSE);

		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = CH_CCB_PROBE;
		xpt_action(start_ccb);
		break;
	}
	}
}

static void
chdone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct ch_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct ch_softc *)periph->softc;
	csio = &done_ccb->csio;

	switch(done_ccb->ccb_h.ccb_state) {
	case CH_CCB_PROBE:
	{
		struct scsi_mode_header_6 *mode_header;
		struct page_element_address_assignment *ea;
		char announce_buf[80];


		mode_header = (struct scsi_mode_header_6 *)csio->data_ptr;

		ea = (struct page_element_address_assignment *)
			find_mode_page_6(mode_header);

		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP){
			
			softc->sc_firsts[CHET_MT] = scsi_2btoul(ea->mtea);
			softc->sc_counts[CHET_MT] = scsi_2btoul(ea->nmte);
			softc->sc_firsts[CHET_ST] = scsi_2btoul(ea->fsea);
			softc->sc_counts[CHET_ST] = scsi_2btoul(ea->nse);
			softc->sc_firsts[CHET_IE] = scsi_2btoul(ea->fieea);
			softc->sc_counts[CHET_IE] = scsi_2btoul(ea->niee);
			softc->sc_firsts[CHET_DT] = scsi_2btoul(ea->fdtea);
			softc->sc_counts[CHET_DT] = scsi_2btoul(ea->ndte);
			softc->sc_picker = softc->sc_firsts[CHET_MT];

#define PLURAL(c)	(c) == 1 ? "" : "s"
			snprintf(announce_buf, sizeof(announce_buf),
				"%d slot%s, %d drive%s, "
				"%d picker%s, %d portal%s",
		    		softc->sc_counts[CHET_ST],
				PLURAL(softc->sc_counts[CHET_ST]),
		    		softc->sc_counts[CHET_DT],
				PLURAL(softc->sc_counts[CHET_DT]),
		    		softc->sc_counts[CHET_MT],
				PLURAL(softc->sc_counts[CHET_MT]),
		    		softc->sc_counts[CHET_IE],
				PLURAL(softc->sc_counts[CHET_IE]));
#undef PLURAL
		} else {
			int error;

			error = cherror(done_ccb, 0, SF_RETRY_UA |
					SF_NO_PRINT | SF_RETRY_SELTO);
			/*
			 * Retry any UNIT ATTENTION type errors.  They
			 * are expected at boot.
			 */
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			} else if (error != 0) {
				int retry_scheduled;
				struct scsi_mode_sense_6 *sms;

				sms = (struct scsi_mode_sense_6 *)
					done_ccb->csio.cdb_io.cdb_bytes;

				/*
				 * Check to see if block descriptors were
				 * disabled.  Some devices don't like that.
				 * We're taking advantage of the fact that
				 * the first few bytes of the 6 and 10 byte
				 * mode sense commands are the same.  If
				 * block descriptors were disabled, enable
				 * them and re-send the command.
				 */
				if (sms->byte2 & SMS_DBD) {
					sms->byte2 &= ~SMS_DBD;
					xpt_action(done_ccb);
					softc->quirks |= CH_Q_NO_DBD;
					retry_scheduled = 1;
				} else
					retry_scheduled = 0;

				/* Don't wedge this device's queue */
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);

				if (retry_scheduled)
					return;

				if ((done_ccb->ccb_h.status & CAM_STATUS_MASK)
				    == CAM_SCSI_STATUS_ERROR) 
					scsi_sense_print(&done_ccb->csio);
				else {
					xpt_print_path(periph->path);
					printf("got CAM status %#x\n",
					       done_ccb->ccb_h.status);
				}
				xpt_print_path(periph->path);
				printf("fatal error, failed to attach to"
				       " device\n");

				cam_periph_invalidate(periph);

				announce_buf[0] = '\0';
			}
		}
		if (announce_buf[0] != '\0')
			xpt_announce_periph(periph, announce_buf);
		softc->state = CH_STATE_NORMAL;
		free(mode_header, M_TEMP);
		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the probe lock on the peripheral.
		 * The peripheral will only go away once the last lock
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		xpt_release_ccb(done_ccb);
		cam_periph_unlock(periph);
		return;
	}
	case CH_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
cherror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct ch_softc *softc;
	struct cam_periph *periph;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct ch_softc *)periph->softc;

	return (cam_periph_error(ccb, cam_flags, sense_flags,
				 &softc->saved_ccb));
}

static int
chioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct cam_periph *periph;
	struct ch_softc *softc;
	u_int8_t unit;
	int error;

	unit = CHUNIT(dev);

	periph = cam_extend_get(chperiphs, unit);
	if (periph == NULL)
		return(ENXIO);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("entering chioctl\n"));

	softc = (struct ch_softc *)periph->softc;

	error = 0;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, 
		  ("trying to do ioctl %#lx\n", cmd));

	/*
	 * If this command can change the device's state, we must
	 * have the device open for writing.
	 */
	switch (cmd) {
	case CHIOGPICKER:
	case CHIOGPARAMS:
	case CHIOGSTATUS:
		break;

	default:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case CHIOMOVE:
		error = chmove(periph, (struct changer_move *)addr);
		break;

	case CHIOEXCHANGE:
		error = chexchange(periph, (struct changer_exchange *)addr);
		break;

	case CHIOPOSITION:
		error = chposition(periph, (struct changer_position *)addr);
		break;

	case CHIOGPICKER:
		*(int *)addr = softc->sc_picker - softc->sc_firsts[CHET_MT];
		break;

	case CHIOSPICKER:
	{
		int new_picker = *(int *)addr;

		if (new_picker > (softc->sc_counts[CHET_MT] - 1))
			return (EINVAL);
		softc->sc_picker = softc->sc_firsts[CHET_MT] + new_picker;
		break;
	}
	case CHIOGPARAMS:
	{
		struct changer_params *cp = (struct changer_params *)addr;

		cp->cp_npickers = softc->sc_counts[CHET_MT];
		cp->cp_nslots = softc->sc_counts[CHET_ST];
		cp->cp_nportals = softc->sc_counts[CHET_IE];
		cp->cp_ndrives = softc->sc_counts[CHET_DT];
		break;
	}
	case CHIOIELEM:
		error = chielem(periph, *(unsigned int *)addr);
		break;

	case CHIOGSTATUS:
	{
		error = chgetelemstatus(periph,
			       (struct changer_element_status_request *) addr);
		break;
	}

	case CHIOSETVOLTAG:
	{
		error = chsetvoltag(periph,
				    (struct changer_set_voltag_request *) addr);
		break;
	}

	/* Implement prevent/allow? */

	default:
		error = cam_periph_ioctl(periph, cmd, addr, cherror);
		break;
	}

	return (error);
}

static int
chmove(struct cam_periph *periph, struct changer_move *cm)
{
	struct ch_softc *softc;
	u_int16_t fromelem, toelem;
	union ccb *ccb;
	int error;

	error = 0;
	softc = (struct ch_softc *)periph->softc;

	/*
	 * Check arguments.
	 */
	if ((cm->cm_fromtype > CHET_DT) || (cm->cm_totype > CHET_DT))
		return (EINVAL);
	if ((cm->cm_fromunit > (softc->sc_counts[cm->cm_fromtype] - 1)) ||
	    (cm->cm_tounit > (softc->sc_counts[cm->cm_totype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if ((softc->sc_movemask[cm->cm_fromtype] & (1 << cm->cm_totype)) == 0)
		return (ENODEV);

	/*
	 * Calculate the source and destination elements.
	 */
	fromelem = softc->sc_firsts[cm->cm_fromtype] + cm->cm_fromunit;
	toelem = softc->sc_firsts[cm->cm_totype] + cm->cm_tounit;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_move_medium(&ccb->csio,
			 /* retries */ 1,
			 /* cbfcnp */ chdone,
			 /* tag_action */ MSG_SIMPLE_Q_TAG,
			 /* tea */ softc->sc_picker,
			 /* src */ fromelem,
			 /* dst */ toelem,
			 /* invert */ (cm->cm_flags & CM_INVERT) ? TRUE : FALSE,
			 /* sense_len */ SSD_FULL_SIZE,
			 /* timeout */ CH_TIMEOUT_MOVE_MEDIUM);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	xpt_release_ccb(ccb);

	return(error);
}

static int
chexchange(struct cam_periph *periph, struct changer_exchange *ce)
{
	struct ch_softc *softc;
	u_int16_t src, dst1, dst2;
	union ccb *ccb;
	int error;

	error = 0;
	softc = (struct ch_softc *)periph->softc;
	/*
	 * Check arguments.
	 */
	if ((ce->ce_srctype > CHET_DT) || (ce->ce_fdsttype > CHET_DT) ||
	    (ce->ce_sdsttype > CHET_DT))
		return (EINVAL);
	if ((ce->ce_srcunit > (softc->sc_counts[ce->ce_srctype] - 1)) ||
	    (ce->ce_fdstunit > (softc->sc_counts[ce->ce_fdsttype] - 1)) ||
	    (ce->ce_sdstunit > (softc->sc_counts[ce->ce_sdsttype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if (((softc->sc_exchangemask[ce->ce_srctype] &
	     (1 << ce->ce_fdsttype)) == 0) ||
	    ((softc->sc_exchangemask[ce->ce_fdsttype] &
	     (1 << ce->ce_sdsttype)) == 0))
		return (ENODEV);

	/*
	 * Calculate the source and destination elements.
	 */
	src = softc->sc_firsts[ce->ce_srctype] + ce->ce_srcunit;
	dst1 = softc->sc_firsts[ce->ce_fdsttype] + ce->ce_fdstunit;
	dst2 = softc->sc_firsts[ce->ce_sdsttype] + ce->ce_sdstunit;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_exchange_medium(&ccb->csio,
			     /* retries */ 1,
			     /* cbfcnp */ chdone,
			     /* tag_action */ MSG_SIMPLE_Q_TAG,
			     /* tea */ softc->sc_picker,
			     /* src */ src,
			     /* dst1 */ dst1,
			     /* dst2 */ dst2,
			     /* invert1 */ (ce->ce_flags & CE_INVERT1) ?
			                   TRUE : FALSE,
			     /* invert2 */ (ce->ce_flags & CE_INVERT2) ?
			                   TRUE : FALSE,
			     /* sense_len */ SSD_FULL_SIZE,
			     /* timeout */ CH_TIMEOUT_EXCHANGE_MEDIUM);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	xpt_release_ccb(ccb);

	return(error);
}

static int
chposition(struct cam_periph *periph, struct changer_position *cp)
{
	struct ch_softc *softc;
	u_int16_t dst;
	union ccb *ccb;
	int error;

	error = 0;
	softc = (struct ch_softc *)periph->softc;

	/*
	 * Check arguments.
	 */
	if (cp->cp_type > CHET_DT)
		return (EINVAL);
	if (cp->cp_unit > (softc->sc_counts[cp->cp_type] - 1))
		return (ENODEV);

	/*
	 * Calculate the destination element.
	 */
	dst = softc->sc_firsts[cp->cp_type] + cp->cp_unit;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_position_to_element(&ccb->csio,
				 /* retries */ 1,
				 /* cbfcnp */ chdone,
				 /* tag_action */ MSG_SIMPLE_Q_TAG,
				 /* tea */ softc->sc_picker,
				 /* dst */ dst,
				 /* invert */ (cp->cp_flags & CP_INVERT) ?
					      TRUE : FALSE,
				 /* sense_len */ SSD_FULL_SIZE,
				 /* timeout */ CH_TIMEOUT_POSITION_TO_ELEMENT);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	xpt_release_ccb(ccb);

	return(error);
}

/*
 * Copy a volume tag to a volume_tag struct, converting SCSI byte order
 * to host native byte order in the volume serial number.  The volume
 * label as returned by the changer is transferred to user mode as
 * nul-terminated string.  Volume labels are truncated at the first
 * space, as suggested by SCSI-2.
 */
static	void
copy_voltag(struct changer_voltag *uvoltag, struct volume_tag *voltag)
{
	int i;
	for (i=0; i<CH_VOLTAG_MAXLEN; i++) {
		char c = voltag->vif[i];
		if (c && c != ' ')
			uvoltag->cv_volid[i] = c;
	        else
			break;
	}
	uvoltag->cv_serial = scsi_2btoul(voltag->vsn);
}

/*
 * Copy an an element status descriptor to a user-mode
 * changer_element_status structure.
 */

static	void
copy_element_status(struct ch_softc *softc,
		    u_int16_t flags,
		    struct read_element_status_descriptor *desc,
		    struct changer_element_status *ces)
{
	u_int16_t eaddr = scsi_2btoul(desc->eaddr);
	u_int16_t et;

	ces->ces_int_addr = eaddr;
	/* set up logical address in element status */
	for (et = CHET_MT; et <= CHET_DT; et++) {
		if ((softc->sc_firsts[et] <= eaddr)
		    && ((softc->sc_firsts[et] + softc->sc_counts[et])
			> eaddr)) {
			ces->ces_addr = eaddr - softc->sc_firsts[et];
			ces->ces_type = et;
			break;
		}
	}

	ces->ces_flags = desc->flags1;

	ces->ces_sensecode = desc->sense_code;
	ces->ces_sensequal = desc->sense_qual;

	if (desc->flags2 & READ_ELEMENT_STATUS_INVERT)
		ces->ces_flags |= CES_INVERT;

	if (desc->flags2 & READ_ELEMENT_STATUS_SVALID) {

		eaddr = scsi_2btoul(desc->ssea);

		/* convert source address to logical format */
		for (et = CHET_MT; et <= CHET_DT; et++) {
			if ((softc->sc_firsts[et] <= eaddr)
			    && ((softc->sc_firsts[et] + softc->sc_counts[et])
				> eaddr)) {
				ces->ces_source_addr = 
					eaddr - softc->sc_firsts[et];
				ces->ces_source_type = et;
				ces->ces_flags |= CES_SOURCE_VALID;
				break;
			}
		}

		if (!(ces->ces_flags & CES_SOURCE_VALID))
			printf("ch: warning: could not map element source "
			       "address %ud to a valid element type\n",
			       eaddr);
	}
			

	if (flags & READ_ELEMENT_STATUS_PVOLTAG)
		copy_voltag(&(ces->ces_pvoltag), &(desc->pvoltag));
	if (flags & READ_ELEMENT_STATUS_AVOLTAG)
		copy_voltag(&(ces->ces_avoltag), &(desc->avoltag));

	if (desc->dt_scsi_flags & READ_ELEMENT_STATUS_DT_IDVALID) {
		ces->ces_flags |= CES_SCSIID_VALID;
		ces->ces_scsi_id = desc->dt_scsi_addr;
	}

	if (desc->dt_scsi_addr & READ_ELEMENT_STATUS_DT_LUVALID) {
		ces->ces_flags |= CES_LUN_VALID;
		ces->ces_scsi_lun = 
			desc->dt_scsi_flags & READ_ELEMENT_STATUS_DT_LUNMASK;
	}
}

static int
chgetelemstatus(struct cam_periph *periph, 
		struct changer_element_status_request *cesr)
{
	struct read_element_status_header *st_hdr;
	struct read_element_status_page_header *pg_hdr;
	struct read_element_status_descriptor *desc;
	caddr_t data = NULL;
	size_t size, desclen;
	int avail, i, error = 0;
	struct changer_element_status *user_data = NULL;
	struct ch_softc *softc;
	union ccb *ccb;
	int chet = cesr->cesr_element_type;
	int want_voltags = (cesr->cesr_flags & CESR_VOLTAGS) ? 1 : 0;

	softc = (struct ch_softc *)periph->softc;

	/* perform argument checking */

	/*
	 * Perform a range check on the cesr_element_{base,count}
	 * request argument fields.
	 */
	if ((softc->sc_counts[chet] - cesr->cesr_element_base) <= 0
	    || (cesr->cesr_element_base + cesr->cesr_element_count)
	        > softc->sc_counts[chet])
		return (EINVAL);

	/*
	 * Request one descriptor for the given element type.  This
	 * is used to determine the size of the descriptor so that
	 * we can allocate enough storage for all of them.  We assume
	 * that the first one can fit into 1k.
	 */
	data = (caddr_t)malloc(1024, M_DEVBUF, M_INTWAIT);

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_read_element_status(&ccb->csio,
				 /* retries */ 1,
				 /* cbfcnp */ chdone,
				 /* tag_action */ MSG_SIMPLE_Q_TAG,
				 /* voltag */ want_voltags,
				 /* sea */ softc->sc_firsts[chet],
				 /* count */ 1,
				 /* data_ptr */ data,
				 /* dxfer_len */ 1024,
				 /* sense_len */ SSD_FULL_SIZE,
				 /* timeout */ CH_TIMEOUT_READ_ELEMENT_STATUS);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	if (error)
		goto done;

	st_hdr = (struct read_element_status_header *)data;
	pg_hdr = (struct read_element_status_page_header *)((uintptr_t)st_hdr +
		  sizeof(struct read_element_status_header));
	desclen = scsi_2btoul(pg_hdr->edl);

	size = sizeof(struct read_element_status_header) +
	       sizeof(struct read_element_status_page_header) +
	       (desclen * cesr->cesr_element_count);

	/*
	 * Reallocate storage for descriptors and get them from the
	 * device.
	 */
	free(data, M_DEVBUF);
	data = (caddr_t)malloc(size, M_DEVBUF, M_INTWAIT);

	scsi_read_element_status(&ccb->csio,
				 /* retries */ 1,
				 /* cbfcnp */ chdone,
				 /* tag_action */ MSG_SIMPLE_Q_TAG,
				 /* voltag */ want_voltags,
				 /* sea */ softc->sc_firsts[chet]
				 + cesr->cesr_element_base,
				 /* count */ cesr->cesr_element_count,
				 /* data_ptr */ data,
				 /* dxfer_len */ size,
				 /* sense_len */ SSD_FULL_SIZE,
				 /* timeout */ CH_TIMEOUT_READ_ELEMENT_STATUS);
	
	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	if (error)
		goto done;

	/*
	 * Fill in the user status array.
	 */
	st_hdr = (struct read_element_status_header *)data;
	avail = scsi_2btoul(st_hdr->count);

	if (avail != cesr->cesr_element_count) {
		xpt_print_path(periph->path);
		printf("warning, READ ELEMENT STATUS avail != count\n");
	}

	user_data = (struct changer_element_status *)
		malloc(avail * sizeof(struct changer_element_status),
		       M_DEVBUF, M_INTWAIT | M_ZERO);

	desc = (struct read_element_status_descriptor *)((uintptr_t)data +
		sizeof(struct read_element_status_header) +
		sizeof(struct read_element_status_page_header));
	/*
	 * Set up the individual element status structures
	 */
	for (i = 0; i < avail; ++i) {
		struct changer_element_status *ces = &(user_data[i]);

		copy_element_status(softc, pg_hdr->flags, desc, ces);

		desc = (struct read_element_status_descriptor *)
		       ((uintptr_t)desc + desclen);
	}

	/* Copy element status structures out to userspace. */
	error = copyout(user_data,
			cesr->cesr_element_status,
			avail * sizeof(struct changer_element_status));

 done:
	xpt_release_ccb(ccb);

	if (data != NULL)
		free(data, M_DEVBUF);
	if (user_data != NULL)
		free(user_data, M_DEVBUF);

	return (error);
}

static int
chielem(struct cam_periph *periph,
	unsigned int timeout)
{
	union ccb *ccb;
	struct ch_softc *softc;
	int error;

	if (!timeout) {
		timeout = CH_TIMEOUT_INITIALIZE_ELEMENT_STATUS;
	} else {
		timeout *= 1000;
	}

	error = 0;
	softc = (struct ch_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_initialize_element_status(&ccb->csio,
				      /* retries */ 1,
				      /* cbfcnp */ chdone,
				      /* tag_action */ MSG_SIMPLE_Q_TAG,
				      /* sense_len */ SSD_FULL_SIZE,
				      /* timeout */ timeout);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	xpt_release_ccb(ccb);

	return(error);
}

static int
chsetvoltag(struct cam_periph *periph,
	    struct changer_set_voltag_request *csvr)
{
	union ccb *ccb;
	struct ch_softc *softc;
	u_int16_t ea;
	u_int8_t sac;
	struct scsi_send_volume_tag_parameters ssvtp;
	int error;
	int i;

	error = 0;
	softc = (struct ch_softc *)periph->softc;

	bzero(&ssvtp, sizeof(ssvtp));
	for (i=0; i<sizeof(ssvtp.vitf); i++) {
		ssvtp.vitf[i] = ' ';
	}

	/*
	 * Check arguments.
	 */
	if (csvr->csvr_type > CHET_DT)
		return EINVAL;
	if (csvr->csvr_addr > (softc->sc_counts[csvr->csvr_type] - 1))
		return ENODEV;

	ea = softc->sc_firsts[csvr->csvr_type] + csvr->csvr_addr;

	if (csvr->csvr_flags & CSVR_ALTERNATE) {
		switch (csvr->csvr_flags & CSVR_MODE_MASK) {
		case CSVR_MODE_SET:
			sac = SEND_VOLUME_TAG_ASSERT_ALTERNATE;
			break;
		case CSVR_MODE_REPLACE:
			sac = SEND_VOLUME_TAG_REPLACE_ALTERNATE;
			break;
		case CSVR_MODE_CLEAR:
			sac = SEND_VOLUME_TAG_UNDEFINED_ALTERNATE;
			break;
		default:
			error = EINVAL;
			goto out;
		}
	} else {
		switch (csvr->csvr_flags & CSVR_MODE_MASK) {
		case CSVR_MODE_SET:
			sac = SEND_VOLUME_TAG_ASSERT_PRIMARY;
			break;
		case CSVR_MODE_REPLACE:
			sac = SEND_VOLUME_TAG_REPLACE_PRIMARY;
			break;
		case CSVR_MODE_CLEAR:
			sac = SEND_VOLUME_TAG_UNDEFINED_PRIMARY;
			break;
		default:
			error = EINVAL;
			goto out;
		}
	}

	memcpy(ssvtp.vitf, csvr->csvr_voltag.cv_volid,
	       min(strlen(csvr->csvr_voltag.cv_volid), sizeof(ssvtp.vitf)));
	scsi_ulto2b(csvr->csvr_voltag.cv_serial, ssvtp.minvsn);

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	scsi_send_volume_tag(&ccb->csio,
			     /* retries */ 1,
			     /* cbfcnp */ chdone,
			     /* tag_action */ MSG_SIMPLE_Q_TAG,
			     /* element_address */ ea,
			     /* send_action_code */ sac,
			     /* parameters */ &ssvtp,
			     /* sense_len */ SSD_FULL_SIZE,
			     /* timeout */ CH_TIMEOUT_SEND_VOLTAG);
	
	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /*sense_flags*/ SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	xpt_release_ccb(ccb);

 out:
	return error;
}

static int
chgetparams(struct cam_periph *periph)
{
	union ccb *ccb;
	struct ch_softc *softc;
	void *mode_buffer;
	int mode_buffer_len;
	struct page_element_address_assignment *ea;
	struct page_device_capabilities *cap;
	int error, from, dbd;
	u_int8_t *moves, *exchanges;

	error = 0;

	softc = (struct ch_softc *)periph->softc;

	ccb = cam_periph_getccb(periph, /*priority*/ 1);

	/*
	 * The scsi_mode_sense_data structure is just a convenience
	 * structure that allows us to easily calculate the worst-case
	 * storage size of the mode sense buffer.
	 */
	mode_buffer_len = sizeof(struct scsi_mode_sense_data);

	mode_buffer = malloc(mode_buffer_len, M_TEMP, M_INTWAIT | M_ZERO);

	if (softc->quirks & CH_Q_NO_DBD)
		dbd = FALSE;
	else
		dbd = TRUE;

	/*
	 * Get the element address assignment page.
	 */
	scsi_mode_sense(&ccb->csio,
			/* retries */ 1,
			/* cbfcnp */ chdone,
			/* tag_action */ MSG_SIMPLE_Q_TAG,
			/* dbd */ dbd,
			/* page_code */ SMS_PAGE_CTRL_CURRENT,
			/* page */ CH_ELEMENT_ADDR_ASSIGN_PAGE,
			/* param_buf */ (u_int8_t *)mode_buffer,
			/* param_len */ mode_buffer_len,
			/* sense_len */ SSD_FULL_SIZE,
			/* timeout */ CH_TIMEOUT_MODE_SENSE);

	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /* sense_flags */ SF_RETRY_UA |
				  SF_NO_PRINT | SF_RETRY_SELTO,
				  &softc->device_stats);

	if (error) {
		if (dbd) {
			struct scsi_mode_sense_6 *sms;

			sms = (struct scsi_mode_sense_6 *)
				ccb->csio.cdb_io.cdb_bytes;

			sms->byte2 &= ~SMS_DBD;
			error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  		  /*sense_flags*/ SF_RETRY_UA |
						  SF_RETRY_SELTO,
						  &softc->device_stats);
		} else {
			/*
			 * Since we disabled sense printing above, print
			 * out the sense here since we got an error.
			 */
			scsi_sense_print(&ccb->csio);
		}

		if (error) {
			xpt_print_path(periph->path);
			printf("chgetparams: error getting element "
			       "address page\n");
			xpt_release_ccb(ccb);
			free(mode_buffer, M_TEMP);
			return(error);
		}
	}

	ea = (struct page_element_address_assignment *)
		find_mode_page_6((struct scsi_mode_header_6 *)mode_buffer);

	softc->sc_firsts[CHET_MT] = scsi_2btoul(ea->mtea);
	softc->sc_counts[CHET_MT] = scsi_2btoul(ea->nmte);
	softc->sc_firsts[CHET_ST] = scsi_2btoul(ea->fsea);
	softc->sc_counts[CHET_ST] = scsi_2btoul(ea->nse);
	softc->sc_firsts[CHET_IE] = scsi_2btoul(ea->fieea);
	softc->sc_counts[CHET_IE] = scsi_2btoul(ea->niee);
	softc->sc_firsts[CHET_DT] = scsi_2btoul(ea->fdtea);
	softc->sc_counts[CHET_DT] = scsi_2btoul(ea->ndte);

	bzero(mode_buffer, mode_buffer_len);

	/*
	 * Now get the device capabilities page.
	 */
	scsi_mode_sense(&ccb->csio,
			/* retries */ 1,
			/* cbfcnp */ chdone,
			/* tag_action */ MSG_SIMPLE_Q_TAG,
			/* dbd */ dbd,
			/* page_code */ SMS_PAGE_CTRL_CURRENT,
			/* page */ CH_DEVICE_CAP_PAGE,
			/* param_buf */ (u_int8_t *)mode_buffer,
			/* param_len */ mode_buffer_len,
			/* sense_len */ SSD_FULL_SIZE,
			/* timeout */ CH_TIMEOUT_MODE_SENSE);
	
	error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  /* sense_flags */ SF_RETRY_UA | SF_NO_PRINT |
				  SF_RETRY_SELTO, &softc->device_stats);

	if (error) {
		if (dbd) {
			struct scsi_mode_sense_6 *sms;

			sms = (struct scsi_mode_sense_6 *)
				ccb->csio.cdb_io.cdb_bytes;

			sms->byte2 &= ~SMS_DBD;
			error = cam_periph_runccb(ccb, cherror, /*cam_flags*/ 0,
				  		  /*sense_flags*/ SF_RETRY_UA |
						  SF_RETRY_SELTO,
						  &softc->device_stats);
		} else {
			/*
			 * Since we disabled sense printing above, print
			 * out the sense here since we got an error.
			 */
			scsi_sense_print(&ccb->csio);
		}

		if (error) {
			xpt_print_path(periph->path);
			printf("chgetparams: error getting device "
			       "capabilities page\n");
			xpt_release_ccb(ccb);
			free(mode_buffer, M_TEMP);
			return(error);
		}
	}

	xpt_release_ccb(ccb);

	cap = (struct page_device_capabilities *)
		find_mode_page_6((struct scsi_mode_header_6 *)mode_buffer);

	bzero(softc->sc_movemask, sizeof(softc->sc_movemask));
	bzero(softc->sc_exchangemask, sizeof(softc->sc_exchangemask));
	moves = &cap->move_from_mt;
	exchanges = &cap->exchange_with_mt;
	for (from = CHET_MT; from <= CHET_DT; ++from) {
		softc->sc_movemask[from] = moves[from];
		softc->sc_exchangemask[from] = exchanges[from];
	}

	free(mode_buffer, M_TEMP);

	return(error);
}

void
scsi_move_medium(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int32_t tea, u_int32_t src,
		 u_int32_t dst, int invert, u_int8_t sense_len,
		 u_int32_t timeout)
{
	struct scsi_move_medium *scsi_cmd;

	scsi_cmd = (struct scsi_move_medium *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = MOVE_MEDIUM;

	scsi_ulto2b(tea, scsi_cmd->tea);
	scsi_ulto2b(src, scsi_cmd->src);
	scsi_ulto2b(dst, scsi_cmd->dst);

	if (invert)
		scsi_cmd->invert |= MOVE_MEDIUM_INVERT;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/ NULL,
		      /*dxfer_len*/ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_exchange_medium(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, u_int32_t tea, u_int32_t src,
		     u_int32_t dst1, u_int32_t dst2, int invert1,
		     int invert2, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_exchange_medium *scsi_cmd;

	scsi_cmd = (struct scsi_exchange_medium *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = EXCHANGE_MEDIUM;

	scsi_ulto2b(tea, scsi_cmd->tea);
	scsi_ulto2b(src, scsi_cmd->src);
	scsi_ulto2b(dst1, scsi_cmd->fdst);
	scsi_ulto2b(dst2, scsi_cmd->sdst);

	if (invert1)
		scsi_cmd->invert |= EXCHANGE_MEDIUM_INV1;

	if (invert2)
		scsi_cmd->invert |= EXCHANGE_MEDIUM_INV2;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/ NULL,
		      /*dxfer_len*/ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_position_to_element(struct ccb_scsiio *csio, u_int32_t retries,
			 void (*cbfcnp)(struct cam_periph *, union ccb *),
			 u_int8_t tag_action, u_int32_t tea, u_int32_t dst,
			 int invert, u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_position_to_element *scsi_cmd;

	scsi_cmd = (struct scsi_position_to_element *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = POSITION_TO_ELEMENT;

	scsi_ulto2b(tea, scsi_cmd->tea);
	scsi_ulto2b(dst, scsi_cmd->dst);

	if (invert)
		scsi_cmd->invert |= POSITION_TO_ELEMENT_INVERT;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /*data_ptr*/ NULL,
		      /*dxfer_len*/ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_read_element_status(struct ccb_scsiio *csio, u_int32_t retries,
			 void (*cbfcnp)(struct cam_periph *, union ccb *),
			 u_int8_t tag_action, int voltag, u_int32_t sea,
			 u_int32_t count, u_int8_t *data_ptr,
			 u_int32_t dxfer_len, u_int8_t sense_len,
			 u_int32_t timeout)
{
	struct scsi_read_element_status *scsi_cmd;

	scsi_cmd = (struct scsi_read_element_status *)&csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = READ_ELEMENT_STATUS;

	scsi_ulto2b(sea, scsi_cmd->sea);
	scsi_ulto2b(count, scsi_cmd->count);
	scsi_ulto3b(dxfer_len, scsi_cmd->len);

	if (voltag)
		scsi_cmd->byte2 |= READ_ELEMENT_STATUS_VOLTAG;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_IN,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_initialize_element_status(struct ccb_scsiio *csio, u_int32_t retries,
			       void (*cbfcnp)(struct cam_periph *, union ccb *),
			       u_int8_t tag_action, u_int8_t sense_len,
			       u_int32_t timeout)
{
	struct scsi_initialize_element_status *scsi_cmd;

	scsi_cmd = (struct scsi_initialize_element_status *)
		    &csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = INITIALIZE_ELEMENT_STATUS;

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_NONE,
		      tag_action,
		      /* data_ptr */ NULL,
		      /* dxfer_len */ 0,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

void
scsi_send_volume_tag(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, 
		     u_int16_t element_address,
		     u_int8_t send_action_code,
		     struct scsi_send_volume_tag_parameters *parameters,
		     u_int8_t sense_len, u_int32_t timeout)
{
	struct scsi_send_volume_tag *scsi_cmd;

	scsi_cmd = (struct scsi_send_volume_tag *) &csio->cdb_io.cdb_bytes;
	bzero(scsi_cmd, sizeof(*scsi_cmd));

	scsi_cmd->opcode = SEND_VOLUME_TAG;
	scsi_ulto2b(element_address, scsi_cmd->ea);
	scsi_cmd->sac = send_action_code;
	scsi_ulto2b(sizeof(*parameters), scsi_cmd->pll);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ CAM_DIR_OUT,
		      tag_action,
		      /* data_ptr */ (u_int8_t *) parameters,
		      sizeof(*parameters),
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}
