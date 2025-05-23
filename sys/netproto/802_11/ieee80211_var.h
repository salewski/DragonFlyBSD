/*
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net80211/ieee80211_var.h,v 1.15 2004/04/05 22:10:26 sam Exp $
 * $DragonFly: src/sys/netproto/802_11/ieee80211_var.h,v 1.2 2005/01/26 00:37:40 joerg Exp $
 */

#ifndef _NETPROTO_802_11_IEEE80211_VAR_H_
#define	_NETPROTO_802_11_IEEE80211_VAR_H_

/*
 * Definitions for IEEE 802.11 drivers.
 */

#include <netproto/802_11/ieee80211.h>
#include <netproto/802_11/ieee80211_crypto.h>
#include <netproto/802_11/ieee80211_ioctl.h>		/* for ieee80211_stats */
#include <netproto/802_11/ieee80211_node.h>
#include <netproto/802_11/ieee80211_proto.h>

#define	IEEE80211_CHAN_MAX	255
#define	IEEE80211_CHAN_ANY	0xffff		/* token for ``any channel'' */
#define	IEEE80211_CHAN_ANYC \
	((struct ieee80211_channel *) IEEE80211_CHAN_ANY)

#define	IEEE80211_TXPOWER_MAX	100	/* max power */
#define	IEEE80211_TXPOWER_MIN	0	/* kill radio (if possible) */

enum ieee80211_phytype {
	IEEE80211_T_DS,			/* direct sequence spread spectrum */
	IEEE80211_T_FH,			/* frequency hopping */
	IEEE80211_T_OFDM,		/* frequency division multiplexing */
	IEEE80211_T_TURBO,		/* high rate OFDM, aka turbo mode */
};
#define	IEEE80211_T_CCK	IEEE80211_T_DS	/* more common nomenclature */

/* XXX not really a mode; there are really multiple PHY's */
enum ieee80211_phymode {
	IEEE80211_MODE_AUTO	= 0,	/* autoselect */
	IEEE80211_MODE_11A	= 1,	/* 5GHz, OFDM */
	IEEE80211_MODE_11B	= 2,	/* 2GHz, CCK */
	IEEE80211_MODE_11G	= 3,	/* 2GHz, OFDM */
	IEEE80211_MODE_FH	= 4,	/* 2GHz, GFSK */
	IEEE80211_MODE_TURBO	= 5,	/* 5GHz, OFDM, 2x clock */
};
#define	IEEE80211_MODE_MAX	(IEEE80211_MODE_TURBO+1)

enum ieee80211_opmode {
	IEEE80211_M_STA		= 1,	/* infrastructure station */
	IEEE80211_M_IBSS 	= 0,	/* IBSS (adhoc) station */
	IEEE80211_M_AHDEMO	= 3,	/* Old lucent compatible adhoc demo */
	IEEE80211_M_HOSTAP	= 6,	/* Software Access Point */
	IEEE80211_M_MONITOR	= 8	/* Monitor mode */
};

/*
 * 802.11g protection mode.
 */
enum ieee80211_protmode {
	IEEE80211_PROT_NONE	= 0,	/* no protection */
	IEEE80211_PROT_CTSONLY	= 1,	/* CTS to self */
	IEEE80211_PROT_RTSCTS	= 2,	/* RTS-CTS */
};

/*
 * Channels are specified by frequency and attributes.
 */
struct ieee80211_channel {
	uint16_t	ic_freq;	/* setting in Mhz */
	uint16_t	ic_flags;	/* see below */
};

/* bits 0-3 are for private use by drivers */
/* channel attributes */
#define	IEEE80211_CHAN_TURBO	0x0010	/* Turbo channel */
#define	IEEE80211_CHAN_CCK	0x0020	/* CCK channel */
#define	IEEE80211_CHAN_OFDM	0x0040	/* OFDM channel */
#define	IEEE80211_CHAN_2GHZ	0x0080	/* 2 GHz spectrum channel. */
#define	IEEE80211_CHAN_5GHZ	0x0100	/* 5 GHz spectrum channel */
#define	IEEE80211_CHAN_PASSIVE	0x0200	/* Only passive scan allowed */
#define	IEEE80211_CHAN_DYN	0x0400	/* Dynamic CCK-OFDM channel */
#define	IEEE80211_CHAN_GFSK	0x0800	/* GFSK channel (FHSS PHY) */

/*
 * Useful combinations of channel characteristics.
 */
#define	IEEE80211_CHAN_FHSS \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_GFSK)
#define	IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define	IEEE80211_CHAN_PUREG \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)
#define	IEEE80211_CHAN_T \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)

#define	IEEE80211_IS_CHAN_FHSS(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define	IEEE80211_IS_CHAN_A(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define	IEEE80211_IS_CHAN_B(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define	IEEE80211_IS_CHAN_PUREG(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define	IEEE80211_IS_CHAN_G(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define	IEEE80211_IS_CHAN_T(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_T) == IEEE80211_CHAN_T)

