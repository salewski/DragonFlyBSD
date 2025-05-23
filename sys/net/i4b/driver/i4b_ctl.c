/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_ctl.c - i4b system control port driver
 *	------------------------------------------
 *
 *	$Id: i4b_ctl.c,v 1.37 2000/05/31 08:04:43 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/driver/i4b_ctl.c,v 1.10.2.3 2001/08/12 16:22:48 hm Exp $
 * $DragonFly: src/sys/net/i4b/driver/i4b_ctl.c,v 1.10 2004/07/02 15:43:10 joerg Exp $
 *
 *	last edit-date: [Sat Aug 11 18:06:38 2001]
 *
 *---------------------------------------------------------------------------*/

#include "use_i4bctl.h"

#if NI4BCTL > 1
#error "only 1 (one) i4bctl device allowed!"
#endif

#if NI4BCTL > 0

#include <sys/param.h>

#if defined(__DragonFly__) || (defined(__FreeBSD__) && __FreeBSD__ >= 3)
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__DragonFly__) || defined(__FreeBSD__)

#if defined(__FreeBSD__) && __FreeBSD__ == 3
#include "opt_devfs.h"
#endif

#ifdef DEVFS
#include <sys/devfsext.h>
#endif

#endif /* __FreeBSD__ */

#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <net/i4b/include/machine/i4b_debug.h>
#include <net/i4b/include/machine/i4b_ioctl.h>
#elif defined(__bsdi__)
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#else
#include <machine/bus.h>
#include <sys/device.h>
#include <i4b/i4b_debug.h>
#include <i4b/i4b_ioctl.h>
#endif

#include "../include/i4b_global.h"
#include "../include/i4b_l3l4.h"
#include "../layer2/i4b_l2.h"

static int openflag = 0;

#if defined(__DragonFly__) || (BSD > 199306 && defined(__FreeBSD__))
static	d_open_t	i4bctlopen;
static	d_close_t	i4bctlclose;
static	d_ioctl_t	i4bctlioctl;

#ifdef OS_USES_POLL
static d_poll_t		i4bctlpoll;
#define POLLFIELD	i4bctlpoll
#else
#define POLLFIELD	noselect
#endif

#define CDEV_MAJOR 55

#if defined(__DragonFly__) || (defined(__FreeBSD__) && __FreeBSD__ >= 4)
static struct cdevsw i4bctl_cdevsw = {
	/* name */      "i4bctl",
	/* maj */       CDEV_MAJOR,
	/* flags */     0,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */      i4bctlopen,
	/* close */     i4bctlclose,
	/* read */      noread,
	/* write */     nowrite,
	/* ioctl */     i4bctlioctl,
	/* poll */      POLLFIELD,
	/* mmap */      nommap,
	/* strategy */  nostrategy,
	/* dump */      nodump,
	/* psize */     nopsize
};
#else
static struct cdevsw i4bctl_cdevsw = 
	{ i4bctlopen,	i4bctlclose,	noread,		nowrite,
	  i4bctlioctl,	nostop,		nullreset,	nodevtotty,
	  POLLFIELD,	nommap,		NULL,	"i4bctl", NULL,	-1 };
#endif

static void i4bctlattach(void *);
PSEUDO_SET(i4bctlattach, i4b_i4bctldrv);

#define PDEVSTATIC	static
#endif /* __FreeBSD__ */

#if defined(__FreeBSD__) && __FreeBSD__ == 3
#ifdef DEVFS
static void *devfs_token;
#endif
#endif

#if !defined(__DragonFly__) && !defined(__FreeBSD__)
#define PDEVSTATIC	/* */
void i4bctlattach (void);
int i4bctlopen (dev_t dev, int flag, int fmt, struct proc *p);
int i4bctlclose (dev_t dev, int flag, int fmt, struct proc *p);
#ifdef __bsdi__
int i4bctlioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p);
#else
int i4bctlioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p);
#endif
#endif	/* !FreeBSD */

#if defined(__DragonFly__) || (BSD > 199306 && defined(__FreeBSD__))
/*---------------------------------------------------------------------------*
 *	initialization at kernel load time
 *---------------------------------------------------------------------------*/
static void
i4bctlinit(void *unused)
{
#if defined(__DragonFly__) || (defined(__FreeBSD__) && __FreeBSD__ >= 4)
	cdevsw_add(&i4bctl_cdevsw, 0, 0);
#else
	dev_t dev = make_adhoc_dev(&i4bctl, 0);
	cdevsw_add(&dev, &i4bctl_cdevsw, NULL);
#endif
}

SYSINIT(i4bctldev, SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR, &i4bctlinit, NULL);

#endif /* BSD > 199306 && defined(__FreeBSD__) */

#ifdef __bsdi__
int i4bctlmatch(struct device *parent, struct cfdata *cf, void *aux);
void dummy_i4bctlattach(struct device*, struct device *, void *);

#define CDEV_MAJOR 64

static struct cfdriver i4bctlcd =
	{ NULL, "i4bctl", i4bctlmatch, dummy_i4bctlattach, DV_DULL,
	  sizeof(struct cfdriver) };
