/*
 * Copyright (c) 1997, 1998, 1999
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
 * $FreeBSD: src/sys/pci/if_sfreg.h,v 1.6.2.1 2001/08/16 20:35:04 wpaul Exp $
 * $DragonFly: src/sys/dev/netif/sf/if_sfreg.h,v 1.3 2004/09/15 00:41:53 joerg Exp $
 */

/*
 * Registers for the Adaptec AIC-6915 Starfire. The Starfire has a 512K
 * register space. These registers can be accessed in the following way:
 * - PCI config registers are always accessible through PCI config space
 * - Full 512K space mapped into memory using PCI memory mapped access
 * - 256-byte I/O space mapped through PCI I/O access
 * - Full 512K space mapped through indirect I/O using PCI I/O access
 * It's possible to use either memory mapped mode or I/O mode to access
 * the registers, but memory mapped is usually the easiest. All registers
 * are 32 bits wide and must be accessed using 32-bit operations.
 */

/*
 * Adaptec PCI vendor ID.
 */
#define AD_VENDORID		0x9004

/*
 * AIC-6915 PCI device ID.
 */
#define AD_DEVICEID_STARFIRE	0x6915

/*
 * AIC-6915 subsystem IDs. Adaptec uses the subsystem ID to identify
 * the exact kind of NIC on which the ASIC is mounted. Currently there
 * are six different variations. Note: the Adaptec manual lists code 0x28
 * for two different NICs: the 62044 and the 69011/TX. This is a typo:
 * the code for the 62044 is really 0x18.
 *
 * Note that there also appears to be an 0x19 code for a newer rev
 * 62044 card.
 */
#define AD_SUBSYSID_62011_REV0	0x0008	/* single port 10/100baseTX 64-bit */
#define AD_SUBSYSID_62011_REV1	0x0009	/* single port 10/100baseTX 64-bit */
#define AD_SUBSYSID_62022	0x0010	/* dual port 10/100baseTX 64-bit */
#define AD_SUBSYSID_62044_REV0	0x0018	/* quad port 10/100baseTX 64-bit */
#define AD_SUBSYSID_62044_REV1	0x0019	/* quad port 10/100baseTX 64-bit */
#define AD_SUBSYSID_62020	0x0020	/* single port 10/100baseFX 64-bit */
#define AD_SUBSYSID_69011	0x0028	/* single port 10/100baseTX 32-bit */

/*
 * Starfire internal register space map. The entire register space
 * is available using PCI memory mapped mode. The SF_RMAP_INTREG
 * space is available using PCI I/O mode. The entire space can be
 * accessed using indirect I/O using the indirect I/O addr and
 * indirect I/O data registers located within the SF_RMAP_INTREG space.
 */
#define SF_RMAP_ROMADDR_BASE	0x00000	/* Expansion ROM space */
#define SF_RMAP_ROMADDR_MAX	0x3FFFF

#define SF_RMAP_EXGPIO_BASE	0x40000 /* External general purpose regs */
#define SF_RMAP_EXGPIO_MAX	0x3FFFF

#define SF_RMAP_INTREG_BASE	0x50000 /* Internal functional registers */
#define SF_RMAP_INTREG_MAX	0x500FF
#define SF_RMAP_GENREG_BASE	0x50100 /* General purpose registers */
#define SF_RMAP_GENREG_MAX	0x5FFFF

#define SF_RMAP_FIFO_BASE	0x60000
#define SF_RMAP_FIFO_MAX	0x6FFFF

#define SF_RMAP_STS_BASE	0x70000
#define SF_RMAP_STS_MAX		0x70083

#define SF_RMAP_RSVD_BASE	0x70084
#define SF_RMAP_RSVD_MAX	0x7FFFF

/*
 * PCI config header registers, 0x0000 to 0x003F
 */
#define SF_PCI_VENDOR_ID	0x0000
#define SF_PCI_DEVICE_ID	0x0002
#define SF_PCI_COMMAND		0x0004
#define SF_PCI_STATUS		0x0006
#define SF_PCI_REVID		0x0008
#define SF_PCI_CLASSCODE	0x0009
#define SF_PCI_CACHELEN		0x000C
#define SF_PCI_LATENCY_TIMER	0x000D
#define SF_PCI_HEADER_TYPE	0x000E
#define SF_PCI_LOMEM		0x0010
#define SF_PCI_LOIO		0x0014
#define SF_PCI_SUBVEN_ID	0x002C
#define SF_PCI_SYBSYS_ID	0x002E
#define SF_PCI_BIOSROM		0x0030
#define SF_PCI_INTLINE		0x003C
#define SF_PCI_INTPIN		0x003D
#define SF_PCI_MINGNT		0x003E
#define SF_PCI_MINLAT		0x003F

/*
 * PCI registers, 0x0040 to 0x006F
 */
#define SF_PCI_DEVCFG		0x0040
#define SF_BACCTL		0x0044
#define SF_PCI_MON1		0x0048
#define SF_PCI_MON2		0x004C
#define SF_PCI_CAPID		0x0050 /* 8 bits */
#define SF_PCI_NEXTPTR		0x0051 /* 8 bits */
#define SF_PCI_PWRMGMTCAP	0x0052 /* 16 bits */
#define SF_PCI_PWRMGMTCTRL	0x0054 /* 16 bits */
#define SF_PCI_PME_EVENT	0x0058
#define SF_PCI_EECTL		0x0060
#define SF_PCI_COMPLIANCE	0x0064
#define SF_INDIRECTIO_ADDR	0x0068
#define SF_INDIRECTIO_DATA	0x006C

