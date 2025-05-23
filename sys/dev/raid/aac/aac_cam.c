/*
 * Copyright (c) 2002 Adaptec, Inc.
 * All rights reserved.
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
 *	$FreeBSD: src/sys/dev/aac/aac_cam.c,v 1.2.2.4 2003/04/08 13:22:08 scottl Exp $
 *	$DragonFly: src/sys/dev/raid/aac/aac_cam.c,v 1.4 2004/03/15 03:05:03 dillon Exp $
 */

/*
 * CAM front-end for communicating with non-DASD devices
 */

#include "opt_aac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

#include <bus/cam/cam.h>
#include <bus/cam/cam_ccb.h>
#include <bus/cam/cam_debug.h>
#include <bus/cam/cam_sim.h>
#include <bus/cam/cam_xpt_sim.h>
#include <bus/cam/scsi/scsi_all.h>
#include <bus/cam/scsi/scsi_message.h>

#include "aac_compat.h"
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "aacreg.h"
#include "aac_ioctl.h"
#include "aacvar.h"
#include "aac_cam.h"

struct aac_cam {
	device_t		dev;
	struct aac_cam_inf	*inf;
	struct cam_sim		*sim;
	struct cam_path		*path;
};

static int aac_cam_probe(device_t dev);
static int aac_cam_attach(device_t dev);
static int aac_cam_detach(device_t dev);
static void aac_cam_action(struct cam_sim *, union ccb *);
static void aac_cam_poll(struct cam_sim *);
static void aac_cam_complete(struct aac_command *);
static u_int32_t aac_cam_reset_bus(struct cam_sim *, union ccb *);
static u_int32_t aac_cam_abort_ccb(struct cam_sim *, union ccb *);
static u_int32_t aac_cam_term_io(struct cam_sim *, union ccb *);
static int aac_cam_get_tran_settings(struct aac_softc *, struct ccb_trans_settings *, u_int32_t);

static devclass_t	aac_pass_devclass;

static device_method_t	aac_pass_methods[] = {
	DEVMETHOD(device_probe,		aac_cam_probe),
	DEVMETHOD(device_attach,	aac_cam_attach),
	DEVMETHOD(device_detach,	aac_cam_detach),
	{ 0, 0 }
};

static driver_t	aac_pass_driver = {
	"aacp",
	aac_pass_methods,
	sizeof(struct aac_cam)
};

DRIVER_MODULE(aacp, aac, aac_pass_driver, aac_pass_devclass, 0, 0);
MODULE_DEPEND(aacp, cam, 1, 1, 1);

MALLOC_DEFINE(M_AACCAM, "aaccam", "AAC CAM info");

static int
aac_cam_probe(device_t dev)
{

	debug_called(2);

	return (0);
}

static int
aac_cam_detach(device_t dev)
{

	return (0);
}

/*
 * Register the driver as a CAM SIM
 */
static int
aac_cam_attach(device_t dev)
{
	struct cam_devq *devq;
	struct cam_sim *sim;
	struct cam_path *path;
	struct aac_cam *camsc;
	struct aac_cam_inf *inf;

	debug_called(1);

	camsc = (struct aac_cam *)device_get_softc(dev);
	inf = (struct aac_cam_inf *)device_get_ivars(dev);
	camsc->inf = inf;

	devq = cam_simq_alloc(inf->TargetsPerBus);
	if (devq == NULL)
		return (EIO);

	sim = cam_sim_alloc(aac_cam_action, aac_cam_poll, "aacp", camsc,
	    device_get_unit(dev), 1, 1, devq);
	cam_simq_release(devq);
	if (sim == NULL) {
		return (EIO);
	}

	/* Since every bus has it's own sim, every bus 'appears' as bus 0 */
	if (xpt_bus_register(sim, 0) != CAM_SUCCESS) {
		cam_sim_free(sim);
		return (EIO);
	}

	if (xpt_create_path(&path, NULL, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim);
		return (EIO);
	}

	camsc->sim = sim;
	camsc->path = path;

	return (0);
}

