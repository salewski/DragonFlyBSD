/* drm_drv.h -- Generic driver template -*- linux-c -*-
 * Created: Thu Nov 23 03:10:50 2000 by gareth@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 * $FreeBSD: src/sys/dev/drm/drm_drv.h,v 1.13.2.1 2003/04/26 07:05:28 anholt Exp $
 * $DragonFly: src/sys/dev/drm/Attic/drm_drv.h,v 1.11 2005/03/01 00:43:02 corecode Exp $
 */

/*
 * To use this template, you must at least define the following (samples
 * given for the MGA driver):
 *
 * #define DRIVER_AUTHOR	"VA Linux Systems, Inc."
 *
 * #define DRIVER_NAME		"mga"
 * #define DRIVER_DESC		"Matrox G200/G400"
 * #define DRIVER_DATE		"20001127"
 *
 * #define DRIVER_MAJOR		2
 * #define DRIVER_MINOR		0
 * #define DRIVER_PATCHLEVEL	2
 *
 * #define DRIVER_IOCTL_COUNT	DRM_ARRAY_SIZE( mga_ioctls )
 *
 * #define DRM(x)		mga_##x
 */

#ifndef __MUST_HAVE_AGP
#define __MUST_HAVE_AGP			0
#endif
#ifndef __HAVE_CTX_BITMAP
#define __HAVE_CTX_BITMAP		0
#endif
#ifndef __HAVE_DMA_IRQ
#define __HAVE_DMA_IRQ			0
#endif
#ifndef __HAVE_DMA_QUEUE
#define __HAVE_DMA_QUEUE		0
#endif
#ifndef __HAVE_DMA_SCHEDULE
#define __HAVE_DMA_SCHEDULE		0
#endif
#ifndef __HAVE_DMA_QUIESCENT
#define __HAVE_DMA_QUIESCENT		0
#endif
#ifndef __HAVE_RELEASE
#define __HAVE_RELEASE			0
#endif
#ifndef __HAVE_COUNTERS
#define __HAVE_COUNTERS			0
#endif
#ifndef __HAVE_SG
#define __HAVE_SG			0
#endif

#ifndef DRIVER_PREINIT
#define DRIVER_PREINIT()
#endif
#ifndef DRIVER_POSTINIT
#define DRIVER_POSTINIT()
#endif
#ifndef DRIVER_PRERELEASE
#define DRIVER_PRERELEASE()
#endif
#ifndef DRIVER_PRETAKEDOWN
#define DRIVER_PRETAKEDOWN()
#endif
#ifndef DRIVER_POSTCLEANUP
#define DRIVER_POSTCLEANUP()
#endif
#ifndef DRIVER_PRESETUP
#define DRIVER_PRESETUP()
#endif
#ifndef DRIVER_POSTSETUP
#define DRIVER_POSTSETUP()
#endif
#ifndef DRIVER_IOCTLS
#define DRIVER_IOCTLS
#endif
#ifndef DRIVER_FOPS
#endif

#if 1 && DRM_DEBUG_CODE
int DRM(flags) = DRM_FLAG_DEBUG;
#else
int DRM(flags) = 0;
#endif

static int DRM(init)(device_t nbdev);
static void DRM(cleanup)(device_t nbdev);

#if defined(__DragonFly__) || defined(__FreeBSD__)
#define DRIVER_SOFTC(unit) \
	((drm_device_t *) devclass_get_softc(DRM(devclass), unit))

#if __REALLY_HAVE_AGP
MODULE_DEPEND(DRIVER_NAME, agp, 1, 1, 1);
#endif
#endif /* __FreeBSD__ */

#ifdef __NetBSD__
#define DRIVER_SOFTC(unit) \
	((drm_device_t *) device_lookup(&DRM(cd), unit))
#endif /* __NetBSD__ */

static drm_ioctl_desc_t		  DRM(ioctls)[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]       = { DRM(version),     0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)]    = { DRM(getunique),   0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]     = { DRM(getmagic),    0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]     = { DRM(irq_busid),   0, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAP)]       = { DRM(getmap),      0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CLIENT)]    = { DRM(getclient),   0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_STATS)]     = { DRM(getstats),    0, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)]    = { DRM(setunique),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]         = { DRM(noop),        1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]       = { DRM(noop),        1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)]    = { DRM(authmagic),   1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]       = { DRM(addmap),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_MAP)]        = { DRM(rmmap),       1, 0 },

#if __HAVE_CTX_BITMAP
	[DRM_IOCTL_NR(DRM_IOCTL_SET_SAREA_CTX)] = { DRM(setsareactx), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_SAREA_CTX)] = { DRM(getsareactx), 1, 0 },