#define SF_PCIDEVCFG_RESET	0x00000001
#define SF_PCIDEVCFG_FORCE64	0x00000002
#define SF_PCIDEVCFG_SYSTEM64	0x00000004
#define SF_PCIDEVCFG_RSVD0	0x00000008
#define SF_PCIDEVCFG_INCR_INB	0x00000010
#define SF_PCIDEVCFG_ABTONPERR	0x00000020
#define SF_PCIDEVCFG_STPONPERR	0x00000040
#define SF_PCIDEVCFG_MR_ENB	0x00000080
#define SF_PCIDEVCFG_FIFOTHR	0x00000F00
#define SF_PCIDEVCFG_STPONCA	0x00001000
#define SF_PCIDEVCFG_PCIMEN	0x00002000	/* enable PCI bus master */
#define SF_PCIDEVCFG_LATSTP	0x00004000
#define SF_PCIDEVCFG_BYTE_ENB	0x00008000
#define SF_PCIDEVCFG_EECSWIDTH	0x00070000
#define SF_PCIDEVCFG_STPMWCA	0x00080000
#define SF_PCIDEVCFG_REGCSWIDTH	0x00700000
#define SF_PCIDEVCFG_INTR_ENB	0x00800000
#define SF_PCIDEVCFG_DPR_ENB	0x01000000
#define SF_PCIDEVCFG_RSVD1	0x02000000
#define SF_PCIDEVCFG_RSVD2	0x04000000
#define SF_PCIDEVCFG_STA_ENB	0x08000000
#define SF_PCIDEVCFG_RTA_ENB	0x10000000
#define SF_PCIDEVCFG_RMA_ENB	0x20000000
#define SF_PCIDEVCFG_SSE_ENB	0x40000000
#define SF_PCIDEVCFG_DPE_ENB	0x80000000

#define SF_BACCTL_BACDMA_ENB	0x00000001
#define SF_BACCTL_PREFER_RXDMA	0x00000002
#define SF_BACCTL_PREFER_TXDMA	0x00000004
#define SF_BACCTL_SINGLE_DMA	0x00000008
#define SF_BACCTL_SWAPMODE_DATA	0x00000030
#define SF_BACCTL_SWAPMODE_DESC	0x000000C0

#define SF_SWAPMODE_LE		0x00000000
#define SF_SWAPMODE_BE		0x00000010

#define SF_PSTATE_MASK		0x0003
#define SF_PSTATE_D0		0x0000
#define SF_PSTATE_D1		0x0001
#define SF_PSTATE_D2		0x0002
#define SF_PSTATE_D3		0x0003
#define SF_PME_EN		0x0010
#define SF_PME_STATUS		0x8000


/*
 * Ethernet registers 0x0070 to 0x00FF
 */
#define SF_GEN_ETH_CTL		0x0070
#define SF_TIMER_CTL		0x0074
#define SF_CURTIME		0x0078
#define SF_ISR			0x0080
#define SF_ISR_SHADOW		0x0084
#define SF_IMR			0x0088
#define SF_GPIO			0x008C
#define SF_TXDQ_CTL		0x0090
#define SF_TXDQ_ADDR_HIPRIO	0x0094
#define SF_TXDQ_ADDR_LOPRIO	0x0098
#define SF_TXDQ_ADDR_HIADDR	0x009C
#define SF_TXDQ_PRODIDX		0x00A0
#define SF_TXDQ_CONSIDX		0x00A4
#define SF_TXDMA_STS1		0x00A8
#define SF_TXDMA_STS2		0x00AC
#define SF_TX_FRAMCTL		0x00B0
#define SF_TXCQ_ADDR_HI		0x00B4
#define SF_TXCQ_CTL		0x00B8
#define SF_RXCQ_CTL_1		0x00BC
#define SF_RXCQ_CTL_2		0x00C0
#define SF_CQ_CONSIDX		0x00C4
#define SF_CQ_PRODIDX		0x00C8
#define SF_CQ_RXQ2		0x00CC
#define SF_RXDMA_CTL		0x00D0
#define SF_RXDQ_CTL_1		0x00D4
#define SF_RXDQ_CTL_2		0x00D8
#define SF_RXDQ_ADDR_HIADDR	0x00DC
#define SF_RXDQ_ADDR_Q1		0x00E0
#define SF_RXDQ_ADDR_Q2		0x00E4
#define SF_RXDQ_PTR_Q1		0x00E8
#define SF_RXDQ_PTR_Q2		0x00EC
#define SF_RXDMA_STS		0x00F0
#define SF_RXFILT		0x00F4
#define SF_RX_FRAMETEST_OUT	0x00F8

/* Ethernet control register */
#define SF_ETHCTL_RX_ENB	0x00000001
#define SF_ETHCTL_TX_ENB	0x00000002
#define SF_ETHCTL_RXDMA_ENB	0x00000004
#define SF_ETHCTL_TXDMA_ENB	0x00000008
#define SF_ETHCTL_RXGFP_ENB	0x00000010
#define SF_ETHCTL_TXGFP_ENB	0x00000020
#define SF_ETHCTL_SOFTINTR	0x00000800

/* Timer control register */
#define SF_TIMER_IMASK_INTERVAL	0x0000001F
#define SF_TIMER_IMASK_MODE	0x00000060
#define SF_TIMER_SMALLFRAME_BYP	0x00000100
#define SF_TIMER_SMALLRX_FRAME	0x00000600
#define SF_TIMER_TIMES_TEN	0x00000800
#define SF_TIMER_RXHIPRIO_BYP	0x00001000
#define SF_TIMER_TX_DMADONE_DLY	0x00002000
#define SF_TIMER_TX_QDONE_DLY	0x00004000
#define SF_TIMER_TX_FRDONE_DLY	0x00008000
#define SF_TIMER_GENTIMER	0x00FF0000
#define SF_TIMER_ONESHOT	0x01000000
#define SF_TIMER_GENTIMER_RES	0x02000000
#define SF_TIMER_TIMEST_RES	0x04000000
#define SF_TIMER_RXQ2DONE_DLY	0x10000000
#define SF_TIMER_EARLYRX2_DLY	0x20000000
#define SF_TIMER_RXQ1DONE_DLY	0x40000000
#define SF_TIMER_EARLYRX1_DLY	0x80000000

