/*
 * Cronyx-Sigma adapter driver for FreeBSD.
 * Supports PPP/HDLC and Cisco/HDLC protocol in synchronous mode,
 * and asyncronous channels with full modem control.
 * Keepalive protocol implemented in both Cisco and PPP modes.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 *
 * $FreeBSD: src/sys/i386/isa/if_cx.c,v 1.32 1999/11/18 08:36:42 peter Exp $
 * $DragonFly: src/sys/dev/netif/cx/if_cx.c,v 1.15 2005/01/23 20:21:30 joerg Exp $
 *
 */
#undef DEBUG

#include "use_cx.h"
#include "use_sppp.h"
#if NSPPP <= 0
#error The device 'cx' requires sppp.
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/conf.h>

#include <net/if.h>

#include <net/bpf.h>

#include <bus/isa/i386/isa_device.h>
#define watchdog_func_t void(*)(struct ifnet *)
#define start_func_t    void(*)(struct ifnet*)

#include <net/sppp/if_sppp.h>
#include <machine/cronyx.h>
#include "cxreg.h"
#include "cx.c"

#if 0
/* XXX exported. */
void cxswitch (cx_chan_t *c, cx_soft_opt_t new);
#endif

static int cxprobe (struct isa_device *id);
static int cxattach (struct isa_device *id);
static void cxput (cx_chan_t *c, char b);
static void cxsend (cx_chan_t *c);
static void cxrinth (cx_chan_t *c);
static ointhand2_t cxintr;
static int cxtinth (cx_chan_t *c);

#ifdef DEBUG
#   define print(s)     printf s
#else
#   define print(s)     {/*void*/}
#endif

#define TXTIMEOUT       10              /* transmit timeout in seconds */
#define DMABUFSZ        (6*256)         /* buffer size */
#define PPP_HEADER_LEN  4               /* size of PPP header */

/*
 * Under BSDI it's possible to use general p2p protocol scheme,
 * as well as our own one.  Switching is done via IFF_ALTPHYS flag.
 * Our ifnet pointer holds the buffer large enough to contain
 * any of sppp and p2p structures.
 */
#define IFSTRUCTSZ   (sizeof (struct sppp))
#define IFNETSZ         (sizeof (struct ifnet))

static int cxsioctl (struct ifnet *ifp, u_long cmd, caddr_t data,
		     struct ucred *cr);
static void cxstart (struct ifnet *ifp);
static void cxwatchdog (struct ifnet *ifp);
static void cxinput (cx_chan_t *c, void *buf, unsigned len);
#if 0
extern int cxrinta (cx_chan_t *c);
extern void cxtinta (cx_chan_t *c);
extern void cxmint (cx_chan_t *c);
extern timeout_t cxtimeout;
#endif
static void cxdown (cx_chan_t *c);
static void cxup (cx_chan_t *c);

cx_board_t cxboard [NCX];           /* adapter state structures */
cx_chan_t *cxchan [NCX*NCHAN];      /* unit to channel struct pointer */
#if 0
extern struct cdevsw cx_cdevsw;
#endif
static unsigned short irq_valid_values [] = { 3, 5, 7, 10, 11, 12, 15, 0 };
static unsigned short drq_valid_values [] = { 5, 6, 7, 0 };
static unsigned short port_valid_values [] = {
	0x240, 0x260, 0x280, 0x300, 0x320, 0x380, 0x3a0, 0,
};
struct callout cxtimeout_ch;

DECLARE_DUMMY_MODULE(if_cx);

/*
 * Check that the value is contained in the list of correct values.
 */
static int valid (unsigned short value, unsigned short *list)
{
	while (*list)
		if (value == *list++)
			return (1);
	return (0);
}

/*
 * Print the mbuf chain, for debug purposes only.
 */
