/*
 * Copyright (c) 2004
 *	Joerg Sonnenberger <joerg@bec.de>.  All rights reserved.
 *
 * Copyright (c) 1997, 1998-2003
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
 * $FreeBSD: src/sys/pci/if_rlreg.h,v 1.42 2004/05/24 19:39:23 jhb Exp $
 * $DragonFly: src/sys/dev/netif/re/if_rereg.h,v 1.3 2004/08/02 13:35:02 joerg Exp $
 */

/*
 * RealTek 8129/8139 register offsets
 */
#define	RE_IDR0		0x0000		/* ID register 0 (station addr) */
#define	RE_IDR1		0x0001		/* Must use 32-bit accesses (?) */
#define	RE_IDR2		0x0002
#define	RE_IDR3		0x0003
#define	RE_IDR4		0x0004
#define	RE_IDR5		0x0005
					/* 0006-0007 reserved */
#define	RE_MAR0		0x0008		/* Multicast hash table */
#define	RE_MAR1		0x0009
#define	RE_MAR2		0x000A
#define	RE_MAR3		0x000B
#define	RE_MAR4		0x000C
#define	RE_MAR5		0x000D
#define	RE_MAR6		0x000E
#define	RE_MAR7		0x000F

#define RE_RXADDR		0x0030	/* RX ring start address */
#define RE_RX_EARLY_BYTES	0x0034	/* RX early byte count */
#define RE_RX_EARLY_STAT	0x0036	/* RX early status */
#define RE_COMMAND	0x0037		/* command register */
#define RE_CURRXADDR	0x0038		/* current address of packet read */
#define RE_CURRXBUF	0x003A		/* current RX buffer address */
#define RE_IMR		0x003C		/* interrupt mask register */
#define RE_ISR		0x003E		/* interrupt status register */
#define RE_TXCFG	0x0040		/* transmit config */
#define RE_RXCFG	0x0044		/* receive config */
#define RE_TIMERCNT	0x0048		/* timer count register */
#define RE_MISSEDPKT	0x004C		/* missed packet counter */
#define RE_EECMD	0x0050		/* EEPROM command register */
#define RE_CFG0		0x0051		/* config register #0 */
#define RE_CFG1		0x0052		/* config register #1 */
                                        /* 0053-0057 reserved */   
#define RE_MEDIASTAT	0x0058		/* media status register (8139) */
					/* 0059-005A reserved */
#define RE_MII		0x005A		/* 8129 chip only */
#define RE_HALTCLK	0x005B
#define RE_MULTIINTR	0x005C		/* multiple interrupt */
#define RE_PCIREV	0x005E		/* PCI revision value */
					/* 005F reserved */
#define RE_TXSTAT_ALL	0x0060		/* TX status of all descriptors */

/* Direct PHY access registers only available on 8139 */
#define RE_BMCR		0x0062		/* PHY basic mode control */
#define RE_BMSR		0x0064		/* PHY basic mode status */
#define RE_ANAR		0x0066		/* PHY autoneg advert */
#define RE_LPAR		0x0068		/* PHY link partner ability */
#define RE_ANER		0x006A		/* PHY autoneg expansion */

#define RE_DISCCNT	0x006C		/* disconnect counter */
#define RE_FALSECAR	0x006E		/* false carrier counter */
#define RE_NWAYTST	0x0070		/* NWAY test register */
#define RE_RX_ER	0x0072		/* RX_ER counter */
#define RE_CSCFG	0x0074		/* CS configuration register */

/*
 * When operating in special C+ mode, some of the registers in an
 * 8139C+ chip have different definitions. These are also used for
 * the 8169 gigE chip.
 */
#define RE_DUMPSTATS_LO		0x0010	/* counter dump command register */
#define RE_DUMPSTATS_HI		0x0014	/* counter dump command register */
#define RE_TXLIST_ADDR_LO	0x0020	/* 64 bits, 256 byte alignment */
#define RE_TXLIST_ADDR_HI	0x0024	/* 64 bits, 256 byte alignment */
#define RE_TXLIST_ADDR_HPRIO_LO	0x0028	/* 64 bits, 256 byte alignment */
#define RE_TXLIST_ADDR_HPRIO_HI	0x002C	/* 64 bits, 256 byte alignment */
#define RE_CFG2			0x0053
#define RE_TIMERINT		0x0054	/* interrupt on timer expire */
#define RE_TXSTART		0x00D9	/* 8 bits */
#define RE_CPLUS_CMD		0x00E0	/* 16 bits */
#define RE_RXLIST_ADDR_LO	0x00E4	/* 64 bits, 256 byte alignment */
#define RE_RXLIST_ADDR_HI	0x00E8	/* 64 bits, 256 byte alignment */
#define RE_EARLY_TX_THRESH	0x00EC	/* 8 bits */