/* Interrupt status register */
#define SF_ISR_PCIINT_ASSERTED	0x00000001
#define SF_ISR_GFP_TX		0x00000002
#define SF_ISR_GFP_RX		0x00000004
#define SF_ISR_TX_BADID_HIPRIO	0x00000008
#define SF_ISR_TX_BADID_LOPRIO	0x00000010
#define SF_ISR_NO_TX_CSUM	0x00000020
#define SF_ISR_RXDQ2_NOBUFS	0x00000040
#define SF_ISR_RXGFP_NORESP	0x00000080
#define SF_ISR_RXDQ1_DMADONE	0x00000100
#define SF_ISR_RXDQ2_DMADONE	0x00000200
#define SF_ISR_RXDQ1_EARLY	0x00000400
#define SF_ISR_RXDQ2_EARLY	0x00000800
#define SF_ISR_TX_QUEUEDONE	0x00001000
#define SF_ISR_TX_DMADONE	0x00002000
#define SF_ISR_TX_TXDONE	0x00004000
#define SF_ISR_NORMALINTR	0x00008000
#define SF_ISR_RXDQ1_NOBUFS	0x00010000
#define SF_ISR_RXCQ2_NOBUFS	0x00020000
#define SF_ISR_TX_LOFIFO	0x00040000
#define SF_ISR_DMAERR		0x00080000
#define SF_ISR_PCIINT		0x00100000
#define SF_ISR_TXCQ_NOBUFS	0x00200000
#define SF_ISR_RXCQ1_NOBUFS	0x00400000
#define SF_ISR_SOFTINTR		0x00800000
#define SF_ISR_GENTIMER		0x01000000
#define SF_ISR_ABNORMALINTR	0x02000000
#define SF_ISR_RSVD0		0x04000000
#define SF_ISR_STATSOFLOW	0x08000000
#define SF_ISR_GPIO		0xF0000000

/*
 * Shadow interrupt status register. Unlike the normal IRQ register,
 * reading bits here does not automatically cause them to reset.
 */
#define SF_SISR_PCIINT_ASSERTED	0x00000001
#define SF_SISR_GFP_TX		0x00000002
#define SF_SISR_GFP_RX		0x00000004
#define SF_SISR_TX_BADID_HIPRIO	0x00000008
#define SF_SISR_TX_BADID_LOPRIO	0x00000010
#define SF_SISR_NO_TX_CSUM	0x00000020
#define SF_SISR_RXDQ2_NOBUFS	0x00000040
#define SF_SISR_RXGFP_NORESP	0x00000080
#define SF_SISR_RXDQ1_DMADONE	0x00000100
#define SF_SISR_RXDQ2_DMADONE	0x00000200
#define SF_SISR_RXDQ1_EARLY	0x00000400
#define SF_SISR_RXDQ2_EARLY	0x00000800
#define SF_SISR_TX_QUEUEDONE	0x00001000
#define SF_SISR_TX_DMADONE	0x00002000
#define SF_SISR_TX_TXDONE	0x00004000
#define SF_SISR_NORMALINTR	0x00008000
#define SF_SISR_RXDQ1_NOBUFS	0x00010000
#define SF_SISR_RXCQ2_NOBUFS	0x00020000
#define SF_SISR_TX_LOFIFO	0x00040000
#define SF_SISR_DMAERR		0x00080000
#define SF_SISR_PCIINT		0x00100000
#define SF_SISR_TXCQ_NOBUFS	0x00200000
#define SF_SISR_RXCQ1_NOBUFS	0x00400000
#define SF_SISR_SOFTINTR	0x00800000
#define SF_SISR_GENTIMER	0x01000000
#define SF_SISR_ABNORMALINTR	0x02000000
#define SF_SISR_RSVD0		0x04000000
#define SF_SISR_STATSOFLOW	0x08000000
#define SF_SISR_GPIO		0xF0000000

/* Interrupt mask register */
#define SF_IMR_PCIINT_ASSERTED	0x00000001
#define SF_IMR_GFP_TX		0x00000002
#define SF_IMR_GFP_RX		0x00000004
#define SF_IMR_TX_BADID_HIPRIO	0x00000008
#define SF_IMR_TX_BADID_LOPRIO	0x00000010
#define SF_IMR_NO_TX_CSUM	0x00000020
#define SF_IMR_RXDQ2_NOBUFS	0x00000040
#define SF_IMR_RXGFP_NORESP	0x00000080
#define SF_IMR_RXDQ1_DMADONE	0x00000100
#define SF_IMR_RXDQ2_DMADONE	0x00000200
#define SF_IMR_RXDQ1_EARLY	0x00000400
#define SF_IMR_RXDQ2_EARLY	0x00000800
#define SF_IMR_TX_QUEUEDONE	0x00001000
#define SF_IMR_TX_DMADONE	0x00002000
#define SF_IMR_TX_TXDONE	0x00004000
#define SF_IMR_NORMALINTR	0x00008000
#define SF_IMR_RXDQ1_NOBUFS	0x00010000
#define SF_IMR_RXCQ2_NOBUFS	0x00020000
#define SF_IMR_TX_LOFIFO	0x00040000
#define SF_IMR_DMAERR		0x00080000
#define SF_IMR_PCIINT		0x00100000
#define SF_IMR_TXCQ_NOBUFS	0x00200000
#define SF_IMR_RXCQ1_NOBUFS	0x00400000
#define SF_IMR_SOFTINTR		0x00800000
#define SF_IMR_GENTIMER		0x01000000
#define SF_IMR_ABNORMALINTR	0x02000000
#define SF_IMR_RSVD0		0x04000000
#define SF_IMR_STATSOFLOW	0x08000000
#define SF_IMR_GPIO		0xF0000000

