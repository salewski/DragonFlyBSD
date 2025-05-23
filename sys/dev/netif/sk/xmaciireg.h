/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pci/xmaciireg.h,v 1.2.4.1 2000/04/27 14:48:07 wpaul Exp $
 * $DragonFly: src/sys/dev/netif/sk/xmaciireg.h,v 1.2 2003/06/17 04:28:57 dillon Exp $
 */

/*
 * Registers and data structures for the XaQti Corporation XMAC II
 * Gigabit Ethernet MAC. Datasheet is available from http://www.xaqti.com.
 * The XMAC can be programmed for 16-bit or 32-bit register access modes.
 * The SysKonnect gigabit ethernet adapters use 16-bit mode, so that's
 * how the registers are laid out here.
 */

#define XM_DEVICEID		0x00E0AE20
#define XM_XAQTI_OUI		0x00E0AE

#define XM_XMAC_REV(x)		(((x) & 0x000000E0) >> 5)

#define XM_XMAC_REV_B2		0x0
#define XM_XMAC_REV_C1		0x1

#define XM_MMUCMD		0x0000
#define XM_POFF			0x0008
#define XM_BURST		0x000C
#define XM_VLAN_TAGLEV1		0x0010
#define XM_VLAN_TAGLEV2		0x0014
#define XM_TXCMD		0x0020
#define XM_TX_RETRYLIMIT	0x0024
#define XM_TX_SLOTTIME		0x0028
#define XM_TX_IPG		0x003C
#define XM_RXCMD		0x0030
#define XM_PHY_ADDR		0x0034
#define XM_PHY_DATA		0x0038
#define XM_GPIO			0x0040
#define XM_IMR			0x0044
#define XM_ISR			0x0048
#define XM_HWCFG		0x004C
#define XM_TX_LOWAT		0x0060
#define XM_TX_HIWAT		0x0062
#define XM_TX_REQTHRESH_LO	0x0064
#define XM_TX_REQTHRESH_HI	0x0066
#define XM_TX_REQTHRESH		XM_TX_REQTHRESH_LO
#define XM_PAUSEDST0		0x0068
#define XM_PAUSEDST1		0x006A
#define XM_PAUSEDST2		0x006C
#define XM_CTLPARM_LO		0x0070
#define XM_CTLPARM_HI		0x0072
#define XM_CTLPARM		XM_CTLPARM_LO
#define XM_OPCODE_PAUSE_TIMER	0x0074
#define XM_TXSTAT_LIFO		0x0078

/*
 * Perfect filter registers. The XMAC has a table of 16 perfect
 * filter entries, spaced 8 bytes apart. This is in addition to
 * the station address registers, which appear below.
 */
#define XM_RXFILT_BASE		0x0080
#define XM_RXFILT_END		0x0107
#define XM_RXFILT_MAX		16
#define XM_RXFILT_ENTRY(ent)		(XM_RXFILT_BASE + ((ent * 8)))

/* Primary station address. */
#define XM_PAR0			0x0108
#define XM_PAR1			0x010A
#define XM_PAR2			0x010C

