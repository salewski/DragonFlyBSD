/* $FreeBSD: src/sys/dev/iir/iir.c,v 1.2.2.3 2002/05/05 08:18:12 asmodai Exp $ */
/* $DragonFly: src/sys/dev/raid/iir/iir.c,v 1.9 2004/09/17 03:39:39 joerg Exp $ */
/*
 *       Copyright (c) 2000-01 Intel Corporation
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 */

/*
 * iir.c: SCSI dependant code for the Intel Integrated RAID Controller driver
 *
 * Written by: Achim Leubner <achim.leubner@intel.com>
 * Fixes/Additions: Boji Tony Kannanthanam <boji.t.kannanthanam@intel.com>
 *
 * credits:     Niklas Hallqvist;       OpenBSD driver for the ICP Controllers.
 *              Mike Smith;             Some driver source code.
 *              FreeBSD.ORG;            Great O/S to work on and for.
 *
 * TODO:     
 */

#ident "$Id: iir.c 1.2 2001/06/21 20:28:32 achim Exp $"

#define _IIR_C_

/* #include "opt_iir.h" */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/stdarg.h>

#include <bus/cam/cam.h>
#include <bus/cam/cam_ccb.h>
#include <bus/cam/cam_sim.h>
#include <bus/cam/cam_xpt_sim.h>
#include <bus/cam/cam_debug.h>
#include <bus/cam/scsi/scsi_all.h>
#include <bus/cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "iir.h"

struct gdt_softc *gdt_wait_gdt;
int     gdt_wait_index;

#ifdef GDT_DEBUG
int     gdt_debug = GDT_DEBUG;
#ifdef __SERIAL__
#define MAX_SERBUF 160
static void ser_init(void);
static void ser_puts(char *str);
static void ser_putc(int c);
static char strbuf[MAX_SERBUF+1];
#ifdef __COM2__
#define COM_BASE 0x2f8
#else
#define COM_BASE 0x3f8
#endif
static void ser_init()
{
    unsigned port=COM_BASE;

    outb(port+3, 0x80);
    outb(port+1, 0);
    /* 19200 Baud, if 9600: outb(12,port) */
    outb(port, 6);
    outb(port+3, 3);
    outb(port+1, 0);
}

static void ser_puts(char *str)
{
    char *ptr;

    ser_init();
    for (ptr=str;*ptr;++ptr)
        ser_putc((int)(*ptr));
}

static void ser_putc(int c)
{
    unsigned port=COM_BASE;

    while ((inb(port+5) & 0x20)==0);
    outb(port, c);
    if (c==0x0a)
    {
        while ((inb(port+5) & 0x20)==0);
        outb(port, 0x0d);
    }
}

int ser_printf(const char *fmt, ...)
{
    __va_list args;
    int i;

    __va_start(args,fmt);
    i = vsprintf(strbuf,fmt,args);
    ser_puts(strbuf);
    __va_end(args);
    return i;
}
#endif
#endif

/* The linked list of softc structures */
struct gdt_softc_list gdt_softcs = TAILQ_HEAD_INITIALIZER(gdt_softcs);
/* controller cnt. */
int gdt_cnt = 0;
/* event buffer */
static gdt_evt_str ebuffer[GDT_MAX_EVENTS];
static int elastidx, eoldidx;
/* statistics */
gdt_statist_t gdt_stat;

/* Definitions for our use of the SIM private CCB area */
#define ccb_sim_ptr     spriv_ptr0
#define ccb_priority    spriv_field1

static void     iir_action(struct cam_sim *sim, union ccb *ccb);
static void     iir_poll(struct cam_sim *sim);
static void     iir_shutdown(void *arg, int howto);
static void     iir_timeout(void *arg);
static void     iir_watchdog(void *arg);

static void     gdt_eval_mapping(u_int32_t size, int *cyls, int *heads, 
                                 int *secs);
static int      gdt_internal_cmd(struct gdt_softc *gdt, struct gdt_ccb *gccb, 
                                 u_int8_t service, u_int16_t opcode, 
                                 u_int32_t arg1, u_int32_t arg2, u_int32_t arg3);
static int      gdt_wait(struct gdt_softc *gdt, struct gdt_ccb *ccb, 
                         int timeout);

static struct gdt_ccb *gdt_get_ccb(struct gdt_softc *gdt);
static u_int32_t gdt_ccb_vtop(struct gdt_softc *gdt, 
                              struct gdt_ccb *gccb);

static int      gdt_sync_event(struct gdt_softc *gdt, int service, 
                               u_int8_t index, struct gdt_ccb *gccb);
static int      gdt_async_event(struct gdt_softc *gdt, int service);
static struct gdt_ccb *gdt_raw_cmd(struct gdt_softc *gdt, 
                                   union ccb *ccb, int *lock);
static struct gdt_ccb *gdt_cache_cmd(struct gdt_softc *gdt, 
                                     union ccb *ccb, int *lock);
static struct gdt_ccb *gdt_ioctl_cmd(struct gdt_softc *gdt, 
                                     gdt_ucmd_t *ucmd, int *lock);
static void     gdt_internal_cache_cmd(struct gdt_softc *gdt,union ccb *ccb);

static void     gdtmapmem(void *arg, bus_dma_segment_t *dm_segs,
                          int nseg, int error);
static void     gdtexecuteccb(void *arg, bus_dma_segment_t *dm_segs,
                              int nseg, int error);

