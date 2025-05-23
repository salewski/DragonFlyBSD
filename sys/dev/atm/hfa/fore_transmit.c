/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_transmit.c,v 1.4 1999/08/28 00:41:53 peter Exp $
 *	@(#) $DragonFly: src/sys/dev/atm/hfa/fore_transmit.c,v 1.4 2005/02/01 00:51:50 joerg Exp $
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Transmit queue management
 *
 */

#include "fore_include.h"

/*
 * Allocate Transmit Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	0		allocations successful
 *	else		allocation failed
 */
int
fore_xmit_allocate(fup)
	Fore_unit	*fup;
{
	void		*memp;
	H_xmit_queue	*hxp;
	int		i;

	/*
	 * Allocate non-cacheable memory for transmit status words
	 */
	memp = atm_dev_alloc(sizeof(Q_status) * XMIT_QUELEN,
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_xmit_stat = (Q_status *) memp;

	memp = DMA_GET_ADDR(fup->fu_xmit_stat, sizeof(Q_status) * XMIT_QUELEN,
			QSTAT_ALIGN, ATM_DEV_NONCACHE);
	if (memp == NULL) {
		return (1);
	}
	fup->fu_xmit_statd = (Q_status *) memp;

	/*
	 * Allocate memory for transmit descriptors
	 *
	 * We will allocate the transmit descriptors individually rather than 
	 * as a single memory block, which will often be larger than a memory
	 * page.  On some systems (eg. FreeBSD) the physical addresses of 
	 * adjacent virtual memory pages are not contiguous.
	 */
	hxp = fup->fu_xmit_q;
	for (i = 0; i < XMIT_QUELEN; i++, hxp++) {

		/*
		 * Allocate a transmit descriptor for this queue entry
		 */
		hxp->hxq_descr = atm_dev_alloc(sizeof(Xmit_descr),
			XMIT_DESCR_ALIGN, 0);
		if (hxp->hxq_descr == NULL) {
			return (1);
		}

		hxp->hxq_descr_dma = DMA_GET_ADDR(hxp->hxq_descr,
			sizeof(Xmit_descr), XMIT_DESCR_ALIGN, 0);
		if (hxp->hxq_descr_dma == NULL) {
			return (1);
		}
	}

	return (0);
}


/*
 * Transmit Queue Initialization
 *
 * Allocate and initialize the host-resident transmit queue structures
 * and then initialize the CP-resident queue structures.
 * 
 * Called at interrupt level.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_xmit_initialize(fup)
	Fore_unit	*fup;
{
	Aali		*aap = fup->fu_aali;
	Xmit_queue	*cqp;
	H_xmit_queue	*hxp;
	Q_status	*qsp;
	Q_status	*qsp_dma;
	int		i;

	/*
	 * Point to CP-resident transmit queue
	 */
	cqp = (Xmit_queue *)(fup->fu_ram + CP_READ(aap->aali_xmit_q));

	/*
	 * Point to host-resident transmit queue structures
	 */
	hxp = fup->fu_xmit_q;
	qsp = fup->fu_xmit_stat;
	qsp_dma = fup->fu_xmit_statd;

	/*
	 * Loop thru all queue entries and do whatever needs doing
	 */
	for (i = 0; i < XMIT_QUELEN; i++) {

		/*
		 * Set queue status word to free
		 */
		*qsp = QSTAT_FREE;

		/*
		 * Set up host queue entry and link into ring
		 */
		hxp->hxq_cpelem = cqp;
		hxp->hxq_status = qsp;
		if (i == (XMIT_QUELEN - 1))
			hxp->hxq_next = fup->fu_xmit_q;
		else
			hxp->hxq_next = hxp + 1;

		/*
		 * Now let the CP into the game
		 */
		cqp->cq_status = (CP_dma) CP_WRITE(qsp_dma);

		/*
		 * Bump all queue pointers
		 */
		hxp++;
		qsp++;
		qsp_dma++;
		cqp++;
	}

	/*
	 * Initialize queue pointers
	 */
	fup->fu_xmit_head = fup->fu_xmit_tail = fup->fu_xmit_q;

	return;
}


/*
 * Drain Transmit Queue
 *
 * This function will free all completed entries at the head of the
 * transmit queue.  Freeing the entry includes releasing the transmit
 * buffers (buffer chain) back to the kernel.  
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_xmit_drain(fup)
	Fore_unit	*fup;
{
	H_xmit_queue	*hxp;
	H_dma		*sdmap;
	Fore_vcc	*fvp;
	struct vccb	*vcp;
	KBuffer		*m;

	/*
	 * Process each completed entry
	 */
	while (*fup->fu_xmit_head->hxq_status & QSTAT_COMPLETED) {

		hxp = fup->fu_xmit_head;

		/*
		 * Release the entry's DMA addresses and buffer chain
		 */
		for (m = hxp->hxq_buf, sdmap = hxp->hxq_dma; m;
				m = KB_NEXT(m), sdmap++) {
			caddr_t		cp;

			KB_DATASTART(m, cp, caddr_t);
			DMA_FREE_ADDR(cp, *sdmap, KB_LEN(m), 0);
		}
		KB_FREEALL(hxp->hxq_buf);

		/*
		 * Get VCC over which data was sent (may be null if
		 * VCC has been closed in the meantime)
		 */
		fvp = hxp->hxq_vcc;

		/*
		 * Now collect some statistics
		 */
		if (*hxp->hxq_status & QSTAT_ERROR) {
			/*
			 * CP ran into problems, not much we can do
			 * other than record the event
			 */
			fup->fu_pif.pif_oerrors++;
			if (fvp) {
				vcp = fvp->fv_connvc->cvc_vcc;
				vcp->vc_oerrors++;
				if (vcp->vc_nif)
					vcp->vc_nif->nif_if.if_oerrors++;
			}
		} else {
			/*
			 * Good transmission
			 */
			int	len = XDS_GET_LEN(hxp->hxq_descr->xd_spec);

			fup->fu_pif.pif_opdus++;
			fup->fu_pif.pif_obytes += len;
			if (fvp) {
				vcp = fvp->fv_connvc->cvc_vcc;
				vcp->vc_opdus++;
				vcp->vc_obytes += len;
				if (vcp->vc_nif) {
					vcp->vc_nif->nif_obytes += len;
					vcp->vc_nif->nif_if.if_opackets++;
					vcp->vc_nif->nif_if.if_obytes += len;
				}
			}
		}

		/*
		 * Mark this entry free for use and bump head pointer
		 * to the next entry in the queue
		 */
		*hxp->hxq_status = QSTAT_FREE;
		fup->fu_xmit_head = hxp->hxq_next;
	}

	return;
}