static void printmbuf (struct mbuf *m)
{
	printf ("mbuf:");
	for (; m; m=m->m_next) {
		if (m->m_flags & M_PKTHDR)
			printf (" HDR %d:", m->m_pkthdr.len);
		if (m->m_flags & M_EXT)
			printf (" EXT:");
		printf (" %d", m->m_len);
	}
	printf ("\n");
}

/*
 * Make an mbuf from data.
 */
static struct mbuf *makembuf (void *buf, unsigned len)
{
	struct mbuf *m, *o, *p;

	MGETHDR (m, MB_DONTWAIT, MT_DATA);
	if (! m)
		return (0);
	if (len >= MINCLSIZE)
		MCLGET (m, MB_DONTWAIT);
	m->m_pkthdr.len = len;
	m->m_len = 0;

	p = m;
	while (len) {
		unsigned n = M_TRAILINGSPACE (p);
		if (n > len)
			n = len;

		if (! n) {
			/* Allocate new mbuf. */
			o = p;
			MGET (p, MB_DONTWAIT, MT_DATA);
			if (! p) {
				m_freem (m);
				return (0);
			}
			if (len >= MINCLSIZE)
				MCLGET (p, MB_DONTWAIT);
			p->m_len = 0;
			o->m_next = p;

			n = M_TRAILINGSPACE (p);
			if (n > len)
				n = len;
		}

		bcopy (buf, mtod (p, caddr_t) + p->m_len, n);

		p->m_len += n;
		buf = (char *)buf + n;
		len -= n;
	}
	return (m);
}

/*
 * Test the presence of the adapter on the given i/o port.
 */
static int
cxprobe (struct isa_device *id)
{
	int unit = id->id_unit;
	int iobase = id->id_iobase;
	int irq = id->id_irq;
	int drq = id->id_drq;
	int irqnum;
	irqnum = ffs (irq) - 1;

	print (("cx%d: probe iobase=0x%x irq=%d drq=%d\n",
		unit, iobase, irqnum, drq));
	if (! valid (irqnum, irq_valid_values)) {
		printf ("cx%d: Incorrect IRQ: %d\n", unit, irqnum);
		return (0);
	}
	if (! valid (iobase, port_valid_values)) {
		printf ("cx%d: Incorrect port address: 0x%x\n", unit, iobase);
		return (0);
	}
	if (! valid (drq, drq_valid_values)) {
		printf ("cx%d: Incorrect DMA channel: %d\n", unit, drq);
		return (0);
	}
	if (! cx_probe_board (iobase))
		return (0);

	return (1);
}

/*
 * The adapter is present, initialize the driver structures.
 */