#define	IEEE80211_IS_CHAN_2GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_2GHZ) != 0)
#define	IEEE80211_IS_CHAN_5GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_5GHZ) != 0)
#define	IEEE80211_IS_CHAN_OFDM(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_OFDM) != 0)
#define	IEEE80211_IS_CHAN_CCK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_CCK) != 0)
#define	IEEE80211_IS_CHAN_GFSK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_GFSK) != 0)

/* ni_chan encoding for FH phy */
#define	IEEE80211_FH_CHANMOD	80
#define	IEEE80211_FH_CHAN(set,pat)	(((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define	IEEE80211_FH_CHANSET(chan)	((chan)/IEEE80211_FH_CHANMOD+1)
#define	IEEE80211_FH_CHANPAT(chan)	((chan)%IEEE80211_FH_CHANMOD)

struct ieee80211com {
	struct arpcom		ic_ac;
	void			(*ic_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, uint32_t);
	int			(*ic_send_mgmt)(struct ieee80211com *,
				    struct ieee80211_node *, int, int);
	int			(*ic_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*ic_newassoc)(struct ieee80211com *,
				    struct ieee80211_node *, int);
	uint8_t			ic_myaddr[IEEE80211_ADDR_LEN];
	struct ieee80211_rateset ic_sup_rates[IEEE80211_MODE_MAX];
	struct ieee80211_channel ic_channels[IEEE80211_CHAN_MAX+1];
	u_char			ic_chan_avail[roundup(IEEE80211_CHAN_MAX,NBBY)];
	u_char			ic_chan_active[roundup(IEEE80211_CHAN_MAX, NBBY)];
	u_char			ic_chan_scan[roundup(IEEE80211_CHAN_MAX,NBBY)];
	struct ifqueue		ic_mgtq;
	uint32_t		ic_flags;	/* state flags */
	uint32_t		ic_caps;	/* capabilities */
	uint16_t		ic_modecaps;	/* set of mode capabilities */
	uint16_t		ic_curmode;	/* current mode */
	enum ieee80211_phytype	ic_phytype;	/* XXX wrong for multi-mode */
	enum ieee80211_opmode	ic_opmode;	/* operation mode */
	enum ieee80211_state	ic_state;	/* 802.11 state */
	enum ieee80211_protmode	ic_protmode;	/* 802.11g protection mode */
	struct ifmedia		ic_media;	/* interface media config */
	struct bpf_if		*ic_rawbpf;	/* packet filter structure */
	struct ieee80211_node	*ic_bss;	/* information for this node */
	struct ieee80211_channel *ic_ibss_chan;
	int			ic_fixed_rate;	/* index to ic_sup_rates[] */
	uint16_t		ic_rtsthreshold;
	uint16_t		ic_fragthreshold;
	struct lwkt_token	ic_nodetoken;	/* on node table */
	u_int			ic_scangen;	/* gen# for timeout scan */
	struct ieee80211_node	*(*ic_node_alloc)(struct ieee80211com *);
	void			(*ic_node_free)(struct ieee80211com *,
					struct ieee80211_node *);
	void			(*ic_node_copy)(struct ieee80211com *,
					struct ieee80211_node *,
					const struct ieee80211_node *);
	uint8_t			(*ic_node_getrssi)(struct ieee80211com *,
					struct ieee80211_node *);
	TAILQ_HEAD(, ieee80211_node) ic_node;	/* information of all nodes */
	LIST_HEAD(, ieee80211_node) ic_hash[IEEE80211_NODE_HASHSIZE];
	uint16_t		ic_lintval;	/* listen interval */
	uint16_t		ic_holdover;	/* PM hold over duration */
	uint16_t		ic_txmin;	/* min tx retry count */
	uint16_t		ic_txmax;	/* max tx retry count */
	uint16_t		ic_txlifetime;	/* tx lifetime */
	uint16_t		ic_txpower;	/* tx power setting (dbM) */
	uint16_t		ic_bmisstimeout;/* beacon miss threshold (ms) */
	int			ic_mgt_timer;	/* mgmt timeout */
	int			ic_inact_timer;	/* inactivity timer wait */
	int			ic_des_esslen;
	uint8_t			ic_des_essid[IEEE80211_NWID_LEN];
	struct ieee80211_channel *ic_des_chan;	/* desired channel */
	uint8_t			ic_des_bssid[IEEE80211_ADDR_LEN];
	struct ieee80211_wepkey	ic_nw_keys[IEEE80211_WEP_NKID];
	int			ic_wep_txkey;	/* default tx key index */
	void			*ic_wep_ctx;	/* wep crypt context */
	uint32_t		ic_iv;		/* initial vector for wep */
	struct ieee80211_stats	ic_stats;	/* statistics */
};
#define	ic_if		ic_ac.ac_if
#define	ic_softc	ic_if.if_softc

#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#define	IEEE80211_ADDR_COPY(dst,src)	memcpy(dst,src,IEEE80211_ADDR_LEN)