int
iir_init(struct gdt_softc *gdt)
{
    u_int16_t cdev_cnt;
    int i, id, drv_cyls, drv_hds, drv_secs;
    struct gdt_ccb *gccb;

    GDT_DPRINTF(GDT_D_DEBUG, ("iir_init()\n"));

    gdt->sc_state = GDT_POLLING;
    gdt_clear_events(); 
    bzero(&gdt_stat, sizeof(gdt_statist_t));

    SLIST_INIT(&gdt->sc_free_gccb);
    SLIST_INIT(&gdt->sc_pending_gccb);
    TAILQ_INIT(&gdt->sc_ccb_queue);
    TAILQ_INIT(&gdt->sc_ucmd_queue);
    TAILQ_INSERT_TAIL(&gdt_softcs, gdt, links);

    /* DMA tag for mapping buffers into device visible space. */
    if (bus_dma_tag_create(gdt->sc_parent_dmat, /*alignment*/1, /*boundary*/0,
                           /*lowaddr*/BUS_SPACE_MAXADDR,
                           /*highaddr*/BUS_SPACE_MAXADDR,
                           /*filter*/NULL, /*filterarg*/NULL,
                           /*maxsize*/MAXBSIZE, /*nsegments*/GDT_MAXSG,
                           /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
                           /*flags*/BUS_DMA_ALLOCNOW,
                           &gdt->sc_buffer_dmat) != 0) {
        printf("iir%d: bus_dma_tag_create(...,gdt->sc_buffer_dmat) failed\n",
               gdt->sc_hanum);
        return (1);
    }
    gdt->sc_init_level++;

    /* DMA tag for our ccb structures */
    if (bus_dma_tag_create(gdt->sc_parent_dmat, /*alignment*/1, /*boundary*/0,
                           /*lowaddr*/BUS_SPACE_MAXADDR,
                           /*highaddr*/BUS_SPACE_MAXADDR,
                           /*filter*/NULL, /*filterarg*/NULL,
                           GDT_MAXCMDS * sizeof(struct gdt_ccb),
                           /*nsegments*/1,
                           /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
                           /*flags*/0, &gdt->sc_gccb_dmat) != 0) {
        printf("iir%d: bus_dma_tag_create(...,gdt->sc_gccb_dmat) failed\n",
               gdt->sc_hanum);
        return (1);
    }
    gdt->sc_init_level++;

    /* Allocation for our ccbs */
    if (bus_dmamem_alloc(gdt->sc_gccb_dmat, (void **)&gdt->sc_gccbs,
                         BUS_DMA_NOWAIT, &gdt->sc_gccb_dmamap) != 0) {
        printf("iir%d: bus_dmamem_alloc(...,&gdt->sc_gccbs,...) failed\n",
               gdt->sc_hanum);
        return (1);
    }
    gdt->sc_init_level++;

    /* And permanently map them */
    bus_dmamap_load(gdt->sc_gccb_dmat, gdt->sc_gccb_dmamap,
                    gdt->sc_gccbs, GDT_MAXCMDS * sizeof(struct gdt_ccb),
                    gdtmapmem, &gdt->sc_gccb_busbase, /*flags*/0);
    gdt->sc_init_level++;

    /* Clear them out. */
    bzero(gdt->sc_gccbs, GDT_MAXCMDS * sizeof(struct gdt_ccb));

    /* Initialize the ccbs */
    for (i = GDT_MAXCMDS-1; i >= 0; i--) {
        gdt->sc_gccbs[i].gc_cmd_index = i + 2;
        gdt->sc_gccbs[i].gc_flags = GDT_GCF_UNUSED;
        gdt->sc_gccbs[i].gc_map_flag = FALSE;
        if (bus_dmamap_create(gdt->sc_buffer_dmat, /*flags*/0,
                              &gdt->sc_gccbs[i].gc_dmamap) != 0)
            return(1);
        gdt->sc_gccbs[i].gc_map_flag = TRUE;
        SLIST_INSERT_HEAD(&gdt->sc_free_gccb, &gdt->sc_gccbs[i], sle);
    }
    gdt->sc_init_level++;

    /* create the control device */
    gdt->sc_dev = gdt_make_dev(gdt->sc_hanum);

    /* allocate ccb for gdt_internal_cmd() */
    gccb = gdt_get_ccb(gdt);
    if (gccb == NULL) {
        printf("iir%d: No free command index found\n",
               gdt->sc_hanum);
        return (1);
    }

    if (!gdt_internal_cmd(gdt, gccb, GDT_SCREENSERVICE, GDT_INIT, 
                          0, 0, 0)) {
        printf("iir%d: Screen service initialization error %d\n",
               gdt->sc_hanum, gdt->sc_status);
        gdt_free_ccb(gdt, gccb);
        return (1);
    }

    if (!gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_INIT, 
                          GDT_LINUX_OS, 0, 0)) {
        printf("iir%d: Cache service initialization error %d\n",
               gdt->sc_hanum, gdt->sc_status);
        gdt_free_ccb(gdt, gccb);
        return (1);
    }
    gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_UNFREEZE_IO,
                     0, 0, 0);

    if (!gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_MOUNT, 
                          0xffff, 1, 0)) {
        printf("iir%d: Cache service mount error %d\n",
               gdt->sc_hanum, gdt->sc_status);
        gdt_free_ccb(gdt, gccb);
        return (1);
    }

    if (!gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_INIT, 
                          GDT_LINUX_OS, 0, 0)) {
        printf("iir%d: Cache service post-mount initialization error %d\n",
               gdt->sc_hanum, gdt->sc_status);
        gdt_free_ccb(gdt, gccb);
        return (1);
    }
    cdev_cnt = (u_int16_t)gdt->sc_info;
    gdt->sc_fw_vers = gdt->sc_service;

    /* Detect number of buses */
    gdt_enc32(gccb->gc_scratch + GDT_IOC_VERSION, GDT_IOC_NEWEST);
    gccb->gc_scratch[GDT_IOC_LIST_ENTRIES] = GDT_MAXBUS;
    gccb->gc_scratch[GDT_IOC_FIRST_CHAN] = 0;
    gccb->gc_scratch[GDT_IOC_LAST_CHAN] = GDT_MAXBUS - 1;
    gdt_enc32(gccb->gc_scratch + GDT_IOC_LIST_OFFSET, GDT_IOC_HDR_SZ);
    if (gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_IOCTL,
                         GDT_IOCHAN_RAW_DESC, GDT_INVALID_CHANNEL,
                         GDT_IOC_HDR_SZ + GDT_MAXBUS * GDT_RAWIOC_SZ)) {
        gdt->sc_bus_cnt = gccb->gc_scratch[GDT_IOC_CHAN_COUNT];
        for (i = 0; i < gdt->sc_bus_cnt; i++) {
            id = gccb->gc_scratch[GDT_IOC_HDR_SZ +
                                 i * GDT_RAWIOC_SZ + GDT_RAWIOC_PROC_ID];
            gdt->sc_bus_id[i] = id < GDT_MAXID_FC ? id : 0xff;
        }
    } else {
        /* New method failed, use fallback. */
        for (i = 0; i < GDT_MAXBUS; i++) {
            gdt_enc32(gccb->gc_scratch + GDT_GETCH_CHANNEL_NO, i);
            if (!gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_IOCTL,
                                  GDT_SCSI_CHAN_CNT | GDT_L_CTRL_PATTERN,
                                  GDT_IO_CHANNEL | GDT_INVALID_CHANNEL,
                                  GDT_GETCH_SZ)) {
                if (i == 0) {
                    printf("iir%d: Cannot get channel count, "
                           "error %d\n", gdt->sc_hanum, gdt->sc_status);
                    gdt_free_ccb(gdt, gccb);
                    return (1);
                }
                break;
            }
            gdt->sc_bus_id[i] =
                (gccb->gc_scratch[GDT_GETCH_SIOP_ID] < GDT_MAXID_FC) ?
                gccb->gc_scratch[GDT_GETCH_SIOP_ID] : 0xff;
        }
        gdt->sc_bus_cnt = i;
    }
    /* add one "virtual" channel for the host drives */
    gdt->sc_virt_bus = gdt->sc_bus_cnt;
    gdt->sc_bus_cnt++;

    if (!gdt_internal_cmd(gdt, gccb, GDT_SCSIRAWSERVICE, GDT_INIT, 
                          0, 0, 0)) {
            printf("iir%d: Raw service initialization error %d\n",
                   gdt->sc_hanum, gdt->sc_status);
            gdt_free_ccb(gdt, gccb);
            return (1);
    }

    /* Set/get features raw service (scatter/gather) */
    gdt->sc_raw_feat = 0;
    if (gdt_internal_cmd(gdt, gccb, GDT_SCSIRAWSERVICE, GDT_SET_FEAT,
                         GDT_SCATTER_GATHER, 0, 0)) {
        if (gdt_internal_cmd(gdt, gccb, GDT_SCSIRAWSERVICE, GDT_GET_FEAT, 
                             0, 0, 0)) {
            gdt->sc_raw_feat = gdt->sc_info;
            if (!(gdt->sc_info & GDT_SCATTER_GATHER)) {
                panic("iir%d: Scatter/Gather Raw Service "
                      "required but not supported!\n", gdt->sc_hanum);
                gdt_free_ccb(gdt, gccb);
                return (1);
            }
        }
    }

    /* Set/get features cache service (scatter/gather) */
    gdt->sc_cache_feat = 0;
    if (gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_SET_FEAT, 
                         0, GDT_SCATTER_GATHER, 0)) {
        if (gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_GET_FEAT, 
                             0, 0, 0)) {
            gdt->sc_cache_feat = gdt->sc_info;
            if (!(gdt->sc_info & GDT_SCATTER_GATHER)) {
                panic("iir%d: Scatter/Gather Cache Service "
                  "required but not supported!\n", gdt->sc_hanum);
                gdt_free_ccb(gdt, gccb);
                return (1);
            }
        }
    }

    /* Scan for cache devices */
    for (i = 0; i < cdev_cnt && i < GDT_MAX_HDRIVES; i++) {
        if (gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE, GDT_INFO, 
                             i, 0, 0)) {
            gdt->sc_hdr[i].hd_present = 1;
            gdt->sc_hdr[i].hd_size = gdt->sc_info;
            
            /*
             * Evaluate mapping (sectors per head, heads per cyl)
             */
            gdt->sc_hdr[i].hd_size &= ~GDT_SECS32;
            if (gdt->sc_info2 == 0)
                gdt_eval_mapping(gdt->sc_hdr[i].hd_size,
                                 &drv_cyls, &drv_hds, &drv_secs);
            else {
                drv_hds = gdt->sc_info2 & 0xff;
                drv_secs = (gdt->sc_info2 >> 8) & 0xff;
                drv_cyls = gdt->sc_hdr[i].hd_size / drv_hds /
                    drv_secs;
            }
            gdt->sc_hdr[i].hd_heads = drv_hds;
            gdt->sc_hdr[i].hd_secs = drv_secs;
            /* Round the size */
            gdt->sc_hdr[i].hd_size = drv_cyls * drv_hds * drv_secs;
            
            if (gdt_internal_cmd(gdt, gccb, GDT_CACHESERVICE,
                                 GDT_DEVTYPE, i, 0, 0))
                gdt->sc_hdr[i].hd_devtype = gdt->sc_info;
        }
    }
    
    GDT_DPRINTF(GDT_D_INIT, ("dpmem %x %d-bus %d cache device%s\n", 
                             gdt->sc_dpmembase,
                             gdt->sc_bus_cnt, cdev_cnt, 
                             cdev_cnt == 1 ? "" : "s"));
    gdt_free_ccb(gdt, gccb);

    gdt_cnt++;
    return (0);
}

void
iir_free(struct gdt_softc *gdt)
{
    int i;

    GDT_DPRINTF(GDT_D_INIT, ("iir_free()\n"));

    switch (gdt->sc_init_level) {
      default:
        gdt_destroy_dev(gdt->sc_dev);
      case 5:
        for (i = GDT_MAXCMDS-1; i >= 0; i--) 
            if (gdt->sc_gccbs[i].gc_map_flag)
                bus_dmamap_destroy(gdt->sc_buffer_dmat,
                                   gdt->sc_gccbs[i].gc_dmamap);
        bus_dmamap_unload(gdt->sc_gccb_dmat, gdt->sc_gccb_dmamap);
      case 4:
        bus_dmamem_free(gdt->sc_gccb_dmat, gdt->sc_gccbs, gdt->sc_gccb_dmamap);
      case 3:
        bus_dma_tag_destroy(gdt->sc_gccb_dmat);
      case 2:
        bus_dma_tag_destroy(gdt->sc_buffer_dmat);
      case 1:
        bus_dma_tag_destroy(gdt->sc_parent_dmat);
      case 0:
        break;
    }
    TAILQ_REMOVE(&gdt_softcs, gdt, links);
}

