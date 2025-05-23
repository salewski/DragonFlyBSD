/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bpf.c	8.2 (Berkeley) 3/28/94
 *
 * $FreeBSD: src/sys/net/bpf.c,v 1.59.2.12 2002/04/14 21:41:48 luigi Exp $
 * $DragonFly: src/sys/net/bpf.c,v 1.24 2005/03/31 12:31:31 joerg Exp $
 */

#include "use_bpf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/ttycom.h>
#include <sys/filedesc.h>

#include <sys/poll.h>

#include <sys/socket.h>
#include <sys/vnode.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

MALLOC_DEFINE(M_BPF, "BPF", "BPF data");

#if NBPF > 0

/*
 * The default read buffer size is patchable.
 */
static int bpf_bufsize = BPF_DEFAULTBUFSIZE;
SYSCTL_INT(_debug, OID_AUTO, bpf_bufsize, CTLFLAG_RW,
	   &bpf_bufsize, 0, "");
static int bpf_maxbufsize = BPF_MAXBUFSIZE;
SYSCTL_INT(_debug, OID_AUTO, bpf_maxbufsize, CTLFLAG_RW,
	   &bpf_maxbufsize, 0, "");

/*
 *  bpf_iflist is the list of interfaces; each corresponds to an ifnet
 */
static struct bpf_if	*bpf_iflist;

static int	bpf_allocbufs(struct bpf_d *);
static void	bpf_attachd(struct bpf_d *d, struct bpf_if *bp);
static void	bpf_detachd(struct bpf_d *d);
static void	bpf_freed(struct bpf_d *);
static void	bpf_mcopy(const void *, void *, size_t);
static int	bpf_movein(struct uio *, int, struct mbuf **,
			   struct sockaddr *, int *);
static int	bpf_setif(struct bpf_d *, struct ifreq *);
static void	bpf_timed_out(void *);
static void	bpf_wakeup(struct bpf_d *);
static void	catchpacket(struct bpf_d *, u_char *, u_int, u_int,
			    void (*)(const void *, void *, size_t));
static void	reset_d(struct bpf_d *);
static int	bpf_setf(struct bpf_d *, struct bpf_program *);
static int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
static int	bpf_setdlt(struct bpf_d *, u_int);
static void	bpf_drvinit(void *unused);

static d_open_t		bpfopen;
static d_close_t	bpfclose;
static d_read_t		bpfread;
static d_write_t	bpfwrite;
static d_ioctl_t	bpfioctl;
static d_poll_t		bpfpoll;

#define CDEV_MAJOR 23
static struct cdevsw bpf_cdevsw = {
	/* name */	"bpf",
	/* maj */	CDEV_MAJOR,
	/* flags */	0,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */	bpfopen,
	/* close */	bpfclose,
	/* read */	bpfread,
	/* write */	bpfwrite,
	/* ioctl */	bpfioctl,
	/* poll */	bpfpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};


static int
bpf_movein(struct uio *uio, int linktype, struct mbuf **mp,
	   struct sockaddr *sockp, int *datlen)
{
	struct mbuf *m;
	int error;
	int len;
	int hlen;

	/*
	 * Build a sockaddr based on the data link layer type.
	 * We do this at this level because the ethernet header
	 * is copied directly into the data field of the sockaddr.
	 * In the case of SLIP, there is no header and the packet
	 * is forwarded as is.
	 * Also, we are careful to leave room at the front of the mbuf
	 * for the link level header.
	 */
	switch (linktype) {

	case DLT_SLIP:
		sockp->sa_family = AF_INET;
		hlen = 0;
		break;

	case DLT_EN10MB:
		sockp->sa_family = AF_UNSPEC;
		/* XXX Would MAXLINKHDR be better? */
		hlen = sizeof(struct ether_header);
		break;

	case DLT_FDDI:
		sockp->sa_family = AF_IMPLINK;
		hlen = 0;
		break;

	case DLT_RAW:
	case DLT_NULL:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		break;

	case DLT_ATM_RFC1483:
		/*
		 * en atm driver requires 4-byte atm pseudo header.
		 * though it isn't standard, vpi:vci needs to be
		 * specified anyway.
		 */
		sockp->sa_family = AF_UNSPEC;
		hlen = 12;	/* XXX 4(ATM_PH) + 3(LLC) + 5(SNAP) */
		break;

	case DLT_PPP:
		sockp->sa_family = AF_UNSPEC;
		hlen = 4;	/* This should match PPP_HDRLEN */
		break;

	default:
		return(EIO);
	}

