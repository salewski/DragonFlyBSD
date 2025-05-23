/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: client.c,v 1.176.2.16 2004/07/23 02:56:59 marka Exp $ */

#include <config.h>

#include <isc/formatcheck.h>
#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/print.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dispatch.h>
#include <dns/events.h>
#include <dns/message.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/tsig.h>
#include <dns/view.h>
#include <dns/zone.h>

#include <named/interfacemgr.h>
#include <named/log.h>
#include <named/notify.h>
#include <named/server.h>
#include <named/update.h>

/***
 *** Client
 ***/

/*
 * Important note!
 *
 * All client state changes, other than that from idle to listening, occur
 * as a result of events.  This guarantees serialization and avoids the
 * need for locking.
 *
 * If a routine is ever created that allows someone other than the client's
 * task to change the client, then the client will have to be locked.
 */

#define NS_CLIENT_TRACE
#ifdef NS_CLIENT_TRACE
#define CTRACE(m)	ns_client_log(client, \
				      NS_LOGCATEGORY_CLIENT, \
				      NS_LOGMODULE_CLIENT, \
				      ISC_LOG_DEBUG(3), \
				      "%s", (m))
#define MTRACE(m)	isc_log_write(ns_g_lctx, \
				      NS_LOGCATEGORY_GENERAL, \
				      NS_LOGMODULE_CLIENT, \
				      ISC_LOG_DEBUG(3), \
				      "clientmgr @%p: %s", manager, (m))
#else
#define CTRACE(m)	((void)(m))
#define MTRACE(m)	((void)(m))
#endif

#define TCP_CLIENT(c)	(((c)->attributes & NS_CLIENTATTR_TCP) != 0)

#define TCP_BUFFER_SIZE			(65535 + 2)
#define SEND_BUFFER_SIZE		4096
#define RECV_BUFFER_SIZE		4096

struct ns_clientmgr {
	/* Unlocked. */
	unsigned int			magic;
	isc_mem_t *			mctx;
	isc_taskmgr_t *			taskmgr;
	isc_timermgr_t *		timermgr;
	isc_mutex_t			lock;
	/* Locked by lock. */
	isc_boolean_t			exiting;
	client_list_t			active; 	/* Active clients */
	client_list_t 			inactive;	/* To be recycled */
};

#define MANAGER_MAGIC			ISC_MAGIC('N', 'S', 'C', 'm')
#define VALID_MANAGER(m)		ISC_MAGIC_VALID(m, MANAGER_MAGIC)

/*
 * Client object states.  Ordering is significant: higher-numbered
 * states are generally "more active", meaning that the client can
 * have more dynamically allocated data, outstanding events, etc.
 * In the list below, any such properties listed for state N
 * also apply to any state > N.
 *
 * To force the client into a less active state, set client->newstate
 * to that state and call exit_check().  This will cause any
 * activities defined for higher-numbered states to be aborted.
 */

#define NS_CLIENTSTATE_FREED    0
/*
 * The client object no longer exists.
 */

#define NS_CLIENTSTATE_INACTIVE 1
/*
 * The client object exists and has a task and timer.
 * Its "query" struct and sendbuf are initialized.
 * It is on the client manager's list of inactive clients.
 * It has a message and OPT, both in the reset state.
 */

#define NS_CLIENTSTATE_READY    2
/*
 * The client object is either a TCP or a UDP one, and
 * it is associated with a network interface.  It is on the
 * client manager's list of active clients.
 *
 * If it is a TCP client object, it has a TCP listener socket
 * and an outstanding TCP listen request.
 *
 * If it is a UDP client object, it has a UDP listener socket
 * and an outstanding UDP receive request.
 */

#define NS_CLIENTSTATE_READING  3
/*
 * The client object is a TCP client object that has received
 * a connection.  It has a tcpsocket, tcpmsg, TCP quota, and an
 * outstanding TCP read request.  This state is not used for
 * UDP client objects.
 */

#define NS_CLIENTSTATE_WORKING  4
/*
 * The client object has received a request and is working
 * on it.  It has a view, and it may  have any of a non-reset OPT,
 * recursion quota, and an outstanding write request.
 */

#define NS_CLIENTSTATE_MAX      9
/*
 * Sentinel value used to indicate "no state".  When client->newstate
 * has this value, we are not attempting to exit the current state.
 * Must be greater than any valid state.
 */


static void client_read(ns_client_t *client);
static void client_accept(ns_client_t *client);
static void client_udprecv(ns_client_t *client);
static void clientmgr_destroy(ns_clientmgr_t *manager);
static isc_boolean_t exit_check(ns_client_t *client);
static void ns_client_endrequest(ns_client_t *client);
static void ns_client_checkactive(ns_client_t *client);
static void client_start(isc_task_t *task, isc_event_t *event);
static void client_request(isc_task_t *task, isc_event_t *event);
static void ns_client_dumpmessage(ns_client_t *client, const char *reason);

void
ns_client_settimeout(ns_client_t *client, unsigned int seconds) {
	isc_result_t result;
	isc_interval_t interval;

	isc_interval_set(&interval, seconds, 0);
	result = isc_timer_reset(client->timer, isc_timertype_once, NULL,
				 &interval, ISC_FALSE);
	client->timerset = ISC_TRUE;
	if (result != ISC_R_SUCCESS) {
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_ERROR,
			      "setting timeout: %s",
			      isc_result_totext(result));
		/* Continue anyway. */
	}
}

/*
 * Check for a deactivation or shutdown request and take appropriate
 * action.  Returns ISC_TRUE if either is in progress; in this case
 * the caller must no longer use the client object as it may have been
 * freed.
 */