static int
cxattach (struct isa_device *id)
{
	int unit = id->id_unit;
	int iobase = id->id_iobase;
	int irq = id->id_irq;
	int drq = id->id_drq;
	cx_board_t *b = cxboard + unit;
	int i;
	struct sppp *sp;

	id->id_ointr = cxintr;

	/* Initialize the board structure. */
	cx_init (b, unit, iobase, ffs(irq)-1, drq);

	for (i=0; i<NCHAN; ++i) {
		cx_chan_t *c = b->chan + i;
		int u = b->num*NCHAN + i;
		cxchan[u] = c;

		if (c->type == T_NONE)
			continue;

		/* Allocate the buffer memory. */
		c->arbuf = malloc (DMABUFSZ, M_DEVBUF, M_WAITOK);
		c->brbuf = malloc (DMABUFSZ, M_DEVBUF, M_WAITOK);
		c->atbuf = malloc (DMABUFSZ, M_DEVBUF, M_WAITOK);
		c->btbuf = malloc (DMABUFSZ, M_DEVBUF, M_WAITOK);

#if 0
		/* All buffers should be located in lower 16M of memory! */
		/* XXX all buffers located where?  I don't think so! */
		if (!c->arbuf || !c->brbuf || !c->atbuf || !c->btbuf) {
			printf ("cx%d.%d: No memory for channel buffers\n",
				c->board->num, c->num);
			c->type = T_NONE;
		}
#endif

		switch (c->type) {
		case T_SYNC_RS232:
		case T_SYNC_V35:
		case T_SYNC_RS449:
		case T_UNIV_RS232:
		case T_UNIV_RS449:
		case T_UNIV_V35:
			c->ifp = malloc (IFSTRUCTSZ, M_DEVBUF, M_WAITOK | M_ZERO);
			c->master = c->ifp;
			c->ifp->if_softc = c;
			if_initname(c->ifp, "cx", u);
			c->ifp->if_mtu = PP_MTU;
			c->ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
			c->ifp->if_ioctl = cxsioctl;
			c->ifp->if_start = (start_func_t) cxstart;
			c->ifp->if_watchdog = (watchdog_func_t) cxwatchdog;
			/* Init routine is never called by upper level? */
			sppp_attach (c->ifp);
			if_attach (c->ifp);
			sp = (struct sppp*) c->ifp;
			/* If BPF is in the kernel, call the attach for it. */
			bpfattach (c->ifp, DLT_PPP, PPP_HEADER_LEN);
		}
	}

	/* Reset the adapter. */
	cx_setup_board (b);

	/* Activate the timeout routine. */
	if (unit == 0) {
		callout_init(&cxtimeout_ch);
		callout_reset(&cxtimeout_ch, hz * 5, cxtimeout, NULL);
	}
	printf ("cx%d: <Cronyx-%s>\n", unit, b->name);
	cdevsw_add(&cx_cdevsw, -1, unit);
	make_dev(&cx_cdevsw, unit, UID_ROOT, GID_WHEEL, 0600, "cx%d", unit);
	return (1);
}

struct isa_driver cxdriver = { cxprobe, cxattach, "cx" };

/*
 * Process an ioctl request.
 */
static int
cxsioctl (struct ifnet *ifp, u_long cmd, caddr_t data, struct ucred *cr)
{
	cx_chan_t *q, *c = ifp->if_softc;
	int error, s, was_up, should_be_up;

	/*
	 * No socket ioctls while the channel is in async mode.
	 */
	if (c->type==T_NONE || c->mode==M_ASYNC)
		return (EINVAL);

	/*
	 * Socket ioctls on slave subchannels are not allowed.
	 */
	if (c->master != c->ifp)
		return (EBUSY);

	was_up = (ifp->if_flags & IFF_RUNNING) != 0;
	error = sppp_ioctl (ifp, cmd, data);
	if (error)
		return (error);

	print (("cxioctl (%d.%d, ", c->board->num, c->num));
	switch (cmd) {
	default:
		print (("0x%x)\n", cmd));
		return (0);
	case SIOCADDMULTI:
		print (("SIOCADDMULTI)\n"));
		return (0);
	case SIOCDELMULTI:
		print (("SIOCDELMULTI)\n"));
		return (0);
	case SIOCSIFFLAGS:
		print (("SIOCSIFFLAGS)\n"));
		break;
	case SIOCSIFADDR:
		print (("SIOCSIFADDR)\n"));
		break;
	}

	/* We get here only in case of SIFFLAGS or SIFADDR. */
	s = splimp ();
	should_be_up = (ifp->if_flags & IFF_RUNNING) != 0;
	if (!was_up && should_be_up) {
		/* Interface goes up -- start it. */
		cxup (c);

		/* Start all slave subchannels. */
		for (q=c->slaveq; q; q=q->slaveq)
			cxup (q);

		cxstart (c->ifp);
	} else if (was_up && !should_be_up) {
		/* Interface is going down -- stop it. */
		cxdown (c);

		/* Stop all slave subchannels. */
		for (q=c->slaveq; q; q=q->slaveq)
			cxdown (q);

		/* Flush the interface output queue */
		if (! c->sopt.ext)
			sppp_flush (c->ifp);
	}
	splx (s);
	return (0);
}