	len = uio->uio_resid;
	*datlen = len - hlen;
	if ((unsigned)len > MCLBYTES)
		return(EIO);

	MGETHDR(m, MB_WAIT, MT_DATA);
	if (m == NULL)
		return(ENOBUFS);
	if (len > MHLEN) {
		MCLGET(m, MB_WAIT);
		if (!(m->m_flags & M_EXT)) {
			error = ENOBUFS;
			goto bad;
		}
	}
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	*mp = m;
	/*
	 * Make room for link header.
	 */
	if (hlen != 0) {
		m->m_pkthdr.len -= hlen;
		m->m_len -= hlen;
		m->m_data += hlen; /* XXX */
		error = uiomove(sockp->sa_data, hlen, uio);
		if (error)
			goto bad;
	}
	error = uiomove(mtod(m, caddr_t), len - hlen, uio);
	if (!error)
		return(0);
bad:
	m_freem(m);
	return(error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 * Must be called at splimp.
 */
static void
bpf_attachd(struct bpf_d *d, struct bpf_if *bp)
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	d->bd_bif = bp;
	SLIST_INSERT_HEAD(&bp->bif_dlist, d, bd_next);
	*bp->bif_driverp = bp;
}

/*
 * Detach a file from its interface.
 */
static void
bpf_detachd(struct bpf_d *d)
{
	int error;
	struct bpf_if *bp;
	struct ifnet *ifp;

	bp = d->bd_bif;
	ifp = bp->bif_ifp;

	/* Remove d from the interface's descriptor list. */
	SLIST_REMOVE(&bp->bif_dlist, d, bpf_d, bd_next);

	if (SLIST_EMPTY(&bp->bif_dlist)) {
		/*
		 * Let the driver know that there are no more listeners.
		 */
		*bp->bif_driverp = NULL;
	}
	d->bd_bif = NULL;
	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		d->bd_promisc = 0;
		error = ifpromisc(ifp, 0);
		if (error != 0 && error != ENXIO) {
			/*
			 * ENXIO can happen if a pccard is unplugged,
			 * Something is really wrong if we were able to put
			 * the driver into promiscuous mode, but can't
			 * take it out.
			 */
			if_printf(ifp, "bpf_detach: ifpromisc failed(%d)\n",
				  error);
		}
	}
}

/*
 * Open ethernet device.  Returns ENXIO for illegal minor device number,
 * EBUSY if file is open by another process.
 */
/* ARGSUSED */
static int
bpfopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct bpf_d *d;
	struct proc *p = td->td_proc;

	KKASSERT(p != NULL);

	if (p->p_ucred->cr_prison)
		return(EPERM);

	d = dev->si_drv1;
	/*
	 * Each minor can be opened by only one process.  If the requested
	 * minor is in use, return EBUSY.
	 */
	if (d != NULL)
		return(EBUSY);
	make_dev(&bpf_cdevsw, minor(dev), 0, 0, 0600, "bpf%d", lminor(dev));
	MALLOC(d, struct bpf_d *, sizeof *d, M_BPF, M_WAITOK | M_ZERO);
	dev->si_drv1 = d;
	d->bd_bufsize = bpf_bufsize;
	d->bd_sig = SIGIO;
	d->bd_seesent = 1;
	callout_init(&d->bd_callout);
	return(0);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
static int
bpfclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct bpf_d *d = dev->si_drv1;
	int s;

	funsetown(d->bd_sigio);
	s = splimp();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	if (d->bd_bif != NULL)
		bpf_detachd(d);
	splx(s);
	bpf_freed(d);
	dev->si_drv1 = NULL;
	free(d, M_BPF);

	return(0);
}

/*
 * Rotate the packet buffers in descriptor d.  Move the store buffer
 * into the hold slot, and the free buffer into the store slot.
 * Zero the length of the new store buffer.
 */
#define ROTATE_BUFFERS(d) \
	(d)->bd_hbuf = (d)->bd_sbuf; \
	(d)->bd_hlen = (d)->bd_slen; \
	(d)->bd_sbuf = (d)->bd_fbuf; \
	(d)->bd_slen = 0; \
	(d)->bd_fbuf = NULL;
/*
 *  bpfread - read next chunk of packets from buffers
 */