/* 64-bit multicast hash table registers */
#define XM_MAR0			0x0110
#define XM_MAR1			0x0112
#define XM_MAR2			0x0114
#define XM_MAR3			0x0116
#define XM_RX_LOWAT		0x0118
#define XM_RX_HIWAT		0x011A
#define XM_RX_REQTHRESH_LO	0x011C
#define XM_RX_REQTHRESH_HI	0x011E
#define XM_RX_REQTHRESH		XM_RX_REQTHRESH_LO
#define XM_DEVID_LO		0x0120
#define XM_DEVID_HI		0x0122
#define XM_DEVID		XM_DEVID_LO
#define XM_MODE_LO		0x0124
#define XM_MODE_HI		0x0126
#define XM_MODE			XM_MODE_LO
#define XM_LASTSRC0		0x0128
#define XM_LASTSRC1		0x012A
#define XM_LASTSRC2		0x012C
#define XM_TSTAMP_READ		0x0130
#define XM_TSTAMP_LOAD		0x0134
#define XM_STATS_CMD		0x0200
#define XM_RXCNT_EVENT_LO	0x0204
#define XM_RXCNT_EVENT_HI	0x0206
#define XM_RXCNT_EVENT		XM_RXCNT_EVENT_LO
#define XM_TXCNT_EVENT_LO	0x0208
#define XM_TXCNT_EVENT_HI	0x020A
#define XM_TXCNT_EVENT		XM_TXCNT_EVENT_LO
#define XM_RXCNT_EVMASK_LO	0x020C
#define XM_RXCNT_EVMASK_HI	0x020E
#define XM_RXCNT_EVMASK		XM_RXCNT_EVMASK_LO
#define XM_TXCNT_EVMASK_LO	0x0210
#define XM_TXCNT_EVMASK_HI	0x0212
#define XM_TXCNT_EVMASK		XM_TXCNT_EVMASK_LO

/* Statistics command register */
#define XM_STATCMD_CLR_TX	0x0001
#define XM_STATCMD_CLR_RX	0x0002
#define XM_STATCMD_COPY_TX	0x0004
#define XM_STATCMD_COPY_RX	0x0008
#define XM_STATCMD_SNAP_TX	0x0010
#define XM_STATCMD_SNAP_RX	0x0020

/* TX statistics registers */
#define XM_TXSTATS_PKTSOK	0x280
#define XM_TXSTATS_BYTESOK_HI	0x284
#define XM_TXSTATS_BYTESOK_LO	0x288
#define XM_TXSTATS_BCASTSOK	0x28C
#define XM_TXSTATS_MCASTSOK	0x290
#define XM_TXSTATS_UCASTSOK	0x294
#define XM_TXSTATS_GIANTS	0x298
#define XM_TXSTATS_BURSTCNT	0x29C
#define XM_TXSTATS_PAUSEPKTS	0x2A0
#define XM_TXSTATS_MACCTLPKTS	0x2A4
#define XM_TXSTATS_SINGLECOLS	0x2A8
#define XM_TXSTATS_MULTICOLS	0x2AC
#define XM_TXSTATS_EXCESSCOLS	0x2B0
#define XM_TXSTATS_LATECOLS	0x2B4
#define XM_TXSTATS_DEFER	0x2B8
#define XM_TXSTATS_EXCESSDEFER	0x2BC
#define XM_TXSTATS_UNDERRUN	0x2C0
#define XM_TXSTATS_CARRIERSENSE	0x2C4
#define XM_TXSTATS_UTILIZATION	0x2C8
#define XM_TXSTATS_64		0x2D0
#define XM_TXSTATS_65_127	0x2D4
#define XM_TXSTATS_128_255	0x2D8
#define XM_TXSTATS_256_511	0x2DC
#define XM_TXSTATS_512_1023	0x2E0
#define XM_TXSTATS_1024_MAX	0x2E4

/* RX statistics registers */
#define XM_RXSTATS_PKTSOK	0x300
#define XM_RXSTATS_BYTESOK_HI	0x304
#define XM_RXSTATS_BYTESOK_LO	0x308
#define XM_RXSTATS_BCASTSOK	0x30C
#define XM_RXSTATS_MCASTSOK	0x310
#define XM_RXSTATS_UCASTSOK	0x314
#define XM_RXSTATS_PAUSEPKTS	0x318
#define XM_RXSTATS_MACCTLPKTS	0x31C
#define XM_RXSTATS_BADPAUSEPKTS	0x320
#define XM_RXSTATS_BADMACCTLPKTS	0x324
#define XM_RXSTATS_BURSTCNT	0x328
#define XM_RXSTATS_MISSEDPKTS	0x32C
#define XM_RXSTATS_FRAMEERRS	0x330
#define XM_RXSTATS_OVERRUN	0x334
#define XM_RXSTATS_JABBER	0x338
#define XM_RXSTATS_CARRLOSS	0x33C
#define XM_RXSTATS_INRNGLENERR	0x340
#define XM_RXSTATS_SYMERR	0x344
#define XM_RXSTATS_SHORTEVENT	0x348
#define XM_RXSTATS_RUNTS	0x34C
#define XM_RXSTATS_GIANTS	0x350
#define XM_RXSTATS_CRCERRS	0x354
#define XM_RXSTATS_CEXTERRS	0x35C
#define XM_RXSTATS_UTILIZATION	0x360
#define XM_RXSTATS_64		0x368
#define XM_RXSTATS_65_127	0x36C
#define XM_RXSTATS_128_255	0x370
#define XM_RXSTATS_256_511	0x374
#define XM_RXSTATS_512_1023	0x378
#define XM_RXSTATS_1024_MAX	0x37C