#define SF_INTRS	\
	(SF_IMR_RXDQ2_NOBUFS|SF_IMR_RXDQ1_DMADONE|SF_IMR_RXDQ2_DMADONE|	\
	 SF_IMR_TX_TXDONE|SF_IMR_RXDQ1_NOBUFS|SF_IMR_RXDQ2_DMADONE|	\
	 SF_IMR_NORMALINTR|SF_IMR_ABNORMALINTR|SF_IMR_TXCQ_NOBUFS|	\
	 SF_IMR_RXCQ1_NOBUFS|SF_IMR_RXCQ2_NOBUFS|SF_IMR_STATSOFLOW|	\
	 SF_IMR_TX_LOFIFO)

/* TX descriptor queue control registers */
#define SF_TXDQCTL_DESCTYPE	0x00000007
#define SF_TXDQCTL_NODMACMP	0x00000008
#define SF_TXDQCTL_MINSPACE	0x00000070
#define SF_TXDQCTL_64BITADDR	0x00000080
#define SF_TXDQCTL_BURSTLEN	0x00003F00
#define SF_TXDQCTL_SKIPLEN	0x001F0000
#define SF_TXDQCTL_HIPRIOTHRESH	0xFF000000

#define SF_TXBUFDESC_TYPE0	0x00000000
#define SF_TXBUFDESC_TYPE1	0x00000001
#define SF_TXBUFDESC_TYPE2	0x00000002
#define SF_TXBUFDESC_TYPE3	0x00000003
#define SF_TXBUFDESC_TYPE4	0x00000004

#define SF_TXMINSPACE_UNLIMIT	0x00000000
#define SF_TXMINSPACE_32BYTES	0x00000010
#define SF_TXMINSPACE_64BYTES	0x00000020
#define SF_TXMINSPACE_128BYTES	0x00000030
#define SF_TXMINSPACE_256BYTES	0x00000040

#define SF_TXSKIPLEN_0BYTES	0x00000000
#define SF_TXSKIPLEN_8BYTES	0x00010000
#define SF_TXSKIPLEN_16BYTES	0x00020000
#define SF_TXSKIPLEN_24BYTES	0x00030000
#define SF_TXSKIPLEN_32BYTES	0x00040000

/* TX frame control register */
#define SF_TXFRMCTL_TXTHRESH	0x000000FF
#define SF_TXFRMCTL_CPLAFTERTX	0x00000100
#define SF_TXFRMCRL_DEBUG	0x0000FE00
#define SF_TXFRMCTL_STATUS	0x01FF0000
#define SF_TXFRMCTL_MAC_TXIF	0xFE000000

/* TX completion queue control register */
#define SF_TXCQ_THRESH		0x0000000F
#define SF_TXCQ_COMMON		0x00000010
#define SF_TXCQ_SIZE		0x00000020
#define SF_TXCQ_WRITEENB	0x00000040
#define SF_TXCQ_USE_64BIT	0x00000080
#define SF_TXCQ_ADDR		0xFFFFFF00

/* RX completion queue control register */
#define SF_RXCQ_THRESH		0x0000000F
#define SF_RXCQ_TYPE		0x00000030
#define SF_RXCQ_WRITEENB	0x00000040
#define SF_RXCQ_USE_64BIT	0x00000080
#define SF_RXCQ_ADDR		0xFFFFFF00

#define SF_RXCQTYPE_0		0x00000000
#define SF_RXCQTYPE_1		0x00000010
#define SF_RXCQTYPE_2		0x00000020
#define SF_RXCQTYPE_3		0x00000030

/* TX descriptor queue producer index register */
#define SF_TXDQ_PRODIDX_LOPRIO	0x000007FF
#define SF_TXDQ_PRODIDX_HIPRIO	0x07FF0000

/* TX descriptor queue consumer index register */
#define SF_TXDQ_CONSIDX_LOPRIO	0x000007FF
#define SF_TXDQ_CONSIDX_HIPRIO	0x07FF0000

/* Completion queue consumer index register */
#define SF_CQ_CONSIDX_RXQ1	0x000003FF
#define SF_CQ_CONSIDX_RXTHRMODE	0x00008000
#define SF_CQ_CONSIDX_TXQ	0x03FF0000
#define SF_CQ_CONSIDX_TXTHRMODE	0x80000000

/* Completion queue producer index register */
#define SF_CQ_PRODIDX_RXQ1	0x000003FF
#define SF_CQ_PRODIDX_TXQ	0x03FF0000

/* RX completion queue 2 consumer/producer index register */
#define SF_CQ_RXQ2_CONSIDX	0x000003FF
#define SF_CQ_RXQ2_RXTHRMODE	0x00008000
#define SF_CQ_RXQ2_PRODIDX	0x03FF0000

#define SF_CQ_RXTHRMODE_INT_ON	0x00008000
#define SF_CQ_RXTHRMODE_INT_OFF	0x00000000
#define SF_CQ_TXTHRMODE_INT_ON	0x80000000
#define SF_CQ_TXTHRMODE_INT_OFF	0x00000000

#define SF_IDX_LO(x)		((x) & 0x000007FF)
#define SF_IDX_HI(x)		(((x) >> 16) & 0x000007FF)

/* RX DMA control register */
#define SF_RXDMA_BURSTSIZE	0x0000007F
#define SF_RXDMA_FPTESTMODE	0x00000080
#define SF_RXDMA_HIPRIOTHRESH	0x00000F00
#define SF_RXDMA_RXEARLYTHRESH	0x0001F000
#define SF_RXDMA_DMACRC		0x00040000
#define SF_RXDMA_USEBKUPQUEUE	0x00080000
#define SF_RXDMA_QUEUEMODE	0x00700000
#define SF_RXDMA_RXCQ2_ON	0x00800000
#define SF_RXDMA_CSUMMODE	0x03000000
#define SF_RXDMA_DMAPAUSEPKTS	0x04000000
#define SF_RXDMA_DMACTLPKTS	0x08000000
#define SF_RXDMA_DMACRXERRPKTS	0x10000000
#define SF_RXDMA_DMABADPKTS	0x20000000
#define SF_RXDMA_DMARUNTS	0x40000000
#define SF_RXDMA_REPORTBADPKTS	0x80000000