/*
 * Stop the interface.  Called on splimp().
 */
static void
cxdown (cx_chan_t *c)
{
	unsigned short port = c->chip->port;

	print (("cx%d.%d: cxdown\n", c->board->num, c->num));

	/* The interface is down, stop it */
	c->ifp->if_flags &= ~IFF_OACTIVE;

	/* Reset the channel (for sync modes only) */
		outb (CAR(port), c->num & 3);
		outb (STCR(port), STC_ABORTTX | STC_SNDSPC);

	cx_setup_chan (c);
}

/*
 * Start the interface.  Called on splimp().
 */
static void
cxup (cx_chan_t *c)
{
	unsigned short port = c->chip->port;

		/* The interface is up, start it */
	        print (("cx%d.%d: cxup\n", c->board->num, c->num));

		/* Initialize channel, enable receiver and transmitter */
		cx_cmd (port, CCR_INITCH | CCR_ENRX | CCR_ENTX);
		/* Repeat the command, to avoid the rev.H bug */
		cx_cmd (port, CCR_INITCH | CCR_ENRX | CCR_ENTX);

		/* Start receiver */
		outw (ARBCNT(port), DMABUFSZ);
		outb (ARBSTS(port), BSTS_OWN24);
		outw (BRBCNT(port), DMABUFSZ);
		outb (BRBSTS(port), BSTS_OWN24);

		/* Raise DTR and RTS */
		cx_chan_dtr (c, 1);
		cx_chan_rts (c, 1);

		/* Enable interrupts */
		outb (IER(port), IER_RXD | IER_TXD);
}

/*
 * Fill transmitter buffer with data.
 */
static void 
cxput (cx_chan_t *c, char b)
{
	struct mbuf *m;
	unsigned char *buf;
	unsigned short port = c->chip->port, len, cnt_port, sts_port;

	/* Choose the buffer. */
	if (b == 'A') {
		buf      = c->atbuf;
		cnt_port = ATBCNT(port);
		sts_port = ATBSTS(port);
	} else {
		buf      = c->btbuf;
		cnt_port = BTBCNT(port);
		sts_port = BTBSTS(port);
	}

	/* Is it busy? */
	if (inb (sts_port) & BSTS_OWN24) {
		if (c->ifp->if_flags & IFF_DEBUG)
			print (("cx%d.%d: tbuf %c already busy, bsts=%b\n",
				c->board->num, c->num, b,
				inb (sts_port), BSTS_BITS));
		goto ret;
	}

	/* Get the packet to send. */
	m = sppp_dequeue (c->master);
	if (! m)
		return;
	len = m->m_pkthdr.len;

	/* Count the transmitted bytes to the subchannel, not the master. */
	c->master->if_obytes -= len + 3;
	c->ifp->if_obytes += len + 3;
	c->stat->obytes += len + 3;

	if (len >= DMABUFSZ) {
		printf ("cx%d.%d: too long packet: %d bytes: ",
			c->board->num, c->num, len);
		printmbuf (m);
		m_freem (m);
		return;
	}
	m_copydata (m, 0, len, buf);
	BPF_MTAP(c->ifp, m);
	m_freem (m);

	/* Start transmitter. */
	outw (cnt_port, len);
	outb (sts_port, BSTS_EOFR | BSTS_INTR | BSTS_OWN24);

	if (c->ifp->if_flags & IFF_DEBUG)
		print (("cx%d.%d: enqueue %d bytes to %c\n",
			c->board->num, c->num, len, buf==c->atbuf ? 'A' : 'B'));
ret:
	c->ifp->if_flags |= IFF_OACTIVE;
}