/*
 * Registers specific to the 8169 gigE chip
 */
#define RE_TIMERINT_8169	0x0058	/* different offset than 8139 */
#define RE_PHYAR		0x0060
#define RE_TBICSR		0x0064
#define RE_TBI_ANAR		0x0068
#define RE_TBI_LPAR		0x006A
#define RE_GMEDIASTAT		0x006C	/* 8 bits */
#define RE_MAXRXPKTLEN		0x00DA	/* 16 bits, chip multiplies by 8 */
#define RE_GTXSTART		0x0038	/* 16 bits */

/*
 * TX config register bits
 */
#define RE_TXCFG_CLRABRT	0x00000001	/* retransmit aborted pkt */
#define RE_TXCFG_MAXDMA		0x00000700	/* max DMA burst size */
#define RE_TXCFG_CRCAPPEND	0x00010000	/* CRC append (0 = yes) */
#define RE_TXCFG_LOOPBKTST	0x00060000	/* loopback test */
#define RE_TXCFG_IFG2		0x00080000	/* 8169 only */
#define RE_TXCFG_IFG		0x03000000	/* interframe gap */
#define RE_TXCFG_HWREV		0x7CC00000

#define RE_LOOPTEST_OFF		0x00000000
#define RE_LOOPTEST_ON		0x00020000
#define RE_LOOPTEST_ON_CPLUS	0x00060000

#define RE_HWREV_8169		0x00000000
#define RE_HWREV_8169S		0x04000000
#define RE_HWREV_8110S		0x00800000
#define RE_HWREV_8139CPLUS	0x74800000

#define RE_TXDMA_16BYTES	0x00000000
#define RE_TXDMA_32BYTES	0x00000100
#define RE_TXDMA_64BYTES	0x00000200
#define RE_TXDMA_128BYTES	0x00000300
#define RE_TXDMA_256BYTES	0x00000400
#define RE_TXDMA_512BYTES	0x00000500
#define RE_TXDMA_1024BYTES	0x00000600
#define RE_TXDMA_2048BYTES	0x00000700

/*
 * Transmit descriptor status register bits.
 */
#define RE_TXSTAT_LENMASK	0x00001FFF
#define RE_TXSTAT_OWN		0x00002000
#define RE_TXSTAT_TX_UNDERRUN	0x00004000
#define RE_TXSTAT_TX_OK		0x00008000
#define RE_TXSTAT_EARLY_THRESH	0x003F0000
#define RE_TXSTAT_COLLCNT	0x0F000000
#define RE_TXSTAT_CARR_HBEAT	0x10000000
#define RE_TXSTAT_OUTOFWIN	0x20000000
#define RE_TXSTAT_TXABRT	0x40000000
#define RE_TXSTAT_CARRLOSS	0x80000000

/*
 * Interrupt status register bits.
 */
#define RE_ISR_RX_OK		0x0001
#define RE_ISR_RX_ERR		0x0002
#define RE_ISR_TX_OK		0x0004
#define RE_ISR_TX_ERR		0x0008
#define RE_ISR_RX_OVERRUN	0x0010
#define RE_ISR_PKT_UNDERRUN	0x0020
#define RE_ISR_LINKCHG		0x0020	/* 8169 only */
#define RE_ISR_FIFO_OFLOW	0x0040	/* 8139 only */
#define RE_ISR_TX_DESC_UNAVAIL	0x0080	/* C+ only */
#define RE_ISR_SWI		0x0100	/* C+ only */
#define RE_ISR_CABLE_LEN_CHGD	0x2000
#define RE_ISR_PCS_TIMEOUT	0x4000	/* 8129 only */
#define RE_ISR_TIMEOUT_EXPIRED	0x4000
#define RE_ISR_SYSTEM_ERR	0x8000