static isc_boolean_t
exit_check(ns_client_t *client) {
	ns_clientmgr_t *locked_manager = NULL;
	ns_clientmgr_t *destroy_manager = NULL;

	REQUIRE(NS_CLIENT_VALID(client));

	if (client->state <= client->newstate)
		return (ISC_FALSE); /* Business as usual. */

	INSIST(client->newstate < NS_CLIENTSTATE_WORKING);

	/*
	 * We need to detach from the view early when shutting down
	 * the server to break the following vicious circle:
	 *
	 *  - The resolver will not shut down until the view refcount is zero
	 *  - The view refcount does not go to zero until all clients detach
	 *  - The client does not detach from the view until references is zero
	 *  - references does not go to zero until the resolver has shut down
	 *
	 * Keep the view attached until any outstanding updates complete.
	 */
	if (client->nupdates == 0 && 
	    client->newstate == NS_CLIENTSTATE_FREED && client->view != NULL)
		dns_view_detach(&client->view);

	if (client->state == NS_CLIENTSTATE_WORKING) {
		INSIST(client->newstate <= NS_CLIENTSTATE_READING);
		/*
		 * Let the update processing complete.
		 */
		if (client->nupdates > 0)
			return (ISC_TRUE);
		/*
		 * We are trying to abort request processing.
		 */
		if (client->nsends > 0) {
			isc_socket_t *socket;
			if (TCP_CLIENT(client))
				socket = client->tcpsocket;
			else
				socket = client->udpsocket;
			isc_socket_cancel(socket, client->task,
					  ISC_SOCKCANCEL_SEND);
		}

		if (! (client->nsends == 0 && client->nrecvs == 0 &&
		       client->references == 0))
		{
			/*
			 * Still waiting for I/O cancel completion.
			 * or lingering references.
			 */
			return (ISC_TRUE);
		}
		/*
		 * I/O cancel is complete.  Burn down all state
		 * related to the current request.
		 */
		ns_client_endrequest(client);

		client->state = NS_CLIENTSTATE_READING;
		INSIST(client->recursionquota == NULL);
		if (NS_CLIENTSTATE_READING == client->newstate) {
			client_read(client);
			client->newstate = NS_CLIENTSTATE_MAX;
			return (ISC_TRUE); /* We're done. */
		}
	}

	if (client->state == NS_CLIENTSTATE_READING) {
		/*
		 * We are trying to abort the current TCP connection,
		 * if any.
		 */
		INSIST(client->recursionquota == NULL);
		INSIST(client->newstate <= NS_CLIENTSTATE_READY);
		if (client->nreads > 0)
			dns_tcpmsg_cancelread(&client->tcpmsg);
		if (! client->nreads == 0) {
			/* Still waiting for read cancel completion. */
			return (ISC_TRUE);
		}

		if (client->tcpmsg_valid) {
			dns_tcpmsg_invalidate(&client->tcpmsg);
			client->tcpmsg_valid = ISC_FALSE;
		}
		if (client->tcpsocket != NULL) {
			CTRACE("closetcp");
			isc_socket_detach(&client->tcpsocket);
		}

		if (client->tcpquota != NULL)
			isc_quota_detach(&client->tcpquota);

		if (client->timerset) {
			(void) isc_timer_reset(client->timer,
					       isc_timertype_inactive,
					       NULL, NULL, ISC_TRUE);
			client->timerset = ISC_FALSE;
		}

		client->peeraddr_valid = ISC_FALSE;

		client->state = NS_CLIENTSTATE_READY;
		INSIST(client->recursionquota == NULL);

		/*
		 * Now the client is ready to accept a new TCP connection
		 * or UDP request, but we may have enough clients doing
		 * that already.  Check whether this client needs to remain
		 * active and force it to go inactive if not.
		 */
		ns_client_checkactive(client);

		if (NS_CLIENTSTATE_READY == client->newstate) {
			if (TCP_CLIENT(client)) {
				client_accept(client);
			} else
				client_udprecv(client);
			client->newstate = NS_CLIENTSTATE_MAX;
			return (ISC_TRUE);
		}
	}

	if (client->state == NS_CLIENTSTATE_READY) {
		INSIST(client->newstate <= NS_CLIENTSTATE_INACTIVE);
		/*
		 * We are trying to enter the inactive state.
		 */
		if (client->naccepts > 0)
			isc_socket_cancel(client->tcplistener, client->task,
					  ISC_SOCKCANCEL_ACCEPT);

		if (! (client->naccepts == 0)) {
			/* Still waiting for accept cancel completion. */
			return (ISC_TRUE);
		}
		/* Accept cancel is complete. */

		if (client->nrecvs > 0)
			isc_socket_cancel(client->udpsocket, client->task,
					  ISC_SOCKCANCEL_RECV);
		if (! (client->nrecvs == 0)) {
			/* Still waiting for recv cancel completion. */
			return (ISC_TRUE);
		}
		/* Recv cancel is complete. */

		if (client->nctls > 0) {
			/* Still waiting for control event to be delivered */
			return (ISC_TRUE);
		}

		/* Deactivate the client. */
		if (client->interface)
			ns_interface_detach(&client->interface);

		INSIST(client->naccepts == 0);
		INSIST(client->recursionquota == NULL);
		if (client->tcplistener != NULL)
			isc_socket_detach(&client->tcplistener);

		if (client->udpsocket != NULL)
			isc_socket_detach(&client->udpsocket);

		if (client->dispatch != NULL)
			dns_dispatch_detach(&client->dispatch);

		client->attributes = 0;
		client->mortal = ISC_FALSE;

		LOCK(&client->manager->lock);
		/*
		 * Put the client on the inactive list.  If we are aiming for
		 * the "freed" state, it will be removed from the inactive
		 * list shortly, and we need to keep the manager locked until
		 * that has been done, lest the manager decide to reactivate
		 * the dying client inbetween.
		 */
		locked_manager = client->manager;
		ISC_LIST_UNLINK(*client->list, client, link);
		ISC_LIST_APPEND(client->manager->inactive, client, link);
		client->list = &client->manager->inactive;
		client->state = NS_CLIENTSTATE_INACTIVE;
		INSIST(client->recursionquota == NULL);

		if (client->state == client->newstate) {
			client->newstate = NS_CLIENTSTATE_MAX;
			goto unlock;
		}
	}

	if (client->state == NS_CLIENTSTATE_INACTIVE) {
		INSIST(client->newstate == NS_CLIENTSTATE_FREED);
		/*
		 * We are trying to free the client.
		 *
		 * When "shuttingdown" is true, either the task has received
		 * its shutdown event or no shutdown event has ever been
		 * set up.  Thus, we have no outstanding shutdown
		 * event at this point.
		 */
		REQUIRE(client->state == NS_CLIENTSTATE_INACTIVE);

		INSIST(client->recursionquota == NULL);

		ns_query_free(client);
		isc_mem_put(client->mctx, client->recvbuf, RECV_BUFFER_SIZE);
		isc_event_free((isc_event_t **)&client->sendevent);
		isc_event_free((isc_event_t **)&client->recvevent);
		isc_timer_detach(&client->timer);

		if (client->tcpbuf != NULL)
			isc_mem_put(client->mctx, client->tcpbuf, TCP_BUFFER_SIZE);
		if (client->opt != NULL) {
			INSIST(dns_rdataset_isassociated(client->opt));
			dns_rdataset_disassociate(client->opt);
			dns_message_puttemprdataset(client->message, &client->opt);
		}
		dns_message_destroy(&client->message);
		if (client->manager != NULL) {
			ns_clientmgr_t *manager = client->manager;
			if (locked_manager == NULL) {
				LOCK(&manager->lock);
				locked_manager = manager;
			}
			ISC_LIST_UNLINK(*client->list, client, link);
			client->list = NULL;
			if (manager->exiting &&
			    ISC_LIST_EMPTY(manager->active) &&
			    ISC_LIST_EMPTY(manager->inactive))
				destroy_manager = manager;
		}
		/*
		 * Detaching the task must be done after unlinking from
		 * the manager's lists because the manager accesses
		 * client->task.
		 */
		if (client->task != NULL)
			isc_task_detach(&client->task);

		CTRACE("free");
		client->magic = 0;
		isc_mem_put(client->mctx, client, sizeof(*client));

		goto unlock;
	}

 unlock:
	if (locked_manager != NULL) {
		UNLOCK(&locked_manager->lock);
		locked_manager = NULL;
	}

	/*
	 * Only now is it safe to destroy the client manager (if needed),
	 * because we have accessed its lock for the last time.
	 */
	if (destroy_manager != NULL)
		clientmgr_destroy(destroy_manager);

	return (ISC_TRUE);
}

/*
 * The client's task has received the client's control event
 * as part of the startup process.
 */
static void
client_start(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client = (ns_client_t *) event->ev_arg;

	INSIST(task == client->task);

	UNUSED(task);

	INSIST(client->nctls == 1);
	client->nctls--;

	if (exit_check(client))
		return;

	if (TCP_CLIENT(client)) {
		client_accept(client);
	} else {
		client_udprecv(client);
	}
}


/*
 * The client's task has received a shutdown event.
 */
static void
client_shutdown(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client;

	REQUIRE(event != NULL);
	REQUIRE(event->ev_type == ISC_TASKEVENT_SHUTDOWN);
	client = event->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);

	UNUSED(task);

	CTRACE("shutdown");

	isc_event_free(&event);

	if (client->shutdown != NULL) {
		(client->shutdown)(client->shutdown_arg, ISC_R_SHUTTINGDOWN);
		client->shutdown = NULL;
		client->shutdown_arg = NULL;
	}

	client->newstate = NS_CLIENTSTATE_FREED;
	(void)exit_check(client);
}


static void
ns_client_endrequest(ns_client_t *client) {
	INSIST(client->naccepts == 0);
	INSIST(client->nreads == 0);
	INSIST(client->nsends == 0);
	INSIST(client->nrecvs == 0);
	INSIST(client->nupdates == 0);
	INSIST(client->state == NS_CLIENTSTATE_WORKING);

	CTRACE("endrequest");

	if (client->next != NULL) {
		(client->next)(client);
		client->next = NULL;
	}

	if (client->view != NULL)
		dns_view_detach(&client->view);
	if (client->opt != NULL) {
		INSIST(dns_rdataset_isassociated(client->opt));
		dns_rdataset_disassociate(client->opt);
		dns_message_puttemprdataset(client->message, &client->opt);
	}

	client->udpsize = 512;
	client->extflags = 0;
	dns_message_reset(client->message, DNS_MESSAGE_INTENTPARSE);

	if (client->recursionquota != NULL)
		isc_quota_detach(&client->recursionquota);

	/*
	 * Clear all client attributes that are specific to
	 * the request; that's all except the TCP flag.
	 */
	client->attributes &= NS_CLIENTATTR_TCP;
}