void
iir_attach(struct gdt_softc *gdt)
{
    struct cam_devq *devq;
    int i;

    GDT_DPRINTF(GDT_D_INIT, ("iir_attach()\n"));

    callout_init(&gdt->watchdog_timer);
    /*
     * Create the device queue for our SIM.
     */
    devq = cam_simq_alloc(GDT_MAXCMDS);
    if (devq == NULL)
        return;

    for (i = 0; i < gdt->sc_bus_cnt; i++) {
        /*
         * Construct our SIM entry
         */
        gdt->sims[i] = cam_sim_alloc(iir_action, iir_poll, "iir",
                                     gdt, gdt->sc_hanum, /*untagged*/2,
                                     /*tagged*/GDT_MAXCMDS, devq);
        if (xpt_bus_register(gdt->sims[i], i) != CAM_SUCCESS) {
            cam_sim_free(gdt->sims[i]);
            break;
        }

        if (xpt_create_path(&gdt->paths[i], /*periph*/NULL,
                            cam_sim_path(gdt->sims[i]),
                            CAM_TARGET_WILDCARD,
                            CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
            xpt_bus_deregister(cam_sim_path(gdt->sims[i]));
            cam_sim_free(gdt->sims[i]);
            break;
        }
    }
    cam_simq_release(devq);
    if (i > 0)
        EVENTHANDLER_REGISTER(shutdown_final, iir_shutdown,
                              gdt, SHUTDOWN_PRI_DEFAULT);
    /* iir_watchdog(gdt); */
    gdt->sc_state = GDT_NORMAL;
}

static void
gdt_eval_mapping(u_int32_t size, int *cyls, int *heads, int *secs)
{
    *cyls = size / GDT_HEADS / GDT_SECS;
    if (*cyls < GDT_MAXCYLS) {
        *heads = GDT_HEADS;
        *secs = GDT_SECS;
    } else {
        /* Too high for 64 * 32 */
        *cyls = size / GDT_MEDHEADS / GDT_MEDSECS;
        if (*cyls < GDT_MAXCYLS) {
            *heads = GDT_MEDHEADS;
            *secs = GDT_MEDSECS;
        } else {
            /* Too high for 127 * 63 */
            *cyls = size / GDT_BIGHEADS / GDT_BIGSECS;
            *heads = GDT_BIGHEADS;
            *secs = GDT_BIGSECS;
        }
    }
}

static int
gdt_wait(struct gdt_softc *gdt, struct gdt_ccb *gccb, 
         int timeout)
{
    int rv = 0;

    GDT_DPRINTF(GDT_D_INIT,
                ("gdt_wait(%p, %p, %d)\n", gdt, gccb, timeout));

    gdt->sc_state |= GDT_POLL_WAIT;
    do {
        iir_intr(gdt);
        if (gdt == gdt_wait_gdt &&
            gccb->gc_cmd_index == gdt_wait_index) {
            rv = 1;
            break;
        }
        DELAY(1);
    } while (--timeout);
    gdt->sc_state &= ~GDT_POLL_WAIT;
    
    while (gdt->sc_test_busy(gdt))
        DELAY(1);               /* XXX correct? */

    return (rv);
}

static int
gdt_internal_cmd(struct gdt_softc *gdt, struct gdt_ccb *gccb,
                 u_int8_t service, u_int16_t opcode, 
                 u_int32_t arg1, u_int32_t arg2, u_int32_t arg3)
{
    int retries;
    
    GDT_DPRINTF(GDT_D_CMD, ("gdt_internal_cmd(%p, %d, %d, %d, %d, %d)\n",
                            gdt, service, opcode, arg1, arg2, arg3));

    bzero(gdt->sc_cmd, GDT_CMD_SZ);

    for (retries = GDT_RETRIES; ; ) {
        gccb->gc_service = service;
        gccb->gc_flags = GDT_GCF_INTERNAL;
        
        gdt->sc_set_sema0(gdt);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
                  gccb->gc_cmd_index);
        gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, opcode);

        switch (service) {
          case GDT_CACHESERVICE:
            if (opcode == GDT_IOCTL) {
                gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
                          GDT_IOCTL_SUBFUNC, arg1);
                gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
                          GDT_IOCTL_CHANNEL, arg2);
                gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION +
                          GDT_IOCTL_PARAM_SIZE, (u_int16_t)arg3);
                gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_IOCTL_P_PARAM,
                          gdt_ccb_vtop(gdt, gccb) + 
                          offsetof(struct gdt_ccb, gc_scratch[0]));
            } else {
                gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION +
                          GDT_CACHE_DEVICENO, (u_int16_t)arg1);
                gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
                          GDT_CACHE_BLOCKNO, arg2);
            }
            break;

          case GDT_SCSIRAWSERVICE:
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION +
                      GDT_RAW_DIRECTION, arg1);
            gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_BUS] =
                (u_int8_t)arg2;
            gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_TARGET] =
                (u_int8_t)arg3;
            gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_LUN] =
                (u_int8_t)(arg3 >> 8);
        }

        gdt->sc_cmd_len = GDT_CMD_SZ;
        gdt->sc_cmd_off = 0;
        gdt->sc_cmd_cnt = 0;
        gdt->sc_copy_cmd(gdt, gccb);
        gdt->sc_release_event(gdt);
        DELAY(20);
        if (!gdt_wait(gdt, gccb, GDT_POLL_TIMEOUT))
            return (0);
        if (gdt->sc_status != GDT_S_BSY || --retries == 0)
            break;
        DELAY(1);
    }
    return (gdt->sc_status == GDT_S_OK);
}

static struct gdt_ccb *
gdt_get_ccb(struct gdt_softc *gdt)
{
    struct gdt_ccb *gccb;
    int lock;
    
    GDT_DPRINTF(GDT_D_QUEUE, ("gdt_get_ccb(%p)\n", gdt));

    lock = splcam();
    gccb = SLIST_FIRST(&gdt->sc_free_gccb);
    if (gccb != NULL) {
        SLIST_REMOVE_HEAD(&gdt->sc_free_gccb, sle);
        SLIST_INSERT_HEAD(&gdt->sc_pending_gccb, gccb, sle);
        ++gdt_stat.cmd_index_act;
        if (gdt_stat.cmd_index_act > gdt_stat.cmd_index_max)
            gdt_stat.cmd_index_max = gdt_stat.cmd_index_act;
    }
    splx(lock);
    return (gccb);
}

void
gdt_free_ccb(struct gdt_softc *gdt, struct gdt_ccb *gccb)
{
    int lock;

    GDT_DPRINTF(GDT_D_QUEUE, ("gdt_free_ccb(%p, %p)\n", gdt, gccb));
    
    lock = splcam();
    gccb->gc_flags = GDT_GCF_UNUSED;
    SLIST_REMOVE(&gdt->sc_pending_gccb, gccb, gdt_ccb, sle);
    SLIST_INSERT_HEAD(&gdt->sc_free_gccb, gccb, sle);
    --gdt_stat.cmd_index_act;
    splx(lock);
    if (gdt->sc_state & GDT_SHUTDOWN)
        wakeup(gccb);
}

static u_int32_t
gdt_ccb_vtop(struct gdt_softc *gdt, struct gdt_ccb *gccb)
{
    return (gdt->sc_gccb_busbase
            + (u_int32_t)((caddr_t)gccb - (caddr_t)gdt->sc_gccbs));
}

void    
gdt_next(struct gdt_softc *gdt)
{
    int lock;
    union ccb *ccb;
    gdt_ucmd_t *ucmd;
    struct cam_sim *sim;
    int bus, target, lun;
    int next_cmd;

    struct ccb_scsiio *csio;
    struct ccb_hdr *ccbh;
    struct gdt_ccb *gccb = NULL;
    u_int8_t cmd;

    GDT_DPRINTF(GDT_D_QUEUE, ("gdt_next(%p)\n", gdt));

    lock = splcam();
    if (gdt->sc_test_busy(gdt)) {
        if (!(gdt->sc_state & GDT_POLLING)) {
            splx(lock);
            return;
        }
        while (gdt->sc_test_busy(gdt))
            DELAY(1);
    }

    gdt->sc_cmd_cnt = gdt->sc_cmd_off = 0;
    next_cmd = TRUE;
    for (;;) {
        /* I/Os in queue? controller ready? */
        if (!TAILQ_FIRST(&gdt->sc_ucmd_queue) &&
            !TAILQ_FIRST(&gdt->sc_ccb_queue))
            break;

        /* 1.: I/Os without ccb (IOCTLs) */
        ucmd = TAILQ_FIRST(&gdt->sc_ucmd_queue);
        if (ucmd != NULL) {
            TAILQ_REMOVE(&gdt->sc_ucmd_queue, ucmd, links);
            if ((gccb = gdt_ioctl_cmd(gdt, ucmd, &lock)) == NULL) {
                TAILQ_INSERT_HEAD(&gdt->sc_ucmd_queue, ucmd, links);
                break;
            }
            break;      
            /* wenn mehrere Kdos. zulassen: if (!gdt_polling) continue; */
        }

        /* 2.: I/Os with ccb */
        ccb = (union ccb *)TAILQ_FIRST(&gdt->sc_ccb_queue); 
        /* ist dann immer != NULL, da oben getestet */
        sim = (struct cam_sim *)ccb->ccb_h.ccb_sim_ptr;
        bus = cam_sim_bus(sim);
        target = ccb->ccb_h.target_id;
        lun = ccb->ccb_h.target_lun;
    
        TAILQ_REMOVE(&gdt->sc_ccb_queue, &ccb->ccb_h, sim_links.tqe);
        --gdt_stat.req_queue_act;
        /* ccb->ccb_h.func_code is XPT_SCSI_IO */
        GDT_DPRINTF(GDT_D_QUEUE, ("XPT_SCSI_IO flags 0x%x)\n", 
                                  ccb->ccb_h.flags));
        csio = &ccb->csio;
        ccbh = &ccb->ccb_h;
        cmd  = csio->cdb_io.cdb_bytes[0];
        /* Max CDB length is 12 bytes */
        if (csio->cdb_len > 12) { 
            ccbh->status = CAM_REQ_INVALID;
            --gdt_stat.io_count_act;
            xpt_done(ccb);
        } else if (bus != gdt->sc_virt_bus) {
            /* raw service command */
            if ((gccb = gdt_raw_cmd(gdt, ccb, &lock)) == NULL) {
                TAILQ_INSERT_HEAD(&gdt->sc_ccb_queue, &ccb->ccb_h, 
                                  sim_links.tqe);
                ++gdt_stat.req_queue_act;
                if (gdt_stat.req_queue_act > gdt_stat.req_queue_max)
                    gdt_stat.req_queue_max = gdt_stat.req_queue_act;
                next_cmd = FALSE;
            }
        } else if (target >= GDT_MAX_HDRIVES || 
                   !gdt->sc_hdr[target].hd_present || lun != 0) {
            ccbh->status = CAM_SEL_TIMEOUT;
            --gdt_stat.io_count_act;
            xpt_done(ccb);
        } else {
            /* cache service command */
            if (cmd == READ_6  || cmd == WRITE_6 ||
                cmd == READ_10 || cmd == WRITE_10) {
                if ((gccb = gdt_cache_cmd(gdt, ccb, &lock)) == NULL) {
                    TAILQ_INSERT_HEAD(&gdt->sc_ccb_queue, &ccb->ccb_h, 
                                      sim_links.tqe);
                    ++gdt_stat.req_queue_act;
                    if (gdt_stat.req_queue_act > gdt_stat.req_queue_max)
                        gdt_stat.req_queue_max = gdt_stat.req_queue_act;
                    next_cmd = FALSE;
                }
            } else {
                splx(lock);
                gdt_internal_cache_cmd(gdt, ccb);
                lock = splcam();
            }
        }           
        if ((gdt->sc_state & GDT_POLLING) || !next_cmd)
            break;
    }
    if (gdt->sc_cmd_cnt > 0)
        gdt->sc_release_event(gdt);

    splx(lock);

    if ((gdt->sc_state & GDT_POLLING) && gdt->sc_cmd_cnt > 0) {
        gdt_wait(gdt, gccb, GDT_POLL_TIMEOUT);
    }
}

