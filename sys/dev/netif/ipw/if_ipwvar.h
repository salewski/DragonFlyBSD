/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
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
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_ipwvar.h,v 1.2.2.1 2005/01/13 20:01:04 damien Exp $
 * $DragonFly: src/sys/dev/netif/ipw/Attic/if_ipwvar.h,v 1.2 2005/03/08 17:50:32 joerg Exp $
 */

struct ipw_firmware {
	void	*main;
	int	main_size;
	void	*ucode;
	int	ucode_size;
};

struct ipw_soft_bd {
	struct ipw_bd	*bd;
	int		type;
#define IPW_SBD_TYPE_NOASSOC	0
#define IPW_SBD_TYPE_COMMAND	1
#define IPW_SBD_TYPE_HEADER	2
#define IPW_SBD_TYPE_DATA	3
	void		*priv;
};

struct ipw_soft_hdr {
	struct ipw_hdr			hdr;
	bus_dmamap_t			map;
	SLIST_ENTRY(ipw_soft_hdr)	next;
};

struct ipw_soft_buf {
	struct mbuf			*m;
	struct ieee80211_node		*ni;
	bus_dmamap_t			map;
	SLIST_ENTRY(ipw_soft_buf)	next;
};

#define IPW_MAX_NSEG	6
struct ipw_dma_mapping {
	bus_dma_segment_t segs[IPW_MAX_NSEG];
	int nseg;
	bus_size_t mapsize;
};

struct ipw_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	u_int8_t	wr_flags;
	u_int16_t	wr_chan_freq;
	u_int16_t	wr_chan_flags;
	u_int8_t	wr_antsignal;
};

#define IPW_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct ipw_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int8_t	wt_flags;
	u_int16_t	wt_chan_freq;
	u_int16_t	wt_chan_flags;
};

#define IPW_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct ipw_softc {
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	device_t			sc_dev;	
	struct ipw_firmware		fw;
	u_int32_t			flags;
#define IPW_FLAG_FW_CACHED          (1 << 0)
#define IPW_FLAG_FW_INITED          (1 << 1)
#define IPW_FLAG_HAS_RADIO_SWITCH   (1 << 2)

	struct resource			*irq;
	struct resource			*mem;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;
	void 				*sc_ih;

	int				authmode;
	int				sc_tx_timer;

	bus_dma_tag_t			tbd_dmat;
	bus_dma_tag_t			rbd_dmat;
	bus_dma_tag_t			status_dmat;
	bus_dma_tag_t			cmd_dmat;
	bus_dma_tag_t			hdr_dmat;
	bus_dma_tag_t			txbuf_dmat;
	bus_dma_tag_t			rxbuf_dmat;

	bus_dmamap_t			tbd_map;
	bus_dmamap_t			rbd_map;
	bus_dmamap_t			status_map;
	bus_dmamap_t			cmd_map;

	bus_addr_t			tbd_phys;
	bus_addr_t			rbd_phys;
	bus_addr_t			status_phys;

	struct ipw_bd			*tbd_list;
	struct ipw_bd			*rbd_list;
	struct ipw_status		*status_list;

	struct ipw_cmd			cmd;
	struct ipw_soft_bd		stbd_list[IPW_NTBD];
	struct ipw_soft_buf		tx_sbuf_list[IPW_NDATA];
	struct ipw_soft_hdr		shdr_list[IPW_NDATA];
	struct ipw_soft_bd		srbd_list[IPW_NRBD];
	struct ipw_soft_buf		rx_sbuf_list[IPW_NRBD];

	SLIST_HEAD(, ipw_soft_hdr)	free_shdr;
	SLIST_HEAD(, ipw_soft_buf)	free_sbuf;

	u_int32_t			table1_base;
	u_int32_t			table2_base;

	u_int32_t			txcur;
	u_int32_t			txold;
	int				txfree;
	u_int32_t			rxcur;

	struct bpf_if			*sc_drvbpf;

	union {
		struct ipw_rx_radiotap_header th;
		u_int8_t	pad[64];
	} sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct ipw_tx_radiotap_header th;
		u_int8_t	pad[64];
	} sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
	struct sysctl_ctx_list		sysctl_ctx;
};

#define SIOCSLOADFW	 _IOW('i', 137, struct ifreq)
#define SIOCSKILLFW	 _IOW('i', 138, struct ifreq)

#define IPW_LOCK_DECL()	intrmask_t s
#define IPW_LOCK(_sc)	s = splimp()
#define IPW_UNLOCK(_sc)	splx(s)