/* ic_flags */
#define	IEEE80211_F_ASCAN	0x00000001	/* STATUS: active scan */
#define	IEEE80211_F_SIBSS	0x00000002	/* STATUS: start IBSS */
#define	IEEE80211_F_WEPON	0x00000100	/* CONF: WEP enabled */
#define	IEEE80211_F_IBSSON	0x00000200	/* CONF: IBSS creation enable */
#define	IEEE80211_F_PMGTON	0x00000400	/* CONF: Power mgmt enable */
#define	IEEE80211_F_DESBSSID	0x00000800	/* CONF: des_bssid is set */
#define	IEEE80211_F_SCANAP	0x00001000	/* CONF: Scanning AP */
#define	IEEE80211_F_ROAMING	0x00002000	/* CONF: roaming enabled */
#define	IEEE80211_F_SWRETRY	0x00004000	/* CONF: sw tx retry enabled */
#define	IEEE80211_F_TXPMGT	0x00018000	/* STATUS: tx power */
#define	IEEE80211_F_TXPOW_OFF	0x00000000	/* TX Power: radio disabled */
#define	IEEE80211_F_TXPOW_FIXED	0x00008000	/* TX Power: fixed rate */
#define	IEEE80211_F_TXPOW_AUTO	0x00010000	/* TX Power: undefined */
#define	IEEE80211_F_SHSLOT	0x00020000	/* CONF: short slot time */
#define	IEEE80211_F_SHPREAMBLE	0x00040000	/* CONF: short preamble */
#define	IEEE80211_F_USEPROT	0x00100000	/* STATUS: protection enabled */
#define	IEEE80211_F_USEBARKER	0x00200000	/* STATUS: use barker preamble*/

/* ic_caps */
#define	IEEE80211_C_WEP		0x00000001	/* CAPABILITY: WEP available */
#define	IEEE80211_C_IBSS	0x00000002	/* CAPABILITY: IBSS available */
#define	IEEE80211_C_PMGT	0x00000004	/* CAPABILITY: Power mgmt */
#define	IEEE80211_C_HOSTAP	0x00000008	/* CAPABILITY: HOSTAP avail */
#define	IEEE80211_C_AHDEMO	0x00000010	/* CAPABILITY: Old Adhoc Demo */
#define	IEEE80211_C_SWRETRY	0x00000020	/* CAPABILITY: sw tx retry */
#define	IEEE80211_C_TXPMGT	0x00000040	/* CAPABILITY: tx power mgmt */
#define	IEEE80211_C_SHSLOT	0x00000080	/* CAPABILITY: short slottime */
#define	IEEE80211_C_SHPREAMBLE	0x00000100	/* CAPABILITY: short preamble */
#define	IEEE80211_C_MONITOR	0x00000200	/* CAPABILITY: monitor mode */

/* flags for ieee80211_fix_rate() */
#define	IEEE80211_F_DOSORT	0x00000001	/* sort rate list */
#define	IEEE80211_F_DOFRATE	0x00000002	/* use fixed rate */
#define	IEEE80211_F_DONEGO	0x00000004	/* calc negotiated rate */
#define	IEEE80211_F_DODEL	0x00000008	/* delete ignore rate */

void	ieee80211_ifattach(struct ifnet *);
void	ieee80211_ifdetach(struct ifnet *);
void	ieee80211_media_init(struct ifnet *, ifm_change_cb_t, ifm_stat_cb_t);
int	ieee80211_media_change(struct ifnet *);
void	ieee80211_media_status(struct ifnet *, struct ifmediareq *);
int	ieee80211_ioctl(struct ifnet *, u_long, caddr_t, struct ucred *);
int	ieee80211_cfgget(struct ifnet *, u_long, caddr_t, struct ucred *);
int	ieee80211_cfgset(struct ifnet *, u_long, caddr_t);
void	ieee80211_watchdog(struct ifnet *);
int	ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_node *, int);
int	ieee80211_rate2media(struct ieee80211com *, int,
		enum ieee80211_phymode);
int	ieee80211_media2rate(int);
u_int	ieee80211_mhz2ieee(u_int, u_int);
u_int	ieee80211_chan2ieee(struct ieee80211com *, struct ieee80211_channel *);
u_int	ieee80211_ieee2mhz(u_int, u_int);
int	ieee80211_setmode(struct ieee80211com *, enum ieee80211_phymode);
enum ieee80211_phymode ieee80211_chan2mode(struct ieee80211com *,
		struct ieee80211_channel *);

#define	IEEE80211_DEBUG
#ifdef IEEE80211_DEBUG
extern	int ieee80211_debug;
#define	IEEE80211_DPRINTF(X)	if (ieee80211_debug) printf X
#define	IEEE80211_DPRINTF2(X)	if (ieee80211_debug>1) printf X
#else
#define	IEEE80211_DPRINTF(X)
#define	IEEE80211_DPRINTF2(X)
#endif

#endif /* _NETPROTO_802_11_IEEE80211_VAR_H_ */