static struct gdt_ccb *
gdt_raw_cmd(struct gdt_softc *gdt, union ccb *ccb, int *lock)
{
    struct gdt_ccb *gccb;
    struct cam_sim *sim;

    GDT_DPRINTF(GDT_D_CMD, ("gdt_raw_cmd(%p, %p)\n", gdt, ccb));

    if (roundup(GDT_CMD_UNION + GDT_RAW_SZ, sizeof(u_int32_t)) +
        gdt->sc_cmd_off + GDT_DPMEM_COMMAND_OFFSET >
        gdt->sc_ic_all_size) {
        GDT_DPRINTF(GDT_D_INVALID, ("iir%d: gdt_raw_cmd(): DPMEM overflow\n", 
                                    gdt->sc_hanum));
        return (NULL);
    }

    bzero(gdt->sc_cmd, GDT_CMD_SZ);

    gccb = gdt_get_ccb(gdt);
    if (gccb == NULL) {
        GDT_DPRINTF(GDT_D_INVALID, ("iir%d: No free command index found\n",
                                    gdt->sc_hanum));
        return (gccb);
    }
    sim = (struct cam_sim *)ccb->ccb_h.ccb_sim_ptr;
    gccb->gc_ccb = ccb;
    gccb->gc_service = GDT_SCSIRAWSERVICE;
    gccb->gc_flags = GDT_GCF_SCSI;
        
    if (gdt->sc_cmd_cnt == 0)
        gdt->sc_set_sema0(gdt);
    splx(*lock);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
              gccb->gc_cmd_index);
    gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, GDT_WRITE);

    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_DIRECTION,
              (ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN ?
              GDT_DATA_IN : GDT_DATA_OUT);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SDLEN,
              ccb->csio.dxfer_len);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_CLEN,
              ccb->csio.cdb_len);
    bcopy(ccb->csio.cdb_io.cdb_bytes, gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_CMD,
          ccb->csio.cdb_len);
    gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_TARGET] = 
        ccb->ccb_h.target_id;
    gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_LUN] = 
        ccb->ccb_h.target_lun;
    gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_BUS] = 
        cam_sim_bus(sim);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SENSE_LEN,
              sizeof(struct scsi_sense_data));
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SENSE_DATA,
              gdt_ccb_vtop(gdt, gccb) + 
              offsetof(struct gdt_ccb, gc_scratch[0]));
 
    /*
     * If we have any data to send with this command,
     * map it into bus space.
     */
    /* Only use S/G if there is a transfer */
    if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
        if ((ccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) { 
            if ((ccb->ccb_h.flags & CAM_DATA_PHYS) == 0) {
                int s;
                int error;
            
                /* vorher unlock von splcam() ??? */
                s = splsoftvm();
                error =
                    bus_dmamap_load(gdt->sc_buffer_dmat,
                                    gccb->gc_dmamap,
                                    ccb->csio.data_ptr,
                                    ccb->csio.dxfer_len,
                                    gdtexecuteccb,
                                    gccb, /*flags*/0);
                if (error == EINPROGRESS) {
                    xpt_freeze_simq(sim, 1);
                    gccb->gc_state |= CAM_RELEASE_SIMQ;
                }
                splx(s);
            } else {
                struct bus_dma_segment seg; 
            
                /* Pointer to physical buffer */
                seg.ds_addr =
                    (bus_addr_t)ccb->csio.data_ptr;
                seg.ds_len = ccb->csio.dxfer_len;
                gdtexecuteccb(gccb, &seg, 1, 0);
            }
        } else {
            struct bus_dma_segment *segs;

            if ((ccb->ccb_h.flags & CAM_DATA_PHYS) != 0)
                panic("iir%d: iir_action - Physical "
                      "segment pointers unsupported", gdt->sc_hanum);

            if ((ccb->ccb_h.flags & CAM_SG_LIST_PHYS)==0)
                panic("iir%d: iir_action - Virtual "
                      "segment addresses unsupported", gdt->sc_hanum);

            /* Just use the segments provided */
            segs = (struct bus_dma_segment *)ccb->csio.data_ptr;
            gdtexecuteccb(gccb, segs, ccb->csio.sglist_cnt, 0);
        }
    } else {
        gdtexecuteccb(gccb, NULL, 0, 0);
    }

    *lock = splcam();
    return (gccb);
}

static struct gdt_ccb *
gdt_cache_cmd(struct gdt_softc *gdt, union ccb *ccb, int *lock)
{
    struct gdt_ccb *gccb;
    struct cam_sim *sim;
    u_int8_t *cmdp;
    u_int16_t opcode;
    u_int32_t blockno, blockcnt;

    GDT_DPRINTF(GDT_D_CMD, ("gdt_cache_cmd(%p, %p)\n", gdt, ccb));

    if (roundup(GDT_CMD_UNION + GDT_CACHE_SZ, sizeof(u_int32_t)) +
        gdt->sc_cmd_off + GDT_DPMEM_COMMAND_OFFSET >
        gdt->sc_ic_all_size) {
        GDT_DPRINTF(GDT_D_INVALID, ("iir%d: gdt_cache_cmd(): DPMEM overflow\n", 
                                    gdt->sc_hanum));
        return (NULL);
    }

    bzero(gdt->sc_cmd, GDT_CMD_SZ);

    gccb = gdt_get_ccb(gdt);
    if (gccb == NULL) {
        GDT_DPRINTF(GDT_D_DEBUG, ("iir%d: No free command index found\n",
                                  gdt->sc_hanum));
        return (gccb);
    }
    sim = (struct cam_sim *)ccb->ccb_h.ccb_sim_ptr;
    gccb->gc_ccb = ccb;
    gccb->gc_service = GDT_CACHESERVICE;
    gccb->gc_flags = GDT_GCF_SCSI;
        
    if (gdt->sc_cmd_cnt == 0)
        gdt->sc_set_sema0(gdt);
    splx(*lock);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
              gccb->gc_cmd_index);
    cmdp = ccb->csio.cdb_io.cdb_bytes;
    opcode = (*cmdp == WRITE_6 || *cmdp == WRITE_10) ? GDT_WRITE : GDT_READ;
    if ((gdt->sc_state & GDT_SHUTDOWN) && opcode == GDT_WRITE)
        opcode = GDT_WRITE_THR;
    gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, opcode);
 
    gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DEVICENO,
              ccb->ccb_h.target_id);
    if (ccb->csio.cdb_len == 6) {
        struct scsi_rw_6 *rw = (struct scsi_rw_6 *)cmdp;
        blockno = scsi_3btoul(rw->addr) & ((SRW_TOPADDR<<16) | 0xffff);
        blockcnt = rw->length ? rw->length : 0x100;
    } else {
        struct scsi_rw_10 *rw = (struct scsi_rw_10 *)cmdp;
        blockno = scsi_4btoul(rw->addr);
        blockcnt = scsi_2btoul(rw->length);
    }
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKNO,
              blockno);
    gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKCNT,
              blockcnt);

    /*
     * If we have any data to send with this command,
     * map it into bus space.
     */
    /* Only use S/G if there is a transfer */
    if ((ccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) { 
        if ((ccb->ccb_h.flags & CAM_DATA_PHYS) == 0) {
            int s;
            int error;
            
            /* vorher unlock von splcam() ??? */
            s = splsoftvm();
            error =
                bus_dmamap_load(gdt->sc_buffer_dmat,
                                gccb->gc_dmamap,
                                ccb->csio.data_ptr,
                                ccb->csio.dxfer_len,
                                gdtexecuteccb,
                                gccb, /*flags*/0);
            if (error == EINPROGRESS) {
                xpt_freeze_simq(sim, 1);
                gccb->gc_state |= CAM_RELEASE_SIMQ;
            }
            splx(s);
        } else {
            struct bus_dma_segment seg; 
            
            /* Pointer to physical buffer */
            seg.ds_addr =
                (bus_addr_t)ccb->csio.data_ptr;
            seg.ds_len = ccb->csio.dxfer_len;
            gdtexecuteccb(gccb, &seg, 1, 0);
        }
    } else {
        struct bus_dma_segment *segs;

        if ((ccb->ccb_h.flags & CAM_DATA_PHYS) != 0)
            panic("iir%d: iir_action - Physical "
                  "segment pointers unsupported", gdt->sc_hanum);

        if ((ccb->ccb_h.flags & CAM_SG_LIST_PHYS)==0)
            panic("iir%d: iir_action - Virtual "
                  "segment addresses unsupported", gdt->sc_hanum);

        /* Just use the segments provided */
        segs = (struct bus_dma_segment *)ccb->csio.data_ptr;
        gdtexecuteccb(gccb, segs, ccb->csio.sglist_cnt, 0);
    }

    *lock = splcam();
    return (gccb);
}