#define RE_INTRS_CPLUS	\
	(RE_ISR_RX_OK|RE_ISR_RX_ERR|RE_ISR_TX_ERR|			\
	RE_ISR_RX_OVERRUN|RE_ISR_PKT_UNDERRUN|RE_ISR_FIFO_OFLOW|	\
	RE_ISR_PCS_TIMEOUT|RE_ISR_SYSTEM_ERR|RE_ISR_TIMEOUT_EXPIRED)

/*
 * Media status register. (8139 only)
 */
#define RE_MEDIASTAT_RXPAUSE	0x01
#define RE_MEDIASTAT_TXPAUSE	0x02
#define RE_MEDIASTAT_LINK	0x04
#define RE_MEDIASTAT_SPEED10	0x08
#define RE_MEDIASTAT_RXFLOWCTL	0x40	/* duplex mode */
#define RE_MEDIASTAT_TXFLOWCTL	0x80	/* duplex mode */

/*
 * Receive config register.
 */
#define RE_RXCFG_RX_ALLPHYS	0x00000001	/* accept all nodes */
#define RE_RXCFG_RX_INDIV	0x00000002	/* match filter */
#define RE_RXCFG_RX_MULTI	0x00000004	/* accept all multicast */
#define RE_RXCFG_RX_BROAD	0x00000008	/* accept all broadcast */
#define RE_RXCFG_RX_RUNT	0x00000010
#define RE_RXCFG_RX_ERRPKT	0x00000020
#define RE_RXCFG_WRAP		0x00000080
#define RE_RXCFG_MAXDMA		0x00000700
#define RE_RXCFG_BUFSZ		0x00001800
#define RE_RXCFG_FIFOTHRESH	0x0000E000
#define RE_RXCFG_EARLYTHRESH	0x07000000

#define RE_RXDMA_16BYTES	0x00000000
#define RE_RXDMA_32BYTES	0x00000100
#define RE_RXDMA_64BYTES	0x00000200
#define RE_RXDMA_128BYTES	0x00000300
#define RE_RXDMA_256BYTES	0x00000400
#define RE_RXDMA_512BYTES	0x00000500
#define RE_RXDMA_1024BYTES	0x00000600
#define RE_RXDMA_UNLIMITED	0x00000700

#define RE_RXBUF_8		0x00000000
#define RE_RXBUF_16		0x00000800
#define RE_RXBUF_32		0x00001000
#define RE_RXBUF_64		0x00001800

#define RE_RXFIFO_16BYTES	0x00000000
#define RE_RXFIFO_32BYTES	0x00002000
#define RE_RXFIFO_64BYTES	0x00004000
#define RE_RXFIFO_128BYTES	0x00006000
#define RE_RXFIFO_256BYTES	0x00008000
#define RE_RXFIFO_512BYTES	0x0000A000
#define RE_RXFIFO_1024BYTES	0x0000C000
#define RE_RXFIFO_NOTHRESH	0x0000E000

/*
 * Bits in RX status header (included with RX'ed packet
 * in ring buffer).
 */
#define RE_RXSTAT_RXOK		0x00000001
#define RE_RXSTAT_ALIGNERR	0x00000002
#define RE_RXSTAT_CRCERR	0x00000004
#define RE_RXSTAT_GIANT		0x00000008
#define RE_RXSTAT_RUNT		0x00000010
#define RE_RXSTAT_BADSYM	0x00000020
#define RE_RXSTAT_BROAD		0x00002000
#define RE_RXSTAT_INDIV		0x00004000
#define RE_RXSTAT_MULTI		0x00008000
#define RE_RXSTAT_LENMASK	0xFFFF0000

#define RE_RXSTAT_UNFINISHED	0xFFF0		/* DMA still in progress */
/*
 * Command register.
 */
#define RE_CMD_EMPTY_RXBUF	0x0001
#define RE_CMD_TX_ENB		0x0004
#define RE_CMD_RX_ENB		0x0008
#define RE_CMD_RESET		0x0010

/*
 * EEPROM control register
 */
#define RE_EE_DATAOUT		0x01	/* Data out */
#define RE_EE_DATAIN		0x02	/* Data in */
#define RE_EE_CLK		0x04	/* clock */
#define RE_EE_SEL		0x08	/* chip select */
#define RE_EE_MODE		(0x40|0x80)

#define RE_EEMODE_OFF		0x00
#define RE_EEMODE_AUTOLOAD	0x40
#define RE_EEMODE_PROGRAM	0x80
#define RE_EEMODE_WRITECFG	(0x80|0x40)