static void
ns_client_checkactive(ns_client_t *client) {
	if (client->mortal) {
		/*
		 * This client object should normally go inactive
		 * at this point, but if we have fewer active client
		 * objects than  desired due to earlier quota exhaustion,
		 * keep it active to make up for the shortage.
		 */
		isc_boolean_t need_another_client = ISC_FALSE;
		if (TCP_CLIENT(client)) {
			LOCK(&client->interface->lock);
			if (client->interface->ntcpcurrent <
			    client->interface->ntcptarget)
				need_another_client = ISC_TRUE;
			UNLOCK(&client->interface->lock);
		} else {
			/*
			 * The UDP client quota is enforced by making
			 * requests fail rather than by not listening
			 * for new ones.  Therefore, there is always a
			 * full set of UDP clients listening.
			 */
		}
		if (! need_another_client) {
			/*
			 * We don't need this client object.  Recycle it.
			 */
			if (client->newstate >= NS_CLIENTSTATE_INACTIVE)
				client->newstate = NS_CLIENTSTATE_INACTIVE;
		}
	}
}

void
ns_client_next(ns_client_t *client, isc_result_t result) {
	int newstate;

	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(client->state == NS_CLIENTSTATE_WORKING ||
		client->state == NS_CLIENTSTATE_READING);

	CTRACE("next");

	if (result != ISC_R_SUCCESS)
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "request failed: %s", isc_result_totext(result));

	/*
	 * An error processing a TCP request may have left
	 * the connection out of sync.  To be safe, we always
	 * sever the connection when result != ISC_R_SUCCESS.
	 */
	if (result == ISC_R_SUCCESS && TCP_CLIENT(client))
		newstate = NS_CLIENTSTATE_READING;
	else
		newstate = NS_CLIENTSTATE_READY;

	if (client->newstate > newstate)
		client->newstate = newstate;
	(void) exit_check(client);
}


static void
client_senddone(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client;
	isc_socketevent_t *sevent = (isc_socketevent_t *) event;

	REQUIRE(sevent != NULL);
	REQUIRE(sevent->ev_type == ISC_SOCKEVENT_SENDDONE);
	client = sevent->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);
	REQUIRE(sevent == client->sendevent);

	UNUSED(task);

	CTRACE("senddone");

	if (sevent->result != ISC_R_SUCCESS)
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_WARNING,
			      "error sending response: %s",
			      isc_result_totext(sevent->result));

	INSIST(client->nsends > 0);
	client->nsends--;

	if (client->tcpbuf != NULL) {
		INSIST(TCP_CLIENT(client));
		isc_mem_put(client->mctx, client->tcpbuf, TCP_BUFFER_SIZE);
		client->tcpbuf = NULL;
	}

	if (exit_check(client))
		return;

	ns_client_next(client, ISC_R_SUCCESS);
}

/*
 * We only want to fail with ISC_R_NOSPACE when called from
 * ns_client_sendraw() and not when called from ns_client_send(),
 * tcpbuffer is NULL when called from ns_client_sendraw() and
 * length != 0.  tcpbuffer != NULL when called from ns_client_send()
 * and length == 0.
 */

static isc_result_t
client_allocsendbuf(ns_client_t *client, isc_buffer_t *buffer,
		    isc_buffer_t *tcpbuffer, isc_uint32_t length,
		    unsigned char *sendbuf, unsigned char **datap)
{
	unsigned char *data;
	isc_uint32_t bufsize;
	isc_result_t result;

	INSIST(datap != NULL);
	INSIST((tcpbuffer == NULL && length != 0) ||
	       (tcpbuffer != NULL && length == 0));

	if (TCP_CLIENT(client)) {
		INSIST(client->tcpbuf == NULL);
		if (length + 2 > TCP_BUFFER_SIZE) {
			result = ISC_R_NOSPACE;
			goto done;
		}
		client->tcpbuf = isc_mem_get(client->mctx, TCP_BUFFER_SIZE);
		if (client->tcpbuf == NULL) {
			result = ISC_R_NOMEMORY;
			goto done;
		}
		data = client->tcpbuf;
		if (tcpbuffer != NULL) {
			isc_buffer_init(tcpbuffer, data, TCP_BUFFER_SIZE);
			isc_buffer_init(buffer, data + 2, TCP_BUFFER_SIZE - 2);
		} else {
			isc_buffer_init(buffer, data, TCP_BUFFER_SIZE);
			INSIST(length <= 0xffff);
			isc_buffer_putuint16(buffer, (isc_uint16_t)length);
		}
	} else {
		data = sendbuf;
		if (client->udpsize < SEND_BUFFER_SIZE)
			bufsize = client->udpsize;
		else
			bufsize = SEND_BUFFER_SIZE;
		if (length > bufsize) {
			result = ISC_R_NOSPACE;
			goto done;
		}
		isc_buffer_init(buffer, data, bufsize);
	}
	*datap = data;
	result = ISC_R_SUCCESS;

 done:
	return (result);
}

static isc_result_t
client_sendpkg(ns_client_t *client, isc_buffer_t *buffer) {
	struct in6_pktinfo *pktinfo;
	isc_result_t result;
	isc_region_t r;
	isc_sockaddr_t *address;
	isc_socket_t *socket;
	isc_netaddr_t netaddr;
	int match;
	unsigned int sockflags = ISC_SOCKFLAG_IMMEDIATE;

	if (TCP_CLIENT(client)) {
		socket = client->tcpsocket;
		address = NULL;
	} else {
		socket = client->udpsocket;
		address = &client->peeraddr;

		isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);
		if (ns_g_server->blackholeacl != NULL &&
		    dns_acl_match(&netaddr, NULL,
			    	  ns_g_server->blackholeacl,
				  &ns_g_server->aclenv,
				  &match, NULL) == ISC_R_SUCCESS &&
		    match > 0)
			return (DNS_R_BLACKHOLED);
		sockflags |= ISC_SOCKFLAG_NORETRY;
	}

	if ((client->attributes & NS_CLIENTATTR_PKTINFO) != 0)
		pktinfo = &client->pktinfo;
	else
		pktinfo = NULL;

	isc_buffer_usedregion(buffer, &r);

	CTRACE("sendto");
	
	result = isc_socket_sendto2(socket, &r, client->task,
				    address, pktinfo,
				    client->sendevent, sockflags);
	if (result == ISC_R_SUCCESS || result == ISC_R_INPROGRESS) {
		client->nsends++;
		if (result == ISC_R_SUCCESS)
			client_senddone(client->task,
					(isc_event_t *)client->sendevent);
		result = ISC_R_SUCCESS;
	}
	return (result);
}

void
ns_client_sendraw(ns_client_t *client, dns_message_t *message) {
	isc_result_t result;
	unsigned char *data;
	isc_buffer_t buffer;
	isc_region_t r;
	isc_region_t *mr;
	unsigned char sendbuf[SEND_BUFFER_SIZE];

	REQUIRE(NS_CLIENT_VALID(client));

	CTRACE("sendraw");

	mr = dns_message_getrawmessage(message);
	if (mr == NULL) {
		result = ISC_R_UNEXPECTEDEND;
		goto done;
	}

	result = client_allocsendbuf(client, &buffer, NULL, mr->length,
				     sendbuf, &data);
	if (result != ISC_R_SUCCESS)
		goto done;

	/*
	 * Copy message to buffer and fixup id.
	 */
	isc_buffer_availableregion(&buffer, &r);
	result = isc_buffer_copyregion(&buffer, mr);
	if (result != ISC_R_SUCCESS)
		goto done;
	r.base[0] = (client->message->id >> 8) & 0xff;
	r.base[1] = client->message->id & 0xff;

	result = client_sendpkg(client, &buffer);
	if (result == ISC_R_SUCCESS)
		return;

 done:
	if (client->tcpbuf != NULL) {
		isc_mem_put(client->mctx, client->tcpbuf, TCP_BUFFER_SIZE);
		client->tcpbuf = NULL;
	}
	ns_client_next(client, result);
}