static struct gdt_ccb *
gdt_ioctl_cmd(struct gdt_softc *gdt, gdt_ucmd_t *ucmd, int *lock)
{
    struct gdt_ccb *gccb;
    u_int32_t cnt;

    GDT_DPRINTF(GDT_D_DEBUG, ("gdt_ioctl_cmd(%p, %p)\n", gdt, ucmd));

    bzero(gdt->sc_cmd, GDT_CMD_SZ);

    gccb = gdt_get_ccb(gdt);
    if (gccb == NULL) {
        GDT_DPRINTF(GDT_D_DEBUG, ("iir%d: No free command index found\n",
                                  gdt->sc_hanum));
        return (gccb);
    }
    gccb->gc_ucmd = ucmd;
    gccb->gc_service = ucmd->service;
    gccb->gc_flags = GDT_GCF_IOCTL;
        
    /* check DPMEM space, copy data buffer from user space */
    if (ucmd->service == GDT_CACHESERVICE) {
        if (ucmd->OpCode == GDT_IOCTL) {
            gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_IOCTL_SZ,
                                      sizeof(u_int32_t));
            cnt = ucmd->u.ioctl.param_size;
            if (cnt > GDT_SCRATCH_SZ) {
                printf("iir%d: Scratch buffer too small (%d/%d)\n", 
                       gdt->sc_hanum, GDT_SCRATCH_SZ, cnt);
                gdt_free_ccb(gdt, gccb);
                return (NULL);
            }
        } else {
            gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_CACHE_SG_LST +
                                      GDT_SG_SZ, sizeof(u_int32_t));
            cnt = ucmd->u.cache.BlockCnt * GDT_SECTOR_SIZE;
            if (cnt > GDT_SCRATCH_SZ) {
                printf("iir%d: Scratch buffer too small (%d/%d)\n", 
                       gdt->sc_hanum, GDT_SCRATCH_SZ, cnt);
                gdt_free_ccb(gdt, gccb);
                return (NULL);
            }
        }
    } else {
        gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_RAW_SG_LST +
                                  GDT_SG_SZ, sizeof(u_int32_t));
        cnt = ucmd->u.raw.sdlen;
        if (cnt + ucmd->u.raw.sense_len > GDT_SCRATCH_SZ) {
            printf("iir%d: Scratch buffer too small (%d/%d)\n", 
                   gdt->sc_hanum, GDT_SCRATCH_SZ, cnt + ucmd->u.raw.sense_len);
            gdt_free_ccb(gdt, gccb);
            return (NULL);
        }
    }
    if (cnt != 0) 
        bcopy(ucmd->data, gccb->gc_scratch, cnt);

    if (gdt->sc_cmd_off + gdt->sc_cmd_len + GDT_DPMEM_COMMAND_OFFSET >
        gdt->sc_ic_all_size) {
        GDT_DPRINTF(GDT_D_INVALID, ("iir%d: gdt_ioctl_cmd(): DPMEM overflow\n", 
                                    gdt->sc_hanum));
        gdt_free_ccb(gdt, gccb);
        return (NULL);
    }

    if (gdt->sc_cmd_cnt == 0)
        gdt->sc_set_sema0(gdt);
    splx(*lock);

    /* fill cmd structure */
    gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
              gccb->gc_cmd_index);
    gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, 
              ucmd->OpCode);

    if (ucmd->service == GDT_CACHESERVICE) {
        if (ucmd->OpCode == GDT_IOCTL) {
            /* IOCTL */
            gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION + GDT_IOCTL_PARAM_SIZE,
                      ucmd->u.ioctl.param_size);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_IOCTL_SUBFUNC,
                      ucmd->u.ioctl.subfunc);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_IOCTL_CHANNEL,
                      ucmd->u.ioctl.channel);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_IOCTL_P_PARAM,
                      gdt_ccb_vtop(gdt, gccb) + 
                      offsetof(struct gdt_ccb, gc_scratch[0]));
        } else {
            /* cache service command */
            gdt_enc16(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DEVICENO,
                      ucmd->u.cache.DeviceNo);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKNO,
                      ucmd->u.cache.BlockNo);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_BLOCKCNT,
                      ucmd->u.cache.BlockCnt);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DESTADDR,
                      0xffffffffUL);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_CANZ,
                      1);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_LST + 
                      GDT_SG_PTR, gdt_ccb_vtop(gdt, gccb) + 
                      offsetof(struct gdt_ccb, gc_scratch[0]));
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_LST +
                      GDT_SG_LEN, ucmd->u.cache.BlockCnt * GDT_SECTOR_SIZE);
        }
    } else {
        /* raw service command */
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_DIRECTION,
                  ucmd->u.raw.direction);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SDATA,
                  0xffffffffUL);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SDLEN,
                  ucmd->u.raw.sdlen);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_CLEN,
                  ucmd->u.raw.clen);
        bcopy(ucmd->u.raw.cmd, gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_CMD,
              12);
        gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_TARGET] = 
            ucmd->u.raw.target;
        gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_LUN] = 
            ucmd->u.raw.lun;
        gdt->sc_cmd[GDT_CMD_UNION + GDT_RAW_BUS] = 
            ucmd->u.raw.bus;
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SENSE_LEN,
                  ucmd->u.raw.sense_len);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SENSE_DATA,
                  gdt_ccb_vtop(gdt, gccb) + 
                  offsetof(struct gdt_ccb, gc_scratch[ucmd->u.raw.sdlen]));
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_RANZ,
                  1);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_LST + 
                  GDT_SG_PTR, gdt_ccb_vtop(gdt, gccb) + 
                  offsetof(struct gdt_ccb, gc_scratch[0]));
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_LST +
                  GDT_SG_LEN, ucmd->u.raw.sdlen);
    }

    *lock = splcam();
    gdt_stat.sg_count_act = 1;
    gdt->sc_copy_cmd(gdt, gccb);
    return (gccb);
}

static void 
gdt_internal_cache_cmd(struct gdt_softc *gdt,union ccb *ccb)
{
    int t;

    t = ccb->ccb_h.target_id;
    GDT_DPRINTF(GDT_D_CMD, ("gdt_internal_cache_cmd(%p, %p, 0x%x, %d)\n", 
        gdt, ccb, ccb->csio.cdb_io.cdb_bytes[0], t));

    switch (ccb->csio.cdb_io.cdb_bytes[0]) {
      case TEST_UNIT_READY:
      case START_STOP:
        break;
      case REQUEST_SENSE:
        GDT_DPRINTF(GDT_D_MISC, ("REQUEST_SENSE\n"));
        break;
      case INQUIRY:
        {
            struct scsi_inquiry_data *inq;

            inq = (struct scsi_inquiry_data *)ccb->csio.data_ptr;       
            bzero(inq, sizeof(struct scsi_inquiry_data));
            inq->device = (gdt->sc_hdr[t].hd_devtype & 4) ?
                T_CDROM : T_DIRECT;
            inq->dev_qual2 = (gdt->sc_hdr[t].hd_devtype & 1) ? 0x80 : 0;
            inq->version = SCSI_REV_2;
            inq->response_format = 2; 
            inq->additional_length = 32; 
            inq->flags = SID_CmdQue | SID_Sync; 
            strcpy(inq->vendor, "IIR     ");
            sprintf(inq->product, "Host Drive   #%02d", t);
            strcpy(inq->revision, "   ");
            break;
        }
      case MODE_SENSE_6:
        {
            struct mpd_data {
                struct scsi_mode_hdr_6 hd;
                struct scsi_mode_block_descr bd;
                struct scsi_control_page cp;
            } *mpd;
            u_int8_t page;

            mpd = (struct mpd_data *)ccb->csio.data_ptr;        
            bzero(mpd, sizeof(struct mpd_data));
            mpd->hd.datalen = sizeof(struct scsi_mode_hdr_6) +
                sizeof(struct scsi_mode_block_descr);
            mpd->hd.dev_specific = (gdt->sc_hdr[t].hd_devtype & 2) ? 0x80 : 0;
            mpd->hd.block_descr_len = sizeof(struct scsi_mode_block_descr);
            mpd->bd.block_len[0] = (GDT_SECTOR_SIZE & 0x00ff0000) >> 16;
            mpd->bd.block_len[1] = (GDT_SECTOR_SIZE & 0x0000ff00) >> 8;
            mpd->bd.block_len[2] = (GDT_SECTOR_SIZE & 0x000000ff);
            page=((struct scsi_mode_sense_6 *)ccb->csio.cdb_io.cdb_bytes)->page;
            switch (page) {
              default:
                GDT_DPRINTF(GDT_D_MISC, ("MODE_SENSE_6: page 0x%x\n", page));
                break;
            }
            break;
        }
      case READ_CAPACITY:
        {
            struct scsi_read_capacity_data *rcd;
              
            rcd = (struct scsi_read_capacity_data *)ccb->csio.data_ptr; 
            bzero(rcd, sizeof(struct scsi_read_capacity_data));
            scsi_ulto4b(gdt->sc_hdr[t].hd_size - 1, rcd->addr);
            scsi_ulto4b(GDT_SECTOR_SIZE, rcd->length);
            break;
        }
      default:
        GDT_DPRINTF(GDT_D_MISC, ("gdt_internal_cache_cmd(%d) unknown\n", 
                                    ccb->csio.cdb_io.cdb_bytes[0]));
        break;
    }
    ccb->ccb_h.status = CAM_REQ_CMP;
    --gdt_stat.io_count_act;
    xpt_done(ccb);
}

static void     
gdtmapmem(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
    bus_addr_t *busaddrp;
    
    busaddrp = (bus_addr_t *)arg;
    *busaddrp = dm_segs->ds_addr;
}

