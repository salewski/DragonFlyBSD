/* $FreeBSD: src/sys/net80211/ieee80211_radiotap.h,v 1.3 2004/04/05 22:13:21 sam Exp $ */
/* $NetBSD: ieee80211_radiotap.h,v 1.3 2003/11/16 09:02:42 dyoung Exp $ */
/* $DragonFly: src/sys/netproto/802_11/ieee80211_radiotap.h,v 1.1 2004/07/26 16:30:17 joerg Exp $ */

/*
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef _NETPROTO_802_11_IEEE80211RADIOTAP_H_
#define	_NETPROTO_802_11_IEEE80211RADIOTAP_H_

/* A generic radio capture format is desirable. There is one for
 * Linux, but it is neither rigidly defined (there were not even
 * units given for some fields) nor easily extensible.
 *
 * I suggest the following extensible radio capture format. It is
 * based on a bitmap indicating which fields are present.
 *
 * I am trying to describe precisely what the application programmer
 * should expect in the following, and for that reason I tell the
 * units and origin of each measurement (where it applies), or else I
 * use sufficiently weaselly language ("is a monotonically nondecreasing
 * function of...") that I cannot set false expectations for lawyerly
 * readers.
 */
#ifdef _KERNEL
#ifndef DLT_IEEE802_11_RADIO
#define	DLT_IEEE802_11_RADIO	127	/* 802.11 plus WLAN header */
#endif
#endif /* _KERNEL */

/* The radio capture header precedes the 802.11 header. */
struct ieee80211_radiotap_header {
	uint8_t		it_version;	/* Version 0. Only increases
					 * for drastic changes,
					 * introduction of compatible
					 * new fields does not count.
					 */
	uint8_t		it_pad;
	uint16_t	it_len;         /* length of the whole
					 * header in bytes, including
					 * it_version, it_pad,
					 * it_len, and data fields.
					 */
	uint32_t	it_present;     /* A bitmap telling which
					 * fields are present. Set bit 31
					 * (0x80000000) to extend the
					 * bitmap by another 32 bits.
					 * Additional extensions are made
					 * by setting bit 31.
					 */
} __attribute__((__packed__));

/* Name                                 Data type	Units
 * ----                                 ---------	-----
 *
 * IEEE80211_RADIOTAP_TSFT              uint64_t	microseconds
 *
 *      Value in microseconds of the MAC's 64-bit 802.11 Time
 *      Synchronization Function timer when the first bit of the
 *      MPDU arrived at the MAC. For received frames, only.
 *
 * IEEE80211_RADIOTAP_CHANNEL           2 x uint16_t	MHz, bitmap
 *
 *      Tx/Rx frequency in MHz, followed by flags (see below).
 *
 * IEEE80211_RADIOTAP_FHSS              uint16_t	see below
 *
 *      For frequency-hopping radios, the hop set (first byte)
 *      and pattern (second byte).
 *
 * IEEE80211_RADIOTAP_RATE              uint8_t		500kb/s
 *
 *      Tx/Rx data rate
 *
 * IEEE80211_RADIOTAP_DBM_ANTSIGNAL     int8_t		decibels from
 *							one milliwatt (dBm)
 *
 *      RF signal power at the antenna, decibel difference from
 *      one milliwatt.
 *
 * IEEE80211_RADIOTAP_DBM_ANTNOISE      int8_t		decibels from
 *							one milliwatt (dBm)
 *
 *      RF noise power at the antenna, decibel difference from one
 *      milliwatt.
 *
 * IEEE80211_RADIOTAP_DB_ANTSIGNAL      uint8_t		decibel (dB)
 *
 *      RF signal power at the antenna, decibel difference from an
 *      arbitrary, fixed reference.
 *
 * IEEE80211_RADIOTAP_DB_ANTNOISE       uint8_t		decibel (dB)
 *
 *      RF noise power at the antenna, decibel difference from an
 *      arbitrary, fixed reference point.
 *
 * IEEE80211_RADIOTAP_BARKER_CODE_LOCK  uint16_t	unitless
 *
 *      Quality of Barker code lock. Unitless. Monotonically
 *      nondecreasing with "better" lock strength. Called "Signal
 *      Quality" in datasheets.  (Is there a standard way to measure
 *      this?)
 *
 * IEEE80211_RADIOTAP_TX_ATTENUATION    uint16_t	unitless
 *
 *      Transmit power expressed as unitless distance from max
 *      power set at factory calibration.  0 is max power.
 *      Monotonically nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DB_TX_ATTENUATION uint16_t	decibels (dB)
 *
 *      Transmit power expressed as decibel distance from max power
 *      set at factory calibration.  0 is max power.  Monotonically
 *      nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DBM_TX_POWER      int8_t		decibels from
 *							one milliwatt (dBm)
 *
 *      Transmit power expressed as dBm (decibels from a 1 milliwatt
 *      reference). This is the absolute power level measured at
 *      the antenna port.
 *
 * IEEE80211_RADIOTAP_FLAGS             uint8_t		bitmap
 *
 *      Properties of transmitted and received frames. See flags
 *      defined below.
 *
 * IEEE80211_RADIOTAP_ANTENNA           uint8_t		antenna index
 *
 *      Unitless indication of the Rx/Tx antenna for this packet.
 *      The first antenna is antenna 0.
 *
 * IEEE80211_RADIOTAP_FCS           	uint32_t	data
 *
 *	FCS from frame in network byte order.
 */