void
ns_client_send(ns_client_t *client) {
	isc_result_t result;
	unsigned char *data;
	isc_buffer_t buffer;
	isc_buffer_t tcpbuffer;
	isc_region_t r;
	dns_compress_t cctx;
	isc_boolean_t cleanup_cctx = ISC_FALSE;
	unsigned char sendbuf[SEND_BUFFER_SIZE];

	REQUIRE(NS_CLIENT_VALID(client));

	CTRACE("send");

	if ((client->attributes & NS_CLIENTATTR_RA) != 0)
		client->message->flags |= DNS_MESSAGEFLAG_RA;

	/*
	 * XXXRTH  The following doesn't deal with TCP buffer resizing.
	 */
	result = client_allocsendbuf(client, &buffer, &tcpbuffer, 0,
				     sendbuf, &data);
	if (result != ISC_R_SUCCESS)
		goto done;

	result = dns_compress_init(&cctx, -1, client->mctx);
	if (result != ISC_R_SUCCESS)
		goto done;
	cleanup_cctx = ISC_TRUE;

	result = dns_message_renderbegin(client->message, &cctx, &buffer);
	if (result != ISC_R_SUCCESS)
		goto done;
	if (client->opt != NULL) {
		result = dns_message_setopt(client->message, client->opt);
		/*
		 * XXXRTH dns_message_setopt() should probably do this...
		 */
		client->opt = NULL;
		if (result != ISC_R_SUCCESS)
			goto done;
	}
	result = dns_message_rendersection(client->message,
					   DNS_SECTION_QUESTION, 0);
	if (result == ISC_R_NOSPACE) {
		client->message->flags |= DNS_MESSAGEFLAG_TC;
		goto renderend;
	}
	if (result != ISC_R_SUCCESS)
		goto done;
	result = dns_message_rendersection(client->message,
					   DNS_SECTION_ANSWER,
					   DNS_MESSAGERENDER_PARTIAL);
	if (result == ISC_R_NOSPACE) {
		client->message->flags |= DNS_MESSAGEFLAG_TC;
		goto renderend;
	}
	if (result != ISC_R_SUCCESS)
		goto done;
	result = dns_message_rendersection(client->message,
					   DNS_SECTION_AUTHORITY,
					   DNS_MESSAGERENDER_PARTIAL);
	if (result == ISC_R_NOSPACE) {
		client->message->flags |= DNS_MESSAGEFLAG_TC;
		goto renderend;
	}
	if (result != ISC_R_SUCCESS)
		goto done;
	result = dns_message_rendersection(client->message,
					   DNS_SECTION_ADDITIONAL, 0);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOSPACE)
		goto done;
 renderend:
	result = dns_message_renderend(client->message);

	if (result != ISC_R_SUCCESS)
		goto done;

	if (cleanup_cctx) {
		dns_compress_invalidate(&cctx);
		cleanup_cctx = ISC_FALSE;
	}

	if (TCP_CLIENT(client)) {
		isc_buffer_usedregion(&buffer, &r);
		isc_buffer_putuint16(&tcpbuffer, (isc_uint16_t) r.length);
		isc_buffer_add(&tcpbuffer, r.length);
		result = client_sendpkg(client, &tcpbuffer);
	} else
		result = client_sendpkg(client, &buffer);
	if (result == ISC_R_SUCCESS)
		return;

 done:
	if (client->tcpbuf != NULL) {
		isc_mem_put(client->mctx, client->tcpbuf, TCP_BUFFER_SIZE);
		client->tcpbuf = NULL;
	}

	if (cleanup_cctx)
		dns_compress_invalidate(&cctx);

	ns_client_next(client, result);
}

void
ns_client_error(ns_client_t *client, isc_result_t result) {
	dns_rcode_t rcode;
	dns_message_t *message;

	REQUIRE(NS_CLIENT_VALID(client));

	CTRACE("error");

	message = client->message;
	rcode = dns_result_torcode(result);

	/*
	 * Message may be an in-progress reply that we had trouble
	 * with, in which case QR will be set.  We need to clear QR before
	 * calling dns_message_reply() to avoid triggering an assertion.
	 */
	message->flags &= ~DNS_MESSAGEFLAG_QR;
	/*
	 * AA and AD shouldn't be set.
	 */
	message->flags &= ~(DNS_MESSAGEFLAG_AA | DNS_MESSAGEFLAG_AD);
	result = dns_message_reply(message, ISC_TRUE);
	if (result != ISC_R_SUCCESS) {
		/*
		 * It could be that we've got a query with a good header,
		 * but a bad question section, so we try again with
		 * want_question_section set to ISC_FALSE.
		 */
		result = dns_message_reply(message, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			ns_client_next(client, result);
			return;
		}
	}
	message->rcode = rcode;

	/*
	 * FORMERR loop avoidance:  If we sent a FORMERR message
	 * with the same ID to the same client less than two
	 * seconds ago, assume that we are in an infinite error 
	 * packet dialog with a server for some protocol whose 
	 * error responses look enough like DNS queries to
	 * elicit a FORMERR response.  Drop a packet to break
	 * the loop.
	 */
	if (rcode == dns_rcode_formerr) {
		if (isc_sockaddr_equal(&client->peeraddr,
				       &client->formerrcache.addr) &&
		    message->id == client->formerrcache.id &&
		    client->requesttime - client->formerrcache.time < 2) {
			/* Drop packet. */
			ns_client_log(client, NS_LOGCATEGORY_CLIENT,
				      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(1),
				      "possible error packet loop, "
				      "FORMERR dropped");
			ns_client_next(client, result);
			return;
		}
		client->formerrcache.addr = client->peeraddr;
		client->formerrcache.time = client->requesttime;
		client->formerrcache.id = message->id;
	}
	ns_client_send(client);
}

static inline isc_result_t
client_addopt(ns_client_t *client) {
	dns_rdataset_t *rdataset;
	dns_rdatalist_t *rdatalist;
	dns_rdata_t *rdata;
	isc_result_t result;

	REQUIRE(client->opt == NULL);	/* XXXRTH free old. */

	rdatalist = NULL;
	result = dns_message_gettemprdatalist(client->message, &rdatalist);
	if (result != ISC_R_SUCCESS)
		return (result);
	rdata = NULL;
	result = dns_message_gettemprdata(client->message, &rdata);
	if (result != ISC_R_SUCCESS)
		return (result);
	rdataset = NULL;
	result = dns_message_gettemprdataset(client->message, &rdataset);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_rdataset_init(rdataset);

	rdatalist->type = dns_rdatatype_opt;
	rdatalist->covers = 0;

	/*
	 * Set the maximum UDP buffer size.
	 */
	rdatalist->rdclass = RECV_BUFFER_SIZE;

	/*
	 * Set EXTENDED-RCODE, VERSION, and Z to 0.
	 */
#ifdef ISC_RFC2535
	rdatalist->ttl = (client->extflags & DNS_MESSAGEEXTFLAG_REPLYPRESERVE);
#else
	rdatalist->ttl = 0;
#endif

	/*
	 * No ENDS options in the default case.
	 */
	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = rdatalist->rdclass;
	rdata->type = rdatalist->type;
	rdata->flags = 0;

	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdatalist_tordataset(rdatalist, rdataset);

	client->opt = rdataset;

	return (ISC_R_SUCCESS);
}