#endif

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]       = { DRM(addctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]        = { DRM(rmctx),       1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]       = { DRM(modctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]       = { DRM(getctx),      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)]    = { DRM(switchctx),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]       = { DRM(newctx),      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]       = { DRM(resctx),      1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]      = { DRM(adddraw),     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]       = { DRM(rmdraw),      1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	        = { DRM(lock),        1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]        = { DRM(unlock),      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]        = { DRM(noop),        1, 0 },

#if __HAVE_DMA
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]      = { DRM(addbufs),     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]     = { DRM(markbufs),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]     = { DRM(infobufs),    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]      = { DRM(mapbufs),     1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]     = { DRM(freebufs),    1, 0 },

	/* The DRM_IOCTL_DMA ioctl should be defined by the driver.
	 */
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]       = { DRM(control),     1, 1 },
#endif

#if __REALLY_HAVE_AGP
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)]   = { DRM(agp_acquire), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)]   = { DRM(agp_release), 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]    = { DRM(agp_enable),  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]      = { DRM(agp_info),    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]     = { DRM(agp_alloc),   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]      = { DRM(agp_free),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]      = { DRM(agp_bind),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]    = { DRM(agp_unbind),  1, 1 },
#endif

#if __HAVE_SG
	[DRM_IOCTL_NR(DRM_IOCTL_SG_ALLOC)]      = { DRM(sg_alloc),    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_SG_FREE)]       = { DRM(sg_free),     1, 1 },
#endif

#if __HAVE_VBL_IRQ
	[DRM_IOCTL_NR(DRM_IOCTL_WAIT_VBLANK)]   = { DRM(wait_vblank), 0, 0 },
#endif

	DRIVER_IOCTLS
};

#define DRIVER_IOCTL_COUNT	DRM_ARRAY_SIZE( DRM(ioctls) )

const char *DRM(find_description)(int vendor, int device);

#if defined(__DragonFly__) || defined(__FreeBSD__)
static struct cdevsw DRM(cdevsw) = {
	.d_name =	DRIVER_NAME,
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_TTY | D_TRACKCLOSE,
	.d_port = 	NULL,
	.d_clone =	NULL,

	.old_open =	DRM( open ),
	.old_close =	DRM( close ),
	.old_read =	DRM( read ),
	.old_ioctl =	DRM( ioctl ),
	.old_poll =	DRM( poll ),
	.old_mmap =	DRM( mmap )
};

static int DRM(probe)(device_t dev)
{
	const char *s = NULL;

	int pciid=pci_get_devid(dev);
	int vendor = (pciid & 0x0000ffff);
	int device = (pciid & 0xffff0000) >> 16;
	
	s = DRM(find_description)(vendor, device);
	if (s) {
		device_set_desc(dev, s);
		return 0;
	}

	return ENXIO;
}

static int DRM(attach)(device_t dev)
{
	return DRM(init)(dev);
}

static int DRM(detach)(device_t dev)
{
	DRM(cleanup)(dev);
	return 0;
}
static device_method_t DRM(methods)[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		DRM( probe)),
	DEVMETHOD(device_attach,	DRM( attach)),
	DEVMETHOD(device_detach,	DRM( detach)),

	{ 0, 0 }
};

static driver_t DRM(driver) = {
	"drm",
	DRM(methods),
	sizeof(drm_device_t),
};

static devclass_t DRM(devclass);

#elif defined(__NetBSD__)

static struct cdevsw DRM(cdevsw) = {
	DRM(open),
	DRM(close),
	DRM(read),
	nowrite,
	DRM(ioctl),
	nostop,
	notty,
	DRM(poll),
	DRM(mmap),
	nokqfilter,
	D_TTY
};

int DRM(refcnt) = 0;
#if __NetBSD_Version__ >= 106080000
MOD_DEV( DRIVER_NAME, DRIVER_NAME, NULL, -1, &DRM(cdevsw), CDEV_MAJOR);
#else
MOD_DEV( DRIVER_NAME, LM_DT_CHAR, CDEV_MAJOR, &DRM(cdevsw) );
#endif

int DRM(lkmentry)(struct lkm_table *lkmtp, int cmd, int ver);
static int DRM(lkmhandle)(struct lkm_table *lkmtp, int cmd);

int DRM(modprobe)();
int DRM(probe)(struct pci_attach_args *pa);
void DRM(attach)(struct pci_attach_args *pa, dev_t kdev);

int DRM(lkmentry)(struct lkm_table *lkmtp, int cmd, int ver) {
	DISPATCH(lkmtp, cmd, ver, DRM(lkmhandle), DRM(lkmhandle), DRM(lkmhandle));
}

