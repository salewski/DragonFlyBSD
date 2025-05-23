/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_ifpnp_l1.c - AVM Fritz PnP layer 1 handler
 *	----------------------------------------------
 *
 *	$Id: i4b_ifpnp_l1.c,v 1.4 2000/06/02 16:14:36 hm Exp $ 
 *	$Ust: src/i4b/layer1-nb/ifpnp/i4b_ifpnp_l1.c,v 1.4 2000/04/18 08:03:05 ust Exp $ 
 *
 * $FreeBSD: src/sys/i4b/layer1/ifpnp/i4b_ifpnp_l1.c,v 1.4.2.1 2001/08/10 14:08:37 obrien Exp $
 * $DragonFly: src/sys/net/i4b/layer1/ifpnp/i4b_ifpnp_l1.c,v 1.3 2003/08/07 21:17:26 dillon Exp $
 *
 *      last edit-date: [Fri Jun  2 14:55:49 2000]
 *
 *---------------------------------------------------------------------------*/

#include "use_ifpnp.h"

#if (NIFPNP > 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>


#include <net/if.h>

#include <net/i4b/include/machine/i4b_debug.h>
#include <net/i4b/include/machine/i4b_ioctl.h>
#include <net/i4b/include/machine/i4b_trace.h>

#include "../isic/i4b_isic.h"
#include "../isic/i4b_isac.h"

#include "i4b_ifpnp_ext.h"

#include "../i4b_l1.h"

#include "../../include/i4b_mbuf.h"
#include "../../include/i4b_global.h"

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-DATA-REQUEST
 *	=========================
 *
 *	parms:
 *		unit		physical interface unit number
 *		m		mbuf containing L2 frame to be sent out
 *		freeflag	MBUF_FREE: free mbuf here after having sent
 *						it out
 *				MBUF_DONTFREE: mbuf is freed by Layer 2
 *	returns:
 *		==0	fail, nothing sent out
 *		!=0	ok, frame sent out
 *
 *---------------------------------------------------------------------------*/
int
ifpnp_ph_data_req(int unit, struct mbuf *m, int freeflag)
{
	u_char cmd;
	int s;
	struct l1_softc *sc = ifpnp_scp[unit];

#ifdef NOTDEF
	NDBGL1(L1_PRIM, "PH-DATA-REQ, unit %d, freeflag=%d", unit, freeflag);
#endif

	if(m == NULL)			/* failsafe */
		return (0);

	s = SPLI4B();

	if(sc->sc_I430state == ST_F3)	/* layer 1 not running ? */
	{
		NDBGL1(L1_I_ERR, "still in state F3!");
		ifpnp_ph_activate_req(unit);
	}

	if(sc->sc_state & ISAC_TX_ACTIVE)
	{
		if(sc->sc_obuf2 == NULL)
		{
			sc->sc_obuf2 = m;		/* save mbuf ptr */

			if(freeflag)
				sc->sc_freeflag2 = 1;	/* IRQ must mfree */
			else
				sc->sc_freeflag2 = 0;	/* IRQ must not mfree */

			NDBGL1(L1_I_MSG, "using 2nd ISAC TX buffer, state = %s", ifpnp_printstate(sc));

			if(sc->sc_trace & TRACE_D_TX)
			{
				i4b_trace_hdr_t hdr;
				hdr.unit = L0IFPNPUNIT(unit);
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_TE;
				hdr.count = ++sc->sc_trace_dcount;
				MICROTIME(hdr.time);
				i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
			}
			splx(s);
			return(1);
		}

		NDBGL1(L1_I_ERR, "No Space in TX FIFO, state = %s", ifpnp_printstate(sc));
	
		if(freeflag == MBUF_FREE)
			i4b_Dfreembuf(m);			
	
		splx(s);
		return (0);
	}

	if(sc->sc_trace & TRACE_D_TX)
	{
		i4b_trace_hdr_t hdr;
		hdr.unit = L0IFPNPUNIT(unit);
		hdr.type = TRC_CH_D;
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_dcount;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, m->m_len, m->m_data);
	}
	
	sc->sc_state |= ISAC_TX_ACTIVE;	/* set transmitter busy flag */

	NDBGL1(L1_I_MSG, "ISAC_TX_ACTIVE set");

	sc->sc_freeflag = 0;		/* IRQ must NOT mfree */
	
	ISAC_WRFIFO(m->m_data, min(m->m_len, ISAC_FIFO_LEN)); /* output to TX fifo */

	if(m->m_len > ISAC_FIFO_LEN)	/* message > 32 bytes ? */
	{
		sc->sc_obuf = m;	/* save mbuf ptr */
		sc->sc_op = m->m_data + ISAC_FIFO_LEN; 	/* ptr for irq hdl */
		sc->sc_ol = m->m_len - ISAC_FIFO_LEN;	/* length for irq hdl */

		if(freeflag)
			sc->sc_freeflag = 1;	/* IRQ must mfree */
		
		cmd = ISAC_CMDR_XTF;
	}
	else
	{
		sc->sc_obuf = NULL;
		sc->sc_op = NULL;
		sc->sc_ol = 0;

		if(freeflag)
			i4b_Dfreembuf(m);

		cmd = ISAC_CMDR_XTF | ISAC_CMDR_XME;
  	}

	ISAC_WRITE(I_CMDR, cmd);
	ISACCMDRWRDELAY();

	splx(s);
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-ACTIVATE-REQUEST
 *	=============================
 *
 *	parms:
 *		unit	physical interface unit number
 *
 *	returns:
 *		==0	
 *		!=0	
 *
 *---------------------------------------------------------------------------*/
int
ifpnp_ph_activate_req(int unit)
{
	struct l1_softc *sc = ifpnp_scp[unit];
	NDBGL1(L1_PRIM, "PH-ACTIVATE-REQ, unit %d\n", unit);
	ifpnp_next_state(sc, EV_PHAR);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	command from the upper layers
 *---------------------------------------------------------------------------*/
int
ifpnp_mph_command_req(int unit, int command, void *parm)
{
	struct l1_softc *sc = ifpnp_scp[unit];

	switch(command)
	{
		case CMR_DOPEN:		/* daemon running */
			NDBGL1(L1_PRIM, "unit %d, command = CMR_DOPEN", unit);
			sc->sc_enabled = 1;			
			break;
			
		case CMR_DCLOSE:	/* daemon not running */
			NDBGL1(L1_PRIM, "unit %d, command = CMR_DCLOSE", unit);
			sc->sc_enabled = 0;
			break;

		case CMR_SETTRACE:
			NDBGL1(L1_PRIM, "unit %d, command = CMR_SETTRACE, parm = %d", unit, (unsigned int)parm);
			sc->sc_trace = (unsigned int)parm;
			break;
		
		default:
			NDBGL1(L1_ERROR, "ERROR, unknown command = %d, unit = %d, parm = %d", command, unit, (unsigned int)parm);
			break;
	}

	return(0);
}

#endif /* NIFPNP > 0 */