static inline isc_boolean_t
allowed(isc_netaddr_t *addr, dns_acl_t *acl) {
	int match;
	isc_result_t result;

	if (acl == NULL)
		return (ISC_TRUE);
	result = dns_acl_match(addr, NULL, acl, &ns_g_server->aclenv,
			       &match, NULL);
	if (result == ISC_R_SUCCESS && match > 0)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*
 * Handle an incoming request event from the socket (UDP case)
 * or tcpmsg (TCP case).
 */
static void
client_request(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client;
	isc_socketevent_t *sevent;
	isc_result_t result;
	isc_result_t sigresult;
	isc_buffer_t *buffer;
	isc_buffer_t tbuffer;
	dns_view_t *view;
	dns_rdataset_t *opt;
	isc_boolean_t ra; 	/* Recursion available. */
	isc_netaddr_t netaddr;
	isc_netaddr_t destaddr;
	int match;
	dns_messageid_t id;
	unsigned int flags;
	isc_boolean_t notimp;

	REQUIRE(event != NULL);
	client = event->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);

	UNUSED(task);

	INSIST(client->recursionquota == NULL);

	INSIST(client->state ==
	       TCP_CLIENT(client) ?
	       NS_CLIENTSTATE_READING :
	       NS_CLIENTSTATE_READY);

	if (event->ev_type == ISC_SOCKEVENT_RECVDONE) {
		INSIST(!TCP_CLIENT(client));
		sevent = (isc_socketevent_t *)event;
		REQUIRE(sevent == client->recvevent);
		isc_buffer_init(&tbuffer, sevent->region.base, sevent->n);
		isc_buffer_add(&tbuffer, sevent->n);
		buffer = &tbuffer;
		result = sevent->result;
		if (result == ISC_R_SUCCESS) {
			client->peeraddr = sevent->address;
			client->peeraddr_valid = ISC_TRUE;
		}
		if ((sevent->attributes & ISC_SOCKEVENTATTR_PKTINFO) != 0) {
			client->attributes |= NS_CLIENTATTR_PKTINFO;
			client->pktinfo = sevent->pktinfo;
		}
		if ((sevent->attributes & ISC_SOCKEVENTATTR_MULTICAST) != 0)
			client->attributes |= NS_CLIENTATTR_MULTICAST;
		client->nrecvs--;
	} else {
		INSIST(TCP_CLIENT(client));
		REQUIRE(event->ev_type == DNS_EVENT_TCPMSG);
		REQUIRE(event->ev_sender == &client->tcpmsg);
		buffer = &client->tcpmsg.buffer;
		result = client->tcpmsg.result;
		INSIST(client->nreads == 1);
		/*
		 * client->peeraddr was set when the connection was accepted.
		 */
		client->nreads--;
	}

	if (exit_check(client))
		goto cleanup;
	client->state = client->newstate = NS_CLIENTSTATE_WORKING;

	isc_stdtime_get(&client->requesttime);
	client->now = client->requesttime;

	if (result != ISC_R_SUCCESS) {
		if (TCP_CLIENT(client)) {
			ns_client_next(client, result);
		} else {
			if  (result != ISC_R_CANCELED)
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_CLIENT,
					      NS_LOGMODULE_CLIENT,
					      ISC_LOG_ERROR,
					      "UDP client handler shutting "
					      "down due to fatal receive "
					      "error: %s",
					      isc_result_totext(result));
			isc_task_shutdown(client->task);
		}
		goto cleanup;
	}

	isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);

	ns_client_log(client, NS_LOGCATEGORY_CLIENT,
		      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
		      "%s request",
		      TCP_CLIENT(client) ? "TCP" : "UDP");

	/*
	 * Check the blackhole ACL for UDP only, since TCP is done in
	 * client_newconn.
	 */
	if (!TCP_CLIENT(client)) {

		if (ns_g_server->blackholeacl != NULL &&
		    dns_acl_match(&netaddr, NULL, ns_g_server->blackholeacl,
				  &ns_g_server->aclenv,
				  &match, NULL) == ISC_R_SUCCESS &&
		    match > 0)
		{
			ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
				      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(10),
				      "blackholed UDP datagram");
			ns_client_next(client, ISC_R_SUCCESS);
			goto cleanup;
		}
	}

	if ((client->attributes & NS_CLIENTATTR_MULTICAST) != 0) {
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(2),
			      "multicast request");
#if 0
		ns_client_error(client, DNS_R_REFUSED);
#endif
	}

	result = dns_message_peekheader(buffer, &id, &flags);
	if (result != ISC_R_SUCCESS) {
		/*
		 * There isn't enough header to determine whether
		 * this was a request or a response.  Drop it.
		 */
		ns_client_next(client, result);
		goto cleanup;
	}

	/*
	 * The client object handles requests, not responses.
	 * If this is a UDP response, forward it to the dispatcher.
	 * If it's a TCP response, discard it here.
	 */
	if ((flags & DNS_MESSAGEFLAG_QR) != 0) {
		if (TCP_CLIENT(client)) {
			CTRACE("unexpected response");
			ns_client_next(client, DNS_R_FORMERR);
			goto cleanup;
		} else {
			dns_dispatch_importrecv(client->dispatch, event);
			ns_client_next(client, ISC_R_SUCCESS);
			goto cleanup;
		}
	}

	/*
	 * It's a request.  Parse it.
	 */
	result = dns_message_parse(client->message, buffer, 0);
	if (result != ISC_R_SUCCESS) {
		/*
		 * Parsing the request failed.  Send a response
		 * (typically FORMERR or SERVFAIL).
		 */
		ns_client_error(client, result);
		goto cleanup;
	}

	switch (client->message->opcode) {
	case dns_opcode_query:
	case dns_opcode_update:
	case dns_opcode_notify:
		notimp = ISC_FALSE;
		break;
	case dns_opcode_iquery:
	default:
		notimp = ISC_TRUE;
		break;
	}

	client->message->rcode = dns_rcode_noerror;

	/*
	 * Deal with EDNS.
	 */
	opt = dns_message_getopt(client->message);
	if (opt != NULL) {
		unsigned int version;

		/*
		 * Set the client's UDP buffer size.
		 */
		client->udpsize = opt->rdclass;

		/*
		 * If the requested UDP buffer size is less than 512,
		 * ignore it and use 512.
		 */
		if (client->udpsize < 512)
			client->udpsize = 512;

		/*
		 * Get the flags out of the OPT record.
		 */
		client->extflags = (isc_uint16_t)(opt->ttl & 0xFFFF);

		/*
		 * Create an OPT for our reply.
		 */
		result = client_addopt(client);
		if (result != ISC_R_SUCCESS) {
			ns_client_error(client, result);
			goto cleanup;
		}

		/*
		 * Do we understand this version of ENDS?
		 *
		 * XXXRTH need library support for this!
		 */
		version = (opt->ttl & 0x00FF0000) >> 16;
		if (version != 0) {
			ns_client_error(client, DNS_R_BADVERS);
			goto cleanup;
		}
	}

	if (client->message->rdclass == 0) {
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(1),
			      "message class could not be determined");
		ns_client_dumpmessage(client,
				      "message class could not be determined");
		ns_client_error(client, notimp ? DNS_R_NOTIMP : DNS_R_FORMERR);
		goto cleanup;
	}

	/*
	 * Determine the destination address.  For TCP/IPv6, we get this from
	 * the receiving socket.  For UDP/IPv6, we get it from the pktinfo
	 * structure (if supported).  For IPv4, we have to do with
	 * the address of the interface where the request was received.
	 */
	if (client->interface->addr.type.sa.sa_family == AF_INET6) {
		result = ISC_R_FAILURE;

		if (TCP_CLIENT(client)) {
			isc_sockaddr_t destsockaddr;

			result = isc_socket_getsockname(client->tcpsocket,
							&destsockaddr);
			if (result == ISC_R_SUCCESS)
				isc_netaddr_fromsockaddr(&destaddr,
							 &destsockaddr);
		}
		if (result != ISC_R_SUCCESS &&
		    (client->attributes & NS_CLIENTATTR_PKTINFO) != 0) {
			isc_netaddr_fromin6(&destaddr, &client->pktinfo.ipi6_addr);
			result = ISC_R_SUCCESS;
		}
		if (result != ISC_R_SUCCESS) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "failed to get request's "
					 "destination: %s",
					 isc_result_totext(result));
			goto cleanup;
		}
	} else {
		isc_netaddr_fromsockaddr(&destaddr, &client->interface->addr);
	}

	/*
	 * Find a view that matches the client's source address.
	 */
	for (view = ISC_LIST_HEAD(ns_g_server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (client->message->rdclass == view->rdclass ||
		    client->message->rdclass == dns_rdataclass_any)
		{
			if (allowed(&netaddr, view->matchclients) &&
			    allowed(&destaddr, view->matchdestinations) &&
			    !((flags & DNS_MESSAGEFLAG_RD) == 0 &&
			      view->matchrecursiveonly))
			{
				dns_view_attach(view, &client->view);
				break;
			}
		}
	}

	if (view == NULL) {
		char classname[DNS_RDATACLASS_FORMATSIZE];

		/*
		 * Do a dummy TSIG verification attempt so that the
		 * response will have a TSIG if the query did, as
		 * required by RFC2845.
		 */
		isc_buffer_t b;
		isc_region_t *r;
		r = dns_message_getrawmessage(client->message);
		isc_buffer_init(&b, r->base, r->length);
		isc_buffer_add(&b, r->length);
		(void)dns_tsig_verify(&b, client->message, NULL, NULL);

		dns_rdataclass_format(client->message->rdclass, classname,
				      sizeof(classname));
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(1),
			      "no matching view in class '%s'", classname);
		ns_client_dumpmessage(client, "no matching view in class");
		ns_client_error(client, notimp ? DNS_R_NOTIMP : DNS_R_REFUSED);
		goto cleanup;
	}

	ns_client_log(client, NS_LOGCATEGORY_CLIENT,
		      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(5),
		      "using view '%s'", view->name);

	/*
	 * Check for a signature.  We log bad signatures regardless of
	 * whether they ultimately cause the request to be rejected or
	 * not.  We do not log the lack of a signature unless we are
	 * debugging.
	 */
	sigresult = dns_message_checksig(client->message, client->view);
	client->signer = NULL;
	dns_name_init(&client->signername, NULL);
	result = dns_message_signer(client->message, &client->signername);
	if (result == ISC_R_SUCCESS) {
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "request has valid signature");
		client->signer = &client->signername;
	} else if (result == ISC_R_NOTFOUND) {
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "request is not signed");
	} else if (result == DNS_R_NOIDENTITY) {
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "request is signed by a nonauthoritative key");
	} else {
		/* There is a signature, but it is bad. */
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_ERROR,
			      "request has invalid signature: %s",
			      isc_result_totext(result));
		/*
		 * Accept update messages signed by unknown keys so that
		 * update forwarding works transparently through slaves
		 * that don't have all the same keys as the master.
		 */
		if (!(client->message->tsigstatus == dns_tsigerror_badkey &&
		      client->message->opcode == dns_opcode_update)) {
			ns_client_error(client, sigresult);
			goto cleanup;
		}
	}

	/*
	 * Decide whether recursive service is available to this client.
	 * We do this here rather than in the query code so that we can
	 * set the RA bit correctly on all kinds of responses, not just
	 * responses to ordinary queries.
	 */
	ra = ISC_FALSE;
	if (client->view->resolver != NULL &&
	    client->view->recursion == ISC_TRUE &&
	    /* XXX this will log too much too early */
	    ns_client_checkacl(client, "recursion available:",
			       client->view->recursionacl,
			       ISC_TRUE, ISC_LOG_DEBUG(1)) == ISC_R_SUCCESS)
		ra = ISC_TRUE;

	if (ra == ISC_TRUE)
		client->attributes |= NS_CLIENTATTR_RA;

	/*
	 * Dispatch the request.
	 */
	switch (client->message->opcode) {
	case dns_opcode_query:
		CTRACE("query");
		ns_query_start(client);
		break;
	case dns_opcode_update:
		CTRACE("update");
		ns_client_settimeout(client, 60);
		ns_update_start(client, sigresult);
		break;
	case dns_opcode_notify:
		CTRACE("notify");
		ns_client_settimeout(client, 60);
		ns_notify_start(client);
		break;
	case dns_opcode_iquery:
		CTRACE("iquery");
		ns_client_error(client, DNS_R_NOTIMP);
		break;
	default:
		CTRACE("unknown opcode");
		ns_client_error(client, DNS_R_NOTIMP);
	}

 cleanup:
	return;
}

