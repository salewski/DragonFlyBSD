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
 *	@(#) $FreeBSD: src/sys/netatm/uni/unisig_proto.c,v 1.5 2000/01/17 20:49:57 mks Exp $
 *	@(#) $DragonFly: src/sys/netproto/atm/uni/unisig_proto.c,v 1.4 2003/08/07 21:54:34 dillon Exp $
 */

/*
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Protocol processing module.
 *
 */

#include <netproto/atm/kern_include.h>

#include "unisig_var.h"

/*
 * Process a UNISIG timeout
 *
 * Called when a previously scheduled protocol instance control block
 * timer expires.  This routine passes a timeout event to the UNISIG
 * signalling manager state machine.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to UNISIG timer control block
 *
 * Returns:
 *	none
 *
 */
void
unisig_timer(tip)
	struct atm_time	*tip;
{
	struct unisig	*usp;

	/*
	 * Back-off to UNISIG control block
	 */
	usp = (struct unisig *)
		((caddr_t)tip - (int)(&((struct unisig *)0)->us_time));

	ATM_DEBUG2("unisig_timer: usp=%p,state=%d\n",
			usp, usp->us_state);

	/*
	 * Pass the timeout to the signalling manager state machine
	 */
	(void) unisig_sigmgr_state(usp,
			UNISIG_SIGMGR_TIMEOUT,
			(KBuffer *) 0);
}


/*
 * Process a UNISIG VCC timeout
 *
 * Called when a previously scheduled UNISIG VCCB timer expires.
 * Processing will based on the current VCC state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to vccb timer control block
 *
 * Returns:
 *	none
 *
 */
void
unisig_vctimer(tip)
	struct atm_time	*tip;
{
	struct unisig		*usp;
	struct unisig_vccb	*uvp;

	/*
	 * Get VCCB and UNISIG control block addresses
	 */
	uvp = (struct unisig_vccb *) ((caddr_t)tip -
			(int)(&((struct vccb *)0)->vc_time));
	usp = (struct unisig *)uvp->uv_pif->pif_siginst;

	ATM_DEBUG3("unisig_vctimer: uvp=%p, sstate=%d, ustate=%d\n",
			uvp, uvp->uv_sstate, uvp->uv_ustate);

	/*
	 * Hand the timeout to the VC finite state machine
	 */
	if (uvp->uv_ustate == VCCU_ABORT) {
		/*
		 * If we're aborting, this is an ABORT call
		 */
		(void) unisig_vc_state(usp, uvp, UNI_VC_ABORT_CALL,
				(struct unisig_msg *) 0);
	} else {
		/*
		 * If we're not aborting, it's a timeout
		 */
		(void) unisig_vc_state(usp, uvp, UNI_VC_TIMEOUT,
				(struct unisig_msg *) 0);
	}
}


/*
 * UNISIG SAAL Control Handler
 *
 * This is the module which receives data on the UNISIG signalling
 * channel.  Processing is based on the indication received from the
 * SSCF and the protocol state.
 *
 * Arguments:
 *	cmd	command code
 *	tok	session token (pointer to UNISIG protocol control block)
 *	a1	argument 1
 *
 * Returns:
 *	none
 *
 */
void
unisig_saal_ctl(cmd, tok, a1)
	int		cmd;
	void		*tok;
	void		*a1;
{
	struct unisig	*usp = tok;

	ATM_DEBUG4("unisig_upper: usp=%p,state=%d,cmd=%d,a1=0x%lx,\n",
			usp, usp->us_state, cmd, (u_long)a1);

	/*
	 * Process command
	 */
	switch (cmd) {

	case SSCF_UNI_ESTABLISH_IND:
		(void) unisig_sigmgr_state(usp,
				UNISIG_SIGMGR_SSCF_EST_IND,
				(KBuffer *) 0);
		break;

	case SSCF_UNI_ESTABLISH_CNF:
		(void) unisig_sigmgr_state(usp,
				UNISIG_SIGMGR_SSCF_EST_CNF,
				(KBuffer *) 0);
		break;

	case SSCF_UNI_RELEASE_IND:
		(void) unisig_sigmgr_state(usp,
				UNISIG_SIGMGR_SSCF_RLS_IND,
				(KBuffer *) 0);
		break;

	case SSCF_UNI_RELEASE_CNF:
		(void) unisig_sigmgr_state(usp,
				UNISIG_SIGMGR_SSCF_RLS_CNF,
				(KBuffer *) 0);
		break;

	default:
		log(LOG_ERR,
			"unisig: unknown SAAL cmd: usp=%p, state=%d, cmd=%d\n",
			usp, usp->us_state, cmd);
	}
}


/*
 * UNISIG SAAL Data Handler
 *
 * This is the module which receives data on the UNISIG signalling
 * channel.  Processing is based on the protocol state.
 *
 * Arguments:
 *	tok	session token (pointer to UNISIG protocol control block)
 *	m	pointer to data
 *
 * Returns:
 *	none
 *
 */
void
unisig_saal_data(tok, m)
	void		*tok;
	KBuffer		*m;
{
	struct unisig	*usp = tok;

	ATM_DEBUG3("unisig_saal_data: usp=%p,state=%d,m=%p,\n",
			usp, usp->us_state, m);

	/*
	 * Pass data to signalling manager state machine
	 */
	(void) unisig_sigmgr_state(usp,
			UNISIG_SIGMGR_SSCF_DATA_IND,
			m);
}


/*
 * Get Connection's Application/Owner Name
 *
 * Arguments:
 *      tok     UNI signalling connection token (pointer to protocol instance)
 *
 * Returns:
 *      addr    pointer to string containing our name
 *
 */
caddr_t
unisig_getname(tok)
        void            *tok;
{
	struct unisig	*usp = tok;

	if (usp->us_proto == ATM_SIG_UNI30)
		return ("UNI3.0");
	else if (usp->us_proto == ATM_SIG_UNI31)
		return ("UNI3.1");
	else if (usp->us_proto == ATM_SIG_UNI40)
		return ("UNI4.0");
	else
		return ("UNI");
}


/*
 * Process a VCC connection notification
 *
 * Should never be called.
 *
 * Arguments:
 *	tok	user's connection token (unisig protocol block)
 *
 * Returns:
 *	none
 *
 */
void
unisig_connected(tok)
	void		*tok;
{
	struct unisig		*usp = tok;

	ATM_DEBUG2("unisig_connected: usp=%p,state=%d\n",
			usp, usp->us_state);

	/*
	 * Connected routine shouldn't ever get called for a PVC
	 */
	log(LOG_ERR, "unisig: connected notification, usp=%p\n",
			usp);
}


/*
 * Process a VCC closed notification
 *
 * Called when UNISIG signalling channel is closed.
 *
 * Arguments:
 *	tok	user's connection token (unisig protocol block)
 *	cp	pointer to cause structure
 *
 * Returns:
 *	none
 *
 */
void
unisig_cleared(tok, cp)
	void			*tok;
	struct t_atm_cause	*cp;
{
	struct unisig		*usp = tok;

	ATM_DEBUG3("unisig_cleared: usp=%p, state=%d, cause=%d\n",
			usp, usp->us_state, cp->cause_value);

	/*
	 * VCC has been closed.  Notify the signalling
	 * manager state machine.
	 */
	(void) unisig_sigmgr_state(usp,
			UNISIG_SIGMGR_CALL_CLEARED,
			(KBuffer *) 0);
}