static void
aac_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	aac_cam *camsc;
	struct	aac_softc *sc;
	struct	aac_srb32 *srb;
	struct	aac_fib *fib;
	struct	aac_command *cm;

	debug_called(2);

	camsc = (struct aac_cam *)cam_sim_softc(sim);
	sc = camsc->inf->aac_sc;

	/* Synchronous ops, and ops that don't require communication with the
	 * controller */
	switch(ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	case XPT_RESET_DEV:
		/* These are handled down below */
		break;
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size /
		    ((1024L * 1024L) / ccg->block_size);
		if (size_mb >= (2 * 1024)) {		/* 2GB */
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else if (size_mb >= (1 * 1024)) {	/* 1GB */
			ccg->heads = 128;
			ccg->secs_per_track = 32;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = camsc->inf->TargetsPerBus;
		cpi->max_lun = 8;	/* Per the controller spec */
		cpi->initiator_id = camsc->inf->InitiatorBusId;
		cpi->bus_id = camsc->inf->BusNumber;
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Adaptec", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		return;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		u_int32_t handle;

		handle = AAC_BTL_TO_HANDLE(camsc->inf->BusNumber,
		    ccb->ccb_h.target_id, ccb->ccb_h.target_lun);
		ccb->ccb_h.status = aac_cam_get_tran_settings(sc, &ccb->cts,
		    handle);
		xpt_done(ccb);
		return;
	}
	case XPT_SET_TRAN_SETTINGS:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		return;
	case XPT_RESET_BUS:
		if (!(sc->flags & AAC_FLAGS_CAM_NORESET)) {
			ccb->ccb_h.status = aac_cam_reset_bus(sim, ccb);
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		return;
	case XPT_ABORT:
		ccb->ccb_h.status = aac_cam_abort_ccb(sim, ccb);
		xpt_done(ccb);
		return;
	case XPT_TERM_IO:
		ccb->ccb_h.status = aac_cam_term_io(sim, ccb);
		xpt_done(ccb);
		return;
	default:
		device_printf(sc->aac_dev, "Unsupported command 0x%x\n",
		    ccb->ccb_h.func_code);
		ccb->ccb_h.status = CAM_PROVIDE_FAIL;
		xpt_done(ccb);
		return;
	}

	/* Async ops that require communcation with the controller */

	if (aac_alloc_command(sc, &cm)) {
		xpt_freeze_simq(sim, 1);
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}

	fib = cm->cm_fib;
	srb = (struct aac_srb32 *)&fib->data[0];
	cm->cm_datalen = 0;

	switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		srb->flags = AAC_SRB_FLAGS_DATA_IN;
		cm->cm_flags |= AAC_CMD_DATAIN;
		break;
	case CAM_DIR_OUT:
		srb->flags = AAC_SRB_FLAGS_DATA_OUT;
		cm->cm_flags |= AAC_CMD_DATAOUT;
		break;
	case CAM_DIR_NONE:
		srb->flags = AAC_SRB_FLAGS_NO_DATA_XFER;
		break;
	default:
		srb->flags = AAC_SRB_FLAGS_UNSPECIFIED_DIRECTION;
		cm->cm_flags |= AAC_CMD_DATAIN | AAC_CMD_DATAOUT;
		break;
	}

	switch(ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio = &ccb->csio;

		srb->function = AAC_SRB_FUNC_EXECUTE_SCSI;

		/*
		 * Copy the CDB into the SRB.  It's only 6-16 bytes,
		 * so a copy is not too expensive.
		 */
		srb->cdb_len = csio->cdb_len;
		if (ccb->ccb_h.flags & CAM_CDB_POINTER)
			bcopy(csio->cdb_io.cdb_ptr, (u_int8_t *)&srb->cdb[0],
			    srb->cdb_len);
		else
			bcopy(csio->cdb_io.cdb_bytes, (u_int8_t *)&srb->cdb[0],
			    srb->cdb_len);

		/* Map the s/g list. XXX 32bit addresses only! */
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			if ((ccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) {
				srb->data_len = csio->dxfer_len;
				if (ccb->ccb_h.flags & CAM_DATA_PHYS) {
					srb->sg_map32.SgCount = 1;
					srb->sg_map32.SgEntry[0].SgAddress =
					    (u_int32_t)csio->data_ptr;
					srb->sg_map32.SgEntry[0].SgByteCount =
					    csio->dxfer_len;
				} else {
					/*
					 * Arrange things so that the S/G
					 * map will get set up automagically
					 */
					cm->cm_data = (void *)csio->data_ptr;
					cm->cm_datalen = csio->dxfer_len;
					cm->cm_sgtable = &srb->sg_map32;
				}
			} else {
				/* XXX Need to handle multiple s/g elements */
				panic("aac_cam: multiple s/g elements");
			}
		} else {
			srb->sg_map32.SgCount = 0;
			srb->sg_map32.SgEntry[0].SgByteCount = 0;
			srb->data_len = 0;
		}

		break;
	}
	case XPT_RESET_DEV:
		if (!(sc->flags & AAC_FLAGS_CAM_NORESET)) {
			srb->function = AAC_SRB_FUNC_RESET_DEVICE;
			break;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(ccb);
			return;
		}
	default:
		break;
	}

	srb->bus = camsc->inf->BusNumber; /* Bus number relative to the card */
	srb->target = ccb->ccb_h.target_id;
	srb->lun = ccb->ccb_h.target_lun;
	srb->timeout = ccb->ccb_h.timeout;	/* XXX */
	srb->retry_limit = 0;

	cm->cm_complete = aac_cam_complete;
	cm->cm_private = ccb;
	cm->cm_timestamp = time_second;
	cm->cm_queue = AAC_ADAP_NORM_CMD_QUEUE;

	fib->Header.XferState =
	    AAC_FIBSTATE_HOSTOWNED	|
	    AAC_FIBSTATE_INITIALISED	|
	    AAC_FIBSTATE_FROMHOST	|
	    AAC_FIBSTATE_REXPECTED	|
	    AAC_FIBSTATE_NORM;
	fib->Header.Command = ScsiPortCommand;
	fib->Header.Size = sizeof(struct aac_fib_header) +
	    sizeof(struct aac_srb32);

	aac_enqueue_ready(cm);
	aac_startio(cm->cm_sc);

	return;
}