/* 9346 EEPROM commands */
#define RE_EECMD_WRITE		0x140
#define RE_EECMD_READ_6BIT	0x180
#define RE_EECMD_READ_8BIT	0x600
#define RE_EECMD_ERASE		0x1c0

#define RE_EE_ID		0x00
#define RE_EE_PCI_VID		0x01
#define RE_EE_PCI_DID		0x02
/* Location of station address inside EEPROM */
#define RE_EE_EADDR		0x07

/*
 * Config 0 register
 */
#define RE_CFG0_ROM0		0x01
#define RE_CFG0_ROM1		0x02
#define RE_CFG0_ROM2		0x04
#define RE_CFG0_PL0		0x08
#define RE_CFG0_PL1		0x10
#define RE_CFG0_10MBPS		0x20	/* 10 Mbps internal mode */
#define RE_CFG0_PCS		0x40
#define RE_CFG0_SCR		0x80

/*
 * Config 1 register
 */
#define RE_CFG1_PWRDWN		0x01
#define RE_CFG1_SLEEP		0x02
#define RE_CFG1_IOMAP		0x04
#define RE_CFG1_MEMMAP		0x08
#define RE_CFG1_RSVD		0x10
#define RE_CFG1_DRVLOAD		0x20
#define RE_CFG1_LED0		0x40
#define RE_CFG1_FULLDUPLEX	0x40	/* 8129 only */
#define RE_CFG1_LED1		0x80

/*
 * 8139C+ register definitions
 */

/* RE_DUMPSTATS_LO register */

#define RE_DUMPSTATS_START	0x00000008

/* Transmit start register */

#define RE_TXSTART_SWI		0x01	/* generate TX interrupt */
#define RE_TXSTART_START	0x40	/* start normal queue transmit */
#define RE_TXSTART_HPRIO_START	0x80	/* start hi prio queue transmit */

/*
 * Config 2 register, 8139C+/8169/8169S/8110S only
 */
#define RE_CFG2_BUSFREQ		0x07
#define RE_CFG2_BUSWIDTH	0x08
#define RE_CFG2_AUXPWRSTS	0x10

#define RE_BUSFREQ_33MHZ	0x00
#define RE_BUSFREQ_66MHZ	0x01
                                        
#define RE_BUSWIDTH_32BITS	0x00
#define RE_BUSWIDTH_64BITS	0x08

/* C+ mode command register */

#define RE_CPLUSCMD_TXENB	0x0001	/* enable C+ transmit mode */
#define RE_CPLUSCMD_RXENB	0x0002	/* enable C+ receive mode */
#define RE_CPLUSCMD_PCI_MRW	0x0008	/* enable PCI multi-read/write */
#define RE_CPLUSCMD_PCI_DAC	0x0010	/* PCI dual-address cycle only */
#define RE_CPLUSCMD_RXCSUM_ENB	0x0020	/* enable RX checksum offload */
#define RE_CPLUSCMD_VLANSTRIP	0x0040	/* enable VLAN tag stripping */

/* C+ early transmit threshold */

#define RE_EARLYTXTHRESH_CNT	0x003F	/* byte count times 8 */ 

/*
 * Gigabit PHY access register (8169 only)
 */

#define RE_PHYAR_PHYDATA	0x0000FFFF
#define RE_PHYAR_PHYREG		0x001F0000
#define RE_PHYAR_BUSY		0x80000000

/*
 * Gigabit media status (8169 only)
 */
#define RE_GMEDIASTAT_FDX	0x01	/* full duplex */
#define RE_GMEDIASTAT_LINK	0x02	/* link up */
#define RE_GMEDIASTAT_10MBPS	0x04	/* 10mps link */
#define RE_GMEDIASTAT_100MBPS	0x08	/* 100mbps link */
#define RE_GMEDIASTAT_1000MBPS	0x10	/* gigE link */
#define RE_GMEDIASTAT_RXFLOW	0x20	/* RX flow control on */
#define RE_GMEDIASTAT_TXFLOW	0x40	/* TX flow control on */
#define RE_GMEDIASTAT_TBI	0x80	/* TBI enabled */