#define SF_RXDQMODE_Q1ONLY	0x00100000
#define SF_RXDQMODE_Q2_ON_FP	0x00200000
#define SF_RXDQMODE_Q2_ON_SHORT	0x00300000
#define SF_RXDQMODE_Q2_ON_PRIO	0x00400000
#define SF_RXDQMODE_SPLITHDR	0x00500000

#define SF_RXCSUMMODE_IGNORE	0x00000000
#define SF_RXCSUMMODE_REJECT_BAD_TCP	0x01000000
#define SF_RXCSUMMODE_REJECT_BAD_TCPUDP	0x02000000
#define SF_RXCSUMMODE_RSVD	0x03000000

/* RX descriptor queue control registers */
#define SF_RXDQCTL_MINDESCTHR	0x0000007F
#define SF_RXDQCTL_Q1_WE	0x00000080
#define SF_RXDQCTL_DESCSPACE	0x00000700
#define SF_RXDQCTL_64BITDADDR	0x00000800
#define SF_RXDQCTL_64BITBADDR	0x00001000
#define SF_RXDQCTL_VARIABLE	0x00002000
#define SF_RXDQCTL_ENTRIES	0x00004000
#define SF_RXDQCTL_PREFETCH	0x00008000
#define SF_RXDQCTL_BUFLEN	0xFFFF0000

#define SF_DESCSPACE_4BYTES	0x00000000
#define SF_DESCSPACE_8BYTES	0x00000100
#define SF_DESCSPACE_16BYTES	0x00000200
#define SF_DESCSPACE_32BYTES	0x00000300
#define SF_DESCSPACE_64BYTES	0x00000400
#define SF_DESCSPACE_128_BYTES	0x00000500

/* RX buffer consumer/producer index registers */
#define SF_RXDQ_PRODIDX		0x000007FF
#define SF_RXDQ_CONSIDX		0x07FF0000

/* RX filter control register */
#define SF_RXFILT_PROMISC	0x00000001
#define SF_RXFILT_ALLMULTI	0x00000002
#define SF_RXFILT_BROAD		0x00000004
#define SF_RXFILT_HASHPRIO	0x00000008
#define SF_RXFILT_HASHMODE	0x00000030
#define SF_RXFILT_PERFMODE	0x000000C0
#define SF_RXFILT_VLANMODE	0x00000300
#define SF_RXFILT_WAKEMODE	0x00000C00
#define SF_RXFILT_MULTI_NOBROAD	0x00001000
#define SF_RXFILT_MIN_VLANPRIO	0x0000E000
#define SF_RXFILT_PEFECTPRIO	0xFFFF0000

/* Hash filtering mode */
#define SF_HASHMODE_OFF		0x00000000
#define SF_HASHMODE_WITHVLAN	0x00000010
#define SF_HASHMODE_ANYVLAN	0x00000020
#define SF_HASHMODE_ANY		0x00000030

/* Perfect filtering mode */
#define SF_PERFMODE_OFF		0x00000000
#define SF_PERFMODE_NORMAL	0x00000040
#define SF_PERFMODE_INVERSE	0x00000080
#define SF_PERFMODE_VLAN	0x000000C0

/* VLAN mode */
#define SF_VLANMODE_OFF		0x00000000
#define SF_VLANMODE_NOSTRIP	0x00000100
#define SF_VLANMODE_STRIP	0x00000200
#define SF_VLANMODE_RSVD	0x00000300

/* Wakeup mode */
#define SF_WAKEMODE_OFF		0x00000000
#define SF_WAKEMODE_FILTER	0x00000400
#define SF_WAKEMODE_FP		0x00000800
#define SF_WAKEMODE_HIPRIO	0x00000C00

/*
 * Extra PCI registers 0x0100 to 0x0FFF
 */
#define SF_PCI_TARGSTAT		0x0100
#define SF_PCI_MASTSTAT1	0x0104
#define SF_PCI_MASTSTAT2	0x0108
#define SF_PCI_DMAHOSTADDR_LO	0x010C
#define SF_BAC_DMADIAG0		0x0110
#define SF_BAC_DMADIAG1		0x0114
#define SF_BAC_DMADIAG2		0x0118
#define SF_BAC_DMADIAG3		0x011C
#define SF_PAR0			0x0120
#define SF_PAR1			0x0124
#define SF_PCICB_FUNCEVENT	0x0130
#define SF_PCICB_FUNCEVENT_MASK	0x0134
#define SF_PCICB_FUNCSTATE	0x0138
#define SF_PCICB_FUNCFORCE	0x013C

/*
 * Serial EEPROM registers 0x1000 to 0x1FFF
 * Presumeably the EEPROM is mapped into this 8K window.
 */
#define SF_EEADDR_BASE		0x1000
#define SF_EEADDR_MAX		0x1FFF

#define SF_EE_NODEADDR		14

/*
 * MII registers registers 0x2000 to 0x3FFF
 * There are 32 sets of 32 registers, one set for each possible
 * PHY address. Each 32 bit register is split into a 16-bit data
 * port and a couple of status bits.
 */

#define SF_MIIADDR_BASE		0x2000
#define SF_MIIADDR_MAX		0x3FFF
#define SF_MII_BLOCKS		32

#define SF_MII_DATAVALID	0x80000000
#define SF_MII_BUSY		0x40000000
#define SF_MII_DATAPORT		0x0000FFFF

#define SF_PHY_REG(phy, reg)						\
	(SF_MIIADDR_BASE + (phy * SF_MII_BLOCKS * sizeof(u_int32_t)) +	\
	(reg * sizeof(u_int32_t)))

/*
 * Ethernet extra registers 0x4000 to 0x4FFF
 */
#define SF_TESTMODE		0x4000
#define SF_RX_FRAMEPROC_CTL	0x4004
#define SF_TX_FRAMEPROC_CTL	0x4008

/*
 * MAC registers 0x5000 to 0x5FFF
 */