static void     
gdtexecuteccb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
    struct gdt_ccb *gccb;
    union ccb *ccb;
    struct gdt_softc *gdt;
    int i, lock;

    lock = splcam();

    gccb = (struct gdt_ccb *)arg;
    ccb = gccb->gc_ccb;
    gdt = cam_sim_softc((struct cam_sim *)ccb->ccb_h.ccb_sim_ptr);

    GDT_DPRINTF(GDT_D_CMD, ("gdtexecuteccb(%p, %p, %p, %d, %d)\n", 
                            gdt, gccb, dm_segs, nseg, error));
    gdt_stat.sg_count_act = nseg;
    if (nseg > gdt_stat.sg_count_max)
        gdt_stat.sg_count_max = nseg;

    /* Copy the segments into our SG list */
    if (gccb->gc_service == GDT_CACHESERVICE) {
        for (i = 0; i < nseg; ++i) {
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_LST +
                      i * GDT_SG_SZ + GDT_SG_PTR, dm_segs->ds_addr);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_LST +
                      i * GDT_SG_SZ + GDT_SG_LEN, dm_segs->ds_len);
            dm_segs++;
        }
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_SG_CANZ,      
                  nseg);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_CACHE_DESTADDR, 
                  0xffffffffUL);

        gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_CACHE_SG_LST +
                                  nseg * GDT_SG_SZ, sizeof(u_int32_t));
    } else {
        for (i = 0; i < nseg; ++i) {
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_LST +
                      i * GDT_SG_SZ + GDT_SG_PTR, dm_segs->ds_addr);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_LST +
                      i * GDT_SG_SZ + GDT_SG_LEN, dm_segs->ds_len);
            dm_segs++;
        }
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SG_RANZ,        
                  nseg);
        gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_RAW_SDATA, 
                  0xffffffffUL);

        gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_RAW_SG_LST +
                                  nseg * GDT_SG_SZ, sizeof(u_int32_t));
    }

    if (nseg != 0) {
        bus_dmasync_op_t op;

        if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
            op = BUS_DMASYNC_PREREAD;
        else
            op = BUS_DMASYNC_PREWRITE;
        bus_dmamap_sync(gdt->sc_buffer_dmat, gccb->gc_dmamap, op);
    }
    
    /* We must NOT abort the command here if CAM_REQ_INPROG is not set,
     * because command semaphore is already set!
     */
    
    ccb->ccb_h.status |= CAM_SIM_QUEUED;
    /* timeout handling */
    callout_reset(&ccb->ccb_h.timeout_ch, (ccb->ccb_h.timeout * hz) / 1000,
        iir_timeout, gccb);

    gdt->sc_copy_cmd(gdt, gccb);
    splx(lock);
}


static void     
iir_action( struct cam_sim *sim, union ccb *ccb )
{
    struct gdt_softc *gdt;
    int lock, bus, target, lun;

    gdt = (struct gdt_softc *)cam_sim_softc( sim );
    ccb->ccb_h.ccb_sim_ptr = sim;
    bus = cam_sim_bus(sim);
    target = ccb->ccb_h.target_id;
    lun = ccb->ccb_h.target_lun;
    GDT_DPRINTF(GDT_D_CMD, 
                ("iir_action(%p) func 0x%x cmd 0x%x bus %d target %d lun %d\n", 
                 gdt, ccb->ccb_h.func_code, ccb->csio.cdb_io.cdb_bytes[0], 
                 bus, target, lun)); 
    ++gdt_stat.io_count_act;
    if (gdt_stat.io_count_act > gdt_stat.io_count_max)
        gdt_stat.io_count_max = gdt_stat.io_count_act;

    switch (ccb->ccb_h.func_code) {
      case XPT_SCSI_IO:
        lock = splcam();
        TAILQ_INSERT_TAIL(&gdt->sc_ccb_queue, &ccb->ccb_h, sim_links.tqe);
        ++gdt_stat.req_queue_act;
        if (gdt_stat.req_queue_act > gdt_stat.req_queue_max)
            gdt_stat.req_queue_max = gdt_stat.req_queue_act;
        splx(lock);
        gdt_next(gdt);
        break;
      case XPT_RESET_DEV:   /* Bus Device Reset the specified SCSI device */
      case XPT_ABORT:                       /* Abort the specified CCB */
        /* XXX Implement */
        ccb->ccb_h.status = CAM_REQ_INVALID;
        --gdt_stat.io_count_act;
        xpt_done(ccb);
        break;
      case XPT_SET_TRAN_SETTINGS:
        ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
        --gdt_stat.io_count_act;
        xpt_done(ccb);  
        break;
      case XPT_GET_TRAN_SETTINGS:
        /* Get default/user set transfer settings for the target */
          {
              struct        ccb_trans_settings *cts;
              u_int target_mask;
              
              cts = &ccb->cts;
              target_mask = 0x01 << target;
              if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) { 
                  cts->flags = CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB;
                  cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
                  cts->sync_period = 25; /* 10MHz */
                  if (cts->sync_period != 0)
                      cts->sync_offset = 15;
                  
                  cts->valid = CCB_TRANS_SYNC_RATE_VALID
                      | CCB_TRANS_SYNC_OFFSET_VALID
                      | CCB_TRANS_BUS_WIDTH_VALID
                      | CCB_TRANS_DISC_VALID
                      | CCB_TRANS_TQ_VALID;
                  ccb->ccb_h.status = CAM_REQ_CMP;
              } else {
                  ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
              }
              --gdt_stat.io_count_act;
              xpt_done(ccb);
              break;
          }
      case XPT_CALC_GEOMETRY:
          {
              struct ccb_calc_geometry *ccg;
              u_int32_t secs_per_cylinder;

              ccg = &ccb->ccg;
              ccg->heads = gdt->sc_hdr[target].hd_heads;
              ccg->secs_per_track = gdt->sc_hdr[target].hd_secs;
              secs_per_cylinder = ccg->heads * ccg->secs_per_track;
              ccg->cylinders = ccg->volume_size / secs_per_cylinder;
              ccb->ccb_h.status = CAM_REQ_CMP;
              --gdt_stat.io_count_act;
              xpt_done(ccb);
              break;
          }
      case XPT_RESET_BUS:           /* Reset the specified SCSI bus */
          {
              /* XXX Implement */
              ccb->ccb_h.status = CAM_REQ_CMP;
              --gdt_stat.io_count_act;
              xpt_done(ccb);
              break;
          }
      case XPT_TERM_IO:             /* Terminate the I/O process */
        /* XXX Implement */
        ccb->ccb_h.status = CAM_REQ_INVALID;
        --gdt_stat.io_count_act;
        xpt_done(ccb);
        break;
      case XPT_PATH_INQ:            /* Path routing inquiry */
          {
              struct ccb_pathinq *cpi = &ccb->cpi;
              
              cpi->version_num = 1;
              cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE;
              cpi->hba_inquiry |= PI_WIDE_16;
              cpi->target_sprt = 1;
              cpi->hba_misc = 0;
              cpi->hba_eng_cnt = 0;
              if (bus == gdt->sc_virt_bus)
                  cpi->max_target = GDT_MAX_HDRIVES - 1;
              else if (gdt->sc_class & GDT_FC)
                  cpi->max_target = GDT_MAXID_FC - 1;
              else
                  cpi->max_target = GDT_MAXID - 1;
              cpi->max_lun = 7;
              cpi->unit_number = cam_sim_unit(sim);
              cpi->bus_id = bus;
              cpi->initiator_id = 
                  (bus == gdt->sc_virt_bus ? 127 : gdt->sc_bus_id[bus]);
              cpi->base_transfer_speed = 3300;
              strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
              strncpy(cpi->hba_vid, "Intel Corp.", HBA_IDLEN);
              strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
              cpi->ccb_h.status = CAM_REQ_CMP;
              --gdt_stat.io_count_act;
              xpt_done(ccb);
              break;
          }
      default:
        GDT_DPRINTF(GDT_D_INVALID, ("gdt_next(%p) cmd 0x%x invalid\n", 
                                    gdt, ccb->ccb_h.func_code));
        ccb->ccb_h.status = CAM_REQ_INVALID;
        --gdt_stat.io_count_act;
        xpt_done(ccb);
        break;
    }
}

static void     
iir_poll( struct cam_sim *sim )
{
    struct gdt_softc *gdt;

    gdt = (struct gdt_softc *)cam_sim_softc( sim );
    GDT_DPRINTF(GDT_D_CMD, ("iir_poll sim %p gdt %p\n", sim, gdt));
    iir_intr(gdt);
}

static void     
iir_timeout(void *arg)
{
    GDT_DPRINTF(GDT_D_TIMEOUT, ("iir_timeout(%p)\n", arg));
}

static void
iir_watchdog(void *arg)
{
    struct gdt_softc *gdt;

    gdt = (struct gdt_softc *)arg;
    GDT_DPRINTF(GDT_D_DEBUG, ("iir_watchdog(%p)\n", gdt));

    {
        int ccbs = 0, ucmds = 0, frees = 0, pends = 0;
        struct gdt_ccb *p;
        struct ccb_hdr *h;
        struct gdt_ucmd *u;

        for (h = TAILQ_FIRST(&gdt->sc_ccb_queue); h != NULL; 
             h = TAILQ_NEXT(h, sim_links.tqe))
            ccbs++;
        for (u = TAILQ_FIRST(&gdt->sc_ucmd_queue); u != NULL; 
             u = TAILQ_NEXT(u, links))
            ucmds++;
        for (p = SLIST_FIRST(&gdt->sc_free_gccb); p != NULL; 
             p = SLIST_NEXT(p, sle))
            frees++;
        for (p = SLIST_FIRST(&gdt->sc_pending_gccb); p != NULL; 
             p = SLIST_NEXT(p, sle))
            pends++;

        GDT_DPRINTF(GDT_D_TIMEOUT, ("ccbs %d ucmds %d frees %d pends %d\n",
                                    ccbs, ucmds, frees, pends));
    }

    callout_reset(&gdt->watchdog_timer, hz * 15, iir_watchdog, gdt);
}