static int DRM(lkmhandle)(struct lkm_table *lkmtp, int cmd)
{
	int j, error = 0;
#if defined(__NetBSD__) && (__NetBSD_Version__ > 106080000)
	struct lkm_dev *args = lkmtp->private.lkm_dev;
#endif

	switch(cmd) {
	case LKM_E_LOAD:
		if (lkmexists(lkmtp))
			return EEXIST;

		if(DRM(modprobe)())
			return 0;

		return 1;

	case LKM_E_UNLOAD:
		if (DRM(refcnt) > 0)
			return (EBUSY);
		break;
	case LKM_E_STAT:
		break;

	default:
		error = EIO;
		break;
	}
	
	return error;
}

int DRM(modprobe)() {
	struct pci_attach_args pa;
	int error = 0;
	if((error = pci_find_device(&pa, DRM(probe))) != 0)
		DRM(attach)(&pa, 0);

	return error;
}

int DRM(probe)(struct pci_attach_args *pa)
{
	const char *desc;

	desc = DRM(find_description)(PCI_VENDOR(pa->pa_id), PCI_PRODUCT(pa->pa_id));
	if (desc != NULL) {
		return 1;
	}

	return 0;
}

void DRM(attach)(struct pci_attach_args *pa, dev_t kdev)
{
	int i;
	drm_device_t *dev;

	config_makeroom(kdev, &DRM(cd));
	DRM(cd).cd_devs[(kdev)] = DRM(alloc)(sizeof(drm_device_t),
	    DRM_MEM_DRIVER);
	dev = DRIVER_SOFTC(kdev);

	memset(dev, 0, sizeof(drm_device_t));
	memcpy(&dev->pa, pa, sizeof(dev->pa));

	DRM_INFO("%s", DRM(find_description)(PCI_VENDOR(pa->pa_id), PCI_PRODUCT(pa->pa_id)));
	DRM(init)(dev);
}

int DRM(detach)(struct device *self, int flags)
{
	DRM(cleanup)((drm_device_t *)self);
	return 0;
}

int DRM(activate)(struct device *self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* FIXME */
		break;
	}
	return (0);
}
#endif /* __NetBSD__ */

const char *DRM(find_description)(int vendor, int device) {
	const char *s = NULL;
	int i=0, done=0;
	
	while ( !done && (DRM(devicelist)[i].vendor != 0 ) ) {
		if ( (DRM(devicelist)[i].vendor == vendor) &&
		     (DRM(devicelist)[i].device == device) ) {
			done=1;
			if ( DRM(devicelist)[i].supported )
				s = DRM(devicelist)[i].name;
			else
				DRM_INFO("%s not supported\n", DRM(devicelist)[i].name);
		}
		i++;
	}
	return s;
}

static int DRM(setup)( drm_device_t *dev )
{
	int i;

	DRIVER_PRESETUP();
	dev->buf_use = 0;
	dev->buf_alloc = 0;

#if __HAVE_DMA
	i = DRM(dma_setup)( dev );
	if ( i < 0 )
		return i;
#endif

	dev->counters  = 6 + __HAVE_COUNTERS;
	dev->types[0]  = _DRM_STAT_LOCK;
	dev->types[1]  = _DRM_STAT_OPENS;
	dev->types[2]  = _DRM_STAT_CLOSES;
	dev->types[3]  = _DRM_STAT_IOCTLS;
	dev->types[4]  = _DRM_STAT_LOCKS;
	dev->types[5]  = _DRM_STAT_UNLOCKS;
#ifdef __HAVE_COUNTER6
	dev->types[6]  = __HAVE_COUNTER6;
#endif
#ifdef __HAVE_COUNTER7
	dev->types[7]  = __HAVE_COUNTER7;
#endif
#ifdef __HAVE_COUNTER8
	dev->types[8]  = __HAVE_COUNTER8;
#endif
#ifdef __HAVE_COUNTER9
	dev->types[9]  = __HAVE_COUNTER9;
#endif
#ifdef __HAVE_COUNTER10
	dev->types[10] = __HAVE_COUNTER10;
#endif
#ifdef __HAVE_COUNTER11
	dev->types[11] = __HAVE_COUNTER11;
#endif
#ifdef __HAVE_COUNTER12
	dev->types[12] = __HAVE_COUNTER12;
#endif
#ifdef __HAVE_COUNTER13
	dev->types[13] = __HAVE_COUNTER13;
#endif
#ifdef __HAVE_COUNTER14
	dev->types[14] = __HAVE_COUNTER14;
#endif
#ifdef __HAVE_COUNTER15
	dev->types[14] = __HAVE_COUNTER14;
#endif

	for ( i = 0 ; i < DRM_ARRAY_SIZE(dev->counts) ; i++ )
		atomic_set( &dev->counts[i], 0 );

	for ( i = 0 ; i < DRM_HASH_SIZE ; i++ ) {
		dev->magiclist[i].head = NULL;
		dev->magiclist[i].tail = NULL;
	}

	dev->maplist = DRM(alloc)(sizeof(*dev->maplist),
				  DRM_MEM_MAPS);
	if(dev->maplist == NULL) return DRM_ERR(ENOMEM);
	memset(dev->maplist, 0, sizeof(*dev->maplist));
	TAILQ_INIT(dev->maplist);

	dev->lock.hw_lock = NULL;
	dev->lock.lock_queue = 0;
	dev->irq = 0;
	dev->context_flag = 0;
	dev->last_context = 0;
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	callout_init( &dev->timer, 1 );
#else
	callout_init( &dev->timer );
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__)
	dev->buf_sigio = NULL;