static void
aac_cam_poll(struct cam_sim *sim)
{
	/*
	 * Pinging the interrupt routine isn't very safe, nor is it
	 * really necessary.  Do nothing.
	 */
}

static void
aac_cam_complete(struct aac_command *cm)
{
	union	ccb *ccb;
	struct 	aac_srb_response *srbr;
	struct	aac_softc *sc;

	debug_called(2);

	sc = cm->cm_sc;
	ccb = cm->cm_private;
	srbr = (struct aac_srb_response *)&cm->cm_fib->data[0];

	if (srbr->fib_status != 0) {
		device_printf(sc->aac_dev, "Passthru FIB failed!\n");
		ccb->ccb_h.status = CAM_REQ_ABORTED;
	} else {
		/*
		 * The SRB error codes just happen to match the CAM error
		 * codes.  How convienient!
		 */
		ccb->ccb_h.status = srbr->srb_status;

		/* Take care of SCSI_IO ops. */
		if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
			u_int8_t command, device;

			ccb->csio.scsi_status = srbr->scsi_status;

			/* Take care of autosense */
			if (srbr->sense_len) {
				int sense_len, scsi_sense_len;

				scsi_sense_len = sizeof(struct scsi_sense_data);
				bzero(&ccb->csio.sense_data, scsi_sense_len);
				sense_len = (srbr->sense_len > 
				    scsi_sense_len) ? scsi_sense_len :
				    srbr->sense_len;
				bcopy(&srbr->sense[0], &ccb->csio.sense_data,
				    srbr->sense_len);
				ccb->csio.sense_len = sense_len;
				ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
				scsi_sense_print(&ccb->csio);
			}

			/* If this is an inquiry command, fake things out */
			if (ccb->ccb_h.flags & CAM_CDB_POINTER)
				command = ccb->csio.cdb_io.cdb_ptr[0];
			else
				command = ccb->csio.cdb_io.cdb_bytes[0];

			if ((command == INQUIRY) &&
			    (ccb->ccb_h.status == CAM_REQ_CMP)) {
				device = ccb->csio.data_ptr[0] & 0x1f;
				/*
				 * We want DASD and PROC devices to only be
				 * visible through the pass device.
				 */
				if ((device == T_DIRECT) ||
				    (device == T_PROCESSOR) ||
				    (sc->flags & AAC_FLAGS_CAM_PASSONLY))
					ccb->csio.data_ptr[0] =
					    ((device & 0xe0) | T_NODEVICE);
			}
		}
	}

	aac_release_command(cm);

	xpt_done(ccb);

	return;
}