#define XM_MMUCMD_TX_ENB	0x0001
#define XM_MMUCMD_RX_ENB	0x0002
#define XM_MMUCMD_GMIILOOP	0x0004
#define XM_MMUCMD_RATECTL	0x0008
#define XM_MMUCMD_GMIIFDX	0x0010
#define XM_MMUCMD_NO_MGMT_PRMB	0x0020
#define XM_MMUCMD_SIMCOL	0x0040
#define XM_MMUCMD_FORCETX	0x0080
#define XM_MMUCMD_LOOPENB	0x0200
#define XM_MMUCMD_IGNPAUSE	0x0400
#define XM_MMUCMD_PHYBUSY	0x0800
#define XM_MMUCMD_PHYDATARDY	0x1000

#define XM_TXCMD_AUTOPAD	0x0001
#define XM_TXCMD_NOCRC		0x0002
#define XM_TXCMD_NOPREAMBLE	0x0004
#define XM_TXCMD_NOGIGAMODE	0x0008
#define XM_TXCMD_SAMPLELINE	0x0010
#define XM_TXCMD_ENCBYPASS	0x0020
#define XM_TXCMD_XMITBK2BK	0x0040
#define XM_TXCMD_FAIRSHARE	0x0080

#define XM_RXCMD_DISABLE_CEXT	0x0001
#define XM_RXCMD_STRIPPAD	0x0002
#define XM_RXCMD_SAMPLELINE	0x0004
#define XM_RXCMD_SELFRX		0x0008
#define XM_RXCMD_STRIPFCS	0x0010
#define XM_RXCMD_TRANSPARENT	0x0020
#define XM_RXCMD_IPGCAPTURE	0x0040
#define XM_RXCMD_BIGPKTOK	0x0080
#define XM_RXCMD_LENERROK	0x0100

#define XM_GPIO_GP0_SET		0x0001
#define XM_GPIO_RESETSTATS	0x0004
#define XM_GPIO_RESETMAC	0x0008
#define XM_GPIO_FORCEINT	0x0020
#define XM_GPIO_ANEGINPROG	0x0040

#define XM_IMR_RX_EOF		0x0001
#define XM_IMR_TX_EOF		0x0002
#define XM_IMR_TX_UNDERRUN	0x0004
#define XM_IMR_RX_OVERRUN	0x0008
#define XM_IMR_TX_STATS_OFLOW	0x0010
#define XM_IMR_RX_STATS_OFLOW	0x0020
#define XM_IMR_TSTAMP_OFLOW	0x0040
#define XM_IMR_AUTONEG_DONE	0x0080
#define XM_IMR_NEXTPAGE_RDY	0x0100
#define XM_IMR_PAGE_RECEIVED	0x0200
#define XM_IMR_LP_REQCFG	0x0400
#define XM_IMR_GP0_SET		0x0800
#define XM_IMR_FORCEINTR	0x1000
#define XM_IMR_TX_ABORT		0x2000
#define XM_IMR_LINKEVENT	0x4000

#define XM_INTRS	\
	(~(XM_IMR_GP0_SET|XM_IMR_AUTONEG_DONE|XM_IMR_TX_UNDERRUN))