static void
client_timeout(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client;

	REQUIRE(event != NULL);
	REQUIRE(event->ev_type == ISC_TIMEREVENT_LIFE ||
		event->ev_type == ISC_TIMEREVENT_IDLE);
	client = event->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);
	REQUIRE(client->timer != NULL);

	UNUSED(task);

	CTRACE("timeout");

	isc_event_free(&event);

	if (client->shutdown != NULL) {
		(client->shutdown)(client->shutdown_arg, ISC_R_TIMEDOUT);
		client->shutdown = NULL;
		client->shutdown_arg = NULL;
	}

	if (client->newstate > NS_CLIENTSTATE_READY)
		client->newstate = NS_CLIENTSTATE_READY;
	(void) exit_check(client);
}

static isc_result_t
client_create(ns_clientmgr_t *manager, ns_client_t **clientp)
{
	ns_client_t *client;
	isc_result_t result;

	/*
	 * Caller must be holding the manager lock.
	 *
	 * Note: creating a client does not add the client to the
	 * manager's client list or set the client's manager pointer.
	 * The caller is responsible for that.
	 */

	REQUIRE(clientp != NULL && *clientp == NULL);

	client = isc_mem_get(manager->mctx, sizeof *client);
	if (client == NULL)
		return (ISC_R_NOMEMORY);

	client->task = NULL;
	result = isc_task_create(manager->taskmgr, 0, &client->task);
	if (result != ISC_R_SUCCESS)
		goto cleanup_client;
	isc_task_setname(client->task, "client", client);

	client->timer = NULL;
	result = isc_timer_create(manager->timermgr, isc_timertype_inactive,
				  NULL, NULL, client->task, client_timeout,
				  client, &client->timer);
	if (result != ISC_R_SUCCESS)
		goto cleanup_task;
	client->timerset = ISC_FALSE;

	client->message = NULL;
	result = dns_message_create(manager->mctx, DNS_MESSAGE_INTENTPARSE,
				    &client->message);
	if (result != ISC_R_SUCCESS)
		goto cleanup_timer;

	/* XXXRTH  Hardwired constants */

	client->sendevent = (isc_socketevent_t *)
			    isc_event_allocate(manager->mctx, client,
					       ISC_SOCKEVENT_SENDDONE,
					       client_senddone, client,
					       sizeof(isc_socketevent_t));
	if (client->sendevent == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_message;
	}

	client->recvbuf = isc_mem_get(manager->mctx, RECV_BUFFER_SIZE);
	if  (client->recvbuf == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_sendevent;
	}

	client->recvevent = (isc_socketevent_t *)
			    isc_event_allocate(manager->mctx, client,
					       ISC_SOCKEVENT_RECVDONE,
					       client_request, client,
					       sizeof(isc_socketevent_t));
	if (client->recvevent == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_recvbuf;
	}

	client->magic = NS_CLIENT_MAGIC;
	client->mctx = manager->mctx;
	client->manager = NULL;
	client->state = NS_CLIENTSTATE_INACTIVE;
	client->newstate = NS_CLIENTSTATE_MAX;
	client->naccepts = 0;
	client->nreads = 0;
	client->nsends = 0;
	client->nrecvs = 0;
	client->nupdates = 0;
	client->nctls = 0;
	client->references = 0;
	client->attributes = 0;
	client->view = NULL;
	client->dispatch = NULL;
	client->udpsocket = NULL;
	client->tcplistener = NULL;
	client->tcpsocket = NULL;
	client->tcpmsg_valid = ISC_FALSE;
	client->tcpbuf = NULL;
	client->opt = NULL;
	client->udpsize = 512;
	client->extflags = 0;
	client->next = NULL;
	client->shutdown = NULL;
	client->shutdown_arg = NULL;
	dns_name_init(&client->signername, NULL);
	client->mortal = ISC_FALSE;
	client->tcpquota = NULL;
	client->recursionquota = NULL;
	client->interface = NULL;
	client->peeraddr_valid = ISC_FALSE;
	ISC_EVENT_INIT(&client->ctlevent, sizeof(client->ctlevent), 0, NULL,
		       NS_EVENT_CLIENTCONTROL, client_start, client, client,
		       NULL, NULL);
	/*
	 * Initialize FORMERR cache to sentinel value that will not match
	 * any actual FORMERR response.
	 */
	isc_sockaddr_any(&client->formerrcache.addr);
	client->formerrcache.time = 0;
	client->formerrcache.id = 0;
	ISC_LINK_INIT(client, link);
	client->list = NULL;

	/*
	 * We call the init routines for the various kinds of client here,
	 * after we have created an otherwise valid client, because some
	 * of them call routines that REQUIRE(NS_CLIENT_VALID(client)).
	 */
	result = ns_query_init(client);
	if (result != ISC_R_SUCCESS)
		goto cleanup_recvevent;

	result = isc_task_onshutdown(client->task, client_shutdown, client);
	if (result != ISC_R_SUCCESS)
		goto cleanup_query;

	CTRACE("create");

	*clientp = client;

	return (ISC_R_SUCCESS);

 cleanup_query:
	ns_query_free(client);

 cleanup_recvevent:
	isc_event_free((isc_event_t **)&client->recvevent);

 cleanup_recvbuf:
	isc_mem_put(manager->mctx, client->recvbuf, RECV_BUFFER_SIZE);

 cleanup_sendevent:
	isc_event_free((isc_event_t **)&client->sendevent);

	client->magic = 0;

 cleanup_message:
	dns_message_destroy(&client->message);

 cleanup_timer:
	isc_timer_detach(&client->timer);

 cleanup_task:
	isc_task_detach(&client->task);

 cleanup_client:
	isc_mem_put(manager->mctx, client, sizeof *client);

	return (result);
}