static u_int32_t
aac_cam_reset_bus(struct cam_sim *sim, union ccb *ccb)
{
	struct aac_fib *fib;
	struct aac_softc *sc;
	struct aac_cam *camsc;
	struct aac_vmioctl *vmi;
	struct aac_resetbus *rbc;
	int e;

	camsc = (struct aac_cam *)cam_sim_softc(sim);
	sc = camsc->inf->aac_sc;

	if (sc == NULL) {
		printf("Null sc?\n");
		return (CAM_REQ_ABORTED);
	}

	aac_alloc_sync_fib(sc, &fib, 0);

	vmi = (struct aac_vmioctl *)&fib->data[0];
	bzero(vmi, sizeof(struct aac_vmioctl));

	vmi->Command = VM_Ioctl;
	vmi->ObjType = FT_DRIVE;
	vmi->MethId = sc->scsi_method_id;
	vmi->ObjId = 0;
	vmi->IoctlCmd = ResetBus;

	rbc = (struct aac_resetbus *)&vmi->IoctlBuf[0];
	rbc->BusNumber = camsc->inf->BusNumber;

	e = aac_sync_fib(sc, ContainerCommand, 0, fib,
	    sizeof(struct aac_vmioctl));
	if (e) {
		device_printf(sc->aac_dev, "Error 0x%x sending passthrough\n",
		    e);
		aac_release_sync_fib(sc);
		return (CAM_REQ_ABORTED);
	}

	aac_release_sync_fib(sc);
	return (CAM_REQ_CMP);
}

static u_int32_t
aac_cam_abort_ccb(struct cam_sim *sim, union ccb *ccb)
{
	return (CAM_UA_ABORT);
}

static u_int32_t
aac_cam_term_io(struct cam_sim *sim, union ccb *ccb)
{
	return (CAM_UA_TERMIO);
}

static int
aac_cam_get_tran_settings(struct aac_softc *sc, struct ccb_trans_settings *cts, u_int32_t handle)
{
	struct aac_fib *fib;
	struct aac_vmioctl *vmi;
	struct aac_vmi_devinfo_resp *vmi_resp;
	int error;

	aac_alloc_sync_fib(sc, &fib, 0);
	vmi = (struct aac_vmioctl *)&fib->data[0];
	bzero(vmi, sizeof(struct aac_vmioctl));

	vmi->Command = VM_Ioctl;
	vmi->ObjType = FT_DRIVE;
	vmi->MethId = sc->scsi_method_id;
	vmi->ObjId = handle;
	vmi->IoctlCmd = GetDeviceProbeInfo;

	error = aac_sync_fib(sc, ContainerCommand, 0, fib,
	    sizeof(struct aac_vmioctl));
	if (error) {
		device_printf(sc->aac_dev, "Error %d sending VMIoctl command\n",
		    error);
		aac_release_sync_fib(sc);
		return (CAM_REQ_INVALID);
	}

	vmi_resp = (struct aac_vmi_devinfo_resp *)&fib->data[0];
	if (vmi_resp->Status != ST_OK) {
		debug(1, "VM_Ioctl returned %d\n", vmi_resp->Status);
		aac_release_sync_fib(sc);
		return (CAM_REQ_CMP_ERR);
	}

	cts->bus_width = ((vmi_resp->Inquiry7 & 0x60) >> 5);
	if (vmi_resp->ScsiRate) {
		cts->sync_period =
		    scsi_calc_syncparam((10000 / vmi_resp->ScsiRate));
		cts->sync_offset = vmi_resp->ScsiOffset;
	} else {
		cts->sync_period = 0;
		cts->sync_offset = 0;
	}
	cts->flags &= ~(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
	cts->valid = CCB_TRANS_DISC_VALID		|
		     CCB_TRANS_SYNC_RATE_VALID		|
		     CCB_TRANS_SYNC_OFFSET_VALID	|
		     CCB_TRANS_BUS_WIDTH_VALID		|
		     CCB_TRANS_TQ_VALID;

	aac_release_sync_fib(sc);
	return (CAM_REQ_CMP);
}