#define SF_MACCFG_1		0x5000
#define SF_MACCFG_2		0x5004
#define SF_BKTOBKIPG		0x5008
#define SF_NONBKTOBKIPG		0x500C
#define SF_COLRETRY		0x5010
#define SF_MAXLEN		0x5014
#define SF_TXNIBBLECNT		0x5018
#define SF_TXBYTECNT		0x501C
#define SF_RETXCNT		0x5020
#define SF_RANDNUM		0x5024
#define SF_RANDNUM_MASK		0x5028
#define SF_TOTALTXCNT		0x5034
#define SF_RXBYTECNT		0x5040
#define SF_TXPAUSETIMER		0x5060
#define SF_VLANTYPE		0x5064
#define SF_MIISTATUS		0x5070

#define SF_MACCFG1_HUGEFRAMES	0x00000001
#define SF_MACCFG1_FULLDUPLEX	0x00000002
#define SF_MACCFG1_AUTOPAD	0x00000004
#define SF_MACCFG1_HDJAM	0x00000008
#define SF_MACCFG1_DELAYCRC	0x00000010
#define SF_MACCFG1_NOBACKOFF	0x00000020
#define SF_MACCFG1_LENGTHCHECK	0x00000040
#define SF_MACCFG1_PUREPREAMBLE	0x00000080
#define SF_MACCFG1_PASSALLRX	0x00000100
#define SF_MACCFG1_PREAM_DETCNT	0x00000200
#define SF_MACCFG1_RX_FLOWENB	0x00000400
#define SF_MACCFG1_TX_FLOWENB	0x00000800
#define SF_MACCFG1_TESTMODE	0x00003000
#define SF_MACCFG1_MIILOOPBK	0x00004000
#define SF_MACCFG1_SOFTRESET	0x00008000

/*
 * There are the recommended IPG nibble counter settings
 * specified in the Adaptec manual for full duplex and
 * half duplex operation.
 */
#define SF_IPGT_FDX		0x15
#define SF_IPGT_HDX		0x11

/*
 * RX filter registers 0x6000 to 0x6FFF
 */
#define SF_RXFILT_PERFECT_BASE	0x6000
#define SF_RXFILT_PERFECT_MAX	0x60FF
#define SF_RXFILT_PERFECT_SKIP	0x0010
#define SF_RXFILT_PERFECT_CNT	0x0010

#define SF_RXFILT_HASH_BASE	0x6100
#define SF_RXFILT_HASH_MAX	0x62FF
#define SF_RXFILT_HASH_SKIP	0x0010
#define SF_RXFILT_HASH_CNT	0x001F
#define SF_RXFILT_HASH_ADDROFF	0x0000
#define SF_RXFILT_HASH_PRIOOFF	0x0004
#define SF_RXFILT_HASH_VLANOFF	0x0008

/*
 * Statistics registers 0x7000 to 0x7FFF
 */
#define SF_STATS_BASE		0x7000
#define SF_STATS_END		0x7FFF

/*
 * TX frame processor instruction space 0x8000 to 0x9FFF
 */

/*
 * RX frame processor instruction space 0xA000 to 0xBFFF
 */

/*
 * Ethernet FIFO access space 0xC000 to 0xDFFF
 */

/*
 * Reserved 0xE000 to 0xFFFF
 */

/*
 * Descriptor data structures.
 */


/* Receive descriptor formats. */
#define SF_RX_MINSPACING	8
#define SF_RX_DLIST_CNT		256
#define SF_RX_CLIST_CNT		1024
#define SF_RX_HOSTADDR(x)	(((x) >> 2) & 0x3FFFFFFF)

/*
 * RX buffer descriptor type 0, 32-bit addressing. Note that we
 * program the RX buffer queue control register(s) to allow a
 * descriptor spacing of 16 bytes, which leaves room after each
 * descriptor to store a pointer to the mbuf for each buffer.
 */
struct sf_rx_bufdesc_type0 {
	u_int32_t		sf_valid:1,
				sf_end:1,
				sf_addrlo:30;
	u_int32_t		sf_pad0;
#ifdef __i386__
	u_int32_t		sf_pad1;
#endif
	struct mbuf		*sf_mbuf;
};

/*
 * RX buffer descriptor type 0, 64-bit addressing.
 */
struct sf_rx_bufdesc_type1 {
	u_int32_t		sf_valid:1,
				sf_end:1,
				sf_addrlo:30;
	u_int32_t		sf_addrhi;
#ifdef __i386__
	u_int32_t		sf_pad;
#endif
	struct mbuf		*sf_mbuf;
};

/*
 * RX completion descriptor, type 0 (short).
 */
struct sf_rx_cmpdesc_type0 {
	u_int32_t		sf_len:16,
				sf_endidx:11,
				sf_status1:3,
				sf_id:2;
};

/*
 * RX completion descriptor, type 1 (basic). Includes vlan ID
 * if this is a vlan-addressed packet, plus extended status.
 */
struct sf_rx_cmpdesc_type1 {
	u_int32_t		sf_len:16,
				sf_endidx:11,
				sf_status1:3,
				sf_id:2;
	u_int16_t		sf_status2;
	u_int16_t		sf_vlanid;
};

/*
 * RX completion descriptor, type 2 (checksum). Includes partial TCP/IP
 * checksum instead of vlan tag, plus extended status.
 */
struct sf_rx_cmpdesc_type2 {
	u_int32_t		sf_len:16,
				sf_endidx:11,
				sf_status1:3,
				sf_id:2;
	u_int16_t		sf_status2;
	u_int16_t		sf_cksum;
};

/*
 * RX completion descriptor type 3 (full). Includes timestamp, partial
 * TCP/IP checksum, vlan tag plus priority, two extended status fields.
 */
struct sf_rx_cmpdesc_type3 {
	u_int32_t		sf_len:16,
				sf_endidx:11,
				sf_status1:3,
				sf_id:2;
	u_int32_t		sf_startidx:10,
				sf_status3:6,
				sf_status2:16;
	u_int16_t		sf_cksum;
	u_int16_t		sf_vlanid_prio;
	u_int32_t		sf_timestamp;
};