struct devsw i4bctlsw = 
	{ &i4bctlcd,
	  i4bctlopen,	i4bctlclose,	noread,		nowrite,
	  i4bctlioctl,	seltrue,	nommap,		nostrat,
	  nodump,	nopsize,	0,		nostop
};

int
i4bctlmatch(struct device *parent, struct cfdata *cf, void *aux)
{
	printf("i4bctlmatch: aux=0x%x\n", aux);
	return 1;
}
void
dummy_i4bctlattach(struct device *parent, struct device *self, void *aux)
{
	printf("dummy_i4bctlattach: aux=0x%x\n", aux);
}
#endif /* __bsdi__ */
/*---------------------------------------------------------------------------*
 *	interface attach routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC void
#if defined(__DragonFly__) || defined(__FreeBSD__)
i4bctlattach(void *dummy)
#else
i4bctlattach()
#endif
{
#ifndef HACK_NO_PSEUDO_ATTACH_MSG
	printf("i4bctl: ISDN system control port attached\n");
#endif

#if defined(__FreeBSD__)
#if __FreeBSD__ == 3

#ifdef DEVFS
	devfs_token = devfs_add_devswf(&i4bctl_cdevsw, 0, DV_CHR,
				       UID_ROOT, GID_WHEEL, 0600,
				       "i4bctl");
#endif

#else
	make_dev(&i4bctl_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "i4bctl");
#endif
#endif
}

/*---------------------------------------------------------------------------*
 *	i4bctlopen - device driver open routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4bctlopen(dev_t dev, int flag, int fmt, struct thread *td)
{
	if(minor(dev))
		return (ENXIO);

	if(openflag)
		return (EBUSY);
	
	openflag = 1;
	
	return (0);
}

/*---------------------------------------------------------------------------*
 *	i4bctlclose - device driver close routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
i4bctlclose(dev_t dev, int flag, int fmt, struct thread *td)
{
	openflag = 0;
	return (0);
}

/*---------------------------------------------------------------------------*
 *	i4bctlioctl - device driver ioctl routine
 *---------------------------------------------------------------------------*/
PDEVSTATIC int
#if defined(__DragonFly__) || (defined (__FreeBSD_version) && __FreeBSD_version >= 300003)
i4bctlioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
#elif defined(__bsdi__)
i4bctlioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
#else
i4bctlioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
#endif
{
#if DO_I4B_DEBUG
	ctl_debug_t *cdbg;	
	int error = 0;
#endif
	
#if !DO_I4B_DEBUG
       return(ENODEV);
#else
	if(minor(dev))
		return(ENODEV);

	switch(cmd)
	{
		case I4B_CTL_GET_DEBUG:
			cdbg = (ctl_debug_t *)data;
			cdbg->l1 = i4b_l1_debug;
			cdbg->l2 = i4b_l2_debug;
			cdbg->l3 = i4b_l3_debug;
			cdbg->l4 = i4b_l4_debug;
			break;
		
		case I4B_CTL_SET_DEBUG:
			cdbg = (ctl_debug_t *)data;
			i4b_l1_debug = cdbg->l1;
			i4b_l2_debug = cdbg->l2;
			i4b_l3_debug = cdbg->l3;
			i4b_l4_debug = cdbg->l4;
			break;

                case I4B_CTL_GET_CHIPSTAT:
                {
                        struct chipstat *cst;
			cst = (struct chipstat *)data;
			(*ctrl_desc[cst->driver_unit].N_MGMT_COMMAND)(cst->driver_unit, CMR_GCST, cst);
                        break;
                }

                case I4B_CTL_CLR_CHIPSTAT:
                {
                        struct chipstat *cst;
			cst = (struct chipstat *)data;
			(*ctrl_desc[cst->driver_unit].N_MGMT_COMMAND)(cst->driver_unit, CMR_CCST, cst);
                        break;
                }

                case I4B_CTL_GET_LAPDSTAT:
                {
                        l2stat_t *l2s;
                        l2_softc_t *sc;
                        l2s = (l2stat_t *)data;

                        if( l2s->unit < 0 || l2s->unit > MAXL1UNITS)
                        {
                        	error = EINVAL;
				break;
			}
			  
			sc = &l2_softc[l2s->unit];

			bcopy(&sc->stat, &l2s->lapdstat, sizeof(lapdstat_t));
                        break;
                }

                case I4B_CTL_CLR_LAPDSTAT:
                {
                        int *up;
                        l2_softc_t *sc;
                        up = (int *)data;

                        if( *up < 0 || *up > MAXL1UNITS)
                        {
                        	error = EINVAL;
				break;
			}
			  
			sc = &l2_softc[*up];

			bzero(&sc->stat, sizeof(lapdstat_t));
                        break;
                }

		default:
			error = ENOTTY;
			break;
	}
	return(error);
#endif /* DO_I4B_DEBUG */
}

#if (defined(__DragonFly__) || defined(__FreeBSD__)) && defined(OS_USES_POLL)

/*---------------------------------------------------------------------------*
 *	i4bctlpoll - device driver poll routine
 *---------------------------------------------------------------------------*/
static int
i4bctlpoll (dev_t dev, int events, struct thread *td)
{
	return (ENODEV);
}

#endif

#endif /* NI4BCTL > 0 */