static void     
iir_shutdown( void *arg, int howto )
{
    struct gdt_softc *gdt;
    struct gdt_ccb *gccb;
    gdt_ucmd_t *ucmd;
    int lock, i;

    gdt = (struct gdt_softc *)arg;
    GDT_DPRINTF(GDT_D_CMD, ("iir_shutdown(%p, %d)\n", gdt, howto));

    printf("iir%d: Flushing all Host Drives. Please wait ...  ",
           gdt->sc_hanum);

    /* allocate ucmd buffer */
    ucmd = malloc(sizeof(gdt_ucmd_t), M_DEVBUF, M_INTWAIT | M_ZERO);

    /* wait for pending IOs */
    lock = splcam();
    gdt->sc_state = GDT_SHUTDOWN;
    splx(lock);
    if ((gccb = SLIST_FIRST(&gdt->sc_pending_gccb)) != NULL)
        (void) tsleep((void *)gccb, PCATCH, "iirshw", 100 * hz);

    /* flush */
    for (i = 0; i < GDT_MAX_HDRIVES; ++i) {
        if (gdt->sc_hdr[i].hd_present) {
            ucmd->service = GDT_CACHESERVICE;
            ucmd->OpCode = GDT_FLUSH;
            ucmd->u.cache.DeviceNo = i;
            lock = splcam();
            TAILQ_INSERT_TAIL(&gdt->sc_ucmd_queue, ucmd, links);
            ucmd->complete_flag = FALSE;
            splx(lock);
            gdt_next(gdt);
            if (!ucmd->complete_flag)
                (void) tsleep((void *)ucmd, PCATCH, "iirshw", 10*hz);
        }
    }

    free(ucmd, M_DEVBUF);
    printf("Done.\n");
}

void
iir_intr(void *arg)
{
    struct gdt_softc *gdt = arg;
    struct gdt_intr_ctx ctx;
    int lock = 0;
    struct gdt_ccb *gccb;
    gdt_ucmd_t *ucmd;
    u_int32_t cnt;

    GDT_DPRINTF(GDT_D_INTR, ("gdt_intr(%p)\n", gdt));

    /* If polling and we were not called from gdt_wait, just return */
    if ((gdt->sc_state & GDT_POLLING) &&
        !(gdt->sc_state & GDT_POLL_WAIT))
        return;

    if (!(gdt->sc_state & GDT_POLLING)) 
        lock = splcam();
    gdt_wait_index = 0;

    ctx.istatus = gdt->sc_get_status(gdt);
    if (!ctx.istatus) {
        if (!(gdt->sc_state & GDT_POLLING)) 
            splx(lock);
        gdt->sc_status = GDT_S_NO_STATUS;
        return;
    }

    gdt->sc_intr(gdt, &ctx);

    gdt->sc_status = ctx.cmd_status;
    gdt->sc_service = ctx.service;
    gdt->sc_info = ctx.info;
    gdt->sc_info2 = ctx.info2;

    if (gdt->sc_state & GDT_POLL_WAIT) { 
        gdt_wait_gdt = gdt;
        gdt_wait_index = ctx.istatus;
    }

    if (ctx.istatus == GDT_ASYNCINDEX) {
        gdt_async_event(gdt, ctx.service);
        if (!(gdt->sc_state & GDT_POLLING)) 
            splx(lock);
        return;
    }
    if (ctx.istatus == GDT_SPEZINDEX) {
        GDT_DPRINTF(GDT_D_INVALID, 
                    ("iir%d: Service unknown or not initialized!\n", 
                     gdt->sc_hanum));   
        gdt->sc_dvr.size = sizeof(gdt->sc_dvr.eu.driver);
        gdt->sc_dvr.eu.driver.ionode = gdt->sc_hanum;
        gdt_store_event(GDT_ES_DRIVER, 4, &gdt->sc_dvr);
        if (!(gdt->sc_state & GDT_POLLING)) 
            splx(lock);
        return;
    }

    gccb = &gdt->sc_gccbs[ctx.istatus - 2];
    ctx.service = gccb->gc_service;

    switch (gccb->gc_flags) {
      case GDT_GCF_UNUSED:
        GDT_DPRINTF(GDT_D_INVALID, ("iir%d: Index (%d) to unused command!\n",
                    gdt->sc_hanum, ctx.istatus));
        gdt->sc_dvr.size = sizeof(gdt->sc_dvr.eu.driver);
        gdt->sc_dvr.eu.driver.ionode = gdt->sc_hanum;
        gdt->sc_dvr.eu.driver.index = ctx.istatus;
        gdt_store_event(GDT_ES_DRIVER, 1, &gdt->sc_dvr);
        gdt_free_ccb(gdt, gccb);
        /* fallthrough */

      case GDT_GCF_INTERNAL:
        if (!(gdt->sc_state & GDT_POLLING)) 
            splx(lock);
        break;

      case GDT_GCF_IOCTL:
        ucmd = gccb->gc_ucmd; 
        if (gdt->sc_status == GDT_S_BSY) {
            GDT_DPRINTF(GDT_D_DEBUG, ("iir_intr(%p) ioctl: gccb %p busy\n", 
                                      gdt, gccb));
            TAILQ_INSERT_HEAD(&gdt->sc_ucmd_queue, ucmd, links);
            if (!(gdt->sc_state & GDT_POLLING)) 
                splx(lock);
        } else {
            ucmd->status = gdt->sc_status;
            ucmd->info = gdt->sc_info;
            ucmd->complete_flag = TRUE;
            if (ucmd->service == GDT_CACHESERVICE) {
                if (ucmd->OpCode == GDT_IOCTL) {
                    cnt = ucmd->u.ioctl.param_size;
                    if (cnt != 0)
                        bcopy(gccb->gc_scratch, ucmd->data, cnt);
                } else {
                    cnt = ucmd->u.cache.BlockCnt * GDT_SECTOR_SIZE;
                    if (cnt != 0)
                        bcopy(gccb->gc_scratch, ucmd->data, cnt);
                }
            } else {
                cnt = ucmd->u.raw.sdlen;
                if (cnt != 0)
                    bcopy(gccb->gc_scratch, ucmd->data, cnt);
                if (ucmd->u.raw.sense_len != 0) 
                    bcopy(gccb->gc_scratch, ucmd->data, cnt);
            }
            gdt_free_ccb(gdt, gccb);
            if (!(gdt->sc_state & GDT_POLLING)) 
                splx(lock);
            /* wakeup */
            wakeup(ucmd);
        }
        gdt_next(gdt); 
        break;

      default:
        gdt_free_ccb(gdt, gccb);
        gdt_sync_event(gdt, ctx.service, ctx.istatus, gccb);
        if (!(gdt->sc_state & GDT_POLLING)) 
            splx(lock);
        gdt_next(gdt); 
        break;
    }
}

int
gdt_async_event(struct gdt_softc *gdt, int service)
{
    struct gdt_ccb *gccb;

    GDT_DPRINTF(GDT_D_INTR, ("gdt_async_event(%p, %d)\n", gdt, service));

    if (service == GDT_SCREENSERVICE) {
        if (gdt->sc_status == GDT_MSG_REQUEST) {
            while (gdt->sc_test_busy(gdt))
                DELAY(1);
            bzero(gdt->sc_cmd, GDT_CMD_SZ);
            gccb = gdt_get_ccb(gdt);
            if (gccb == NULL) {
                printf("iir%d: No free command index found\n",
                       gdt->sc_hanum);
                return (1);
            }
            gccb->gc_service = service;
            gccb->gc_flags = GDT_GCF_SCREEN;
            gdt->sc_set_sema0(gdt);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
                      gccb->gc_cmd_index);
            gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, GDT_READ);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_HANDLE,
                      GDT_MSG_INV_HANDLE);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_ADDR,
                      gdt_ccb_vtop(gdt, gccb) + 
                      offsetof(struct gdt_ccb, gc_scratch[0]));
            gdt->sc_cmd_off = 0;
            gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_SCREEN_SZ, 
                                      sizeof(u_int32_t));
            gdt->sc_cmd_cnt = 0;
            gdt->sc_copy_cmd(gdt, gccb);
            printf("iir%d: [PCI %d/%d] ",
                gdt->sc_hanum,gdt->sc_bus,gdt->sc_slot);
            gdt->sc_release_event(gdt);
        }

    } else {
        if ((gdt->sc_fw_vers & 0xff) >= 0x1a) {
            gdt->sc_dvr.size = 0;
            gdt->sc_dvr.eu.async.ionode = gdt->sc_hanum;
            gdt->sc_dvr.eu.async.status  = gdt->sc_status;
            /* severity and event_string already set! */
        } else {        
            gdt->sc_dvr.size = sizeof(gdt->sc_dvr.eu.async);
            gdt->sc_dvr.eu.async.ionode   = gdt->sc_hanum;
            gdt->sc_dvr.eu.async.service = service;
            gdt->sc_dvr.eu.async.status  = gdt->sc_status;
            gdt->sc_dvr.eu.async.info    = gdt->sc_info;
            *(u_int32_t *)gdt->sc_dvr.eu.async.scsi_coord  = gdt->sc_info2;
        }
        gdt_store_event(GDT_ES_ASYNC, service, &gdt->sc_dvr);
        printf("iir%d: %s\n", gdt->sc_hanum, gdt->sc_dvr.event_string);
    }
    
    return (0);
}