static void
client_read(ns_client_t *client) {
	isc_result_t result;

	CTRACE("read");

	result = dns_tcpmsg_readmessage(&client->tcpmsg, client->task,
					client_request, client);
	if (result != ISC_R_SUCCESS)
		goto fail;

	/*
	 * Set a timeout to limit the amount of time we will wait
	 * for a request on this TCP connection.
	 */
	ns_client_settimeout(client, 30);

	client->state = client->newstate = NS_CLIENTSTATE_READING;
	INSIST(client->nreads == 0);
	INSIST(client->recursionquota == NULL);
	client->nreads++;

	return;
 fail:
	ns_client_next(client, result);
}

static void
client_newconn(isc_task_t *task, isc_event_t *event) {
	ns_client_t *client = event->ev_arg;
	isc_socket_newconnev_t *nevent = (isc_socket_newconnev_t *)event;
	isc_result_t result;

	REQUIRE(event->ev_type == ISC_SOCKEVENT_NEWCONN);
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(client->task == task);

	UNUSED(task);

	INSIST(client->state == NS_CLIENTSTATE_READY);

	INSIST(client->naccepts == 1);
	client->naccepts--;

	LOCK(&client->interface->lock);
	INSIST(client->interface->ntcpcurrent > 0);
	client->interface->ntcpcurrent--;
	UNLOCK(&client->interface->lock);

	/*
	 * We must take ownership of the new socket before the exit
	 * check to make sure it gets destroyed if we decide to exit.
	 */
	if (nevent->result == ISC_R_SUCCESS) {
		client->tcpsocket = nevent->newsocket;
		client->state = NS_CLIENTSTATE_READING;
		INSIST(client->recursionquota == NULL);

		(void) isc_socket_getpeername(client->tcpsocket,
					      &client->peeraddr);
		client->peeraddr_valid = ISC_TRUE;
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			   NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			   "new TCP connection");
	} else {
		/*
		 * XXXRTH  What should we do?  We're trying to accept but
		 *         it didn't work.  If we just give up, then TCP
		 *	   service may eventually stop.
		 *
		 *	   For now, we just go idle.
		 *
		 *	   Going idle is probably the right thing if the
		 *	   I/O was canceled.
		 */
		ns_client_log(client, NS_LOGCATEGORY_CLIENT,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "accept failed: %s",
			      isc_result_totext(nevent->result));
	}

	if (exit_check(client))
		goto freeevent;

	if (nevent->result == ISC_R_SUCCESS) {
		int match;
		isc_netaddr_t netaddr;

		isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);

		if (ns_g_server->blackholeacl != NULL &&
		    dns_acl_match(&netaddr, NULL,
			    	  ns_g_server->blackholeacl,
				  &ns_g_server->aclenv,
				  &match, NULL) == ISC_R_SUCCESS &&
		    match > 0)
		{
			ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
				      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(10),
				      "blackholed connection attempt");
			client->newstate = NS_CLIENTSTATE_READY;
			(void)exit_check(client);
			goto freeevent;
		}

		INSIST(client->tcpmsg_valid == ISC_FALSE);
		dns_tcpmsg_init(client->mctx, client->tcpsocket,
				&client->tcpmsg);
		client->tcpmsg_valid = ISC_TRUE;

		/*
		 * Let a new client take our place immediately, before
		 * we wait for a request packet.  If we don't,
		 * telnetting to port 53 (once per CPU) will
		 * deny service to legititmate TCP clients.
		 */
		result = isc_quota_attach(&ns_g_server->tcpquota,
					  &client->tcpquota);
		if (result == ISC_R_SUCCESS)
			result = ns_client_replace(client);
		if (result != ISC_R_SUCCESS) {
			ns_client_log(client, NS_LOGCATEGORY_CLIENT,
				      NS_LOGMODULE_CLIENT, ISC_LOG_WARNING,
				      "no more TCP clients: %s",
				      isc_result_totext(result));
		}

		client_read(client);
	}

 freeevent:
	isc_event_free(&event);
}

static void
client_accept(ns_client_t *client) {
	isc_result_t result;

	CTRACE("accept");

	result = isc_socket_accept(client->tcplistener, client->task,
				   client_newconn, client);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_socket_accept() failed: %s",
				 isc_result_totext(result));
		/*
		 * XXXRTH  What should we do?  We're trying to accept but
		 *         it didn't work.  If we just give up, then TCP
		 *	   service may eventually stop.
		 *
		 *	   For now, we just go idle.
		 */
		return;
	}
	INSIST(client->naccepts == 0);
	client->naccepts++;
	LOCK(&client->interface->lock);
	client->interface->ntcpcurrent++;
	UNLOCK(&client->interface->lock);
}

static void
client_udprecv(ns_client_t *client) {
	isc_result_t result;
	isc_region_t r;

	CTRACE("udprecv");

	r.base = client->recvbuf;
	r.length = RECV_BUFFER_SIZE;
	result = isc_socket_recv2(client->udpsocket, &r, 1,
				  client->task, client->recvevent, 0);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_socket_recv() failed: %s",
				 isc_result_totext(result));
		/*
		 * This cannot happen in the current implementation, since
		 * isc_socket_recv2() cannot fail if flags == 0A
		 *
		 * If this does fail, we just go idle.
		 */
		return;
	}
	INSIST(client->nrecvs == 0);
	client->nrecvs++;
}

