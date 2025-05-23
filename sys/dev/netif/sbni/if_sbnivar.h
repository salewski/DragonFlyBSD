/*
 * Copyright (c) 1997-2001 Granch, Ltd. All rights reserved.
 * Author: Denis I.Timofeev <timofeev@granch.ru>
 *
 * Redistributon and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * LIABILITY, OR TORT (INCLUDING NEIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/sbni/if_sbnivar.h,v 1.1.2.3 2002/08/11 09:32:00 fjoe Exp $
 * $DragonFly: src/sys/dev/netif/sbni/if_sbnivar.h,v 1.3 2004/09/15 00:39:53 joerg Exp $
 */

/*
 * SBNI12 definitions
 */

/*
 * CONFIGURATION PARAMETER:
 *
 *	Uncomment this if you want to use model SBNI12D-11/ISA with same IRQ
 *	for both first and second channels.
 */
#define SBNI_DUAL_COMPOUND 1

#define SBNI_DEBUG 0

#if SBNI_DEBUG
#define DP(A) A
#else
#define DP(A)
#endif

struct sbni_in_stats {
	u_int32_t	all_rx_number;
	u_int32_t	bad_rx_number;
	u_int32_t	timeout_number;
	u_int32_t	all_tx_number;
	u_int32_t	resend_tx_number;
};

struct sbni_flags {
	u_int	mac_addr	: 24;
	u_int	rxl		: 4;
	u_int	rate		: 2;
	u_int	fixed_rxl	: 1;
	u_int	fixed_rate	: 1;
};


#ifdef _KERNEL	/* to avoid compile this decls with sbniconfig */

struct sbni_softc {
	struct	arpcom arpcom;		/* ethernet common */

	int	io_rid;
	struct	resource *io_res;
	int	io_off;

	int	irq_rid;
	struct	resource *irq_res;
	void	*irq_handle;

	struct	mbuf *rx_buf_p;		/* receive buffer ptr */
	struct	mbuf *tx_buf_p;		/* transmit buffer ptr */
	
	u_int	pktlen;			/* length of transmitting pkt */
	u_int	framelen;		/* current frame length */
	u_int	maxframe;		/* maximum valid frame length */
	u_int	state;
	u_int	inppos;			/* positions in rx/tx buffers */
	u_int	outpos;			/* positions in rx/tx buffers */

	/* transmitting frame number - from frames qty to 1 */
	u_int	tx_frameno;

	/* expected number of next receiving frame */
	u_int	wait_frameno;

	/* count of failed attempts to frame send - 32 attempts do before
	   error - while receiver tunes on opposite side of wire */
	u_int	trans_errors;

	/* idle time; send pong when limit exceeded */
	u_int	timer_ticks;

	/* fields used for receive level autoselection */
	int	delta_rxl;
	u_int	cur_rxl_index;
	u_int	timeout_rxl;
	u_int32_t	cur_rxl_rcvd;
	u_int32_t	prev_rxl_rcvd;

	struct	sbni_csr1 csr1;			/* current value of CSR1 */
	struct	sbni_in_stats in_stats; 	/* internal statistics */ 

	struct	callout	sbni_stat_timer;

	struct	sbni_softc *slave_sc;

#ifdef SBNI_DUAL_COMPOUND
	struct	sbni_softc *link;
#endif
};

void	sbni_intr(void *);
int	sbni_probe(struct sbni_softc *);
void	sbni_attach(struct sbni_softc *, int, struct sbni_flags);

extern u_int32_t next_sbni_unit;

#ifdef SBNI_DUAL_COMPOUND
extern struct sbni_softc *sbni_headlist;

struct sbni_softc	*connect_to_master(struct sbni_softc *);
#endif
#endif	/* _KERNEL */

/*
 * SBNI socket ioctl params
 */
#define	SIOCGHWFLAGS	_IOWR('i', 62, struct ifreq)	/* get flags */
#define	SIOCSHWFLAGS	_IOWR('i', 61, struct ifreq)	/* set flags */
#define SIOCGINSTATS	_IOWR('i', 60, struct ifreq)	/* get internal stats */
#define SIOCRINSTATS	_IOWR('i', 63, struct ifreq)	/* reset internal stats */


/*
 * CRC-32 stuff
 */
#define CRC32(c,crc) (crc32tab[((size_t)(crc) ^ (c)) & 0xff] ^ (((crc) >> 8) & 0x00ffffff))
      /* CRC generator EDB88320 */
      /* CRC remainder 2144DF1C */
      /* CRC initial value 0 */
#define CRC32_REMAINDER 0x2144df1c
#define CRC32_INITIAL 0x00000000