/*
 * Start output on the (slave) interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
static void
cxsend (cx_chan_t *c)
{
	unsigned short port = c->chip->port;

	if (c->ifp->if_flags & IFF_DEBUG)
		print (("cx%d.%d: cxsend\n", c->board->num, c->num));

	/* No output if the interface is down. */
	if (! (c->ifp->if_flags & IFF_RUNNING))
		return;

	/* Set the current channel number. */
	outb (CAR(port), c->num & 3);

	/* Determine the buffer order. */
	if (inb (DMABSTS(port)) & DMABSTS_NTBUF) {
		cxput (c, 'B');
		cxput (c, 'A');
	} else {
		cxput (c, 'A');
		cxput (c, 'B');
	}

	/* Set up transmit timeout. */
	if (c->master->if_flags & IFF_OACTIVE)
		c->master->if_timer = TXTIMEOUT;

	/*
	 * Enable TXMPTY interrupt,
	 * to catch the case when the second buffer is empty.
	 */
	if ((inb (ATBSTS(port)) & BSTS_OWN24) &&
	    (inb (BTBSTS(port)) & BSTS_OWN24)) {
		outb (IER(port), IER_RXD | IER_TXD | IER_TXMPTY);
	} else
		outb (IER(port), IER_RXD | IER_TXD);
}

/*
 * Start output on the (master) interface and all slave interfaces.
 * Always called on splimp().
 */
static void
cxstart (struct ifnet *ifp)
{
	cx_chan_t *q, *c = ifp->if_softc;

	if (c->ifp->if_flags & IFF_DEBUG)
		print (("cx%d.%d: cxstart\n", c->board->num, c->num));

	/* Start the master subchannel. */
	cxsend (c);

	/* Start all slave subchannels. */
	if (c->slaveq && ! sppp_isempty (c->master))
		for (q=c->slaveq; q; q=q->slaveq)
			if ((q->ifp->if_flags & IFF_RUNNING) &&
			    ! (q->ifp->if_flags & IFF_OACTIVE))
				cxsend (q);
}

/*
 * Handle transmit timeouts.
 * Recover after lost transmit interrupts.
 * Always called on splimp().
 */
static void
cxwatchdog (struct ifnet *ifp)
{
	cx_chan_t *q, *c = ifp->if_softc;

	if (! (ifp->if_flags & IFF_RUNNING))
		return;
	if (ifp->if_flags & IFF_DEBUG)
		printf ("cx%d.%d: device timeout\n", c->board->num, c->num);

	cxdown (c);
	for (q=c->slaveq; q; q=q->slaveq)
		cxdown (q);

	cxup (c);
	for (q=c->slaveq; q; q=q->slaveq)
		cxup (q);

		cxstart (ifp);
}

/*
 * Handle receive interrupts, including receive errors and
 * receive timeout interrupt.
 */