#define SF_RXSTAT1_QUEUE	0x1
#define SF_RXSTAT1_FIFOFULL	0x2
#define SF_RXSTAT1_OK		0x4

					/* 0=unknown,5=unsupported */
#define SF_RXSTAT2_FRAMETYPE	0x0007	/* 1=IPv4,2=IPv2,3=IPX,4=ICMP */
#define SF_RXSTAT2_UDP		0x0008
#define SF_RXSTAT2_TCP		0x0010
#define SF_RXSTAT2_FRAG		0x0020
#define SF_RXSTAT2_PCSUM_OK	0x0040	/* partial checksum ok */
#define SF_RXSTAT2_CSUM_BAD	0x0080	/* TCP/IP checksum bad */
#define SF_RXSTAT2_CSUM_OK	0x0100	/* TCP/IP checksum ok */
#define SF_RXSTAT2_VLAN		0x0200
#define SF_RXSTAT2_BADRXCODE	0x0400
#define SF_RXSTAT2_DRIBBLE	0x0800
#define SF_RXSTAT2_ISL_CRCERR	0x1000
#define SF_RXSTAT2_CRCERR	0x2000
#define SF_RXSTAT2_HASH		0x4000
#define SF_RXSTAT2_PERFECT	0x8000

#define SF_RXSTAT3_TRAILER	0x01
#define SF_RXSTAT3_HEADER	0x02
#define SF_RXSTAT3_CONTROL	0x04
#define SF_RXSTAT3_PAUSE	0x08
#define SF_RXSTAT3_ISL		0x10

/*
 * Transmit descriptor formats.
 * Each transmit descriptor type allows for a skip field at the
 * start of each structure. The size of the skip field can vary,
 * however we always set it for 8 bytes, which is enough to hold
 * a pointer (32 bits on x86, 64-bits on alpha) that we can use
 * to hold the address of the head of the mbuf chain for the
 * frame or fragment associated with the descriptor. This saves
 * us from having to create a separate pointer array to hold
 * the mbuf addresses.
 */
#define SF_TX_BUFDESC_ID		0xB
#define SF_MAXFRAGS			14
#define SF_TX_MINSPACING		128
#define SF_TX_DLIST_CNT			128
#define SF_TX_DLIST_SIZE		16384
#define SF_TX_SKIPLEN			1
#define SF_TX_CLIST_CNT			1024

struct sf_frag {
	u_int32_t		sf_addr;
	u_int16_t		sf_fraglen;
	u_int16_t		sf_pktlen;
};

struct sf_frag_msdos {
	u_int16_t		sf_pktlen;
	u_int16_t		sf_fraglen;
	u_int32_t		sf_addr;
};

/*
 * TX frame descriptor type 0, 32-bit addressing. One descriptor can
 * be used to map multiple packet fragments. We use this format since
 * BSD networking fragments packet data across mbuf chains. Note that
 * the number of fragments can be variable depending on how the descriptor
 * spacing is specified in the TX descriptor queue control register.
 * We always use a spacing of 128 bytes, and a skipfield length of 8
 * bytes: this means 16 bytes for the descriptor, including the skipfield,
 * with 121 bytes left for fragment maps. Each fragment requires 8 bytes,
 * which allows for 14 fragments per descriptor. The total size of the
 * transmit buffer queue is limited to 16384 bytes, so with a spacing of
 * 128 bytes per descriptor, we have room for 128 descriptors in the queue.
 */
struct sf_tx_bufdesc_type0 {
#ifdef __i386__
	u_int32_t		sf_pad;
#endif
	struct mbuf		*sf_mbuf;
	u_int32_t		sf_rsvd0:24,
				sf_crcen:1,
				sf_caltcp:1,
				sf_end:1,
				sf_intr:1,
				sf_id:4;
	u_int8_t		sf_fragcnt;
	u_int8_t		sf_rsvd2;
	u_int16_t		sf_rsvd1;
	struct sf_frag		sf_frags[14];
};

/*
 * TX buffer descriptor type 1, 32-bit addressing. Each descriptor
 * maps a single fragment.
 */
struct sf_tx_bufdesc_type1 {
#ifdef __i386__
	u_int32_t		sf_pad;
#endif
	struct mbuf		*sf_mbuf;
	u_int32_t		sf_fraglen:16,
				sf_fragcnt:8,
				sf_crcen:1,
				sf_caltcp:1,
				sf_end:1,
				sf_intr:1,
				sf_id:4;
	u_int32_t		sf_addr;
};

/*
 * TX buffer descriptor type 2, 64-bit addressing. Each descriptor
 * maps a single fragment.
 */
struct sf_tx_bufdesc_type2 {
#ifdef __i386__
	u_int32_t		sf_pad;
#endif
	struct mbuf		*sf_mbuf;
	u_int32_t		sf_fraglen:16,
				sf_fragcnt:8,
				sf_crcen:1,
				sf_caltcp:1,
				sf_end:1,
				sf_intr:1,
				sf_id:4;
	u_int32_t		sf_addrlo;
	u_int32_t		sf_addrhi;
};

/* TX buffer descriptor type 3 is not defined. */

/*
 * TX frame descriptor type 4, 32-bit addressing. This is a special
 * case of the type 0 descriptor, identical except that the fragment
 * address and length fields are ordered differently. This is done
 * to optimize copies in MS-DOS and OS/2 drivers.
 */
struct sf_tx_bufdesc_type4 {
#ifdef __i386__
	u_int32_t		sf_pad;
#endif
	struct mbuf		*sf_mbuf;
	u_int32_t		sf_rsvd0:24,
				sf_crcen:1,
				sf_caltcp:1,
				sf_end:1,
				sf_intr:1,
				sf_id:4;
	u_int8_t		sf_fragcnt;
	u_int8_t		sf_rsvd2;
	u_int16_t		sf_rsvd1;
	struct sf_frag_msdos	sf_frags[14];
};

