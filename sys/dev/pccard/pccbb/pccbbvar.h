/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/pccbb/pccbbvar.h,v 1.15 2002/10/07 23:11:29 imp Exp $
 * $DragonFly: src/sys/dev/pccard/pccbb/pccbbvar.h,v 1.1 2004/02/10 07:55:47 joerg Exp $
 */

/*
 * Structure definitions for the Cardbus Bridge driver
 */

struct cbb_intrhand {
	driver_intr_t *intr;
	void	*arg;
	STAILQ_ENTRY(cbb_intrhand) entries;
};

struct cbb_reslist {
	SLIST_ENTRY(cbb_reslist) link;
	struct	resource *res;
	int	type;
	int	rid;
		/* note: unlike the regular resource list, there can be
		 * duplicate rid's in the same list.  However, the
		 * combination of rid and res->r_dev should be unique.
		 */
	bus_addr_t cardaddr; /* for 16-bit pccard memory */
};

#define	CBB_AUTO_OPEN_SMALLHOLE 0x100

struct cbb_softc {
	device_t	dev;
	struct exca_softc exca;
	struct		resource *base_res;
	struct		resource *irq_res;
	void		*intrhand;
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t	secbus;
	u_int8_t	subbus;
	struct lock	lock;
	u_int32_t	flags;
#define CBB_CARD_OK		0x08000000
#define	CBB_KLUDGE_ALLOC	0x10000000
#define	CBB_16BIT_CARD		0x20000000
#define	CBB_KTHREAD_RUNNING	0x40000000
#define	CBB_KTHREAD_DONE	0x80000000
	int		chipset;		/* chipset id */
#define	CB_UNKNOWN	0		/* NOT Cardbus-PCI bridge */
#define	CB_TI113X	1		/* TI PCI1130/1131 */
#define	CB_TI12XX	2		/* TI PCI12xx/14xx/44xx/15xx/45xx */
#define	CB_TI125X	3		/* TI PCI1250/1251(B)/1450 */
#define	CB_RF5C47X	4		/* RICOH RF5C475/476/477 */
#define	CB_RF5C46X	5		/* RICOH RF5C465/466/467 */
#define	CB_CIRRUS	6		/* Cirrus Logic CLPD683x */
#define	CB_TOPIC95	7		/* Toshiba ToPIC95 */
#define	CB_TOPIC97	8		/* Toshiba ToPIC97/100 */
	SLIST_HEAD(, cbb_reslist) rl;
	STAILQ_HEAD(, cbb_intrhand) intr_handlers;

	device_t	cbdev;
	device_t	pccarddev;

	struct thread	*event_thread;
};

/* result of detect_card */
#define	CARD_UKN_CARD	0x00
#define	CARD_5V_CARD	0x01
#define	CARD_3V_CARD	0x02
#define	CARD_XV_CARD	0x04
#define	CARD_YV_CARD	0x08

/* for power_socket */
#define	CARD_VCC_UC	0x0000
#define	CARD_VCC_3V	0x0001
#define	CARD_VCC_XV	0x0002
#define	CARD_VCC_YV	0x0003
#define	CARD_VCC_0V	0x0004
#define	CARD_VCC_5V	0x0005
#define	CARD_VCCMASK	0x000f
#define	CARD_VPP_UC	0x0000
#define	CARD_VPP_VCC	0x0010
#define	CARD_VPP_12V	0x0030
#define	CARD_VPP_0V	0x0040
#define	CARD_VPPMASK	0x00f0