/*
 * The RealTek doesn't use a fragment-based descriptor mechanism.
 * Instead, there are only four register sets, each or which represents
 * one 'descriptor.' Basically, each TX descriptor is just a contiguous
 * packet buffer (32-bit aligned!) and we place the buffer addresses in
 * the registers so the chip knows where they are.
 *
 * We can sort of kludge together the same kind of buffer management
 * used in previous drivers, but we have to do buffer copies almost all
 * the time, so it doesn't really buy us much.
 *
 * For reception, there's just one large buffer where the chip stores
 * all received packets.
 */

#define RE_RX_BUF_SZ		RE_RXBUF_64
#define RE_RXBUFLEN		(1 << ((RE_RX_BUF_SZ >> 11) + 13))
#define RE_TX_LIST_CNT		4
#define RE_MIN_FRAMELEN		60
#define RE_TXTHRESH(x)		((x) << 11)
#define RE_TX_THRESH_INIT	96
#define RE_RX_FIFOTHRESH	RE_RXFIFO_NOTHRESH
#define RE_RX_MAXDMA		RE_RXDMA_UNLIMITED
#define RE_TX_MAXDMA		RE_TXDMA_2048BYTES

#define RE_RXCFG_CONFIG (RE_RX_FIFOTHRESH|RE_RX_MAXDMA|RE_RX_BUF_SZ)
#define RE_TXCFG_CONFIG	(RE_TXCFG_IFG|RE_TX_MAXDMA)

#define RE_ETHER_ALIGN	2

struct re_chain_data {
	uint16_t		cur_rx;
	caddr_t			re_rx_buf;
	caddr_t			re_rx_buf_ptr;
	bus_dmamap_t		re_rx_dmamap;

	struct mbuf		*re_tx_chain[RE_TX_LIST_CNT];
	bus_dmamap_t		re_tx_dmamap[RE_TX_LIST_CNT];
	uint8_t			last_tx;
	uint8_t			cur_tx;
};

#define RE_INC(x)		(x = (x + 1) % RE_TX_LIST_CNT)
#define RE_CUR_TXADDR(x)	((x->re_cdata.cur_tx * 4) + RE_TXADDR0)
#define RE_CUR_TXSTAT(x)	((x->re_cdata.cur_tx * 4) + RE_TXSTAT0)
#define RE_CUR_TXMBUF(x)	(x->re_cdata.re_tx_chain[x->re_cdata.cur_tx])
#define RE_CUR_DMAMAP(x)	(x->re_cdata.re_tx_dmamap[x->re_cdata.cur_tx])
#define RE_LAST_TXADDR(x)	((x->re_cdata.last_tx * 4) + RE_TXADDR0)
#define RE_LAST_TXSTAT(x)	((x->re_cdata.last_tx * 4) + RE_TXSTAT0)
#define RE_LAST_TXMBUF(x)	(x->re_cdata.re_tx_chain[x->re_cdata.last_tx])
#define RE_LAST_DMAMAP(x)	(x->re_cdata.re_tx_dmamap[x->re_cdata.last_tx])

struct re_type {
	uint16_t		re_vid;
	uint16_t		re_did;
	int			re_basetype;
	const char		*re_name;
};

struct re_hwrev {
	uint32_t		re_rev;
	int			re_type;
	const char		*re_desc;
};

struct re_mii_frame {
	uint8_t			mii_stdelim;
	uint8_t			mii_opcode;
	uint8_t			mii_phyaddr;
	uint8_t			mii_regaddr;
	uint8_t			mii_turnaround;
	uint16_t		mii_data;
};

/*
 * MII constants
 */
#define RE_MII_STARTDELIM	0x01
#define RE_MII_READOP		0x02
#define RE_MII_WRITEOP		0x01
#define RE_MII_TURNAROUND	0x02

#define RE_8139CPLUS		3
#define RE_8169			4

#define RE_ISCPLUS(x)		((x)->re_type == RE_8139CPLUS ||	\
				 (x)->re_type == RE_8169)

/*
 * The 8139C+ and 8160 gigE chips support descriptor-based TX
 * and RX. In fact, they even support TCP large send. Descriptors
 * must be allocated in contiguous blocks that are aligned on a
 * 256-byte boundary. The rings can hold a maximum of 64 descriptors.
 */

/*
 * RX/TX descriptor definition. When large send mode is enabled, the
 * lower 11 bits of the TX re_cmd word are used to hold the MSS, and
 * the checksum offload bits are disabled. The structure layout is
 * the same for RX and TX descriptors
 */

