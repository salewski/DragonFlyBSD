/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/if_mib.c,v 1.8.2.1 2000/08/03 00:09:34 ps Exp $
 * $DragonFly: src/sys/net/if_mib.c,v 1.4.2.1 2005/05/23 18:33:50 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_mib.h>

/*
 * A sysctl(3) MIB for generic interface information.  This information
 * is exported in the net.link.generic branch, which has the following
 * structure:
 *
 * net.link.generic	.system			- system-wide control variables
 *						  and statistics (node)
 *			.ifdata.<ifindex>.general
 *						- what's in `struct ifdata'
 *						  plus some other info
 *			.ifdata.<ifindex>.linkspecific
 *						- a link-type-specific data
 *						  structure (as might be used
 *						  by an SNMP agent
 *
 * Perhaps someday we will make addresses accessible via this interface
 * as well (then there will be four such...).  The reason that the
 * index comes before the last element in the name is because it
 * seems more orthogonal that way, particularly with the possibility
 * of other per-interface data living down here as well (e.g., integrated
 * services stuff).
 */

SYSCTL_DECL(_net_link_generic);
SYSCTL_NODE(_net_link_generic, IFMIB_SYSTEM, system, CTLFLAG_RW, 0,
	    "Variables global to all interfaces");
SYSCTL_INT(_net_link_generic_system, IFMIB_IFCOUNT, ifcount, CTLFLAG_RD,
	   &if_index, 0, "Number of configured interfaces");

static int
sysctl_ifdata(SYSCTL_HANDLER_ARGS) /* XXX bad syntax! */
{
	int *name = (int *)arg1;
	int error, s;
	u_int namelen = arg2;
	struct ifnet *ifp;
	struct ifmibdata ifmd;

	if (namelen != 2)
		return EINVAL;

	s = splnet();
	if (name[0] <= 0 || name[0] > if_index ||
	    ifnet_addrs[name[0] - 1] == NULL) {
		splx(s);
		return ENOENT;
	}
	
	ifp = ifnet_addrs[name[0] - 1]->ifa_ifp;
	splx(s);

	switch(name[1]) {
	default:
		return ENOENT;

	case IFDATA_GENERAL:
		bzero(&ifmd, sizeof(ifmd));
		strlcpy(ifmd.ifmd_name, ifp->if_xname, sizeof(ifmd.ifmd_name));

#define COPY(fld) ifmd.ifmd_##fld = ifp->if_##fld
		COPY(pcount);
		COPY(flags);
		COPY(data);
#undef COPY
		ifmd.ifmd_snd_len = ifp->if_snd.ifq_len;
		ifmd.ifmd_snd_maxlen = ifp->if_snd.ifq_maxlen;
		ifmd.ifmd_snd_drops = ifp->if_snd.ifq_drops;

		error = SYSCTL_OUT(req, &ifmd, sizeof ifmd);
		if (error || !req->newptr)
			return error;

		error = SYSCTL_IN(req, &ifmd, sizeof ifmd);
		if (error)
			return error;

#define DONTCOPY(fld) ifmd.ifmd_data.ifi_##fld = ifp->if_data.ifi_##fld
		DONTCOPY(type);
		DONTCOPY(physical);
		DONTCOPY(addrlen);
		DONTCOPY(hdrlen);
		DONTCOPY(mtu);
		DONTCOPY(metric);
		DONTCOPY(baudrate);
#undef DONTCOPY
#define COPY(fld) ifp->if_##fld = ifmd.ifmd_##fld
		COPY(data);
		ifp->if_snd.ifq_maxlen = ifmd.ifmd_snd_maxlen;
		ifp->if_snd.ifq_drops = ifmd.ifmd_snd_drops;
#undef COPY
		break;

	case IFDATA_LINKSPECIFIC:
		error = SYSCTL_OUT(req, ifp->if_linkmib, ifp->if_linkmiblen);
		if (error || !req->newptr)
			return error;

		error = SYSCTL_IN(req, ifp->if_linkmib, ifp->if_linkmiblen);
		if (error)
			return error;
		
	}
	return 0;
}

SYSCTL_NODE(_net_link_generic, IFMIB_IFDATA, ifdata, CTLFLAG_RW,
	    sysctl_ifdata, "Interface table");