static int
bpfread(dev_t dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d = dev->si_drv1;
	int timed_out;
	int error;
	int s;

	/*
	 * Restrict application to use a buffer the same size as
	 * as kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return(EINVAL);

	s = splimp();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	timed_out = (d->bd_state == BPF_TIMED_OUT);
	d->bd_state = BPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == NULL) {
		if ((d->bd_immediate || timed_out) && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}

		/*
		 * No data is available, check to see if the bpf device
		 * is still pointed at a real interface.  If not, return
		 * ENXIO so that the userland process knows to rebind
		 * it before using it again.
		 */
		if (d->bd_bif == NULL) {
			splx(s);
			return(ENXIO);
		}

		if (ioflag & IO_NDELAY) {
			splx(s);
			return(EWOULDBLOCK);
		}
		error = tsleep(d, PCATCH, "bpf", d->bd_rtout);
		if (error == EINTR || error == ERESTART) {
			splx(s);
			return(error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (d->bd_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (d->bd_slen == 0) {
				splx(s);
				return(0);
			}
			ROTATE_BUFFERS(d);
			break;
		}
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	splx(s);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is bpf_bufsize bytes.
	 */
	error = uiomove(d->bd_hbuf, d->bd_hlen, uio);

	s = splimp();
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = NULL;
	d->bd_hlen = 0;
	splx(s);

	return(error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static void
bpf_wakeup(struct bpf_d *d)
{
	if (d->bd_state == BPF_WAITING) {
		callout_stop(&d->bd_callout);
		d->bd_state = BPF_IDLE;
	}
	wakeup(d);
	if (d->bd_async && d->bd_sig && d->bd_sigio)
		pgsigio(d->bd_sigio, d->bd_sig, 0);

	selwakeup(&d->bd_sel);
	/* XXX */
	d->bd_sel.si_pid = 0;
}

static void
bpf_timed_out(void *arg)
{
	struct bpf_d *d = (struct bpf_d *)arg;
	int s;

	s = splimp();
	if (d->bd_state == BPF_WAITING) {
		d->bd_state = BPF_TIMED_OUT;
		if (d->bd_slen != 0)
			bpf_wakeup(d);
	}
	splx(s);
}

static	int
bpfwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct bpf_d *d = dev->si_drv1;
	struct ifnet *ifp;
	struct mbuf *m;
	int error, s;
	static struct sockaddr dst;
	int datlen;

	if (d->bd_bif == NULL)
		return(ENXIO);

	ifp = d->bd_bif->bif_ifp;

	if (uio->uio_resid == 0)
		return(0);

	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, &m, &dst, &datlen);
	if (error)
		return(error);

	if (datlen > ifp->if_mtu)
		return(EMSGSIZE);

	if (d->bd_hdrcmplt)
		dst.sa_family = pseudo_AF_HDRCMPLT;

	s = splnet();
	error = (*ifp->if_output)(ifp, m, &dst, (struct rtentry *)NULL);
	splx(s);
	/*
	 * The driver frees the mbuf.
	 */
	return(error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.  Should be called at splimp.
 */
static void
reset_d(struct bpf_d *d)
{
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  SIOCGIFADDR		Get interface address - convenient hook to driver.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set ethernet read filter.
 *  BIOCFLUSH		Flush read packet buffer.
 *  BIOCPROMISC		Put interface into promiscuous mode.
 *  BIOCGDLT		Get link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 *  BIOCGHDRCMPLT	Get "header already complete" flag
 *  BIOCSHDRCMPLT	Set "header already complete" flag
 *  BIOCGSEESENT	Get "see packets sent" flag
 *  BIOCSSEESENT	Set "see packets sent" flag
 */
/* ARGSUSED */
static int
bpfioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct bpf_d *d = dev->si_drv1;
	int s, error = 0;

	s = splimp();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	splx(s);

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Check for read packet available.
	 */
	case FIONREAD:
		{
			int n;

			s = splimp();
			n = d->bd_slen;
			if (d->bd_hbuf)
				n += d->bd_hlen;
			splx(s);

			*(int *)addr = n;
			break;
		}

	case SIOCGIFADDR:
		{
			struct ifnet *ifp;

			if (d->bd_bif == NULL)
				error = EINVAL;
			else {
				ifp = d->bd_bif->bif_ifp;
				error = (*ifp->if_ioctl)(ifp, cmd, addr,
							 td->td_proc->p_ucred);
			}
			break;
		}

	/*
	 * Get buffer len [for read()].
	 */
	case BIOCGBLEN:
		*(u_int *)addr = d->bd_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case BIOCSBLEN:
		if (d->bd_bif != 0)
			error = EINVAL;
		else {
			u_int size = *(u_int *)addr;

			if (size > bpf_maxbufsize)
				*(u_int *)addr = size = bpf_maxbufsize;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
		error = bpf_setf(d, (struct bpf_program *)addr);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		s = splimp();
		reset_d(d);
		splx(s);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == NULL) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		s = splimp();
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		splx(s);
		break;

	/*
	 * Get device parameters.
	 */
	case BIOCGDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Get a list of supported data link types.
	 */
	case BIOCGDLTLIST:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, (struct bpf_dltlist *)addr);
		break;

	/*
	 * Set data link type.
	 */
	case BIOCSDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Get interface name.
	 */
	case BIOCGETIF:
		if (d->bd_bif == NULL) {
			error = EINVAL;
		} else {
			struct ifnet *const ifp = d->bd_bif->bif_ifp;
			struct ifreq *const ifr = (struct ifreq *)addr;

			strlcpy(ifr->ifr_name, ifp->if_xname,
				sizeof ifr->ifr_name);
		}
		break;

	/*
	 * Set interface.
	 */
	case BIOCSETIF:
		error = bpf_setif(d, (struct ifreq *)addr);
		break;

	/*
	 * Set read timeout.
	 */
	case BIOCSRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			/*
			 * Subtract 1 tick from tvtohz() since this isn't
			 * a one-shot timer.
			 */
			if ((error = itimerfix(tv)) == 0)
				d->bd_rtout = tvtohz_low(tv);
			break;
		}

	/*
	 * Get read timeout.
	 */
	case BIOCGRTIMEOUT:
		{
			struct timeval *tv = (struct timeval *)addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}

	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = (struct bpf_stat *)addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			break;
		}

	/*
	 * Set immediate mode.
	 */
	case BIOCIMMEDIATE:
		d->bd_immediate = *(u_int *)addr;
		break;

	case BIOCVERSION:
		{
			struct bpf_version *bv = (struct bpf_version *)addr;

			bv->bv_major = BPF_MAJOR_VERSION;
			bv->bv_minor = BPF_MINOR_VERSION;
			break;
		}

	/*
	 * Get "header already complete" flag
	 */
	case BIOCGHDRCMPLT:
		*(u_int *)addr = d->bd_hdrcmplt;
		break;

	/*
	 * Set "header already complete" flag
	 */
	case BIOCSHDRCMPLT:
		d->bd_hdrcmplt = *(u_int *)addr ? 1 : 0;
		break;

	/*
	 * Get "see sent packets" flag
	 */
	case BIOCGSEESENT:
		*(u_int *)addr = d->bd_seesent;
		break;

	/*
	 * Set "see sent packets" flag
	 */
	case BIOCSSEESENT:
		d->bd_seesent = *(u_int *)addr;
		break;

	case FIONBIO:		/* Non-blocking I/O */
		break;

	case FIOASYNC:		/* Send signal on receive packets */
		d->bd_async = *(int *)addr;
		break;

	case FIOSETOWN:
		error = fsetown(*(int *)addr, &d->bd_sigio);
		break;

	case FIOGETOWN:
		*(int *)addr = fgetown(d->bd_sigio);
		break;

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		error = fsetown(-(*(int *)addr), &d->bd_sigio);
		break;

	/* This is deprecated, FIOGETOWN should be used instead. */
	case TIOCGPGRP:
		*(int *)addr = -fgetown(d->bd_sigio);
		break;

	case BIOCSRSIG:		/* Set receive signal */
		{
			u_int sig;

			sig = *(u_int *)addr;

			if (sig >= NSIG)
				error = EINVAL;
			else
				d->bd_sig = sig;
			break;
		}
	case BIOCGRSIG:
		*(u_int *)addr = d->bd_sig;
		break;
	}
	return(error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
static int
bpf_setf(struct bpf_d *d, struct bpf_program *fp)
{
	struct bpf_insn *fcode, *old;
	u_int flen, size;
	int s;

	old = d->bd_filter;
	if (fp->bf_insns == NULL) {
		if (fp->bf_len != 0)
			return(EINVAL);
		s = splimp();
		d->bd_filter = NULL;
		reset_d(d);
		splx(s);
		if (old != 0)
			free(old, M_BPF);
		return(0);
	}
	flen = fp->bf_len;
	if (flen > BPF_MAXINSNS)
		return(EINVAL);

	size = flen * sizeof *fp->bf_insns;
	fcode = (struct bpf_insn *)malloc(size, M_BPF, M_WAITOK);
	if (copyin(fp->bf_insns, fcode, size) == 0 &&
	    bpf_validate(fcode, (int)flen)) {
		s = splimp();
		d->bd_filter = fcode;
		reset_d(d);
		splx(s);
		if (old != 0)
			free(old, M_BPF);

		return(0);
	}
	free(fcode, M_BPF);
	return(EINVAL);
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
static int
bpf_setif(struct bpf_d *d, struct ifreq *ifr)
{
	struct bpf_if *bp;
	int s, error;
	struct ifnet *theywant;

	theywant = ifunit(ifr->ifr_name);
	if (theywant == NULL)
		return(ENXIO);

	/*
	 * Look through attached interfaces for the named one.
	 */
	for (bp = bpf_iflist; bp != 0; bp = bp->bif_next) {
		struct ifnet *ifp = bp->bif_ifp;

		if (ifp == NULL || ifp != theywant)
			continue;
		/* skip additional entry */
		if (bp->bif_driverp != &ifp->if_bpf)
			continue;
		/*
		 * We found the requested interface.
		 * If it's not up, return an error.
		 * Allocate the packet buffers if we need to.
		 * If we're already attached to requested interface,
		 * just flush the buffer.
		 */
		if (!(ifp->if_flags & IFF_UP))
			return(ENETDOWN);

		if (d->bd_sbuf == NULL) {
			error = bpf_allocbufs(d);
			if (error != 0)
				return(error);
		}
		s = splimp();
		if (bp != d->bd_bif) {
			if (d->bd_bif != NULL) {
				/*
				 * Detach if attached to something else.
				 */
				bpf_detachd(d);
			}

			bpf_attachd(d, bp);
		}
		reset_d(d);
		splx(s);
		return(0);
	}

	/* Not found. */
	return(ENXIO);
}

/*
 * Support for select() and poll() system calls
 *
 * Return true iff the specific operation will not block indefinitely.
 * Otherwise, return false but make a note that a selwakeup() must be done.
 */
int
bpfpoll(dev_t dev, int events, struct thread *td)
{
	struct bpf_d *d;
	int s;
	int revents;

	d = dev->si_drv1;
	if (d->bd_bif == NULL)
		return(ENXIO);

	revents = events & (POLLOUT | POLLWRNORM);
	s = splimp();
	if (events & (POLLIN | POLLRDNORM)) {
		/*
		 * An imitation of the FIONREAD ioctl code.
		 * XXX not quite.  An exact imitation:
		 *	if (d->b_slen != 0 ||
		 *	    (d->bd_hbuf != NULL && d->bd_hlen != 0)
		 */
		if (d->bd_hlen != 0 ||
		    ((d->bd_immediate || d->bd_state == BPF_TIMED_OUT) &&
		    d->bd_slen != 0))
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			selrecord(td, &d->bd_sel);
			/* Start the read timeout if necessary. */
			if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
				callout_reset(&d->bd_callout, d->bd_rtout,
				    bpf_timed_out, d);
				d->bd_state = BPF_WAITING;
			}
		}
	}
	splx(s);
	return(revents);
}

/*
 * Process the packet pkt of length pktlen.  The packet is parsed
 * by each listener's filter, and if accepted, stashed into the
 * corresponding buffer.
 */
void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
	struct bpf_d *d;
	u_int slen;

	/*
	 * Note that the ipl does not have to be raised at this point.
	 * The only problem that could arise here is that if two different
	 * interfaces shared any data.  This is not the case.
	 */
	SLIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, pkt, pktlen, pktlen);
		if (slen != 0)
			catchpacket(d, pkt, pktlen, slen, ovbcopy);
	}
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
static void
bpf_mcopy(const void *src_arg, void *dst_arg, size_t len)
{
	const struct mbuf *m;
	u_int count;
	u_char *dst;

	m = src_arg;
	dst = dst_arg;
	while (len > 0) {
		if (m == NULL)
			panic("bpf_mcopy");
		count = min(m->m_len, len);
		bcopy(mtod(m, void *), dst, count);
		m = m->m_next;
		dst += count;
		len -= count;
	}
}