int
gdt_sync_event(struct gdt_softc *gdt, int service, 
               u_int8_t index, struct gdt_ccb *gccb)
{
    union ccb *ccb;
    bus_dmasync_op_t op;

    GDT_DPRINTF(GDT_D_INTR,
                ("gdt_sync_event(%p, %d, %d, %p)\n", gdt,service,index,gccb));

    ccb = gccb->gc_ccb;

    if (service == GDT_SCREENSERVICE) {
        u_int32_t msg_len;

        msg_len = gdt_dec32(gccb->gc_scratch + GDT_SCR_MSG_LEN);
        if (msg_len)
            if (!(gccb->gc_scratch[GDT_SCR_MSG_ANSWER] && 
                  gccb->gc_scratch[GDT_SCR_MSG_EXT])) {
                gccb->gc_scratch[GDT_SCR_MSG_TEXT + msg_len] = '\0';
                printf("%s",&gccb->gc_scratch[GDT_SCR_MSG_TEXT]);
            }

        if (gccb->gc_scratch[GDT_SCR_MSG_EXT] && 
            !gccb->gc_scratch[GDT_SCR_MSG_ANSWER]) {
            while (gdt->sc_test_busy(gdt))
                DELAY(1);
            bzero(gdt->sc_cmd, GDT_CMD_SZ);
            gccb = gdt_get_ccb(gdt);
            if (gccb == NULL) {
                printf("iir%d: No free command index found\n",
                       gdt->sc_hanum);
                return (1);
            }
            gccb->gc_service = service;
            gccb->gc_flags = GDT_GCF_SCREEN;
            gdt->sc_set_sema0(gdt);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
                      gccb->gc_cmd_index);
            gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, GDT_READ);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_HANDLE,
                      gccb->gc_scratch[GDT_SCR_MSG_HANDLE]);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_ADDR,
                      gdt_ccb_vtop(gdt, gccb) + 
                      offsetof(struct gdt_ccb, gc_scratch[0]));
            gdt->sc_cmd_off = 0;
            gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_SCREEN_SZ, 
                                      sizeof(u_int32_t));
            gdt->sc_cmd_cnt = 0;
            gdt->sc_copy_cmd(gdt, gccb);
            gdt->sc_release_event(gdt);
            return (0);
        }

        if (gccb->gc_scratch[GDT_SCR_MSG_ANSWER] && 
            gdt_dec32(gccb->gc_scratch + GDT_SCR_MSG_ALEN)) {
            /* default answers (getchar() not possible) */
            if (gdt_dec32(gccb->gc_scratch + GDT_SCR_MSG_ALEN) == 1) {
                gdt_enc32(gccb->gc_scratch + GDT_SCR_MSG_ALEN, 0);
                gdt_enc32(gccb->gc_scratch + GDT_SCR_MSG_LEN, 1);
                gccb->gc_scratch[GDT_SCR_MSG_TEXT] = 0;
            } else {
                gdt_enc32(gccb->gc_scratch + GDT_SCR_MSG_ALEN, 
                          gdt_dec32(gccb->gc_scratch + GDT_SCR_MSG_ALEN) - 2);
                gdt_enc32(gccb->gc_scratch + GDT_SCR_MSG_LEN, 2);
                gccb->gc_scratch[GDT_SCR_MSG_TEXT] = 1;
                gccb->gc_scratch[GDT_SCR_MSG_TEXT + 1] = 0;
            }
            gccb->gc_scratch[GDT_SCR_MSG_EXT] = 0;
            gccb->gc_scratch[GDT_SCR_MSG_ANSWER] = 0;
            while (gdt->sc_test_busy(gdt))
                DELAY(1);
            bzero(gdt->sc_cmd, GDT_CMD_SZ);
            gccb = gdt_get_ccb(gdt);
            if (gccb == NULL) {
                printf("iir%d: No free command index found\n",
                       gdt->sc_hanum);
                return (1);
            }
            gccb->gc_service = service;
            gccb->gc_flags = GDT_GCF_SCREEN;
            gdt->sc_set_sema0(gdt);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_COMMANDINDEX,
                      gccb->gc_cmd_index);
            gdt_enc16(gdt->sc_cmd + GDT_CMD_OPCODE, GDT_WRITE);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_HANDLE,
                      gccb->gc_scratch[GDT_SCR_MSG_HANDLE]);
            gdt_enc32(gdt->sc_cmd + GDT_CMD_UNION + GDT_SCREEN_MSG_ADDR,
                      gdt_ccb_vtop(gdt, gccb) + 
                      offsetof(struct gdt_ccb, gc_scratch[0]));
            gdt->sc_cmd_off = 0;
            gdt->sc_cmd_len = roundup(GDT_CMD_UNION + GDT_SCREEN_SZ, 
                                      sizeof(u_int32_t));
            gdt->sc_cmd_cnt = 0;
            gdt->sc_copy_cmd(gdt, gccb);
            gdt->sc_release_event(gdt);
            return (0);
        }
        printf("\n");
        return (0);
    } else {
        callout_stop(&ccb->ccb_h.timeout_ch);
        if (gdt->sc_status == GDT_S_BSY) {
            GDT_DPRINTF(GDT_D_DEBUG, ("gdt_sync_event(%p) gccb %p busy\n", 
                                      gdt, gccb));
            TAILQ_INSERT_HEAD(&gdt->sc_ccb_queue, &ccb->ccb_h, sim_links.tqe);
            ++gdt_stat.req_queue_act;
            if (gdt_stat.req_queue_act > gdt_stat.req_queue_max)
                gdt_stat.req_queue_max = gdt_stat.req_queue_act;
            return (2);
        }

        if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
            op = BUS_DMASYNC_POSTREAD;
        else
            op = BUS_DMASYNC_POSTWRITE;
        bus_dmamap_sync(gdt->sc_buffer_dmat, gccb->gc_dmamap, op);

        ccb->csio.resid = 0;
        if (gdt->sc_status == GDT_S_OK) {
            ccb->ccb_h.status = CAM_REQ_CMP;
        } else {
            /* error */
            if (gccb->gc_service == GDT_CACHESERVICE) {
                ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR | CAM_AUTOSNS_VALID;
                ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
                bzero(&ccb->csio.sense_data, ccb->csio.sense_len);
                ccb->csio.sense_data.error_code =
                    SSD_CURRENT_ERROR | SSD_ERRCODE_VALID;
                ccb->csio.sense_data.flags = SSD_KEY_NOT_READY;

                gdt->sc_dvr.size = sizeof(gdt->sc_dvr.eu.sync);
                gdt->sc_dvr.eu.sync.ionode  = gdt->sc_hanum;
                gdt->sc_dvr.eu.sync.service = service;
                gdt->sc_dvr.eu.sync.status  = gdt->sc_status;
                gdt->sc_dvr.eu.sync.info    = gdt->sc_info;
                gdt->sc_dvr.eu.sync.hostdrive = ccb->ccb_h.target_id;
                if (gdt->sc_status >= 0x8000)
                    gdt_store_event(GDT_ES_SYNC, 0, &gdt->sc_dvr);
                else
                    gdt_store_event(GDT_ES_SYNC, service, &gdt->sc_dvr);
            } else {
                /* raw service */
                if (gdt->sc_status != GDT_S_RAW_SCSI || gdt->sc_info >= 0x100) {
                    ccb->ccb_h.status = CAM_SEL_TIMEOUT;
                } else {
                    ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR|CAM_AUTOSNS_VALID;
                    ccb->csio.scsi_status = gdt->sc_info;
                    bcopy(gccb->gc_scratch, &ccb->csio.sense_data,
                          ccb->csio.sense_len);
                }
            }
        }
        --gdt_stat.io_count_act;
        xpt_done(ccb);
    }
    return (0);
}

/* Controller event handling functions */
gdt_evt_str *gdt_store_event(u_int16_t source, u_int16_t idx,
                             gdt_evt_data *evt)
{
    gdt_evt_str *e;
    struct timeval tv;

    GDT_DPRINTF(GDT_D_MISC, ("gdt_store_event(%d, %d)\n", source, idx));
    if (source == 0)                        /* no source -> no event */
        return 0;

    if (ebuffer[elastidx].event_source == source &&
        ebuffer[elastidx].event_idx == idx &&
        ((evt->size != 0 && ebuffer[elastidx].event_data.size != 0 &&
          !memcmp((char *)&ebuffer[elastidx].event_data.eu,
                  (char *)&evt->eu, evt->size)) ||
         (evt->size == 0 && ebuffer[elastidx].event_data.size == 0 &&
          !strcmp((char *)&ebuffer[elastidx].event_data.event_string,
                  (char *)&evt->event_string)))) { 
        e = &ebuffer[elastidx];
        getmicrotime(&tv);
        e->last_stamp = tv.tv_sec;
        ++e->same_count;
    } else {
        if (ebuffer[elastidx].event_source != 0) {  /* entry not free ? */
            ++elastidx;
            if (elastidx == GDT_MAX_EVENTS)
                elastidx = 0;
            if (elastidx == eoldidx) {              /* reached mark ? */
                ++eoldidx;
                if (eoldidx == GDT_MAX_EVENTS)
                    eoldidx = 0;
            }
        }
        e = &ebuffer[elastidx];
        e->event_source = source;
        e->event_idx = idx;
        getmicrotime(&tv);
        e->first_stamp = e->last_stamp = tv.tv_sec;
        e->same_count = 1;
        e->event_data = *evt;
        e->application = 0;
    }
    return e;
}

int gdt_read_event(int handle, gdt_evt_str *estr)
{
    gdt_evt_str *e;
    int eindex, lock;
    
    GDT_DPRINTF(GDT_D_MISC, ("gdt_read_event(%d)\n", handle));
    lock = splcam();
    if (handle == -1)
        eindex = eoldidx;
    else
        eindex = handle;
    estr->event_source = 0;

    if (eindex >= GDT_MAX_EVENTS) {
        splx(lock);
        return eindex;
    }
    e = &ebuffer[eindex];
    if (e->event_source != 0) {
        if (eindex != elastidx) {
            if (++eindex == GDT_MAX_EVENTS)
                eindex = 0;
        } else {
            eindex = -1;
        }
        memcpy(estr, e, sizeof(gdt_evt_str));
    }
    splx(lock);
    return eindex;
}

void gdt_readapp_event(u_int8_t application, gdt_evt_str *estr)
{
    gdt_evt_str *e;
    int found = FALSE;
    int eindex, lock;
    
    GDT_DPRINTF(GDT_D_MISC, ("gdt_readapp_event(%d)\n", application));
    lock = splcam();
    eindex = eoldidx;
    for (;;) {
        e = &ebuffer[eindex];
        if (e->event_source == 0)
            break;
        if ((e->application & application) == 0) {
            e->application |= application;
            found = TRUE;
            break;
        }
        if (eindex == elastidx)
            break;
        if (++eindex == GDT_MAX_EVENTS)
            eindex = 0;
    }
    if (found)
        memcpy(estr, e, sizeof(gdt_evt_str));
    else
        estr->event_source = 0;
    splx(lock);
}

void gdt_clear_events()
{
    GDT_DPRINTF(GDT_D_MISC, ("gdt_clear_events\n"));

    eoldidx = elastidx = 0;
    ebuffer[0].event_source = 0;
}