/*
 * Free Transmit Queue Data Structures
 *
 * Arguments:
 *	fup		pointer to device unit structure
 *
 * Returns:
 *	none
 */
void
fore_xmit_free(fup)
	Fore_unit	*fup;
{
	H_xmit_queue	*hxp;
	H_dma		*sdmap;
	KBuffer		*m;
	int		i;

	/*
	 * Free any transmit buffers left on the queue
	 */
	if (fup->fu_flags & CUF_INITED) {
		while (*fup->fu_xmit_head->hxq_status != QSTAT_FREE) {

			hxp = fup->fu_xmit_head;

			/*
			 * Release the entry's DMA addresses and buffer chain
			 */
			for (m = hxp->hxq_buf, sdmap = hxp->hxq_dma; m;
					m = KB_NEXT(m), sdmap++) {
				caddr_t		cp;

				KB_DATASTART(m, cp, caddr_t);
				DMA_FREE_ADDR(cp, *sdmap, KB_LEN(m), 0);
			}
			KB_FREEALL(hxp->hxq_buf);

			*hxp->hxq_status = QSTAT_FREE;
			fup->fu_xmit_head = hxp->hxq_next;
		}
	}

	/*
	 * Free the status words
	 */
	if (fup->fu_xmit_stat) {
		if (fup->fu_xmit_statd) {
			DMA_FREE_ADDR(fup->fu_xmit_stat, fup->fu_xmit_statd,
				sizeof(Q_status) * XMIT_QUELEN,
				ATM_DEV_NONCACHE);
		}
		atm_dev_free((volatile void *)fup->fu_xmit_stat);
		fup->fu_xmit_stat = NULL;
		fup->fu_xmit_statd = NULL;
	}

	/*
	 * Free the transmit descriptors
	 */
	hxp = fup->fu_xmit_q;
	for (i = 0; i < XMIT_QUELEN; i++, hxp++) {

		/*
		 * Free the transmit descriptor for this queue entry
		 */
		if (hxp->hxq_descr_dma) {
			DMA_FREE_ADDR(hxp->hxq_descr, hxp->hxq_descr_dma,
				sizeof(Xmit_descr), 0);
			hxp->hxq_descr_dma = NULL;
		}

		if (hxp->hxq_descr) {
			atm_dev_free(hxp->hxq_descr);
			hxp->hxq_descr = NULL;
		}
	}

	return;
}