#define XM_ISR_RX_EOF		0x0001
#define XM_ISR_TX_EOF		0x0002
#define XM_ISR_TX_UNDERRUN	0x0004
#define XM_ISR_RX_OVERRUN	0x0008
#define XM_ISR_TX_STATS_OFLOW	0x0010
#define XM_ISR_RX_STATS_OFLOW	0x0020
#define XM_ISR_TSTAMP_OFLOW	0x0040
#define XM_ISR_AUTONEG_DONE	0x0080
#define XM_ISR_NEXTPAGE_RDY	0x0100
#define XM_ISR_PAGE_RECEIVED	0x0200
#define XM_ISR_LP_REQCFG	0x0400
#define XM_ISR_GP0_SET		0x0800
#define XM_ISR_FORCEINTR	0x1000
#define XM_ISR_TX_ABORT		0x2000
#define XM_ISR_LINKEVENT	0x4000

#define XM_HWCFG_GENEOP		0x0008
#define XM_HWCFG_SIGSTATCKH	0x0004
#define XM_HWCFG_GMIIMODE	0x0001

#define XM_MODE_FLUSH_RXFIFO	0x00000001
#define XM_MODE_FLUSH_TXFIFO	0x00000002
#define XM_MODE_BIGENDIAN	0x00000004
#define XM_MODE_RX_PROMISC	0x00000008
#define XM_MODE_RX_NOBROAD	0x00000010
#define XM_MODE_RX_NOMULTI	0x00000020
#define XM_MODE_RX_NOUNI	0x00000040
#define XM_MODE_RX_BADFRAMES	0x00000080
#define XM_MODE_RX_CRCERRS	0x00000100
#define XM_MODE_RX_GIANTS	0x00000200
#define XM_MODE_RX_INRANGELEN	0x00000400
#define XM_MODE_RX_RUNTS	0x00000800
#define XM_MODE_RX_MACCTL	0x00001000
#define XM_MODE_RX_USE_PERFECT	0x00002000
#define XM_MODE_RX_USE_STATION	0x00004000
#define XM_MODE_RX_USE_HASH	0x00008000
#define XM_MODE_RX_ADDRPAIR	0x00010000
#define XM_MODE_PAUSEONHI	0x00020000
#define XM_MODE_PAUSEONLO	0x00040000
#define XM_MODE_TIMESTAMP	0x00080000
#define XM_MODE_SENDPAUSE	0x00100000
#define XM_MODE_SENDCONTINUOUS	0x00200000
#define XM_MODE_LE_STATUSWORD	0x00400000
#define XM_MODE_AUTOFIFOPAUSE	0x00800000
#define XM_MODE_EXPAUSEGEN	0x02000000
#define XM_MODE_RX_INVERSE	0x04000000

#define XM_RXSTAT_MACCTL	0x00000001
#define XM_RXSTAT_ERRFRAME	0x00000002
#define XM_RXSTAT_CRCERR	0x00000004
#define XM_RXSTAT_GIANT		0x00000008
#define XM_RXSTAT_RUNT		0x00000010
#define XM_RXSTAT_FRAMEERR	0x00000020
#define XM_RXSTAT_INRANGEERR	0x00000040
#define XM_RXSTAT_CARRIERERR	0x00000080
#define XM_RXSTAT_COLLERR	0x00000100
#define XM_RXSTAT_802_3		0x00000200
#define XM_RXSTAT_CARREXTERR	0x00000400
#define XM_RXSTAT_BURSTMODE	0x00000800
#define XM_RXSTAT_UNICAST	0x00002000
#define XM_RXSTAT_MULTICAST	0x00004000
#define XM_RXSTAT_BROADCAST	0x00008000
#define XM_RXSTAT_VLAN_LEV1	0x00010000
#define XM_RXSTAT_VLAN_LEV2	0x00020000
#define XM_RXSTAT_LEN		0xFFFC0000

/*
 * XMAC PHY registers, indirectly accessed through
 * XM_PHY_ADDR and XM_PHY_REG.
 */