static void 
cxrinth (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned short len, risr = inw (RISR(port));

	/* Receive errors. */
	if (risr & (RIS_BUSERR | RIS_OVERRUN | RISH_CRCERR | RISH_RXABORT)) {
		if (c->ifp->if_flags & IFF_DEBUG)
			printf ("cx%d.%d: receive error, risr=%b\n",
				c->board->num, c->num, risr, RISH_BITS);
		++c->ifp->if_ierrors;
		++c->stat->ierrs;
		if (risr & RIS_OVERRUN)
			++c->ifp->if_collisions;
	} else if (risr & RIS_EOBUF) {
		if (c->ifp->if_flags & IFF_DEBUG)
			print (("cx%d.%d: hdlc receive interrupt, risr=%b, arbsts=%b, brbsts=%b\n",
				c->board->num, c->num, risr, RISH_BITS,
				inb (ARBSTS(port)), BSTS_BITS,
				inb (BRBSTS(port)), BSTS_BITS));
		++c->stat->ipkts;

		/* Handle received data. */
		len = (risr & RIS_BB) ? inw(BRBCNT(port)) : inw(ARBCNT(port));
		c->stat->ibytes += len;
		if (len > DMABUFSZ) {
			/* Fatal error: actual DMA transfer size
			 * exceeds our buffer size.  It could be caused
			 * by incorrectly programmed DMA register or
			 * hardware fault.  Possibly, should panic here. */
			printf ("cx%d.%d: panic! DMA buffer overflow: %d bytes\n",
			       c->board->num, c->num, len);
			++c->ifp->if_ierrors;
		} else if (! (risr & RIS_EOFR)) {
			/* The received frame does not fit in the DMA buffer.
			 * It could be caused by serial lie noise,
			 * or if the peer has too big MTU. */
			if (c->ifp->if_flags & IFF_DEBUG)
				printf ("cx%d.%d: received frame length exceeds MTU, risr=%b\n",
					c->board->num, c->num, risr, RISH_BITS);
			++c->ifp->if_ierrors;
		} else {
			/* Valid frame received. */
			if (c->ifp->if_flags & IFF_DEBUG)
				print (("cx%d.%d: hdlc received %d bytes\n",
				c->board->num, c->num, len));
			cxinput (c, (risr & RIS_BB) ? c->brbuf : c->arbuf, len);
			++c->ifp->if_ipackets;
		}
	} else if (c->ifp->if_flags & IFF_DEBUG) {
		print (("cx%d.%d: unknown hdlc receive interrupt, risr=%b\n",
			c->board->num, c->num, risr, RISH_BITS));
		++c->stat->ierrs;
	}

	/* Restart receiver. */
	if (! (inb (ARBSTS(port)) & BSTS_OWN24)) {
		outw (ARBCNT(port), DMABUFSZ);
		outb (ARBSTS(port), BSTS_OWN24);
	}
	if (! (inb (BRBSTS(port)) & BSTS_OWN24)) {
		outw (BRBCNT(port), DMABUFSZ);
		outb (BRBSTS(port), BSTS_OWN24);
	}
}

/*
 * Handle transmit interrupt.
 */
static int
cxtinth (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned char tisr = inb (TISR(port));
	unsigned char teoir = 0;

	c->ifp->if_flags &= ~IFF_OACTIVE;
	if (c->ifp == c->master)
		c->ifp->if_timer = 0;

	if (tisr & (TIS_BUSERR | TIS_UNDERRUN)) {
		/* if (c->ifp->if_flags & IFF_DEBUG) */
			print (("cx%d.%d: transmit error, tisr=%b, atbsts=%b, btbsts=%b\n",
				c->board->num, c->num, tisr, TIS_BITS,
				inb (ATBSTS(port)), BSTS_BITS,
				inb (BTBSTS(port)), BSTS_BITS));
		++c->ifp->if_oerrors;
		++c->stat->oerrs;

		/* Terminate the failed buffer. */
		/* teoir = TEOI_TERMBUFF; */
	} else if (c->ifp->if_flags & IFF_DEBUG)
		print (("cx%d.%d: hdlc transmit interrupt, tisr=%b, atbsts=%b, btbsts=%b\n",
			c->board->num, c->num, tisr, TIS_BITS,
			inb (ATBSTS(port)), BSTS_BITS,
			inb (BTBSTS(port)), BSTS_BITS));

	if (tisr & TIS_EOFR) {
		++c->ifp->if_opackets;
		++c->stat->opkts;
	}

	/* Start output on the (sub-) channel. */
	cxsend (c);

	return (teoir);
}