#elif defined(__NetBSD__)
	dev->buf_pgid = 0;
#endif

	DRM_DEBUG( "\n" );

	DRIVER_POSTSETUP();
	return 0;
}


static int DRM(takedown)( drm_device_t *dev )
{
	drm_magic_entry_t *pt, *next;
	drm_local_map_t *map;
	drm_map_list_entry_t *list;
	int i;

	DRM_DEBUG( "\n" );

	DRIVER_PRETAKEDOWN();
#if __HAVE_DMA_IRQ
	if ( dev->irq ) DRM(irq_uninstall)( dev );
#endif

	DRM_LOCK;
	callout_stop( &dev->timer );

	if ( dev->unique ) {
		DRM(free)( dev->unique, strlen( dev->unique ) + 1,
			   DRM_MEM_DRIVER );
		dev->unique = NULL;
		dev->unique_len = 0;
	}
				/* Clear pid list */
	for ( i = 0 ; i < DRM_HASH_SIZE ; i++ ) {
		for ( pt = dev->magiclist[i].head ; pt ; pt = next ) {
			next = pt->next;
			DRM(free)( pt, sizeof(*pt), DRM_MEM_MAGIC );
		}
		dev->magiclist[i].head = dev->magiclist[i].tail = NULL;
	}

#if __REALLY_HAVE_AGP
				/* Clear AGP information */
	if ( dev->agp ) {
		drm_agp_mem_t *entry;
		drm_agp_mem_t *nexte;

				/* Remove AGP resources, but leave dev->agp
                                   intact until drv_cleanup is called. */
		for ( entry = dev->agp->memory ; entry ; entry = nexte ) {
			nexte = entry->next;
			if ( entry->bound ) DRM(unbind_agp)( entry->handle );
			DRM(free_agp)( entry->handle, entry->pages );
			DRM(free)( entry, sizeof(*entry), DRM_MEM_AGPLISTS );
		}
		dev->agp->memory = NULL;

		if ( dev->agp->acquired ) DRM(agp_do_release)();

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
	}
#endif

	if( dev->maplist ) {
		while ((list=TAILQ_FIRST(dev->maplist))) {
			map = list->map;
			switch ( map->type ) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
#if __REALLY_HAVE_MTRR
				if ( map->mtrr >= 0 ) {
					int retcode;
#if defined(__DragonFly__) || defined(__FreeBSD__)
					int act;
					struct mem_range_desc mrdesc;
					mrdesc.mr_base = map->offset;
					mrdesc.mr_len = map->size;
					mrdesc.mr_flags = MDF_WRITECOMBINE;
					act = MEMRANGE_SET_UPDATE;
					bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
					retcode = mem_range_attr_set(&mrdesc, &act);
					map->mtrr=1;
#elif defined __NetBSD__
					struct mtrr mtrrmap;
					int one = 1;
					mtrrmap.base = map->offset;
					mtrrmap.len = map->size;
					mtrrmap.type = MTRR_TYPE_WC;
					mtrrmap.flags = 0;
					retcode = mtrr_set( &mtrrmap, &one, 
						DRM_CURPROC, MTRR_GETSET_KERNEL);
#endif
					DRM_DEBUG( "mtrr_del=%d\n", retcode );
				}
#endif
				DRM(ioremapfree)( map );
				break;
			case _DRM_SHM:
				DRM(free)(map->handle,
					       map->size,
					       DRM_MEM_SAREA);
				break;

			case _DRM_AGP:
				/* Do nothing here, because this is all
				 * handled in the AGP/GART driver.
				 */
				break;
                       case _DRM_SCATTER_GATHER:
				/* Handle it, but do nothing, if REALLY_HAVE_SG
				 * isn't defined.
				 */
#if __REALLY_HAVE_SG
				if(dev->sg) {
					DRM(sg_cleanup)(dev->sg);
					dev->sg = NULL;
				}
#endif
				break;
			}
			TAILQ_REMOVE(dev->maplist, list, link);
			DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);
			DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
		}
		DRM(free)(dev->maplist, sizeof(*dev->maplist), DRM_MEM_MAPS);
		dev->maplist   = NULL;
 	}

#if __HAVE_DMA
	DRM(dma_takedown)( dev );