/*
 * Process the packet in the mbuf chain m.  The packet is parsed by each
 * listener's filter, and if accepted, stashed into the corresponding
 * buffer.
 */
void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
	struct bpf_d *d;
	u_int pktlen, slen;
	struct mbuf *m0;

	/* Don't compute pktlen, if no descriptor is attached. */
	if (SLIST_EMPTY(&bp->bif_dlist))
		return;

	pktlen = 0;
	for (m0 = m; m0 != NULL; m0 = m0->m_next)
		pktlen += m0->m_len;

	SLIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		if (!d->bd_seesent && (m->m_pkthdr.rcvif == NULL))
			continue;
		++d->bd_rcount;
		slen = bpf_filter(d->bd_filter, (u_char *)m, pktlen, 0);
		if (slen != 0)
			catchpacket(d, (u_char *)m, pktlen, slen, bpf_mcopy);
	}
}

/*
 * Process the packet in the mbuf chain m with the header in m prepended.
 * The packet is parsed by each listener's filter, and if accepted,
 * stashed into the corresponding buffer.
 */
void
bpf_ptap(struct bpf_if *bp, struct mbuf *m, const void *data, u_int dlen)
{
	struct mbuf mb;

	/*
	 * Craft on-stack mbuf suitable for passing to bpf_mtap.
	 * Note that we cut corners here; we only setup what's
	 * absolutely needed--this mbuf should never go anywhere else.
	 */
	mb.m_next = m;
	mb.m_data = __DECONST(void *, data); /* LINTED */
	mb.m_len = dlen;

	bpf_mtap(bp, &mb);
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer.  Return 1 if it's time to wakeup a listener (buffer full),
 * otherwise 0.  "copy" is the routine called to do the actual data
 * transfer.  bcopy is passed in to copy contiguous chunks, while
 * bpf_mcopy is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(struct bpf_d *d, u_char *pkt, u_int pktlen, u_int snaplen,
	    void (*cpfn)(const void *, void *, size_t))
{
	struct bpf_hdr *hp;
	int totlen, curlen;
	int hdrlen = d->bd_bif->bif_hdrlen;
	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;

	/*
	 * Round up the end of the previous packet to the next longword.
	 */
	curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize) {
		/*
		 * This packet will overflow the storage buffer.
		 * Rotate the buffers if we can, then wakeup any
		 * pending reads.
		 */
		if (d->bd_fbuf == NULL) {
			/*
			 * We haven't completed the previous read yet,
			 * so drop the packet.
			 */
			++d->bd_dcount;
			return;
		}
		ROTATE_BUFFERS(d);
		bpf_wakeup(d);
		curlen = 0;
	}
	else if (d->bd_immediate || d->bd_state == BPF_TIMED_OUT)
		/*
		 * Immediate mode is set, or the read timeout has
		 * already expired during a select call.  A packet
		 * arrived, so the reader should be woken up.
		 */
		bpf_wakeup(d);

	/*
	 * Append the bpf header.
	 */
	hp = (struct bpf_hdr *)(d->bd_sbuf + curlen);
	microtime(&hp->bh_tstamp);
	hp->bh_datalen = pktlen;
	hp->bh_hdrlen = hdrlen;
	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(pkt, (u_char *)hp + hdrlen, (hp->bh_caplen = totlen - hdrlen));
	d->bd_slen = curlen + totlen;
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
static int
bpf_allocbufs(struct bpf_d *d)
{
	d->bd_fbuf = malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	if (d->bd_fbuf == NULL)
		return(ENOBUFS);

	d->bd_sbuf = malloc(d->bd_bufsize, M_BPF, M_WAITOK);
	if (d->bd_sbuf == NULL) {
		free(d->bd_fbuf, M_BPF);
		return(ENOBUFS);
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	return(0);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
bpf_freed(struct bpf_d *d)
{
	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	if (d->bd_sbuf != NULL) {
		free(d->bd_sbuf, M_BPF);
		if (d->bd_hbuf != NULL)
			free(d->bd_hbuf, M_BPF);
		if (d->bd_fbuf != NULL)
			free(d->bd_fbuf, M_BPF);
	}
	if (d->bd_filter)
		free(d->bd_filter, M_BPF);
}

/*
 * Attach an interface to bpf.  ifp is a pointer to the structure
 * defining the interface to be attached, dlt is the link layer type,
 * and hdrlen is the fixed size of the link header (variable length
 * headers are not yet supported).
 */
void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{
	bpfattach_dlt(ifp, dlt, hdrlen, &ifp->if_bpf);
}

void
bpfattach_dlt(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{
	struct bpf_if *bp;

	bp = malloc(sizeof *bp, M_BPF, M_WAITOK | M_ZERO);

	SLIST_INIT(&bp->bif_dlist);
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;
	bp->bif_driverp = driverp;
	*bp->bif_driverp = NULL;

	bp->bif_next = bpf_iflist;
	bpf_iflist = bp;

	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
	bp->bif_hdrlen = BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen;

	if (bootverbose)
		if_printf(ifp, "bpf attached\n");
}

/*
 * Detach bpf from an interface.  This involves detaching each descriptor
 * associated with the interface, and leaving bd_bif NULL.  Notify each
 * descriptor as it's detached so that any sleepers wake up and get
 * ENXIO.
 */
void
bpfdetach(struct ifnet *ifp)
{
	struct bpf_if *bp, *bp_prev;
	struct bpf_d *d;
	int s;

	s = splimp();

	/* Locate BPF interface information */
	bp_prev = NULL;
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (ifp == bp->bif_ifp)
			break;
		bp_prev = bp;
	}

	/* Interface wasn't attached */
	if (bp->bif_ifp == NULL) {
		splx(s);
		printf("bpfdetach: %s was not attached\n", ifp->if_xname);
		return;
	}

	while ((d = SLIST_FIRST(&bp->bif_dlist)) != NULL) {
		bpf_detachd(d);
		bpf_wakeup(d);
	}

	if (bp_prev != NULL)
		bp_prev->bif_next = bp->bif_next;
	else
		bpf_iflist = bp->bif_next;

	free(bp, M_BPF);

	splx(s);
}

/*
 * Get a list of available data link type of the interface.
 */
static int
bpf_getdltlist(struct bpf_d *d, struct bpf_dltlist *bfl)
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	for (bp = bpf_iflist; bp != 0; bp = bp->bif_next) {
		if (bp->bif_ifp != ifp)
			continue;
		if (bfl->bfl_list != NULL) {
			if (n >= bfl->bfl_len) {
				return (ENOMEM);
			}
			error = copyout(&bp->bif_dlt,
			    bfl->bfl_list + n, sizeof(u_int));
		}
		n++;
	}
	bfl->bfl_len = n;
	return(error);
}

/*
 * Set the data link type of a BPF instance.
 */
static int
bpf_setdlt(struct bpf_d *d, u_int dlt)
{
	int error, opromisc;
	struct ifnet *ifp;
	struct bpf_if *bp;
	int s;

	if (d->bd_bif->bif_dlt == dlt)
		return (0);
	ifp = d->bd_bif->bif_ifp;
	for (bp = bpf_iflist; bp != 0; bp = bp->bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	if (bp != NULL) {
		opromisc = d->bd_promisc;
		s = splimp();	
		bpf_detachd(d);
		bpf_attachd(d, bp);
		reset_d(d);
		if (opromisc) {
			error = ifpromisc(bp->bif_ifp, 1);
			if (error)
				if_printf(bp->bif_ifp,
					"bpf_setdlt: ifpromisc failed (%d)\n",
					error);
			else
				d->bd_promisc = 1;
		}
		splx(s);
	}
	return(bp == NULL ? EINVAL : 0);
}

static void
bpf_drvinit(void *unused)
{
	cdevsw_add(&bpf_cdevsw, 0, 0);
}

SYSINIT(bpfdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,bpf_drvinit,NULL)

#else /* !BPF */
/*
 * NOP stubs to allow bpf-using drivers to load and function.
 *
 * A 'better' implementation would allow the core bpf functionality
 * to be loaded at runtime.
 */

void
bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{
}

void
bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
}

void
bpf_ptap(struct bpf_if *bp, struct mbuf *m, const void *data, u_int dlen)
{
}

void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{
}

void
bpfattach_dlt(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{
}

void
bpfdetach(struct ifnet *ifp)
{
}

u_int
bpf_filter(const struct bpf_insn *pc, u_char *p, u_int wirelen, u_int buflen)
{
	return -1;	/* "no filter" behaviour */
}

#endif /* !BPF */