/*
 * Transmit completion queue descriptor formats.
 */

/*
 * Transmit DMA completion descriptor, type 0.
 */
#define SF_TXCMPTYPE_DMA	0x4
struct sf_tx_cmpdesc_type0 {
	u_int32_t		sf_index:15,
				sf_priority:1,
				sf_timestamp:13,
				sf_type:3;
};

/*
 * Transmit completion descriptor, type 1.
 */
#define SF_TXCMPTYPE_TX		0x5
struct sf_tx_cmpdesc_type1 {
	u_int32_t		sf_index:15,
				sf_priority:1,
				sf_txstat:13,
				sf_type:3;
};

#define SF_TXSTAT_CRCERR	0x0001
#define SF_TXSTAT_LENCHECKERR	0x0002
#define SF_TXSTAT_LENRANGEERR	0x0004
#define SF_TXSTAT_TX_OK		0x0008
#define SF_TXSTAT_TX_DEFERED	0x0010
#define SF_TXSTAT_EXCESS_DEFER	0x0020
#define SF_TXSTAT_EXCESS_COLL	0x0040
#define SF_TXSTAT_LATE_COLL	0x0080
#define SF_TXSTAT_TOOBIG	0x0100
#define SF_TXSTAT_TX_UNDERRUN	0x0200
#define SF_TXSTAT_CTLFRAME_OK	0x0400
#define SF_TXSTAT_PAUSEFRAME_OK	0x0800
#define SF_TXSTAT_PAUSED	0x1000

/* Statistics counters. */
struct sf_stats {
	u_int32_t		sf_tx_frames;
	u_int32_t		sf_tx_single_colls;
	u_int32_t		sf_tx_multi_colls;
	u_int32_t		sf_tx_crcerrs;
	u_int32_t		sf_tx_bytes;
	u_int32_t		sf_tx_defered;
	u_int32_t		sf_tx_late_colls;
	u_int32_t		sf_tx_pause_frames;
	u_int32_t		sf_tx_control_frames;
	u_int32_t		sf_tx_excess_colls;
	u_int32_t		sf_tx_excess_defer;
	u_int32_t		sf_tx_mcast_frames;
	u_int32_t		sf_tx_bcast_frames;
	u_int32_t		sf_tx_frames_lost;
	u_int32_t		sf_rx_rx_frames;
	u_int32_t		sf_rx_crcerrs;
	u_int32_t		sf_rx_alignerrs;
	u_int32_t		sf_rx_bytes;
	u_int32_t		sf_rx_control_frames;
	u_int32_t		sf_rx_unsup_control_frames;
	u_int32_t		sf_rx_giants;
	u_int32_t		sf_rx_runts;
	u_int32_t		sf_rx_jabbererrs;
	u_int32_t		sf_rx_pkts_64;
	u_int32_t		sf_rx_pkts_65_127;
	u_int32_t		sf_rx_pkts_128_255;
	u_int32_t		sf_rx_pkts_256_511;
	u_int32_t		sf_rx_pkts_512_1023;
	u_int32_t		sf_rx_pkts_1024_1518;
	u_int32_t		sf_rx_frames_lost;
	u_int16_t		sf_tx_underruns;
	u_int16_t		sf_pad;
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->sf_btag, sc->sf_bhandle, reg, val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->sf_btag, sc->sf_bhandle, reg)

#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->sf_btag, sc->sf_bhandle, reg)


struct sf_type {
	u_int16_t		sf_vid;
	u_int16_t		sf_did;
	char			*sf_name;
};

#define SF_INC(x, y)	(x) = (x + 1) % y

#define ETHER_ALIGN 2

/*
 * Note: alignment is important here: each list must be aligned to
 * a 256-byte boundary. It turns out that each ring is some multiple
 * of 4K in length, so we can stack them all on top of each other
 * and just worry about aligning the whole mess. There's one transmit
 * buffer ring and two receive buffer rings: one RX ring is for small
 * packets and the other is for large packets. Each buffer ring also
 * has a companion completion queue.
 */
struct sf_list_data {
	struct sf_tx_bufdesc_type0	sf_tx_dlist[SF_TX_DLIST_CNT];
	struct sf_tx_cmpdesc_type1	sf_tx_clist[SF_TX_CLIST_CNT];
	struct sf_rx_bufdesc_type0	sf_rx_dlist_big[SF_RX_DLIST_CNT];
#ifdef notdef
	/*
	 * Unfortunately, because the Starfire doesn't allow arbitrary
	 * byte alignment, we have to copy packets in the RX handler in
	 * order to align the payload correctly. This means that we
	 * don't gain anything by having separate large and small descriptor
	 * lists, so for now we don't bother with the small one.
	 */
	struct sf_rx_bufdesc_type0	sf_rx_dlist_small[SF_RX_DLIST_CNT];
#endif
	struct sf_rx_cmpdesc_type3	sf_rx_clist[SF_RX_CLIST_CNT];
};

struct sf_softc {
	struct arpcom		arpcom;		/* interface info */
	bus_space_handle_t	sf_bhandle;	/* bus space handle */
	bus_space_tag_t		sf_btag;	/* bus space tag */
	void			*sf_intrhand;	/* interrupt handler cookie */
	struct resource		*sf_irq;	/* irq resource descriptor */
	struct resource		*sf_res;	/* mem/ioport resource */
	struct sf_type		*sf_info;	/* Starfire adapter info */
	device_t		sf_miibus;
	u_int8_t		sf_unit;	/* interface number */
	struct sf_list_data	*sf_ldata;
	int			sf_tx_cnt;
	u_int8_t		sf_link;
	int			sf_if_flags;
	struct callout		sf_stat_timer;
};

#define SF_TIMEOUT	1000

#ifdef __alpha__
#undef vtophys
#define vtophys(va)		alpha_XXX_dmamap((vm_offset_t)va)
#endif