#endif
	if ( dev->lock.hw_lock ) {
		dev->lock.hw_lock = NULL; /* SHM removed */
		dev->lock.filp = NULL;
		DRM_WAKEUP_INT((void *)&dev->lock.lock_queue);
	}
	DRM_UNLOCK;

	return 0;
}

/* linux: drm_init is called via init_module at module load time, or via
 *        linux/init/main.c (this is not currently supported).
 * bsd:   drm_init is called via the attach function per device.
 */
static int DRM(init)( device_t nbdev )
{
	int unit;
#if defined(__DragonFly__) || defined(__FreeBSD__)
	drm_device_t *dev;
#elif defined(__NetBSD__)
	drm_device_t *dev = nbdev;
#endif
#if __HAVE_CTX_BITMAP
	int retcode;
#endif
	DRM_DEBUG( "\n" );
	DRIVER_PREINIT();

#if defined(__DragonFly__) || defined(__FreeBSD__)
	unit = device_get_unit(nbdev);
	dev = device_get_softc(nbdev);
	memset( (void *)dev, 0, sizeof(*dev) );
	dev->device = nbdev;
	cdevsw_add(&DRM(cdevsw), -1, unit);
	dev->devnode = make_dev( &DRM(cdevsw),
			unit,
			DRM_DEV_UID,
			DRM_DEV_GID,
			DRM_DEV_MODE,
			"dri/card%d", unit );
	reference_dev(dev->devnode);
#elif defined(__NetBSD__)
	unit = minor(dev->device.dv_unit);
#endif
	DRM_SPININIT(dev->count_lock, "drm device");
	lockinit(&dev->dev_lock, 0, "drmlk", 0, 0);
	dev->name = DRIVER_NAME;
	DRM(mem_init)();
	DRM(sysctl_init)(dev);
	TAILQ_INIT(&dev->files);

#if __REALLY_HAVE_AGP
	dev->agp = DRM(agp_init)();
#if __MUST_HAVE_AGP
	if ( dev->agp == NULL ) {
		DRM_ERROR( "Cannot initialize the agpgart module.\n" );
		DRM(sysctl_cleanup)( dev );
#if defined(__DragonFly__) || defined(__FreeBSD__)
		destroy_dev(dev->devnode);
#endif
		DRM(takedown)( dev );
		return DRM_ERR(ENOMEM);
	}
#endif /* __MUST_HAVE_AGP */
#if __REALLY_HAVE_MTRR
	if (dev->agp) {
#if defined(__DragonFly__) || defined(__FreeBSD__)
		int retcode = 0, act;
		struct mem_range_desc mrdesc;
		mrdesc.mr_base = dev->agp->info.ai_aperture_base;
		mrdesc.mr_len = dev->agp->info.ai_aperture_size;
		mrdesc.mr_flags = MDF_WRITECOMBINE;
		act = MEMRANGE_SET_UPDATE;
		bcopy(DRIVER_NAME, &mrdesc.mr_owner, strlen(DRIVER_NAME));
		retcode = mem_range_attr_set(&mrdesc, &act);
		dev->agp->agp_mtrr=1;
#elif defined __NetBSD__
		struct mtrr mtrrmap;
		int one = 1;
		mtrrmap.base = dev->agp->info.ai_aperture_base;
		mtrrmap.len = dev->agp->info.ai_aperture_size;
		mtrrmap.type = MTRR_TYPE_WC;
		mtrrmap.flags = MTRR_VALID;
		dev->agp->agp_mtrr = mtrr_set( &mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#endif /* __NetBSD__ */
	}
#endif /* __REALLY_HAVE_MTRR */
#endif /* __REALLY_HAVE_AGP */

#if __HAVE_CTX_BITMAP
	retcode = DRM(ctxbitmap_init)( dev );
	if( retcode ) {
		DRM_ERROR( "Cannot allocate memory for context bitmap.\n" );
		DRM(sysctl_cleanup)( dev );
#if defined(__DragonFly__) || defined(__FreeBSD__)
		destroy_dev(dev->devnode);
#endif
		DRM(takedown)( dev );
		return retcode;
	}
#endif
	DRM_INFO( "Initialized %s %d.%d.%d %s on minor %d\n",
	  	DRIVER_NAME,
	  	DRIVER_MAJOR,
	  	DRIVER_MINOR,
	  	DRIVER_PATCHLEVEL,
	  	DRIVER_DATE,
	  	unit );

	DRIVER_POSTINIT();

	return 0;
}

/* linux: drm_cleanup is called via cleanup_module at module unload time.
 * bsd:   drm_cleanup is called per device at module unload time.
 * FIXME: NetBSD
 */
static void DRM(cleanup)(device_t nbdev)
{
	drm_device_t *dev;
#ifdef __NetBSD__
#if __REALLY_HAVE_MTRR
	struct mtrr mtrrmap;
	int one = 1;
#endif /* __REALLY_HAVE_MTRR */
	dev = nbdev;
#endif /* __NetBSD__ */

	DRM_DEBUG( "\n" );

#if defined(__DragonFly__) || defined(__FreeBSD__)
	dev = device_get_softc(nbdev);
#endif
	DRM(sysctl_cleanup)( dev );
#if defined(__DragonFly__) || defined(__FreeBSD__)
	destroy_dev(dev->devnode);
#endif
#if __HAVE_CTX_BITMAP
	DRM(ctxbitmap_cleanup)( dev );
#endif

#if __REALLY_HAVE_AGP && __REALLY_HAVE_MTRR
	if ( dev->agp && dev->agp->agp_mtrr >= 0) {
#if defined(__NetBSD__)
		mtrrmap.base = dev->agp->info.ai_aperture_base;
		mtrrmap.len = dev->agp->info.ai_aperture_size;
		mtrrmap.type = 0;
		mtrrmap.flags = 0;
		mtrr_set( &mtrrmap, &one, NULL, MTRR_GETSET_KERNEL);
#endif
	}
#endif
	cdevsw_remove(&DRM(cdevsw), -1, device_get_unit(nbdev));

	DRM(takedown)( dev );

#if __REALLY_HAVE_AGP
	if ( dev->agp ) {
		DRM(agp_uninit)();
		DRM(free)( dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS );
		dev->agp = NULL;
	}
#endif
	DRIVER_POSTCLEANUP();
	DRM(mem_uninit)();
	DRM_SPINUNINIT(dev->count_lock);
}


int DRM(version)( DRM_IOCTL_ARGS )
{
	drm_version_t version;
	int len;

	DRM_COPY_FROM_USER_IOCTL( version, (drm_version_t *)data, sizeof(version) );

#define DRM_COPY( name, value )						\
	len = strlen( value );						\
	if ( len > name##_len ) len = name##_len;			\
	name##_len = strlen( value );					\
	if ( len && name ) {						\
		if ( DRM_COPY_TO_USER( name, value, len ) )		\
			return DRM_ERR(EFAULT);				\
	}

	version.version_major = DRIVER_MAJOR;
	version.version_minor = DRIVER_MINOR;
	version.version_patchlevel = DRIVER_PATCHLEVEL;

	DRM_COPY( version.name, DRIVER_NAME );
	DRM_COPY( version.date, DRIVER_DATE );
	DRM_COPY( version.desc, DRIVER_DESC );

	DRM_COPY_TO_USER_IOCTL( (drm_version_t *)data, version, sizeof(version) );

	return 0;
}

int DRM(open)(dev_t kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	drm_device_t *dev = NULL;
	int retcode = 0;

	dev = DRIVER_SOFTC(minor(kdev));

	DRM_DEBUG( "open_count = %d\n", dev->open_count );

	retcode = DRM(open_helper)(kdev, flags, fmt, p, dev);

	if ( !retcode ) {
		atomic_inc( &dev->counts[_DRM_STAT_OPENS] );
		DRM_SPINLOCK( &dev->count_lock );
#if defined(__DragonFly__) || defined(__FreeBSD__)
		device_busy(dev->device);
#endif
		if ( !dev->open_count++ )
			retcode = DRM(setup)( dev );
		DRM_SPINUNLOCK( &dev->count_lock );
	}

	return retcode;
}

int DRM(close)(dev_t kdev, int flags, int fmt, DRM_STRUCTPROC *p)
{
	drm_file_t *priv;
	DRM_DEVICE;
	int retcode = 0;
	DRMFILE __unused filp = (void *)(DRM_CURRENTPID);
	
	DRM_DEBUG( "open_count = %d\n", dev->open_count );
	priv = DRM(find_file_by_proc)(dev, p);
	if (!priv) {
		DRM_DEBUG("can't find authenticator\n");
		return EINVAL;
	}

	DRIVER_PRERELEASE();

	/* ========================================================
	 * Begin inline drm_release
	 */

#if defined(__DragonFly__) || defined(__FreeBSD__)
	DRM_DEBUG( "pid = %d, device = 0x%lx, open_count = %d\n",
		   DRM_CURRENTPID, (long)dev->device, dev->open_count );
#elif defined(__NetBSD__)
	DRM_DEBUG( "pid = %d, device = 0x%lx, open_count = %d\n",
		   DRM_CURRENTPID, (long)&dev->device, dev->open_count);
#endif

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.filp == (void *)DRM_CURRENTPID) {
		DRM_DEBUG("Process %d dead, freeing lock for context %d\n",
			  DRM_CURRENTPID,
			  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
#if HAVE_DRIVER_RELEASE
		DRIVER_RELEASE();
#endif
		DRM(lock_free)(dev,
			      &dev->lock.hw_lock->lock,
			      _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		
				/* FIXME: may require heavy-handed reset of
                                   hardware at this point, possibly
                                   processed via a callback to the X
                                   server. */
	}
#if __HAVE_RELEASE
	else if ( dev->lock.hw_lock ) {
		/* The lock is required to reclaim buffers */
		for (;;) {
			if ( !dev->lock.hw_lock ) {
				/* Device has been unregistered */
				retcode = DRM_ERR(EINTR);
				break;
			}
			if ( DRM(lock_take)( &dev->lock.hw_lock->lock,
					     DRM_KERNEL_CONTEXT ) ) {
				dev->lock.pid       = DRM_CURRENTPID;
				dev->lock.lock_time = jiffies;
                                atomic_inc( &dev->counts[_DRM_STAT_LOCKS] );
				break;	/* Got lock */
			}
				/* Contention */
#if 0
			atomic_inc( &dev->total_sleeps );
#endif
			retcode = tsleep((void *)&dev->lock.lock_queue,
					PCATCH, "drmlk2", 0);
			if (retcode)
				break;
		}
		if( !retcode ) {
			DRIVER_RELEASE();
			DRM(lock_free)( dev, &dev->lock.hw_lock->lock,
					DRM_KERNEL_CONTEXT );
		}
	}
#elif __HAVE_DMA
	DRM(reclaim_buffers)( dev, (void *)priv->pid );
#endif

#if defined (__FreeBSD__) && (__FreeBSD_version >= 500000)
	funsetown(&dev->buf_sigio);
#elif defined(__DragonFly__) || defined(__FreeBSD__)
	funsetown(dev->buf_sigio);
#elif defined(__NetBSD__)
	dev->buf_pgid = 0;
#endif /* __NetBSD__ */

	DRM_LOCK;
	priv = DRM(find_file_by_proc)(dev, p);
	if (priv) {
		priv->refs--;
		if (!priv->refs) {
			TAILQ_REMOVE(&dev->files, priv, link);
			DRM(free)( priv, sizeof(*priv), DRM_MEM_FILES );
		}
	}
	DRM_UNLOCK;


	/* ========================================================
	 * End inline drm_release
	 */

	atomic_inc( &dev->counts[_DRM_STAT_CLOSES] );
	DRM_SPINLOCK( &dev->count_lock );
#if defined(__DragonFly__) || defined(__FreeBSD__)
	device_unbusy(dev->device);
#endif
	if ( !--dev->open_count ) {
		DRM_SPINUNLOCK( &dev->count_lock );
		return DRM(takedown)( dev );
	}
	DRM_SPINUNLOCK( &dev->count_lock );
	
	return retcode;
}

/* DRM(ioctl) is called whenever a process performs an ioctl on /dev/drm.
 */
int DRM(ioctl)(dev_t kdev, u_long cmd, caddr_t data, int flags, 
    DRM_STRUCTPROC *p)
{
	DRM_DEVICE;
	int retcode = 0;
	drm_ioctl_desc_t *ioctl;
	int (*func)(DRM_IOCTL_ARGS);
	int nr = DRM_IOCTL_NR(cmd);
	DRM_PRIV;

	atomic_inc( &dev->counts[_DRM_STAT_IOCTLS] );
	++priv->ioctl_count;

#if defined(__DragonFly__) || defined(__FreeBSD__)
	DRM_DEBUG( "pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
		 DRM_CURRENTPID, cmd, nr, (long)dev->device, priv->authenticated );
#elif defined(__NetBSD__)
	DRM_DEBUG( "pid=%d, cmd=0x%02lx, nr=0x%02x, dev 0x%lx, auth=%d\n",
		 DRM_CURRENTPID, cmd, nr, (long)&dev->device, priv->authenticated );
#endif

	switch (cmd) {
	case FIONBIO:
		return 0;

	case FIOASYNC:
		dev->flags |= FASYNC;
		return 0;

#if defined(__DragonFly__) || defined(__FreeBSD__)
	case FIOSETOWN:
		return fsetown(*(int *)data, &dev->buf_sigio);

	case FIOGETOWN:
#if defined(__FreeBSD__) && (__FreeBSD_version >= 500000)
		*(int *) data = fgetown(&dev->buf_sigio);
#else
		*(int *) data = fgetown(dev->buf_sigio);
#endif
		return 0;
#endif /* __FreeBSD__ */
#ifdef __NetBSD__
	case TIOCSPGRP:
		dev->buf_pgid = *(int *)data;
		return 0;

	case TIOCGPGRP:
		*(int *)data = dev->buf_pgid;
		return 0;
#endif /* __NetBSD__ */
	}

	if ( nr >= DRIVER_IOCTL_COUNT ) {
		retcode = EINVAL;
	} else {
		ioctl = &DRM(ioctls)[nr];
		func = ioctl->func;

		if ( !func ) {
			DRM_DEBUG( "no function\n" );
			retcode = EINVAL;
		} else if ( ( ioctl->root_only && DRM_SUSER(p) ) 
			 || ( ioctl->auth_needed && !priv->authenticated ) ) {
			retcode = EACCES;
		} else {
			retcode = func(kdev, cmd, data, flags, p, (void *)DRM_CURRENTPID);
		}
	}

	return DRM_ERR(retcode);
}

int DRM(lock)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
        drm_lock_t lock;
        int ret = 0;

	DRM_COPY_FROM_USER_IOCTL( lock, (drm_lock_t *)data, sizeof(lock) );

        if ( lock.context == DRM_KERNEL_CONTEXT ) {
                DRM_ERROR( "Process %d using kernel context %d\n",
			   DRM_CURRENTPID, lock.context );
                return DRM_ERR(EINVAL);
        }

        DRM_DEBUG( "%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		   lock.context, DRM_CURRENTPID,
		   dev->lock.hw_lock->lock, lock.flags );

#if __HAVE_DMA_QUEUE
        if ( lock.context < 0 )
                return DRM_ERR(EINVAL);
#endif

        if ( !ret ) {
                for (;;) {
                        if ( !dev->lock.hw_lock ) {
                                /* Device has been unregistered */
                                ret = EINTR;
                                break;
                        }
                        if ( DRM(lock_take)( &dev->lock.hw_lock->lock,
					     lock.context ) ) {
                                dev->lock.filp = (void *)DRM_CURRENTPID;
                                dev->lock.lock_time = jiffies;
                                atomic_inc( &dev->counts[_DRM_STAT_LOCKS] );
                                break;  /* Got lock */
                        }

                                /* Contention */
			ret = tsleep((void *)&dev->lock.lock_queue,
					PCATCH, "drmlk2", 0);
			if (ret)
				break;
                }
        }

        if ( !ret ) {
		/* FIXME: Add signal blocking here */

#if __HAVE_DMA_QUIESCENT
                if ( lock.flags & _DRM_LOCK_QUIESCENT ) {
			DRIVER_DMA_QUIESCENT();
		}
#endif
        }

        DRM_DEBUG( "%d %s\n", lock.context, ret ? "interrupted" : "has lock" );

	return DRM_ERR(ret);
}


int DRM(unlock)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_lock_t lock;

	DRM_COPY_FROM_USER_IOCTL( lock, (drm_lock_t *)data, sizeof(lock) ) ;

	if ( lock.context == DRM_KERNEL_CONTEXT ) {
		DRM_ERROR( "Process %d using kernel context %d\n",
			   DRM_CURRENTPID, lock.context );
		return DRM_ERR(EINVAL);
	}

	atomic_inc( &dev->counts[_DRM_STAT_UNLOCKS] );

	DRM(lock_transfer)( dev, &dev->lock.hw_lock->lock,
			    DRM_KERNEL_CONTEXT );
#if __HAVE_DMA_SCHEDULE
	DRM(dma_schedule)( dev, 1 );
#endif

	/* FIXME: Do we ever really need to check this?
	 */
	if ( 1 /* !dev->context_flag */ ) {
		if ( DRM(lock_free)( dev, &dev->lock.hw_lock->lock,
				     DRM_KERNEL_CONTEXT ) ) {
			DRM_ERROR( "\n" );
		}
	}

	return 0;
}

#if DRM_LINUX

#include <sys/file2.h>
#include <sys/mapped_ioctl.h>
#include <sys/sysproto.h>
#include <emulation/linux/linux_ioctl.h>

MODULE_DEPEND(DRIVER_NAME, linux, 1, 1, 1);

#define LINUX_IOCTL_DRM_MIN		0x6400
#define LINUX_IOCTL_DRM_MAX		0x64ff

static struct ioctl_map_range DRM(ioctl_cmds)[] = {
	MAPPED_IOCTL_MAPRANGE(LINUX_IOCTL_DRM_MIN, LINUX_IOCTL_DRM_MAX, LINUX_IOCTL_DRM_MIN,
			      LINUX_IOCTL_DRM_MAX, NULL, linux_gen_dirmap),
	MAPPED_IOCTL_MAPF(0, 0, NULL)
};

static struct ioctl_map_handler DRM(ioctl_handler) = {
	&linux_ioctl_map,
	__XSTRING(DRM(linux)),
	DRM(ioctl_cmds)
};

SYSINIT(DRM(register), SI_SUB_KLD, SI_ORDER_MIDDLE, 
    mapped_ioctl_register_handler, &DRM(ioctl_handler));
SYSUNINIT(DRM(unregister), SI_SUB_KLD, SI_ORDER_MIDDLE, 
    mapped_ioctl_unregister_handler, &DRM(ioctl_handler));

#endif /* DRM_LINUX */