void
ns_client_attach(ns_client_t *source, ns_client_t **targetp) {
	REQUIRE(NS_CLIENT_VALID(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	source->references++;
	ns_client_log(source, NS_LOGCATEGORY_CLIENT,
		      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(10),
		      "ns_client_attach: ref = %d", source->references);
	*targetp = source;
}

void
ns_client_detach(ns_client_t **clientp) {
	ns_client_t *client = *clientp;

	client->references--;
	INSIST(client->references >= 0);
	*clientp = NULL;
	ns_client_log(client, NS_LOGCATEGORY_CLIENT,
		      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(10),
		      "ns_client_detach: ref = %d", client->references);
	(void) exit_check(client);
}

isc_boolean_t
ns_client_shuttingdown(ns_client_t *client) {
	return (ISC_TF(client->newstate == NS_CLIENTSTATE_FREED));
}

isc_result_t
ns_client_replace(ns_client_t *client) {
	isc_result_t result;

	CTRACE("replace");

	result = ns_clientmgr_createclients(client->manager,
					    1, client->interface,
					    (TCP_CLIENT(client) ?
					     ISC_TRUE : ISC_FALSE));
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * The responsibility for listening for new requests is hereby
	 * transferred to the new client.  Therefore, the old client
	 * should refrain from listening for any more requests.
	 */
	client->mortal = ISC_TRUE;

	return (ISC_R_SUCCESS);
}

/***
 *** Client Manager
 ***/

static void
clientmgr_destroy(ns_clientmgr_t *manager) {
	REQUIRE(ISC_LIST_EMPTY(manager->active));
	REQUIRE(ISC_LIST_EMPTY(manager->inactive));

	MTRACE("clientmgr_destroy");

	DESTROYLOCK(&manager->lock);
	manager->magic = 0;
	isc_mem_put(manager->mctx, manager, sizeof *manager);
}

isc_result_t
ns_clientmgr_create(isc_mem_t *mctx, isc_taskmgr_t *taskmgr,
		    isc_timermgr_t *timermgr, ns_clientmgr_t **managerp)
{
	ns_clientmgr_t *manager;
	isc_result_t result;

	manager = isc_mem_get(mctx, sizeof *manager);
	if (manager == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_mutex_init(&manager->lock);
	if (result != ISC_R_SUCCESS)
		goto cleanup_manager;

	manager->mctx = mctx;
	manager->taskmgr = taskmgr;
	manager->timermgr = timermgr;
	manager->exiting = ISC_FALSE;
	ISC_LIST_INIT(manager->active);
	ISC_LIST_INIT(manager->inactive);
	manager->magic = MANAGER_MAGIC;

	MTRACE("create");

	*managerp = manager;

	return (ISC_R_SUCCESS);

 cleanup_manager:
	isc_mem_put(manager->mctx, manager, sizeof *manager);

	return (result);
}

void
ns_clientmgr_destroy(ns_clientmgr_t **managerp) {
	ns_clientmgr_t *manager;
	ns_client_t *client;
	isc_boolean_t need_destroy = ISC_FALSE;

	REQUIRE(managerp != NULL);
	manager = *managerp;
	REQUIRE(VALID_MANAGER(manager));

	MTRACE("destroy");

	LOCK(&manager->lock);

	manager->exiting = ISC_TRUE;

	for (client = ISC_LIST_HEAD(manager->active);
	     client != NULL;
	     client = ISC_LIST_NEXT(client, link))
		isc_task_shutdown(client->task);

	for (client = ISC_LIST_HEAD(manager->inactive);
	     client != NULL;
	     client = ISC_LIST_NEXT(client, link))
		isc_task_shutdown(client->task);

	if (ISC_LIST_EMPTY(manager->active) &&
	    ISC_LIST_EMPTY(manager->inactive))
		need_destroy = ISC_TRUE;

	UNLOCK(&manager->lock);

	if (need_destroy)
		clientmgr_destroy(manager);

	*managerp = NULL;
}

isc_result_t
ns_clientmgr_createclients(ns_clientmgr_t *manager, unsigned int n,
			   ns_interface_t *ifp, isc_boolean_t tcp)
{
	isc_result_t result = ISC_R_SUCCESS;
	unsigned int i;
	ns_client_t *client;

	REQUIRE(VALID_MANAGER(manager));
	REQUIRE(n > 0);

	MTRACE("createclients");

	/*
	 * We MUST lock the manager lock for the entire client creation
	 * process.  If we didn't do this, then a client could get a
	 * shutdown event and disappear out from under us.
	 */

	LOCK(&manager->lock);

	for (i = 0; i < n; i++) {
		isc_event_t *ev;
		/*
		 * Allocate a client.  First try to get a recycled one;
		 * if that fails, make a new one.
		 */
		client = ISC_LIST_HEAD(manager->inactive);
		if (client != NULL) {
			MTRACE("recycle");
			ISC_LIST_UNLINK(manager->inactive, client, link);
			client->list = NULL;
		} else {
			MTRACE("create new");
			result = client_create(manager, &client);
			if (result != ISC_R_SUCCESS)
				break;
		}

		ns_interface_attach(ifp, &client->interface);
		client->state = NS_CLIENTSTATE_READY;
		INSIST(client->recursionquota == NULL);

		if (tcp) {
			client->attributes |= NS_CLIENTATTR_TCP;
			isc_socket_attach(ifp->tcpsocket,
					  &client->tcplistener);
		} else {
			isc_socket_t *sock;

			dns_dispatch_attach(ifp->udpdispatch,
					    &client->dispatch);
			sock = dns_dispatch_getsocket(client->dispatch);
			isc_socket_attach(sock, &client->udpsocket);
		}
		client->manager = manager;
		ISC_LIST_APPEND(manager->active, client, link);
		client->list = &manager->active;

		INSIST(client->nctls == 0);
		client->nctls++;
		ev = &client->ctlevent;
		isc_task_send(client->task, &ev);
	}
	if (i != 0) {
		/*
		 * We managed to create at least one client, so we
		 * declare victory.
		 */
		result = ISC_R_SUCCESS;
	}

	UNLOCK(&manager->lock);

	return (result);
}

isc_sockaddr_t *
ns_client_getsockaddr(ns_client_t *client) {
	return (&client->peeraddr);
}

isc_result_t
ns_client_checkaclsilent(ns_client_t *client, dns_acl_t *acl,
			 isc_boolean_t default_allow)
{
	isc_result_t result;
	int match;
	isc_netaddr_t netaddr;

	if (acl == NULL) {
		if (default_allow)
			goto allow;
		else
			goto deny;
	}

	isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);

	result = dns_acl_match(&netaddr, client->signer, acl,
			       &ns_g_server->aclenv,
			       &match, NULL);
	if (result != ISC_R_SUCCESS)
		goto deny; /* Internal error, already logged. */
	if (match > 0)
		goto allow;
	goto deny; /* Negative match or no match. */

 allow:
	return (ISC_R_SUCCESS);

 deny:
	return (DNS_R_REFUSED);
}

isc_result_t
ns_client_checkacl(ns_client_t *client,
		   const char *opname, dns_acl_t *acl,
		   isc_boolean_t default_allow, int log_level)
{
	isc_result_t result =
		ns_client_checkaclsilent(client, acl, default_allow);

	if (result == ISC_R_SUCCESS) 
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(3),
			      "%s approved", opname);
	else
		ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
			      NS_LOGMODULE_CLIENT,
			      log_level, "%s denied", opname);
	return (result);
}

static void
ns_client_name(ns_client_t *client, char *peerbuf, size_t len) {
	if (client->peeraddr_valid)
		isc_sockaddr_format(&client->peeraddr, peerbuf, len);
	else
		snprintf(peerbuf, len, "@%p", client);
}

static void
ns_client_logv(ns_client_t *client, isc_logcategory_t *category,
	   isc_logmodule_t *module, int level, const char *fmt, va_list ap)
     ISC_FORMAT_PRINTF(5, 0);

static void
ns_client_logv(ns_client_t *client, isc_logcategory_t *category,
	   isc_logmodule_t *module, int level, const char *fmt, va_list ap)
{
	char msgbuf[2048];
	char peerbuf[ISC_SOCKADDR_FORMATSIZE];

	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	ns_client_name(client, peerbuf, sizeof peerbuf);

	isc_log_write(ns_g_lctx, category, module, level,
		      "client %s: %s", peerbuf, msgbuf);
}

void
ns_client_log(ns_client_t *client, isc_logcategory_t *category,
	   isc_logmodule_t *module, int level, const char *fmt, ...)
{
	va_list ap;

	if (! isc_log_wouldlog(ns_g_lctx, level))
		return;

	va_start(ap, fmt);
	ns_client_logv(client, category, module, level, fmt, ap);
	va_end(ap);
}

void
ns_client_aclmsg(const char *msg, dns_name_t *name, dns_rdataclass_t rdclass,
		 char *buf, size_t len) 
{
        char namebuf[DNS_NAME_FORMATSIZE];
        char classbuf[DNS_RDATACLASS_FORMATSIZE];

        dns_name_format(name, namebuf, sizeof(namebuf));
        dns_rdataclass_format(rdclass, classbuf, sizeof(classbuf));
        (void)snprintf(buf, len, "%s '%s/%s'", msg, namebuf, classbuf);
}

static void
ns_client_dumpmessage(ns_client_t *client, const char *reason) {
	isc_buffer_t buffer;
	char *buf = NULL;
	int len = 1024;
	isc_result_t result;

	/*
	 * Note that these are multiline debug messages.  We want a newline
	 * to appear in the log after each message.
	 */

	do {
		buf = isc_mem_get(client->mctx, len);
		if (buf == NULL)
			break;
		isc_buffer_init(&buffer, buf, len);
		result = dns_message_totext(client->message,
					    &dns_master_style_debug,
					    0, &buffer);
		if (result == ISC_R_NOSPACE) {
			isc_mem_put(client->mctx, buf, len);
			len += 1024;
		} else if (result == ISC_R_SUCCESS)
		        ns_client_log(client, NS_LOGCATEGORY_UNMATCHED,
				      NS_LOGMODULE_CLIENT, ISC_LOG_DEBUG(1),
				      "%s\n%.*s", reason,
				       (int)isc_buffer_usedlength(&buffer),
				       buf);
	} while (result == ISC_R_NOSPACE);

	if (buf != NULL)
		isc_mem_put(client->mctx, buf, len);
}