#define XM_PHY_BMCR		0x0000	/* control */
#define XM_PHY_BMSR		0x0001	/* status */
#define XM_PHY_VENID		0x0002	/* vendor id */
#define XM_PHY_DEVID		0x0003	/* device id */
#define XM_PHY_ANAR		0x0004	/* autoneg advertisenemt */
#define XM_PHY_LPAR		0x0005	/* link partner ability */
#define XM_PHY_ANEXP		0x0006	/* autoneg expansion */
#define XM_PHY_NEXTP		0x0007	/* nextpage */
#define XM_PHY_LPNEXTP		0x0008	/* link partner's nextpage */
#define XM_PHY_EXTSTS		0x000F	/* extented status */
#define XM_PHY_RESAB		0x0010	/* resolved ability */

#define XM_BMCR_DUPLEX		0x0100
#define XM_BMCR_RENEGOTIATE	0x0200
#define XM_BMCR_AUTONEGENBL	0x1000
#define XM_BMCR_LOOPBACK	0x4000
#define XM_BMCR_RESET		0x8000

#define XM_BMSR_EXTCAP		0x0001
#define XM_BMSR_LINKSTAT	0x0004
#define XM_BMSR_AUTONEGABLE	0x0008
#define XM_BMSR_REMFAULT	0x0010
#define XM_BMSR_AUTONEGDONE	0x0020
#define XM_BMSR_EXTSTAT		0x0100

#define XM_VENID_XAQTI		0xD14C
#define XM_DEVID_XMAC		0x0002

#define XM_ANAR_FULLDUPLEX	0x0020
#define XM_ANAR_HALFDUPLEX	0x0040
#define XM_ANAR_PAUSEBITS	0x0180
#define XM_ANAR_REMFAULTBITS	0x1800
#define XM_ANAR_ACK		0x4000
#define XM_ANAR_NEXTPAGE	0x8000

#define XM_LPAR_FULLDUPLEX	0x0020
#define XM_LPAR_HALFDUPLEX	0x0040
#define XM_LPAR_PAUSEBITS	0x0180
#define XM_LPAR_REMFAULTBITS	0x1800
#define XM_LPAR_ACK		0x4000
#define XM_LPAR_NEXTPAGE	0x8000

#define XM_PAUSE_NOPAUSE	0x0000
#define XM_PAUSE_SYMPAUSE	0x0080
#define XM_PAUSE_ASYMPAUSE	0x0100
#define XM_PAUSE_BOTH		0x0180

#define XM_REMFAULT_LINKOK	0x0000
#define XM_REMFAULT_LINKFAIL	0x0800
#define XM_REMFAULT_OFFLINE	0x1000
#define XM_REMFAULT_ANEGERR	0x1800

#define XM_ANEXP_GOTPAGE	0x0002
#define XM_ANEXP_NEXTPAGE_SELF	0x0004
#define XM_ANEXP_NEXTPAGE_LP	0x0008

#define XM_NEXTP_MESSAGE	0x07FF
#define XM_NEXTP_TOGGLE		0x0800
#define XM_NEXTP_ACK2		0x1000
#define XM_NEXTP_MPAGE		0x2000
#define XM_NEXTP_ACK1		0x4000
#define XM_NEXTP_NPAGE		0x8000

#define XM_LPNEXTP_MESSAGE	0x07FF
#define XM_LPNEXTP_TOGGLE	0x0800
#define XM_LPNEXTP_ACK2		0x1000
#define XM_LPNEXTP_MPAGE	0x2000
#define XM_LPNEXTP_ACK1		0x4000
#define XM_LPNEXTP_NPAGE	0x8000

#define XM_EXTSTS_HALFDUPLEX	0x4000
#define XM_EXTSTS_FULLDUPLEX	0x8000

#define XM_RESAB_PAUSEMISMATCH	0x0008
#define XM_RESAB_ABLMISMATCH	0x0010
#define XM_RESAB_FDMODESEL	0x0020
#define XM_RESAB_HDMODESEL	0x0040
#define XM_RESAB_PAUSEBITS	0x0180