enum ieee80211_radiotap_type {
	IEEE80211_RADIOTAP_TSFT = 0,
	IEEE80211_RADIOTAP_FLAGS = 1,
	IEEE80211_RADIOTAP_RATE = 2,
	IEEE80211_RADIOTAP_CHANNEL = 3,
	IEEE80211_RADIOTAP_FHSS = 4,
	IEEE80211_RADIOTAP_DBM_ANTSIGNAL = 5,
	IEEE80211_RADIOTAP_DBM_ANTNOISE = 6,
	IEEE80211_RADIOTAP_LOCK_QUALITY = 7,
	IEEE80211_RADIOTAP_TX_ATTENUATION = 8,
	IEEE80211_RADIOTAP_DB_TX_ATTENUATION = 9,
	IEEE80211_RADIOTAP_DBM_TX_POWER = 10,
	IEEE80211_RADIOTAP_ANTENNA = 11,
	IEEE80211_RADIOTAP_DB_ANTSIGNAL = 12,
	IEEE80211_RADIOTAP_DB_ANTNOISE = 13,
	IEEE80211_RADIOTAP_FCS = 14,
	IEEE80211_RADIOTAP_EXT = 31,
};

#ifndef _KERNEL
/* Channel flags. */
#define	IEEE80211_CHAN_TURBO    0x0010  /* Turbo channel */
#define	IEEE80211_CHAN_CCK      0x0020  /* CCK channel */
#define	IEEE80211_CHAN_OFDM     0x0040  /* OFDM channel */
#define	IEEE80211_CHAN_2GHZ     0x0080  /* 2 GHz spectrum channel. */
#define	IEEE80211_CHAN_5GHZ     0x0100  /* 5 GHz spectrum channel */
#define	IEEE80211_CHAN_PASSIVE  0x0200  /* Only passive scan allowed */
#define	IEEE80211_CHAN_DYN	0x0400	/* Dynamic CCK-OFDM channel */
#define	IEEE80211_CHAN_GFSK	0x0800	/* GFSK channel (FHSS PHY) */
#endif /* !_KERNEL */

/* For IEEE80211_RADIOTAP_FLAGS */
#define	IEEE80211_RADIOTAP_F_CFP	0x01	/* sent/received
						 * during CFP
						 */
#define	IEEE80211_RADIOTAP_F_SHORTPRE	0x02	/* sent/received
						 * with short
						 * preamble
						 */
#define	IEEE80211_RADIOTAP_F_WEP	0x04	/* sent/received
						 * with WEP encryption
						 */
#define	IEEE80211_RADIOTAP_F_FRAG	0x08	/* sent/received
						 * with fragmentation
						 */

#endif /* _NETPROTO_802_11_IEEE80211RADIOTAP_H_ */