struct re_desc {
	uint32_t		re_cmdstat;
	uint32_t		re_vlanctl;
	uint32_t		re_bufaddr_lo;
	uint32_t		re_bufaddr_hi;
};

#define RE_TDESC_CMD_FRAGLEN	0x0000FFFF
#define RE_TDESC_CMD_TCPCSUM	0x00010000	/* TCP checksum enable */
#define RE_TDESC_CMD_UDPCSUM	0x00020000	/* UDP checksum enable */
#define RE_TDESC_CMD_IPCSUM	0x00040000	/* IP header checksum enable */
#define RE_TDESC_CMD_MSSVAL	0x07FF0000	/* Large send MSS value */
#define RE_TDESC_CMD_LGSEND	0x08000000	/* TCP large send enb */
#define RE_TDESC_CMD_EOF	0x10000000	/* end of frame marker */
#define RE_TDESC_CMD_SOF	0x20000000	/* start of frame marker */
#define RE_TDESC_CMD_EOR	0x40000000	/* end of ring marker */
#define RE_TDESC_CMD_OWN	0x80000000	/* chip owns descriptor */

#define RE_TDESC_VLANCTL_TAG	0x00020000	/* Insert VLAN tag */
#define RE_TDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */

/*
 * Error bits are valid only on the last descriptor of a frame
 * (i.e. RE_TDESC_CMD_EOF == 1)
 */

#define RE_TDESC_STAT_COLCNT	0x000F0000	/* collision count */
#define RE_TDESC_STAT_EXCESSCOL	0x00100000	/* excessive collisions */
#define RE_TDESC_STAT_LINKFAIL	0x00200000	/* link faulure */
#define RE_TDESC_STAT_OWINCOL	0x00400000	/* out-of-window collision */
#define RE_TDESC_STAT_TXERRSUM	0x00800000	/* transmit error summary */
#define RE_TDESC_STAT_UNDERRUN	0x02000000	/* TX underrun occured */
#define RE_TDESC_STAT_OWN	0x80000000

/*
 * RX descriptor cmd/vlan definitions
 */

#define RE_RDESC_CMD_EOR	0x40000000
#define RE_RDESC_CMD_OWN	0x80000000
#define RE_RDESC_CMD_BUFLEN	0x00001FFF

#define RE_RDESC_STAT_OWN	0x80000000
#define RE_RDESC_STAT_EOR	0x40000000
#define RE_RDESC_STAT_SOF	0x20000000
#define RE_RDESC_STAT_EOF	0x10000000
#define RE_RDESC_STAT_FRALIGN	0x08000000	/* frame alignment error */
#define RE_RDESC_STAT_MCAST	0x04000000	/* multicast pkt received */
#define RE_RDESC_STAT_UCAST	0x02000000	/* unicast pkt received */
#define RE_RDESC_STAT_BCAST	0x01000000	/* broadcast pkt received */
#define RE_RDESC_STAT_BUFOFLOW	0x00800000	/* out of buffer space */
#define RE_RDESC_STAT_FIFOOFLOW	0x00400000	/* FIFO overrun */
#define RE_RDESC_STAT_GIANT	0x00200000	/* pkt > 4096 bytes */
#define RE_RDESC_STAT_RXERRSUM	0x00100000	/* RX error summary */
#define RE_RDESC_STAT_RUNT	0x00080000	/* runt packet received */
#define RE_RDESC_STAT_CRCERR	0x00040000	/* CRC error */
#define RE_RDESC_STAT_PROTOID	0x00030000	/* Protocol type */
#define RE_RDESC_STAT_IPSUMBAD	0x00008000	/* IP header checksum bad */
#define RE_RDESC_STAT_UDPSUMBAD	0x00004000	/* UDP checksum bad */
#define RE_RDESC_STAT_TCPSUMBAD	0x00002000	/* TCP checksum bad */
#define RE_RDESC_STAT_FRAGLEN	0x00001FFF	/* RX'ed frame/frag len */
#define RE_RDESC_STAT_GFRAGLEN	0x00003FFF	/* RX'ed frame/frag len */

#define RE_RDESC_VLANCTL_TAG	0x00010000	/* VLAN tag available
						   (re_vlandata valid)*/
#define RE_RDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */

#define RE_PROTOID_NONIP	0x00000000
#define RE_PROTOID_TCPIP	0x00010000
#define RE_PROTOID_UDPIP	0x00020000
#define RE_PROTOID_IP		0x00030000
#define RE_TCPPKT(x)		(((x) & RE_RDESC_STAT_PROTOID) == \
				 RE_PROTOID_TCPIP)
#define RE_UDPPKT(x)		(((x) & RE_RDESC_STAT_PROTOID) == \
				 RE_PROTOID_UDPIP)

/*
 * Statistics counter structure (8139C+ and 8169 only)
 */
struct re_stats {
	uint32_t		re_tx_pkts_lo;
	uint32_t		re_tx_pkts_hi;
	uint32_t		re_tx_errs_lo;
	uint32_t		re_tx_errs_hi;
	uint32_t		re_tx_errs;
	uint16_t		re_missed_pkts;
	uint16_t		re_rx_framealign_errs;
	uint32_t		re_tx_onecoll;
	uint32_t		re_tx_multicolls;
	uint32_t		re_rx_ucasts_hi;
	uint32_t		re_rx_ucasts_lo;
	uint32_t		re_rx_bcasts_lo;
	uint32_t		re_rx_bcasts_hi;
	uint32_t		re_rx_mcasts;
	uint16_t		re_tx_aborts;
	uint16_t		re_rx_underruns;
};

#define RE_RX_DESC_CNT		64
#define RE_TX_DESC_CNT		64
#define RE_RX_LIST_SZ		(RE_RX_DESC_CNT * sizeof(struct re_desc))
#define RE_TX_LIST_SZ		(RE_TX_DESC_CNT * sizeof(struct re_desc))
#define RE_RING_ALIGN		256
#define RE_IFQ_MAXLEN		512
#define RE_DESC_INC(x)		(x = (x + 1) % RE_TX_DESC_CNT)
#define RE_OWN(x)		(le32toh((x)->re_cmdstat) & RE_RDESC_STAT_OWN)
#define RE_RXBYTES(x)		(le32toh((x)->re_cmdstat) & sc->re_rxlenmask)
#define RE_PKTSZ(x)		((x)/* >> 3*/)

#define RE_ADDR_LO(y)	((u_int64_t) (y) & 0xFFFFFFFF)
#define RE_ADDR_HI(y)	((u_int64_t) (y) >> 32)

#define RE_JUMBO_FRAMELEN	9018
#define RE_JUMBO_MTU		(RE_JUMBO_FRAMELEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

struct re_softc;

struct re_dmaload_arg {
	struct re_softc		*sc;
	int			re_idx;
	int			re_maxsegs;
	uint32_t		re_flags;
	struct re_desc		*re_ring;
};

struct re_list_data {
	struct mbuf		*re_tx_mbuf[RE_TX_DESC_CNT];
	struct mbuf		*re_rx_mbuf[RE_TX_DESC_CNT];
	int			re_tx_prodidx;
	int			re_rx_prodidx;
	int			re_tx_considx;
	int			re_tx_free;
	bus_dmamap_t		re_tx_dmamap[RE_TX_DESC_CNT];
	bus_dmamap_t		re_rx_dmamap[RE_RX_DESC_CNT];
	bus_dma_tag_t		re_mtag;	/* mbuf mapping tag */
	bus_dma_tag_t		re_stag;	/* stats mapping tag */
	bus_dmamap_t		re_smap;	/* stats map */
	struct re_stats		*re_stats;
	bus_addr_t		re_stats_addr;
	bus_dma_tag_t		re_rx_list_tag;
	bus_dmamap_t		re_rx_list_map;
	struct re_desc		*re_rx_list;
	bus_addr_t		re_rx_list_addr;
	bus_dma_tag_t		re_tx_list_tag;
	bus_dmamap_t		re_tx_list_map;
	struct re_desc		*re_tx_list;
	bus_addr_t		re_tx_list_addr;
};