static void
cxintr (int bnum)
{
	cx_board_t *b = cxboard + bnum;
	while (! (inw (BSR(b->port)) & BSR_NOINTR)) {
		/* Acknowledge the interrupt to enter the interrupt context. */
		/* Read the local interrupt vector register. */
		unsigned char livr = inb (IACK(b->port, BRD_INTR_LEVEL));
		cx_chan_t *c = b->chan + (livr>>2 & 0xf);
		unsigned short port = c->chip->port;
		unsigned short eoiport = REOIR(port);
		unsigned char eoi = 0;

		if (c->type == T_NONE) {
			printf ("cx%d.%d: unexpected interrupt, livr=0x%x\n",
				c->board->num, c->num, livr);
			continue;       /* incorrect channel number? */
		}
		/* print (("cx%d.%d: interrupt, livr=0x%x\n",
			c->board->num, c->num, livr)); */

		/* Clear RTS to stop receiver data flow while we are busy
		 * processing the interrupt, thus avoiding underruns. */
		if (! c->sopt.norts) {
			outb (MSVR_RTS(port), 0);
			c->rts = 0;
		}

		switch (livr & 3) {
		case LIV_EXCEP:         /* receive exception */
		case LIV_RXDATA:        /* receive interrupt */
			++c->stat->rintr;
			switch (c->mode) {
			case M_ASYNC: eoi = cxrinta (c); break;
			case M_HDLC:  cxrinth (c);       break;
			default:;       /* No bisync and X.21 yet */
			}
			break;
		case LIV_TXDATA:        /* transmit interrupt */
			++c->stat->tintr;
			eoiport = TEOIR(port);
			switch (c->mode) {
			case M_ASYNC: cxtinta (c);       break;
			case M_HDLC:  eoi = cxtinth (c); break;
			default:;       /* No bisync and X.21 yet */
			}
			break;
		case LIV_MODEM:         /* modem/timer interrupt */
			++c->stat->mintr;
			eoiport = MEOIR(port);
			cxmint (c);
			break;
		}

		/* Raise RTS for this channel if and only if
		 * both receive buffers are empty. */
		if (! c->sopt.norts && (inb (CSR(port)) & CSRA_RXEN) &&
		    (inb (ARBSTS(port)) & BSTS_OWN24) &&
		    (inb (BRBSTS(port)) & BSTS_OWN24)) {
			outb (MSVR_RTS(port), MSV_RTS);
			c->rts = 1;
		}

		/* Exit from interrupt context. */
		outb (eoiport, eoi);

		/* Master channel - start output on all idle subchannels. */
		if (c->master == c->ifp && c->slaveq &&
		    (livr & 3) == LIV_TXDATA && c->mode == M_HDLC &&
		    ! sppp_isempty (c->ifp)) {
			cx_chan_t *q;

			for (q=c->slaveq; q; q=q->slaveq)
				if ((q->ifp->if_flags & IFF_RUNNING) &&
				    ! (q->ifp->if_flags & IFF_OACTIVE))
					cxsend (q);
		}
	}
}

/*
 * Process the received packet.
 */
static void 
cxinput (cx_chan_t *c, void *buf, unsigned len)
{
	/* Make an mbuf. */
	struct mbuf *m = makembuf (buf, len);
	if (! m) {
		if (c->ifp->if_flags & IFF_DEBUG)
			printf ("cx%d.%d: no memory for packet\n",
				c->board->num, c->num);
		++c->ifp->if_iqdrops;
		return;
	}
	m->m_pkthdr.rcvif = c->master;
#ifdef DEBUG
	if (c->ifp->if_flags & IFF_DEBUG)
	printmbuf (m);
#endif

	BPF_TAP(c->ifp, buf, len);

	/* Count the received bytes to the subchannel, not the master. */
	c->master->if_ibytes -= len + 3;
	c->ifp->if_ibytes += len + 3;

	sppp_input (c->master, m);
}

void cxswitch (cx_chan_t *c, cx_soft_opt_t new)
{
	new.ext = 0;
	if (! new.ext) {
		struct sppp *sp = (struct sppp*) c->ifp;

#if 0 /* Doesn't work this way any more 990402 /phk */
		if (new.cisco)
			sp->pp_flags |= PP_CISCO;
		else
			sp->pp_flags &= ~PP_CISCO;
#endif
		if (new.keepalive)
			sp->pp_flags |= PP_KEEPALIVE;
		else
			sp->pp_flags &= ~PP_KEEPALIVE;
	}
	c->sopt = new;
}