struct re_softc {
	struct arpcom		arpcom;		/* interface info */
	bus_space_handle_t	re_bhandle;	/* bus space handle */
	bus_space_tag_t		re_btag;	/* bus space tag */
	struct resource		*re_res;
	struct resource		*re_irq;
	void			*re_intrhand;
	device_t		re_miibus;
	bus_dma_tag_t		re_parent_tag;
	bus_dma_tag_t		re_tag;
	u_int8_t		re_type;
	int			re_eecmd_read;
	u_int8_t		re_stats_no_timeout;
	int			re_txthresh;
	struct re_chain_data	re_cdata;
	struct re_list_data	re_ldata;
	struct callout		re_timer;
	struct mbuf		*re_head;
	struct mbuf		*re_tail;
	uint32_t		re_hwrev;
	uint32_t		re_rxlenmask;
	int			re_testmode;
	int			suspended;	/* 0 = normal  1 = suspended */
#ifdef DEVICE_POLLING
	int			rxcycles;
#endif

#ifndef BURN_BRIDGES
	uint32_t		saved_maps[5];	/* pci data */
	uint32_t		saved_biosaddr;
	uint8_t			saved_intline;
	uint8_t			saved_cachelnsz;
	uint8_t			saved_lattimer;
#endif
};

/*
 * register space access macros
 */
#define CSR_WRITE_STREAM_4(sc, reg, val)	\
	bus_space_write_stream_4(sc->re_btag, sc->re_bhandle, reg, val)
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->re_btag, sc->re_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->re_btag, sc->re_bhandle, reg, val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->re_btag, sc->re_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->re_btag, sc->re_bhandle, reg)
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->re_btag, sc->re_bhandle, reg)
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->re_btag, sc->re_bhandle, reg)

#define	RE_TIMEOUT		1000

/*
 * General constants that are fun to know.
 *
 * RealTek PCI vendor ID
 */
#define	RT_VENDORID				0x10EC

/*
 * RealTek chip device IDs.
 */
#define	RT_DEVICEID_8129			0x8129
#define	RT_DEVICEID_8138			0x8138
#define	RT_DEVICEID_8139			0x8139
#define RT_DEVICEID_8169			0x8169
#define RT_DEVICEID_8100			0x8100

#define RT_REVID_8139CPLUS			0x20

/*
 * Accton PCI vendor ID
 */
#define ACCTON_VENDORID				0x1113

/*
 * Accton MPX 5030/5038 device ID.
 */
#define ACCTON_DEVICEID_5030			0x1211

/*
 * Nortel PCI vendor ID
 */
#define NORTEL_VENDORID				0x126C

/*
 * Delta Electronics Vendor ID.
 */
#define DELTA_VENDORID				0x1500

/*
 * Delta device IDs.
 */
#define DELTA_DEVICEID_8139			0x1360

/*
 * Addtron vendor ID.
 */
#define ADDTRON_VENDORID			0x4033

/*
 * Addtron device IDs.
 */
#define ADDTRON_DEVICEID_8139			0x1360

/*
 * D-Link vendor ID.
 */
#define DLINK_VENDORID				0x1186

/*
 * D-Link DFE-530TX+ device ID
 */
#define DLINK_DEVICEID_530TXPLUS		0x1300

/*
 * D-Link DFE-690TXD device ID
 */
#define DLINK_DEVICEID_690TXD			0x1340

/*
 * Corega K.K vendor ID
 */
#define COREGA_VENDORID				0x1259

/*
 * Corega FEther CB-TXD device ID
 */
#define COREGA_DEVICEID_FETHERCBTXD			0xa117

/*
 * Corega FEtherII CB-TXD device ID
 */
#define COREGA_DEVICEID_FETHERIICBTXD			0xa11e

/*
 * Peppercon vendor ID
 */
#define PEPPERCON_VENDORID			0x1743

/*
 * Peppercon ROL-F device ID
 */
#define PEPPERCON_DEVICEID_ROLF			0x8139

/*
 * Planex Communications, Inc. vendor ID
 */
#define PLANEX_VENDORID				0x14ea

/*
 * Planex FNW-3800-TX device ID
 */
#define PLANEX_DEVICEID_FNW3800TX		0xab07

/*
 * LevelOne vendor ID
 */
#define LEVEL1_VENDORID				0x018A

/*
 * LevelOne FPC-0106TX devide ID
 */
#define LEVEL1_DEVICEID_FPC0106TX		0x0106

/*
 * Compaq vendor ID
 */
#define CP_VENDORID				0x021B

/*
 * Edimax vendor ID
 */
#define EDIMAX_VENDORID				0x13D1

/*
 * Edimax EP-4103DL cardbus device ID
 */
#define EDIMAX_DEVICEID_EP4103DL		0xAB06

/*
 * PCI low memory base and low I/O base register, and
 * other PCI registers.
 */

#define RE_PCI_LOMEM		0x14
#define RE_PCI_LOIO		0x10
