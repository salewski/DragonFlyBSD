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

/* $Id: query.c,v 1.198.2.19 2004/04/15 02:16:25 marka Exp $ */

#include <config.h>

#include <string.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/byaddr.h>
#include <dns/db.h>
#include <dns/events.h>
#include <dns/message.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/stats.h>
#include <dns/tkey.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/client.h>
#include <named/log.h>
#include <named/server.h>
#include <named/sortlist.h>
#include <named/xfrout.h>

#define PARTIALANSWER(c)	(((c)->query.attributes & \
				  NS_QUERYATTR_PARTIALANSWER) != 0)
#define USECACHE(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_CACHEOK) != 0)
#define RECURSIONOK(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_RECURSIONOK) != 0)
#define RECURSING(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_RECURSING) != 0)
#define CACHEGLUEOK(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_CACHEGLUEOK) != 0)
#define WANTRECURSION(c)	(((c)->query.attributes & \
				  NS_QUERYATTR_WANTRECURSION) != 0)
#define WANTDNSSEC(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_WANTDNSSEC) != 0)
#define NOAUTHORITY(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_NOAUTHORITY) != 0)
#define NOADDITIONAL(c)		(((c)->query.attributes & \
				  NS_QUERYATTR_NOADDITIONAL) != 0)

#if 0
#define CTRACE(m)       isc_log_write(ns_g_lctx, \
				      NS_LOGCATEGORY_CLIENT, \
				      NS_LOGMODULE_QUERY, \
				      ISC_LOG_DEBUG(3), \
				      "client %p: %s", client, (m))
#define QTRACE(m)       isc_log_write(ns_g_lctx, \
				      NS_LOGCATEGORY_GENERAL, \
				      NS_LOGMODULE_QUERY, \
				      ISC_LOG_DEBUG(3), \
				      "query %p: %s", query, (m))
#else
#define CTRACE(m) ((void)m)
#define QTRACE(m) ((void)m)
#endif

#define DNS_GETDB_NOEXACT 0x01U
#define DNS_GETDB_NOLOG 0x02U

static unsigned char ip6int_ndata[] = "\003ip6\003int";
static unsigned char ip6int_offsets[] = { 0, 4, 8 };

static dns_name_t ip6int_name = {
	DNS_NAME_MAGIC,
	ip6int_ndata, 9, 3,
	DNS_NAMEATTR_READONLY | DNS_NAMEATTR_ABSOLUTE,
	ip6int_offsets, NULL,
	{(void *)-1, (void *)-1},
	{NULL, NULL}
};

static isc_result_t
query_simplefind(void *arg, dns_name_t *name, dns_rdatatype_t type,
		 isc_stdtime_t now,
		 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);

static inline void
query_adda6rrset(void *arg, dns_name_t *name, dns_rdataset_t *rdataset,
		      dns_rdataset_t *sigrdataset);

static void
query_find(ns_client_t *client, dns_fetchevent_t *event, dns_rdatatype_t qtype);

static void
synth_fwd_start(ns_client_t *client);

static void
synth_fwd_startfind(ns_client_t *client);

static void
synth_fwd_respond(ns_client_t *client, dns_adbfind_t *find);

static void
synth_fwd_finddone(isc_task_t *task, isc_event_t *ev);

static void
synth_finish(ns_client_t *client, isc_result_t result);

static void
synth_rev_start(ns_client_t *client);

static void
synth_rev_byaddrdone_arpa(isc_task_t *task, isc_event_t *event);

static void
synth_rev_byaddrdone_int(isc_task_t *task, isc_event_t *event);

static void
synth_rev_respond(ns_client_t *client, dns_byaddrevent_t *bevent);

/*
 * Increment query statistics counters.
 */
static inline void
inc_stats(ns_client_t *client, dns_statscounter_t counter) {
	dns_zone_t *zone = client->query.authzone;
	
	REQUIRE(counter < DNS_STATS_NCOUNTERS);

	ns_g_server->querystats[counter]++;

	if (zone != NULL) {
		isc_uint64_t *zonestats = dns_zone_getstatscounters(zone);
		if (zonestats != NULL)
			zonestats[counter]++;
	}
}

static void
query_send(ns_client_t *client) {
	dns_statscounter_t counter;
	if (client->message->rcode == dns_rcode_noerror) {
		if (ISC_LIST_EMPTY(client->message->sections[DNS_SECTION_ANSWER])) {
			if (client->query.isreferral) {
				counter = dns_statscounter_referral;
			} else {
				counter = dns_statscounter_nxrrset;
			}
		} else {
			counter = dns_statscounter_success;
		}
	} else if (client->message->rcode == dns_rcode_nxdomain) {
		counter = dns_statscounter_nxdomain;
	} else {
		/* We end up here in case of YXDOMAIN, and maybe others */
		counter = dns_statscounter_failure;
	}
	inc_stats(client, counter);
	ns_client_send(client);
}

static void
query_error(ns_client_t *client, isc_result_t result) {
	inc_stats(client, dns_statscounter_failure);
	ns_client_error(client, result);
}

static void
query_next(ns_client_t *client, isc_result_t result) {
	inc_stats(client, dns_statscounter_failure);
	ns_client_next(client, result);
}

static inline void
query_maybeputqname(ns_client_t *client) {
	if (client->query.restarts > 0) {
		/*
		 * client->query.qname was dynamically allocated.
		 */
		dns_message_puttempname(client->message,
					&client->query.qname);
		client->query.qname = NULL;
	}
}

static inline void
query_reset(ns_client_t *client, isc_boolean_t everything) {
	isc_buffer_t *dbuf, *dbuf_next;
	ns_dbversion_t *dbversion, *dbversion_next;
	unsigned int i;

	/*
	 * Reset the query state of a client to its default state.
	 */

	/*
	 * Cancel the fetch if it's running.
	 */
	if (client->query.fetch != NULL) {
		dns_resolver_cancelfetch(client->query.fetch);

		client->query.fetch = NULL;
	}

	/*
	 * Cleanup any active versions.
	 */
	for (dbversion = ISC_LIST_HEAD(client->query.activeversions);
	     dbversion != NULL;
	     dbversion = dbversion_next) {
		dbversion_next = ISC_LIST_NEXT(dbversion, link);
		dns_db_closeversion(dbversion->db, &dbversion->version,
				    ISC_FALSE);
		dns_db_detach(&dbversion->db);
		ISC_LIST_INITANDAPPEND(client->query.freeversions,
				      dbversion, link);
	}
	ISC_LIST_INIT(client->query.activeversions);

	if (client->query.authdb != NULL)
		dns_db_detach(&client->query.authdb);
	if (client->query.authzone != NULL)
		dns_zone_detach(&client->query.authzone);

	/*
	 * Clean up free versions.
	 */
	for (dbversion = ISC_LIST_HEAD(client->query.freeversions), i = 0;
	     dbversion != NULL;
	     dbversion = dbversion_next, i++) {
		dbversion_next = ISC_LIST_NEXT(dbversion, link);
		/*
		 * If we're not freeing everything, we keep the first three
		 * dbversions structures around.
		 */
		if (i > 3 || everything) {
			ISC_LIST_UNLINK(client->query.freeversions, dbversion,
					link);
			isc_mem_put(client->mctx, dbversion,
				    sizeof *dbversion);
		}
	}

	for (dbuf = ISC_LIST_HEAD(client->query.namebufs);
	     dbuf != NULL;
	     dbuf = dbuf_next) {
		dbuf_next = ISC_LIST_NEXT(dbuf, link);
		if (dbuf_next != NULL || everything) {
			ISC_LIST_UNLINK(client->query.namebufs, dbuf, link);
			isc_buffer_free(&dbuf);
		}
	}

	query_maybeputqname(client);

	client->query.attributes = (NS_QUERYATTR_RECURSIONOK |
				    NS_QUERYATTR_CACHEOK);
	client->query.restarts = 0;
	client->query.timerset = ISC_FALSE;
	client->query.origqname = NULL;
	client->query.qname = NULL;
	client->query.dboptions = 0;
	client->query.fetchoptions = 0;
	client->query.gluedb = NULL;
	client->query.authdbset = ISC_FALSE;
	client->query.isreferral = ISC_FALSE;
}

static void
query_next_callback(ns_client_t *client) {
	query_reset(client, ISC_FALSE);
}

void
ns_query_free(ns_client_t *client) {
	query_reset(client, ISC_TRUE);
}

static inline isc_result_t
query_newnamebuf(ns_client_t *client) {
	isc_buffer_t *dbuf;
	isc_result_t result;

	CTRACE("query_newnamebuf");
	/*
	 * Allocate a name buffer.
	 */

	dbuf = NULL;
	result = isc_buffer_allocate(client->mctx, &dbuf, 1024);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_newnamebuf: isc_buffer_allocate failed: done");
		return (result);
	}
	ISC_LIST_APPEND(client->query.namebufs, dbuf, link);

	CTRACE("query_newnamebuf: done");
	return (ISC_R_SUCCESS);
}

static inline isc_buffer_t *
query_getnamebuf(ns_client_t *client) {
	isc_buffer_t *dbuf;
	isc_result_t result;
	isc_region_t r;

	CTRACE("query_getnamebuf");
	/*
	 * Return a name buffer with space for a maximal name, allocating
	 * a new one if necessary.
	 */

	if (ISC_LIST_EMPTY(client->query.namebufs)) {
		result = query_newnamebuf(client);
		if (result != ISC_R_SUCCESS) {
		    CTRACE("query_getnamebuf: query_newnamebuf failed: done");
			return (NULL);
		}
	}

	dbuf = ISC_LIST_TAIL(client->query.namebufs);
	INSIST(dbuf != NULL);
	isc_buffer_availableregion(dbuf, &r);
	if (r.length < 255) {
		result = query_newnamebuf(client);
		if (result != ISC_R_SUCCESS) {
		    CTRACE("query_getnamebuf: query_newnamebuf failed: done");
			return (NULL);

		}
		dbuf = ISC_LIST_TAIL(client->query.namebufs);
		isc_buffer_availableregion(dbuf, &r);
		INSIST(r.length >= 255);
	}
	CTRACE("query_getnamebuf: done");
	return (dbuf);
}

static inline void
query_keepname(ns_client_t *client, dns_name_t *name, isc_buffer_t *dbuf) {
	isc_region_t r;

	CTRACE("query_keepname");
	/*
	 * 'name' is using space in 'dbuf', but 'dbuf' has not yet been
	 * adjusted to take account of that.  We do the adjustment.
	 */

	REQUIRE((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED) != 0);

	dns_name_toregion(name, &r);
	isc_buffer_add(dbuf, r.length);
	dns_name_setbuffer(name, NULL);
	client->query.attributes &= ~NS_QUERYATTR_NAMEBUFUSED;
}

static inline void
query_releasename(ns_client_t *client, dns_name_t **namep) {
	dns_name_t *name = *namep;

	/*
	 * 'name' is no longer needed.  Return it to our pool of temporary
	 * names.  If it is using a name buffer, relinquish its exclusive
	 * rights on the buffer.
	 */

	CTRACE("query_releasename");
	if (dns_name_hasbuffer(name)) {
		INSIST((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED)
		       != 0);
		client->query.attributes &= ~NS_QUERYATTR_NAMEBUFUSED;
	}
	dns_message_puttempname(client->message, namep);
	CTRACE("query_releasename: done");
}

static inline dns_name_t *
query_newname(ns_client_t *client, isc_buffer_t *dbuf,
	      isc_buffer_t *nbuf)
{
	dns_name_t *name;
	isc_region_t r;
	isc_result_t result;

	REQUIRE((client->query.attributes & NS_QUERYATTR_NAMEBUFUSED) == 0);

	CTRACE("query_newname");
	name = NULL;
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_newname: dns_message_gettempname failed: done");
		return (NULL);
	}
	isc_buffer_availableregion(dbuf, &r);
	isc_buffer_init(nbuf, r.base, r.length);
	dns_name_init(name, NULL);
	dns_name_setbuffer(name, nbuf);
	client->query.attributes |= NS_QUERYATTR_NAMEBUFUSED;

	CTRACE("query_newname: done");
	return (name);
}

static inline dns_rdataset_t *
query_newrdataset(ns_client_t *client) {
	dns_rdataset_t *rdataset;
	isc_result_t result;

	CTRACE("query_newrdataset");
	rdataset = NULL;
	result = dns_message_gettemprdataset(client->message, &rdataset);
	if (result != ISC_R_SUCCESS) {
	  CTRACE("query_newrdataset: "
		 "dns_message_gettemprdataset failed: done");
		return (NULL);
	}
	dns_rdataset_init(rdataset);

	CTRACE("query_newrdataset: done");
	return (rdataset);
}

static inline void
query_putrdataset(ns_client_t *client, dns_rdataset_t **rdatasetp) {
	dns_rdataset_t *rdataset = *rdatasetp;

	CTRACE("query_putrdataset");
	if (rdataset != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_message_puttemprdataset(client->message, rdatasetp);
	}
	CTRACE("query_putrdataset: done");
}


static inline isc_result_t
query_newdbversion(ns_client_t *client, unsigned int n) {
	unsigned int i;
	ns_dbversion_t *dbversion;

	for (i = 0; i < n; i++) {
		dbversion = isc_mem_get(client->mctx, sizeof *dbversion);
		if (dbversion != NULL) {
			dbversion->db = NULL;
			dbversion->version = NULL;
			ISC_LIST_INITANDAPPEND(client->query.freeversions,
					      dbversion, link);
		} else {
			/*
			 * We only return ISC_R_NOMEMORY if we couldn't
			 * allocate anything.
			 */
			if (i == 0)
				return (ISC_R_NOMEMORY);
			else
				return (ISC_R_SUCCESS);
		}
	}

	return (ISC_R_SUCCESS);
}

static inline ns_dbversion_t *
query_getdbversion(ns_client_t *client) {
	isc_result_t result;
	ns_dbversion_t *dbversion;

	if (ISC_LIST_EMPTY(client->query.freeversions)) {
		result = query_newdbversion(client, 1);
		if (result != ISC_R_SUCCESS)
			return (NULL);
	}
	dbversion = ISC_LIST_HEAD(client->query.freeversions);
	INSIST(dbversion != NULL);
	ISC_LIST_UNLINK(client->query.freeversions, dbversion, link);

	return (dbversion);
}

isc_result_t
ns_query_init(ns_client_t *client) {
	isc_result_t result;

	ISC_LIST_INIT(client->query.namebufs);
	ISC_LIST_INIT(client->query.activeversions);
	ISC_LIST_INIT(client->query.freeversions);
	client->query.restarts = 0;
	client->query.timerset = ISC_FALSE;
	client->query.qname = NULL;
	client->query.fetch = NULL;
	client->query.authdb = NULL;
	client->query.authzone = NULL;
	client->query.authdbset = ISC_FALSE;
	client->query.isreferral = ISC_FALSE;	
	query_reset(client, ISC_FALSE);
	result = query_newdbversion(client, 3);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_a6_init(&client->query.a6ctx, query_simplefind, query_adda6rrset,
		    NULL, NULL, client);
	return (query_newnamebuf(client));
}

static inline ns_dbversion_t *
query_findversion(ns_client_t *client, dns_db_t *db,
		  isc_boolean_t *newzonep)
{
	ns_dbversion_t *dbversion;

	/*
	 * We may already have done a query related to this
	 * database.  If so, we must be sure to make subsequent
	 * queries from the same version.
	 */
	for (dbversion = ISC_LIST_HEAD(client->query.activeversions);
	     dbversion != NULL;
	     dbversion = ISC_LIST_NEXT(dbversion, link)) {
		if (dbversion->db == db)
			break;
	}

	if (dbversion == NULL) {
		/*
		 * This is a new zone for this query.  Add it to
		 * the active list.
		 */
		dbversion = query_getdbversion(client);
		if (dbversion == NULL)
			return (NULL);
		dns_db_attach(db, &dbversion->db);
		dns_db_currentversion(db, &dbversion->version);
		dbversion->queryok = ISC_FALSE;
		ISC_LIST_APPEND(client->query.activeversions,
				dbversion, link);
		*newzonep = ISC_TRUE;
	} else
		*newzonep = ISC_FALSE;

	return (dbversion);
}

static inline isc_result_t
query_getzonedb(ns_client_t *client, dns_name_t *name, unsigned int options,
		dns_zone_t **zonep, dns_db_t **dbp, dns_dbversion_t **versionp)
{
	isc_result_t result;
	isc_boolean_t check_acl, new_zone;
	dns_acl_t *queryacl;
	ns_dbversion_t *dbversion;
	unsigned int ztoptions;
	dns_zone_t *zone = NULL;
	dns_db_t *db = NULL;

	REQUIRE(zonep != NULL && *zonep == NULL);
	REQUIRE(dbp != NULL && *dbp == NULL);

	/*
	 * Find a zone database to answer the query.
	 */
	ztoptions = ((options & DNS_GETDB_NOEXACT) != 0) ?
		DNS_ZTFIND_NOEXACT : 0;

	result = dns_zt_find(client->view->zonetable, name, ztoptions, NULL,
			     &zone);
	if (result == ISC_R_SUCCESS || result == DNS_R_PARTIALMATCH)
		result = dns_zone_getdb(zone, &db);

	if (result != ISC_R_SUCCESS)		
		goto fail;

	/*
	 * This limits our searching to the zone where the first name
	 * (the query target) was looked for.  This prevents following
	 * CNAMES or DNAMES into other zones and prevents returning 
	 * additional data from other zones.
	 */
	if (!client->view->additionalfromauth &&
	    client->query.authdbset &&
	    db != client->query.authdb)
		goto refuse;

	/*
	 * If the zone has an ACL, we'll check it, otherwise
	 * we use the view's "allow-query" ACL.  Each ACL is only checked
	 * once per query.
	 *
	 * Also, get the database version to use.
	 */

	check_acl = ISC_TRUE;	/* Keep compiler happy. */
	queryacl = NULL;

	/*
	 * Get the current version of this database.
	 */
	dbversion = query_findversion(client, db, &new_zone);
	if (dbversion == NULL) {
		result = DNS_R_SERVFAIL;
		goto fail;
	}
	if (new_zone) {
		check_acl = ISC_TRUE;
	} else if (!dbversion->queryok) {
		goto refuse;
	} else {
		check_acl = ISC_FALSE;
	}

	queryacl = dns_zone_getqueryacl(zone);
	if (queryacl == NULL) {
		queryacl = client->view->queryacl;
		if ((client->query.attributes &
		     NS_QUERYATTR_QUERYOKVALID) != 0) {
			/*
			 * We've evaluated the view's queryacl already.  If
			 * NS_QUERYATTR_QUERYOK is set, then the client is
			 * allowed to make queries, otherwise the query should
			 * be refused.
			 */
			check_acl = ISC_FALSE;
			if ((client->query.attributes &
			     NS_QUERYATTR_QUERYOK) == 0)
				goto refuse;
		} else {
			/*
			 * We haven't evaluated the view's queryacl yet.
			 */
			check_acl = ISC_TRUE;
		}
	}

	if (check_acl) {
		isc_boolean_t log = ISC_TF((options & DNS_GETDB_NOLOG) == 0);

		result = ns_client_checkaclsilent(client, queryacl, ISC_TRUE);
		if (log) {
			char msg[DNS_NAME_FORMATSIZE + DNS_RDATACLASS_FORMATSIZE
				+ sizeof "query '/'"];
			if (result == ISC_R_SUCCESS) {
				if (isc_log_wouldlog(ns_g_lctx,
						     ISC_LOG_DEBUG(3)))
				{
					ns_client_aclmsg("query", name,
							 client->view->rdclass,
							 msg, sizeof(msg));
					ns_client_log(client,
						      DNS_LOGCATEGORY_SECURITY,
						      NS_LOGMODULE_QUERY,
						      ISC_LOG_DEBUG(3),
						      "%s approved", msg);
				}
		    	} else {
				ns_client_aclmsg("query", name,
						 client->view->rdclass,
						 msg, sizeof(msg));
				ns_client_log(client, DNS_LOGCATEGORY_SECURITY,
					      NS_LOGMODULE_QUERY, ISC_LOG_INFO,
					      "%s denied", msg);
			}
		}

		if (queryacl == client->view->queryacl) {
			if (result == ISC_R_SUCCESS) {
				/*
				 * We were allowed by the default
				 * "allow-query" ACL.  Remember this so we
				 * don't have to check again.
				 */
				client->query.attributes |=
					NS_QUERYATTR_QUERYOK;
			}
			/*
			 * We've now evaluated the view's query ACL, and
			 * the NS_QUERYATTR_QUERYOK attribute is now valid.
			 */
			client->query.attributes |= NS_QUERYATTR_QUERYOKVALID;
		}

		if (result != ISC_R_SUCCESS)
			goto refuse;
	}

	/* Approved. */

	/*
	 * Remember the result of the ACL check so we
	 * don't have to check again.
	 */
	dbversion->queryok = ISC_TRUE;

	/* Transfer ownership. */
	*zonep = zone;
	*dbp = db;
	*versionp = dbversion->version;

	return (ISC_R_SUCCESS);

 refuse:
	result = DNS_R_REFUSED;
 fail:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (db != NULL)
		dns_db_detach(&db);

	return (result);
}

static inline isc_result_t
query_getcachedb(ns_client_t *client, dns_db_t **dbp, unsigned int options)
{
	isc_result_t result;
	isc_boolean_t check_acl;
	dns_db_t *db = NULL;

	REQUIRE(dbp != NULL && *dbp == NULL);

	/*
	 * Find a cache database to answer the query.
	 * This may fail with DNS_R_REFUSED if the client
	 * is not allowed to use the cache.
	 */

	if (!USECACHE(client))
		return (DNS_R_REFUSED);
	dns_db_attach(client->view->cachedb, &db);

	if ((client->query.attributes &
	     NS_QUERYATTR_QUERYOKVALID) != 0) {
		/*
		 * We've evaluated the view's queryacl already.  If
		 * NS_QUERYATTR_QUERYOK is set, then the client is
		 * allowed to make queries, otherwise the query should
		 * be refused.
		 */
		check_acl = ISC_FALSE;
		if ((client->query.attributes &
		     NS_QUERYATTR_QUERYOK) == 0)
			goto refuse;
	} else {
		/*
		 * We haven't evaluated the view's queryacl yet.
		 */
		check_acl = ISC_TRUE;
	}

	if (check_acl) {
		isc_boolean_t log = ISC_TF((options & DNS_GETDB_NOLOG) == 0);
		
		result = ns_client_checkacl(client, "query (cache)",
					    client->view->queryacl,
					    ISC_TRUE,
					    log ? ISC_LOG_INFO :
						  ISC_LOG_DEBUG(3));
		if (result == ISC_R_SUCCESS) {
			/*
			 * We were allowed by the default
			 * "allow-query" ACL.  Remember this so we
			 * don't have to check again.
			 */
			client->query.attributes |=
				NS_QUERYATTR_QUERYOK;
		}
		/*
		 * We've now evaluated the view's query ACL, and
		 * the NS_QUERYATTR_QUERYOK attribute is now valid.
		 */
		client->query.attributes |= NS_QUERYATTR_QUERYOKVALID;

		if (result != ISC_R_SUCCESS)
			goto refuse;
	}

	/* Approved. */

	/* Transfer ownership. */
	*dbp = db;

	return (ISC_R_SUCCESS);

 refuse:
	result = DNS_R_REFUSED;

	if (db != NULL)
		dns_db_detach(&db);

	return (result);
}


static inline isc_result_t
query_getdb(ns_client_t *client, dns_name_t *name, unsigned int options,
	    dns_zone_t **zonep, dns_db_t **dbp, dns_dbversion_t **versionp,
	    isc_boolean_t *is_zonep)
{
	isc_result_t result;

	result = query_getzonedb(client, name, options, zonep, dbp, versionp);
	if (result == ISC_R_SUCCESS) {
		*is_zonep = ISC_TRUE;
	} else if (result == ISC_R_NOTFOUND) {
		result = query_getcachedb(client, dbp, options);
		*is_zonep = ISC_FALSE;
	}
	return (result);
}

static isc_result_t
query_simplefind(void *arg, dns_name_t *name, dns_rdatatype_t type,
		 isc_stdtime_t now,
		 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	ns_client_t *client = arg;
	isc_result_t result;
	dns_fixedname_t foundname;
	dns_db_t *db;
	dns_dbversion_t *version;
	unsigned int dboptions;
	isc_boolean_t is_zone;
	dns_rdataset_t zrdataset, zsigrdataset;
	dns_zone_t *zone;

	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(rdataset != NULL);

	dns_rdataset_init(&zrdataset);
	if (sigrdataset != NULL)
		dns_rdataset_init(&zsigrdataset);

	/*
	 * Find a database to answer the query.
	 */
	zone = NULL;
	db = NULL;
	version = NULL;
	result = query_getdb(client, name, 0, &zone, &db, &version, &is_zone);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

 db_find:
	/*
	 * Now look for an answer in the database.
	 */
	dns_fixedname_init(&foundname);
	dboptions = client->query.dboptions;
	if (db == client->query.gluedb || (!is_zone && CACHEGLUEOK(client)))
		dboptions |= DNS_DBFIND_GLUEOK;
	result = dns_db_find(db, name, version, type, dboptions,
			     now, NULL, dns_fixedname_name(&foundname),
			     rdataset, sigrdataset);
	if (result == DNS_R_DELEGATION ||
	    result == ISC_R_NOTFOUND) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		if (is_zone) {
			if (USECACHE(client)) {
				/*
				 * Either the answer is in the cache, or we
				 * don't know it.
				 */
				is_zone = ISC_FALSE;
				version = NULL;
				dns_db_detach(&db);
				dns_db_attach(client->view->cachedb, &db);
				goto db_find;
			}
		} else {
			/*
			 * We don't have the data in the cache.  If we've got
			 * glue from the zone, use it.
			 */
			if (dns_rdataset_isassociated(&zrdataset)) {
				dns_rdataset_clone(&zrdataset, rdataset);
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(&zsigrdataset))
					dns_rdataset_clone(&zsigrdataset,
							   sigrdataset);
				result = ISC_R_SUCCESS;
				goto cleanup;
			}
		}
		/*
		 * We don't know the answer.
		 */
		result = ISC_R_NOTFOUND;
	} else if (result == DNS_R_GLUE) {
		if (USECACHE(client) && RECURSIONOK(client)) {
			/*
			 * We found an answer, but the cache may be better.
			 * Remember what we've got and go look in the cache.
			 */
			is_zone = ISC_FALSE;
			version = NULL;
			dns_rdataset_clone(rdataset, &zrdataset);
			dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset))
			{
				dns_rdataset_clone(sigrdataset, &zsigrdataset);
				dns_rdataset_disassociate(sigrdataset);
			}
			dns_db_detach(&db);
			dns_db_attach(client->view->cachedb, &db);
			goto db_find;
		}
		/*
		 * Otherwise, the glue is the best answer.
		 */
		result = ISC_R_SUCCESS;
	} else if (result != ISC_R_SUCCESS) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		result = ISC_R_NOTFOUND;
	}
	/*
	 * If we get here, the result is ISC_R_SUCCESS, and we found the
	 * answer we were looking for in the zone.
	 */

 cleanup:
	if (dns_rdataset_isassociated(&zrdataset)) {
		dns_rdataset_disassociate(&zrdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(&zsigrdataset))
			dns_rdataset_disassociate(&zsigrdataset);
	}
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);

	return (result);
}

static inline isc_boolean_t
query_isduplicate(ns_client_t *client, dns_name_t *name,
		  dns_rdatatype_t type, dns_name_t **mnamep)
{
	dns_section_t section;
	dns_name_t *mname = NULL;
	isc_result_t result;

	CTRACE("query_isduplicate");

	for (section = DNS_SECTION_ANSWER;
	     section <= DNS_SECTION_ADDITIONAL;
	     section++) {
		result = dns_message_findname(client->message, section,
					      name, type, 0, &mname, NULL);
		if (result == ISC_R_SUCCESS) {
			/*
			 * We've already got this RRset in the response.
			 */
			CTRACE("query_isduplicate: true: done");
			return (ISC_TRUE);
		} else if (result == DNS_R_NXRRSET) {
			/*
			 * The name exists, but the rdataset does not.
			 */
			if (section == DNS_SECTION_ADDITIONAL)
				break;
		} else
			RUNTIME_CHECK(result == DNS_R_NXDOMAIN);
		mname = NULL;
	}

	/*
	 * If the dns_name_t we're looking up is already in the message,
	 * we don't want to trigger the caller's name replacement logic.
	 */
	if (name == mname)
		mname = NULL;

	*mnamep = mname;

	CTRACE("query_isduplicate: false: done");
	return (ISC_FALSE);
}

static isc_result_t
query_addadditional(void *arg, dns_name_t *name, dns_rdatatype_t qtype) {
	ns_client_t *client = arg;
	isc_result_t result, eresult;
	dns_dbnode_t *node;
	dns_db_t *db;
	dns_name_t *fname, *mname;
	dns_rdataset_t *rdataset, *sigrdataset, *a6rdataset, *trdataset;
	isc_buffer_t *dbuf;
	isc_buffer_t b;
	dns_dbversion_t *version;
	isc_boolean_t added_something, need_addname;
	dns_zone_t *zone;
	dns_rdatatype_t type;

	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(qtype != dns_rdatatype_any);

	if (!WANTDNSSEC(client) && dns_rdatatype_isdnssec(qtype))
		return (ISC_R_SUCCESS);

	CTRACE("query_addadditional");

	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	fname = NULL;
	rdataset = NULL;
	sigrdataset = NULL;
	a6rdataset = NULL;
	trdataset = NULL;
	db = NULL;
	version = NULL;
	node = NULL;
	added_something = ISC_FALSE;
	need_addname = ISC_FALSE;
	zone = NULL;

	/*
	 * We treat type A additional section processing as if it
	 * were "any address type" additional section processing.
	 * To avoid multiple lookups, we do an 'any' database
	 * lookup and iterate over the node.
	 */
	if (qtype == dns_rdatatype_a)
		type = dns_rdatatype_any;
	else
		type = qtype;

	/*
	 * Get some resources.
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL)
		goto cleanup;
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL)
			goto cleanup;
	}

	/*
	 * Look for a zone database that might contain authoritative
	 * additional data.
	 */
	result = query_getzonedb(client, name, DNS_GETDB_NOLOG,
				 &zone, &db, &version);
	if (result != ISC_R_SUCCESS)
		goto try_cache;

	CTRACE("query_addadditional: db_find");

	/*
	 * Since we are looking for authoritative data, we do not set
	 * the GLUEOK flag.  Glue will be looked for later, but not
	 * necessarily in the same database.
	 */
	node = NULL;
	result = dns_db_find(db, name, version, type, client->query.dboptions,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (result == ISC_R_SUCCESS)
		goto found;

	if (dns_rdataset_isassociated(rdataset))
		dns_rdataset_disassociate(rdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset))
		dns_rdataset_disassociate(sigrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	version = NULL;
	dns_db_detach(&db);

	/*
	 * No authoritative data was found.  The cache is our next best bet.
	 */

 try_cache:
	result = query_getcachedb(client, &db, DNS_GETDB_NOLOG);
	if (result != ISC_R_SUCCESS)
		/*
		 * Most likely the client isn't allowed to query the cache.
		 */
		goto try_glue;

	result = dns_db_find(db, name, version, type,  client->query.dboptions,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (result == ISC_R_SUCCESS)
		goto found;

	if (dns_rdataset_isassociated(rdataset))
		dns_rdataset_disassociate(rdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset))
		dns_rdataset_disassociate(sigrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	dns_db_detach(&db);

 try_glue:
	/*
	 * No cached data was found.  Glue is our last chance.
	 * RFC1035 sayeth:
	 *
	 *	NS records cause both the usual additional section
	 *	processing to locate a type A record, and, when used
	 *	in a referral, a special search of the zone in which
	 *	they reside for glue information.
	 *
	 * This is the "special search".  Note that we must search
	 * the zone where the NS record resides, not the zone it
	 * points to, and that we only do the search in the delegation
	 * case (identified by client->query.gluedb being set).
	 */

	if (client->query.gluedb == NULL)
		goto cleanup;

	/*
	 * Don't poision caches using the bailiwick protection model.
	 */
	if (!dns_name_issubdomain(name, dns_db_origin(client->query.gluedb)))
		goto cleanup;

	dns_db_attach(client->query.gluedb, &db);
	result = dns_db_find(db, name, version, type,
			     client->query.dboptions | DNS_DBFIND_GLUEOK,
			     client->now, &node, fname, rdataset,
			     sigrdataset);
	if (!(result == ISC_R_SUCCESS ||
	      result == DNS_R_ZONECUT ||
	      result == DNS_R_GLUE))
		goto cleanup;

 found:
	/*
	 * We have found a potential additional data rdataset, or
	 * at least a node to iterate over.
	 */
	query_keepname(client, fname, dbuf);

	/*
	 * If we have an rdataset, add it to the additional data
	 * section.
	 */
	mname = NULL;
	if (dns_rdataset_isassociated(rdataset) &&
	    !query_isduplicate(client, fname, type, &mname)) {
		if (mname != NULL) {
			query_releasename(client, &fname);
			fname = mname;
		} else
			need_addname = ISC_TRUE;
		ISC_LIST_APPEND(fname->list, rdataset, link);
		trdataset = rdataset;
		rdataset = NULL;
		added_something = ISC_TRUE;
		/*
		 * Note: we only add SIGs if we've added the type they cover,
		 * so we do not need to check if the SIG rdataset is already
		 * in the response.
		 */
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
		{
			ISC_LIST_APPEND(fname->list, sigrdataset, link);
			sigrdataset = NULL;
		}
	}

	if (qtype == dns_rdatatype_a) {
		/*
		 * We now go looking for A, A6, and AAAA records, along with
		 * their signatures.
		 *
		 * XXXRTH  This code could be more efficient.
		 */
		if (rdataset != NULL) {
			if (dns_rdataset_isassociated(rdataset))
				dns_rdataset_disassociate(rdataset);
		} else {
			rdataset = query_newrdataset(client);
			if (rdataset == NULL)
				goto addname;
		}
		if (sigrdataset != NULL) {
			if (dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
		} else if (WANTDNSSEC(client)) {
			sigrdataset = query_newrdataset(client);
			if (sigrdataset == NULL)
				goto addname;
		}
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_a, 0,
					     client->now, rdataset,
					     sigrdataset);
		if (result == DNS_R_NCACHENXDOMAIN)
			goto addname;
		if (result == DNS_R_NCACHENXRRSET) {
			dns_rdataset_disassociate(rdataset);
			/*
			 * Negative cache entries don't have sigrdatasets.
			 */
			INSIST(sigrdataset == NULL ||
			       ! dns_rdataset_isassociated(sigrdataset));
		}
		if (result == ISC_R_SUCCESS) {
			mname = NULL;
			if (!query_isduplicate(client, fname,
					       dns_rdatatype_a, &mname)) {
				if (mname != NULL) {
					query_releasename(client, &fname);
					fname = mname;
				} else
					need_addname = ISC_TRUE;
				ISC_LIST_APPEND(fname->list, rdataset, link);
				added_something = ISC_TRUE;
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
				{
					ISC_LIST_APPEND(fname->list,
							sigrdataset, link);
					sigrdataset =
						query_newrdataset(client);
				}
				rdataset = query_newrdataset(client);
				if (rdataset == NULL)
					goto addname;
				if (WANTDNSSEC(client) && sigrdataset == NULL)
					goto addname;
			} else {
				dns_rdataset_disassociate(rdataset);
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
					dns_rdataset_disassociate(sigrdataset);
			}
		}
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_a6, 0,
					     client->now, rdataset,
					     sigrdataset);
		if (result == DNS_R_NCACHENXDOMAIN)
			goto addname;
		if (result == DNS_R_NCACHENXRRSET) {
			dns_rdataset_disassociate(rdataset);
			INSIST(sigrdataset == NULL ||
			       ! dns_rdataset_isassociated(sigrdataset));
		}
		if (result == ISC_R_SUCCESS) {
			mname = NULL;
			if (!query_isduplicate(client, fname,
					       dns_rdatatype_a6, &mname)) {
				if (mname != NULL) {
					query_releasename(client, &fname);
					fname = mname;
				} else
					need_addname = ISC_TRUE;
				a6rdataset = rdataset;
				ISC_LIST_APPEND(fname->list, rdataset, link);
				added_something = ISC_TRUE;
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
				{
					ISC_LIST_APPEND(fname->list,
							sigrdataset, link);
					sigrdataset =
						query_newrdataset(client);
				}
				rdataset = query_newrdataset(client);
				if (rdataset == NULL)
					goto addname;
				if (WANTDNSSEC(client) && sigrdataset == NULL)
					goto addname;
			} else {
				dns_rdataset_disassociate(rdataset);
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
					dns_rdataset_disassociate(sigrdataset);
			}
		}
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_aaaa, 0,
					     client->now, rdataset,
					     sigrdataset);
		if (result == DNS_R_NCACHENXDOMAIN)
			goto addname;
		if (result == DNS_R_NCACHENXRRSET) {
			dns_rdataset_disassociate(rdataset);
			INSIST(sigrdataset == NULL ||
			       ! dns_rdataset_isassociated(sigrdataset));
		}
		if (result == ISC_R_SUCCESS) {
			mname = NULL;
			if (!query_isduplicate(client, fname,
					       dns_rdatatype_aaaa, &mname)) {
				if (mname != NULL) {
					query_releasename(client, &fname);
					fname = mname;
				} else
					need_addname = ISC_TRUE;
				ISC_LIST_APPEND(fname->list, rdataset, link);
				added_something = ISC_TRUE;
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(sigrdataset))
				{
					ISC_LIST_APPEND(fname->list,
							sigrdataset, link);
					sigrdataset = NULL;
				}
				rdataset = NULL;
			}
		}
	}

 addname:
	CTRACE("query_addadditional: addname");
	/*
	 * If we haven't added anything, then we're done.
	 */
	if (!added_something)
		goto cleanup;

	/*
	 * We may have added our rdatasets to an existing name, if so, then
	 * need_addname will be ISC_FALSE.  Whether we used an existing name
	 * or a new one, we must set fname to NULL to prevent cleanup.
	 */
	if (need_addname)
		dns_message_addname(client->message, fname,
				    DNS_SECTION_ADDITIONAL);
	fname = NULL;

	/*
	 * In a few cases, we want to add additional data for additional
	 * data.  It's simpler to just deal with special cases here than
	 * to try to create a general purpose mechanism and allow the
	 * rdata implementations to do it themselves.
	 *
	 * This involves recursion, but the depth is limited.  The
	 * most complex case is adding a SRV rdataset, which involves
	 * recursing to add address records, which in turn can cause
	 * recursion to add KEYs.
	 */
	if (type == dns_rdatatype_a || type == dns_rdatatype_aaaa) {
		/*
		 * RFC 2535 section 3.5 says that when A or AAAA records are
		 * retrieved as additional data, any KEY RRs for the owner name
		 * should be added to the additional data section.  Note: we
		 * do NOT include A6 in the list of types with such treatment
		 * in additional data because we'd have to do it for each A6
		 * in the A6 chain.
		 *
		 * XXXRTH  We should lower the priority here.  Alternatively,
		 * we could raise the priority of glue records.
		 */
		eresult = query_addadditional(client, name, dns_rdatatype_key);
 	} else if (type == dns_rdatatype_srv && trdataset != NULL) {
		/*
		 * If we're adding SRV records to the additional data
		 * section, it's helpful if we add the SRV additional data
		 * as well.
		 */
		eresult = dns_rdataset_additionaldata(trdataset,
						      query_addadditional,
						      client);
	}

	/*
	 * If we added an A6 rdataset, we should also add everything we
	 * know about the A6 chains.  We wait until now to do this so that
	 * they'll come after any additional data added above.
	 */
	if (a6rdataset != NULL) {
		dns_a6_reset(&client->query.a6ctx);
		dns_a6_foreach(&client->query.a6ctx, a6rdataset, client->now);
	}

 cleanup:
	CTRACE("query_addadditional: cleanup");
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);

	CTRACE("query_addadditional: done");
	return (eresult);
}

static void
query_adda6rrset(void *arg, dns_name_t *name, dns_rdataset_t *rdataset,
		      dns_rdataset_t *sigrdataset)
{
	ns_client_t *client = arg;
	dns_rdataset_t *crdataset, *csigrdataset;
	isc_buffer_t b, *dbuf;
	dns_name_t *fname, *mname;

	/*
	 * Add an rrset to the additional data section.
	 */

	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(rdataset->type == dns_rdatatype_a6);

	/*
	 * Get some resources...
	 */
	fname = NULL;
	crdataset = NULL;
	csigrdataset = NULL;
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	crdataset = query_newrdataset(client);
	if (fname == NULL || crdataset == NULL)
		goto cleanup;
	if (sigrdataset != NULL) {
		csigrdataset = query_newrdataset(client);
		if (csigrdataset == NULL)
			goto cleanup;
	}

	if (dns_name_copy(name, fname, NULL) != ISC_R_SUCCESS)
		goto cleanup;
	dns_rdataset_clone(rdataset, crdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset))
		dns_rdataset_clone(sigrdataset, csigrdataset);

	mname = NULL;
	if (query_isduplicate(client, fname, crdataset->type, &mname))
		goto cleanup;
	if (mname != NULL) {
		query_releasename(client, &fname);
		fname = mname;
	} else {
		query_keepname(client, fname, dbuf);
		dns_message_addname(client->message, fname,
				    DNS_SECTION_ADDITIONAL);
	}

	ISC_LIST_APPEND(fname->list, crdataset, link);
	crdataset = NULL;
	/*
	 * Note: we only add SIGs if we've added the type they cover, so
	 * we do not need to check if the SIG rdataset is already in the
	 * response.
	 */
	if (sigrdataset != NULL && dns_rdataset_isassociated(csigrdataset)) {
		ISC_LIST_APPEND(fname->list, csigrdataset, link);
		csigrdataset = NULL;
	}

	fname = NULL;

	/*
	 * In spite of RFC 2535 section 3.5, we don't currently try to add
	 * KEY RRs for the A6 records.  It's just too much work.
	 */

 cleanup:
	query_putrdataset(client, &crdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &csigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
}

static inline void
query_addrdataset(ns_client_t *client, dns_name_t *fname,
		  dns_rdataset_t *rdataset)
{
	dns_rdatatype_t type = rdataset->type;

	/*
	 * Add 'rdataset' and any pertinent additional data to
	 * 'fname', a name in the response message for 'client'.
	 */

	CTRACE("query_addrdataset");

	ISC_LIST_APPEND(fname->list, rdataset, link);

	if (NOADDITIONAL(client))
		return;

	/*
	 * Add additional data.
	 *
	 * We don't care if dns_a6_foreach or dns_rdataset_additionaldata()
	 * fail.
	 */
	if (type == dns_rdatatype_a6) {
		dns_a6_reset(&client->query.a6ctx);
		(void)dns_a6_foreach(&client->query.a6ctx, rdataset,
				     client->now);
	} else
		(void)dns_rdataset_additionaldata(rdataset,
						  query_addadditional, client);
	/*
	 * RFC 2535 section 3.5 says that when NS, SOA, A, or AAAA records
	 * are retrieved, any KEY RRs for the owner name should be added
	 * to the additional data section.  We treat A6 records the same way.
	 *
	 * We don't care if query_addadditional() fails.
	 */
	if (type == dns_rdatatype_ns || type == dns_rdatatype_soa ||
	    type == dns_rdatatype_a || type == dns_rdatatype_aaaa ||
	    type == dns_rdatatype_a6) {
		/*
		 * XXXRTH  We should lower the priority here.  Alternatively,
		 * we could raise the priority of glue records.
		 */
		(void)query_addadditional(client, fname, dns_rdatatype_key);
	}
	CTRACE("query_addrdataset: done");
}

static void
query_addrrset(ns_client_t *client, dns_name_t **namep,
	       dns_rdataset_t **rdatasetp, dns_rdataset_t **sigrdatasetp,
	       isc_buffer_t *dbuf, dns_section_t section)
{
	dns_name_t *name, *mname;
	dns_rdataset_t *rdataset, *mrdataset, *sigrdataset;
	isc_result_t result;

	/*
	 * To the current response for 'client', add the answer RRset
	 * '*rdatasetp' and an optional signature set '*sigrdatasetp', with
	 * owner name '*namep', to section 'section', unless they are
	 * already there.  Also add any pertinent additional data.
	 *
	 * If 'dbuf' is not NULL, then '*namep' is the name whose data is
	 * stored in 'dbuf'.  In this case, query_addrrset() guarantees that
	 * when it returns the name will either have been kept or released.
	 */
	CTRACE("query_addrrset");
	name = *namep;
	rdataset = *rdatasetp;
	if (sigrdatasetp != NULL)
		sigrdataset = *sigrdatasetp;
	else
		sigrdataset = NULL;
	mname = NULL;
	mrdataset = NULL;
	result = dns_message_findname(client->message, section,
				      name, rdataset->type, rdataset->covers,
				      &mname, &mrdataset);
	if (result == ISC_R_SUCCESS) {
		/*
		 * We've already got an RRset of the given name and type.
		 * There's nothing else to do;
		 */
		CTRACE("query_addrrset: dns_message_findname succeeded: done");
		if (dbuf != NULL)
			query_releasename(client, namep);
		return;
	} else if (result == DNS_R_NXDOMAIN) {
		/*
		 * The name doesn't exist.
		 */
		if (dbuf != NULL)
			query_keepname(client, name, dbuf);
		dns_message_addname(client->message, name, section);
		*namep = NULL;
		mname = name;
	} else {
		RUNTIME_CHECK(result == DNS_R_NXRRSET);
		if (dbuf != NULL)
			query_releasename(client, namep);
	}

	/*
	 * Note: we only add SIGs if we've added the type they cover, so
	 * we do not need to check if the SIG rdataset is already in the
	 * response.
	 */
	query_addrdataset(client, mname, rdataset);
	*rdatasetp = NULL;
	if (sigrdataset != NULL && dns_rdataset_isassociated(sigrdataset)) {
		/*
		 * We have a signature.  Add it to the response.
		 */
		ISC_LIST_APPEND(mname->list, sigrdataset, link);
		*sigrdatasetp = NULL;
	}
	CTRACE("query_addrrset: done");
}

static inline isc_result_t
query_addsoa(ns_client_t *client, dns_db_t *db, isc_boolean_t zero_ttl) {
	dns_name_t *name, *fname;
	dns_dbnode_t *node;
	isc_result_t result, eresult;
	dns_fixedname_t foundname;
	dns_rdataset_t *rdataset = NULL, *sigrdataset = NULL;
	dns_rdataset_t **sigrdatasetp = NULL;

	CTRACE("query_addsoa");
	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	name = NULL;
	rdataset = NULL;
	node = NULL;
	dns_fixedname_init(&foundname);
	fname = dns_fixedname_name(&foundname);

	/*
	 * Get resources and make 'name' be the database origin.
	 */
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_name_init(name, NULL);
	dns_name_clone(dns_db_origin(db), name);
	rdataset = query_newrdataset(client);
	if (rdataset == NULL) {
		eresult = DNS_R_SERVFAIL;
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			eresult = DNS_R_SERVFAIL;
			goto cleanup;
		}
	}

	/*
	 * Find the SOA.
	 */
	result = dns_db_find(db, name, NULL, dns_rdatatype_soa,
			     client->query.dboptions, 0, &node,
			     fname, rdataset, sigrdataset);
	if (result != ISC_R_SUCCESS) {
		/*
		 * This is bad.  We tried to get the SOA RR at the zone top
		 * and it didn't work!
		 */
		eresult = DNS_R_SERVFAIL;
	} else {
		/*
		 * Extract the SOA MINIMUM.
		 */
		dns_rdata_soa_t soa;
		dns_rdata_t rdata = DNS_RDATA_INIT;
		result = dns_rdataset_first(rdataset);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdataset_current(rdataset, &rdata);
		dns_rdata_tostruct(&rdata, &soa, NULL);

		if (zero_ttl) {
			rdataset->ttl = 0;
			if (sigrdataset != NULL)
				sigrdataset->ttl = 0;
		}

		/*
		 * Add the SOA and its SIG to the response, with the
		 * TTLs adjusted per RFC2308 section 3.
		 */
		if (rdataset->ttl > soa.minimum)
			rdataset->ttl = soa.minimum;
		if (sigrdataset != NULL && sigrdataset->ttl > soa.minimum)
			sigrdataset->ttl = soa.minimum;

		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &name, &rdataset, sigrdatasetp, NULL,
			       DNS_SECTION_AUTHORITY);
	}

 cleanup:
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (name != NULL)
		query_releasename(client, &name);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	return (eresult);
}

static inline isc_result_t
query_addns(ns_client_t *client, dns_db_t *db) {
	dns_name_t *name, *fname;
	dns_dbnode_t *node;
	isc_result_t result, eresult;
	dns_fixedname_t foundname;
	dns_rdataset_t *rdataset = NULL, *sigrdataset = NULL;
	dns_rdataset_t **sigrdatasetp = NULL;

	CTRACE("query_addns");
	/*
	 * Initialization.
	 */
	eresult = ISC_R_SUCCESS;
	name = NULL;
	rdataset = NULL;
	node = NULL;
	dns_fixedname_init(&foundname);
	fname = dns_fixedname_name(&foundname);

	/*
	 * Get resources and make 'name' be the database origin.
	 */
	result = dns_message_gettempname(client->message, &name);
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addns: dns_message_gettempname failed: done");
		return (result);
	}
	dns_name_init(name, NULL);
	dns_name_clone(dns_db_origin(db), name);
	rdataset = query_newrdataset(client);
	if (rdataset == NULL) {
		CTRACE("query_addns: query_newrdataset failed");
		eresult = DNS_R_SERVFAIL;
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			CTRACE("query_addns: query_newrdataset failed");
			eresult = DNS_R_SERVFAIL;
			goto cleanup;
		}
	}

	/*
	 * Find the NS rdataset.
	 */
	CTRACE("query_addns: calling dns_db_find");
	result = dns_db_find(db, name, NULL, dns_rdatatype_ns,
			     client->query.dboptions, 0, &node,
			     fname, rdataset, sigrdataset);
	CTRACE("query_addns: dns_db_find complete");
	if (result != ISC_R_SUCCESS) {
		CTRACE("query_addns: dns_db_find failed");
		/*
		 * This is bad.  We tried to get the NS rdataset at the zone
		 * top and it didn't work!
		 */
		eresult = DNS_R_SERVFAIL;
	} else {
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &name, &rdataset, sigrdatasetp, NULL,
			       DNS_SECTION_AUTHORITY);
	}

 cleanup:
	CTRACE("query_addns: cleanup");
	query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (name != NULL)
		query_releasename(client, &name);
	if (node != NULL)
		dns_db_detachnode(db, &node);

	CTRACE("query_addns: done");
	return (eresult);
}

static inline isc_result_t
query_addcnamelike(ns_client_t *client, dns_name_t *qname, dns_name_t *tname,
		   dns_ttl_t ttl, dns_name_t **anamep, dns_rdatatype_t type)
{
	dns_rdataset_t *rdataset;
	dns_rdatalist_t *rdatalist;
	dns_rdata_t *rdata;
	isc_result_t result;
	isc_region_t r;

	/*
	 * We assume the name data referred to by tname won't go away.
	 */

	REQUIRE(anamep != NULL);

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
	result = dns_name_dup(qname, client->mctx, *anamep);
	if (result != ISC_R_SUCCESS) {
		dns_message_puttemprdataset(client->message, &rdataset);
		return (result);
	}

	rdatalist->type = type;
	rdatalist->covers = 0;
	rdatalist->rdclass = client->message->rdclass;
	rdatalist->ttl = ttl;

	dns_name_toregion(tname, &r);
	rdata->data = r.base;
	rdata->length = r.length;
	rdata->rdclass = client->message->rdclass;
	rdata->type = type;

	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdatalist_tordataset(rdatalist, rdataset);

	query_addrrset(client, anamep, &rdataset, NULL, NULL,
		       DNS_SECTION_ANSWER);

	if (rdataset != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_message_puttemprdataset(client->message, &rdataset);
	}

	return (ISC_R_SUCCESS);
}

static void
query_addbestns(ns_client_t *client) {
	dns_db_t *db, *zdb;
	dns_dbnode_t *node;
	dns_name_t *fname, *zfname;
	dns_rdataset_t *rdataset, *sigrdataset, *zrdataset, *zsigrdataset;
	isc_boolean_t is_zone, use_zone;
	isc_buffer_t *dbuf;
	isc_result_t result;
	dns_dbversion_t *version;
	dns_zone_t *zone;
	isc_buffer_t b;

	CTRACE("query_addbestns");
	fname = NULL;
	zfname = NULL;
	rdataset = NULL;
	zrdataset = NULL;
	sigrdataset = NULL;
	zsigrdataset = NULL;
	node = NULL;
	db = NULL;
	zdb = NULL;
	version = NULL;
	zone = NULL;
	is_zone = ISC_FALSE;
	use_zone = ISC_FALSE;

	/*
	 * Find the right database.
	 */
	result = query_getdb(client, client->query.qname, 0, &zone, &db,
			     &version, &is_zone);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

 db_find:
	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL)
		goto cleanup;
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL)
		goto cleanup;
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL)
			goto cleanup;
	}

	/*
	 * Now look for the zonecut.
	 */
	if (is_zone) {
		result = dns_db_find(db, client->query.qname, version,
				     dns_rdatatype_ns, client->query.dboptions,
				     client->now, &node, fname,
				     rdataset, sigrdataset);
		if (result != DNS_R_DELEGATION)
			goto cleanup;
		if (USECACHE(client)) {
			query_keepname(client, fname, dbuf);
			zdb = db;
			zfname = fname;
			fname = NULL;
			zrdataset = rdataset;
			rdataset = NULL;
			zsigrdataset = sigrdataset;
			sigrdataset = NULL;
			dns_db_detachnode(db, &node);
			version = NULL;
			db = NULL;
			dns_db_attach(client->view->cachedb, &db);
			is_zone = ISC_FALSE;
			goto db_find;
		}
	} else {
		result = dns_db_findzonecut(db, client->query.qname,
					    client->query.dboptions,
					    client->now, &node, fname,
					    rdataset, sigrdataset);
		if (result == ISC_R_SUCCESS) {
			if (zfname != NULL &&
			    !dns_name_issubdomain(fname, zfname)) {
				/*
				 * We found a zonecut in the cache, but our
				 * zone delegation is better.
				 */
				use_zone = ISC_TRUE;
			}
		} else if (result == ISC_R_NOTFOUND && zfname != NULL) {
			/*
			 * We didn't find anything in the cache, but we
			 * have a zone delegation, so use it.
			 */
			use_zone = ISC_TRUE;
		} else
			goto cleanup;
	}

	if (use_zone) {
		query_releasename(client, &fname);
		fname = zfname;
		zfname = NULL;
		/*
		 * We've already done query_keepname() on
		 * zfname, so we must set dbuf to NULL to
		 * prevent query_addrrset() from trying to
		 * call query_keepname() again.
		 */
		dbuf = NULL;
		query_putrdataset(client, &rdataset);
		if (sigrdataset != NULL)
			query_putrdataset(client, &sigrdataset);
		rdataset = zrdataset;
		zrdataset = NULL;
		sigrdataset = zsigrdataset;
		zsigrdataset = NULL;
	}

	if ((client->query.dboptions & DNS_DBFIND_PENDINGOK) == 0 &&
	    (rdataset->trust == dns_trust_pending ||
	     (sigrdataset != NULL && sigrdataset->trust == dns_trust_pending)))
		goto cleanup;

	query_addrrset(client, &fname, &rdataset, &sigrdataset, dbuf,
		       DNS_SECTION_AUTHORITY);

 cleanup:
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (zdb != NULL) {
		query_putrdataset(client, &zrdataset);
		if (zsigrdataset != NULL)
			query_putrdataset(client, &zsigrdataset);
		if (zfname != NULL)
			query_releasename(client, &zfname);
		dns_db_detach(&zdb);
	}
}

static void
query_resume(isc_task_t *task, isc_event_t *event) {
	dns_fetchevent_t *devent = (dns_fetchevent_t *)event;
	ns_client_t *client;
	isc_boolean_t fetch_cancelled, client_shuttingdown;

	/*
	 * Resume a query after recursion.
	 */

	UNUSED(task);

	REQUIRE(event->ev_type == DNS_EVENT_FETCHDONE);
	client = devent->ev_arg;
	REQUIRE(NS_CLIENT_VALID(client));
	REQUIRE(task == client->task);
	REQUIRE(RECURSING(client));

	if (devent->fetch != NULL) {
		/*
		 * This is the fetch we've been waiting for.
		 */
		INSIST(devent->fetch == client->query.fetch);
		client->query.fetch = NULL;
		fetch_cancelled = ISC_FALSE;
		/*
		 * Update client->now.
		 */
		isc_stdtime_get(&client->now);
	} else {
		/*
		 * This is a fetch completion event for a cancelled fetch.
		 * Clean up and don't resume the find.
		 */
		fetch_cancelled = ISC_TRUE;
	}
	INSIST(client->query.fetch == NULL);

	client->query.attributes &= ~NS_QUERYATTR_RECURSING;
	dns_resolver_destroyfetch(&devent->fetch);

	/*
	 * If this client is shutting down, or this transaction
	 * has timed out, do not resume the find.
	 */
	client_shuttingdown = ns_client_shuttingdown(client);
	if (fetch_cancelled || client_shuttingdown) {
		if (devent->node != NULL)
			dns_db_detachnode(devent->db, &devent->node);
		if (devent->db != NULL)
			dns_db_detach(&devent->db);
		query_putrdataset(client, &devent->rdataset);
		if (devent->sigrdataset != NULL)
			query_putrdataset(client, &devent->sigrdataset);
		isc_event_free(&event);
		query_next(client, ISC_R_CANCELED);
		/*
		 * This may destroy the client.
		 */
		ns_client_detach(&client);
	} else {
		query_find(client, devent, 0);
	}
}

static isc_result_t
query_recurse(ns_client_t *client, dns_rdatatype_t qtype, dns_name_t *qdomain,
	      dns_rdataset_t *nameservers)
{
	isc_result_t result;
	dns_rdataset_t *rdataset, *sigrdataset;

	inc_stats(client, dns_statscounter_recursion);

	/*
	 * We are about to recurse, which means that this client will
	 * be unavailable for serving new requests for an indeterminate
	 * amount of time.  If this client is currently responsible
	 * for handling incoming queries, set up a new client
	 * object to handle them while we are waiting for a
	 * response.  There is no need to replace TCP clients
	 * because those have already been replaced when the
	 * connection was accepted (if allowed by the TCP quota).
	 */
	if (client->recursionquota == NULL) {
		result = isc_quota_attach(&ns_g_server->recursionquota,
					  &client->recursionquota);
		if (result == ISC_R_SUCCESS && !client->mortal &&
		    (client->attributes & NS_CLIENTATTR_TCP) == 0)
			result = ns_client_replace(client);
		if (result != ISC_R_SUCCESS) {
			ns_client_log(client, NS_LOGCATEGORY_CLIENT,
				      NS_LOGMODULE_QUERY, ISC_LOG_WARNING,
				      "no more recursive clients: %s",
				      isc_result_totext(result));
			return (result);
		}
	}

	/*
	 * Invoke the resolver.
	 */
	REQUIRE(nameservers == NULL || nameservers->type == dns_rdatatype_ns);
	REQUIRE(client->query.fetch == NULL);

	rdataset = query_newrdataset(client);
	if (rdataset == NULL)
		return (ISC_R_NOMEMORY);
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			query_putrdataset(client, &rdataset);
			return (ISC_R_NOMEMORY);
		}
	} else
		sigrdataset = NULL;

	if (client->query.timerset == ISC_FALSE)
		ns_client_settimeout(client, 60);
	result = dns_resolver_createfetch(client->view->resolver,
					  client->query.qname,
					  qtype, qdomain, nameservers,
					  NULL, client->query.fetchoptions,
					  client->task,
					  query_resume, client,
					  rdataset, sigrdataset,
					  &client->query.fetch);

	if (result == ISC_R_SUCCESS) {
		/*
		 * Record that we're waiting for an event.  A client which
		 * is shutting down will not be destroyed until all the
		 * events have been received.
		 */
	} else {
		query_putrdataset(client, &rdataset);
		if (sigrdataset != NULL)
			query_putrdataset(client, &sigrdataset);
	}

	return (result);
}

static inline isc_result_t
query_findparentkey(ns_client_t *client, dns_name_t *name,
		    dns_zone_t **zonep, dns_db_t **dbp,
		    dns_dbversion_t **versionp, dns_dbnode_t **nodep,
		    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_db_t *pdb;
	dns_dbnode_t *pnode;
	dns_dbversion_t *pversion;
	dns_rdataset_t prdataset, psigrdataset;
	dns_rdataset_t *psigrdatasetp;
	isc_result_t result;
	dns_zone_t *pzone;
	isc_boolean_t is_zone;
	dns_fixedname_t pfoundname;

	/*
	 * 'name' is at a zone cut.  Try to find a KEY for 'name' in
	 * the deepest ancestor zone of 'name' (if any).  If it exists,
	 * update *zonep, *dbp, *nodep, rdataset, and sigrdataset and
	 * return ISC_R_SUCCESS.  If not, leave them alone and return a
	 * non-success status.
	 */

	pzone = NULL;
	pdb = NULL;
	pnode = NULL;
	pversion = NULL;
	dns_rdataset_init(&prdataset);
	if (sigrdataset != NULL)
		dns_rdataset_init(&psigrdataset);
	is_zone = ISC_FALSE;
	dns_fixedname_init(&pfoundname);

	result = query_getdb(client, name, DNS_GETDB_NOEXACT,
			     &pzone, &pdb, &pversion, &is_zone);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	if (!is_zone) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	if (sigrdataset != NULL)
		psigrdatasetp = &psigrdataset;
	else
		psigrdatasetp = NULL;
	result = dns_db_find(pdb, name, pversion, dns_rdatatype_key,
			     client->query.dboptions,
			     client->now, &pnode,
			     dns_fixedname_name(&pfoundname),
			     &prdataset, psigrdatasetp);
	if (result == ISC_R_SUCCESS) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_rdataset_clone(&prdataset, rdataset);
		if (sigrdataset != NULL) {
			if (dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
			if (dns_rdataset_isassociated(&psigrdataset))
				dns_rdataset_clone(&psigrdataset, sigrdataset);
		}
		if (*nodep != NULL)
			dns_db_detachnode(*dbp, nodep);
		*nodep = pnode;
		pnode = NULL;
		*versionp = pversion;
		if (*dbp != NULL)
			dns_db_detach(dbp);
		*dbp = pdb;
		pdb = NULL;
		if (*zonep != NULL)
			dns_zone_detach(zonep);
		*zonep = pzone;
		pzone = NULL;
	}

 cleanup:
	if (dns_rdataset_isassociated(&prdataset))
		dns_rdataset_disassociate(&prdataset);
	if (sigrdataset != NULL && dns_rdataset_isassociated(&psigrdataset))
		dns_rdataset_disassociate(&psigrdataset);
	if (pnode != NULL)
		dns_db_detachnode(pdb, &pnode);
	if (pdb != NULL)
		dns_db_detach(&pdb);
	if (pzone != NULL)
		dns_zone_detach(&pzone);

	return (result);
}

#define MAX_RESTARTS 16

#define QUERY_ERROR(r) \
do { \
	eresult = r; \
	want_restart = ISC_FALSE; \
} while (0)

/*
 * Extract a network address from the RDATA of an A or AAAA
 * record.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOTIMPLEMENTED	The rdata is not a known address type.
 */
static isc_result_t
rdata_tonetaddr(dns_rdata_t *rdata, isc_netaddr_t *netaddr) {
	struct in_addr ina;
	struct in6_addr in6a;
	
	switch (rdata->type) {
	case dns_rdatatype_a:
		INSIST(rdata->length == 4);
		memcpy(&ina.s_addr, rdata->data, 4);
		isc_netaddr_fromin(netaddr, &ina);
		return (ISC_R_SUCCESS);
	case dns_rdatatype_aaaa:
		INSIST(rdata->length == 16);
		memcpy(in6a.s6_addr, rdata->data, 16);
		isc_netaddr_fromin6(netaddr, &in6a);
		return (ISC_R_SUCCESS);
	default:
		return (ISC_R_NOTIMPLEMENTED);
	}
}

/*
 * Find the sort order of 'rdata' in the topology-like
 * ACL forming the second element in a 2-element top-level
 * sortlist statement.
 */
static int
query_sortlist_order_2element(dns_rdata_t *rdata, void *arg) {
	isc_netaddr_t netaddr;

	if (rdata_tonetaddr(rdata, &netaddr) != ISC_R_SUCCESS)
		return (INT_MAX);
	return (ns_sortlist_addrorder2(&netaddr, arg));
}

/*
 * Find the sort order of 'rdata' in the matching element
 * of a 1-element top-level sortlist statement.
 */
static int
query_sortlist_order_1element(dns_rdata_t *rdata, void *arg) {
	isc_netaddr_t netaddr;

	if (rdata_tonetaddr(rdata, &netaddr) != ISC_R_SUCCESS)
		return (INT_MAX);
	return (ns_sortlist_addrorder1(&netaddr, arg));
}

/*
 * Find the sortlist statement that applies to 'client' and set up
 * the sortlist info in in client->message appropriately.
 */
static void
setup_query_sortlist(ns_client_t *client) {
	isc_netaddr_t netaddr;
	dns_rdatasetorderfunc_t order = NULL;
	void *order_arg = NULL;
	
	isc_netaddr_fromsockaddr(&netaddr, &client->peeraddr);
	switch (ns_sortlist_setup(client->view->sortlist,
			       &netaddr, &order_arg)) {
	case NS_SORTLISTTYPE_1ELEMENT:
		order = query_sortlist_order_1element;
		break;
	case NS_SORTLISTTYPE_2ELEMENT:
		order = query_sortlist_order_2element;
		break;
	case NS_SORTLISTTYPE_NONE:
		order = NULL;
		break;
	default:
		INSIST(0);
		break;
	}
	dns_message_setsortorder(client->message, order, order_arg);
}

/*
 * Do the bulk of query processing for the current query of 'client'.
 * If 'event' is non-NULL, we are returning from recursion and 'qtype'
 * is ignored.  Otherwise, 'qtype' is the query type.
 */
static void
query_find(ns_client_t *client, dns_fetchevent_t *event, dns_rdatatype_t qtype) {
	dns_db_t *db, *zdb;
	dns_dbnode_t *node;
	dns_rdatatype_t type;
	dns_name_t *fname, *zfname, *tname, *prefix;
	dns_rdataset_t *rdataset, *trdataset;
	dns_rdataset_t *sigrdataset, *zrdataset, *zsigrdataset;
	dns_rdataset_t **sigrdatasetp;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatasetiter_t *rdsiter;
	isc_boolean_t want_restart, authoritative, is_zone;
	unsigned int n, nlabels, nbits;
	dns_namereln_t namereln;
	int order;
	isc_buffer_t *dbuf;
	isc_buffer_t b;
	isc_result_t result, eresult;
	dns_fixedname_t fixed;
	dns_dbversion_t *version;
	dns_zone_t *zone;
	dns_rdata_cname_t cname;
	dns_rdata_dname_t dname;
	isc_boolean_t empty_wild;

	CTRACE("query_find");

	/*
	 * One-time initialization.
	 *
	 * It's especially important to initialize anything that the cleanup
	 * code might cleanup.
	 */

	eresult = ISC_R_SUCCESS;
	fname = NULL;
	zfname = NULL;
	rdataset = NULL;
	zrdataset = NULL;
	sigrdataset = NULL;
	zsigrdataset = NULL;
	node = NULL;
	db = NULL;
	zdb = NULL;
	version = NULL;
	zone = NULL;
	empty_wild = ISC_FALSE;

	if (event != NULL) {
		/*
		 * We're returning from recursion.  Restore the query context
		 * and resume.
		 */

		want_restart = ISC_FALSE;
		authoritative = ISC_FALSE;
		is_zone = ISC_FALSE;

		qtype = event->qtype;
		if (qtype == dns_rdatatype_sig)
			type = dns_rdatatype_any;
		else
			type = qtype;
		db = event->db;
		node = event->node;
		rdataset = event->rdataset;
		sigrdataset = event->sigrdataset;

		/*
		 * We'll need some resources...
		 */
		dbuf = query_getnamebuf(client);
		if (dbuf == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		fname = query_newname(client, dbuf, &b);
		if (fname == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		tname = dns_fixedname_name(&event->foundname);
		result = dns_name_copy(tname, fname, NULL);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}

		result = event->result;

		goto resume;
	}
	
	/*
	 * Not returning from recursion.
	 */

	/*
	 * If it's a SIG query, we'll iterate the node.
	 */
	if (qtype == dns_rdatatype_sig)
		type = dns_rdatatype_any;
	else
		type = qtype;

 restart:
	CTRACE("query_find: restart");
	want_restart = ISC_FALSE;
	authoritative = ISC_FALSE;
	version = NULL;

	/*
	 * First we must find the right database.
	 */
	result = query_getdb(client, client->query.qname, 0, &zone, &db,
			     &version, &is_zone);
	if (result != ISC_R_SUCCESS) {
		if (result == DNS_R_REFUSED)
			QUERY_ERROR(DNS_R_REFUSED);
		else
			QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}

	if (is_zone)
		authoritative = ISC_TRUE;
       
	if (event == NULL && client->query.restarts == 0) {
		if (is_zone) {
			dns_zone_attach(zone, &client->query.authzone);
			dns_db_attach(db, &client->query.authdb);
		}
		client->query.authdbset = ISC_TRUE;
	}

 db_find:
	CTRACE("query_find: db_find");
	/*
	 * We'll need some resources...
	 */
	dbuf = query_getnamebuf(client);
	if (dbuf == NULL) {
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}
	fname = query_newname(client, dbuf, &b);
	rdataset = query_newrdataset(client);
	if (fname == NULL || rdataset == NULL) {
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}
	if (WANTDNSSEC(client)) {
		sigrdataset = query_newrdataset(client);
		if (sigrdataset == NULL) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
	}

	/*
	 * Now look for an answer in the database.
	 */
	result = dns_db_find(db, client->query.qname, version, type,
			     client->query.dboptions, client->now,
			     &node, fname, rdataset, sigrdataset);

	/*
	 * We interrupt our normal query processing to bring you this special
	 * case...
	 *
	 * RFC 2535 (DNSSEC), section 2.3.4, discusses various special
	 * cases that can occur at delegation points.
	 *
	 * One of these cases is that the NULL KEY for an unsecure zone
	 * may occur in the delegating zone instead of in the delegated zone.
	 * If we're authoritative for both zones, we need to look for the
	 * key in the delegator if we didn't find it in the delegatee.  If
	 * we didn't do this, a client doing DNSSEC validation could fail
	 * because it couldn't get the NULL KEY.
	 */
	if (type == dns_rdatatype_key &&
	    is_zone &&
	    result == DNS_R_NXRRSET &&
	    !dns_db_issecure(db) &&
	    dns_name_equal(client->query.qname, dns_db_origin(db))) {
		/*
		 * We're looking for a KEY at the top of an unsecure zone,
		 * and we didn't find it.
		 */
		result = query_findparentkey(client, client->query.qname,
					     &zone, &db, &version, &node,
					     rdataset, sigrdataset);
		if (result == ISC_R_SUCCESS) {
			/*
			 * We found the parent KEY.
			 *
			 * zone, db, version, node, rdataset, and sigrdataset
			 * have all been updated to refer to the parent's
			 * data.  We will resume query processing as if
			 * we had looked for the KEY in the parent zone in
			 * the first place.
			 *
			 * We need to set fname correctly.  We do this here
			 * instead of in query_findparentkey() because
			 * dns_name_copy() can fail (though it shouldn't
			 * ever do so since we should have enough space).
			 */
			result = dns_name_copy(client->query.qname,
					       fname, NULL);
			if (result != ISC_R_SUCCESS) {
				QUERY_ERROR(DNS_R_SERVFAIL);
				goto cleanup;
			}
		} else {
			/*
			 * We couldn't find the KEY in a parent zone.
			 * Continue with processing of the original
			 * results of dns_db_find().
			 */
			result = DNS_R_NXRRSET;
		}
	}

 resume:
	CTRACE("query_find: resume");
	switch (result) {
	case ISC_R_SUCCESS:
		/*
		 * This case is handled in the main line below.
		 */
		break;
	case DNS_R_GLUE:
	case DNS_R_ZONECUT:
		/*
		 * These cases are handled in the main line below.
		 */
		INSIST(is_zone);
		authoritative = ISC_FALSE;
		break;
	case ISC_R_NOTFOUND:
		/*
		 * The cache doesn't even have the root NS.  Get them from
		 * the hints DB.
		 */
		INSIST(!is_zone);
		if (db != NULL)
			dns_db_detach(&db);

		if (client->view->hints == NULL) {
			/* We have no hints. */
			result = ISC_R_FAILURE;
		} else {
			dns_db_attach(client->view->hints, &db);
			result = dns_db_find(db, dns_rootname,
					     NULL, dns_rdatatype_ns,
					     0, client->now, &node, fname,
					     rdataset, sigrdataset);
		}
		if (result != ISC_R_SUCCESS) {
			/*
			 * Nonsensical root hints may require cleanup.
			 */
			if (dns_rdataset_isassociated(rdataset))
				dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
			if (node != NULL)
				dns_db_detachnode(db, &node);

			/*
			 * We don't have any root server hints, but
			 * we may have working forwarders, so try to
			 * recurse anyway.
			 */
			if (RECURSIONOK(client)) {
				result = query_recurse(client, qtype,
						       NULL, NULL);
				if (result == ISC_R_SUCCESS)
					client->query.attributes |=
						NS_QUERYATTR_RECURSING;
				else {
					/* Unable to recurse. */
					QUERY_ERROR(DNS_R_SERVFAIL);
				}
				goto cleanup;
			} else {
				/* Unable to give root server referral. */
				QUERY_ERROR(DNS_R_SERVFAIL);
				goto cleanup;
			}
		}
		/*
		 * XXXRTH  We should trigger root server priming here.
		 */
		/* FALLTHROUGH */
	case DNS_R_DELEGATION:
		authoritative = ISC_FALSE;
		if (is_zone) {
			/*
			 * We're authoritative for an ancestor of QNAME.
			 */
			if (!USECACHE(client) || !RECURSIONOK(client)) {
				/*
				 * If we don't have a cache, this is the best
				 * answer.
				 *
				 * If the client is making a nonrecursive
				 * query we always give out the authoritative
				 * delegation.  This way even if we get
				 * junk in our cache, we won't fail in our
				 * role as the delegating authority if another
				 * nameserver asks us about a delegated
				 * subzone.
				 *
				 * We enable the retrieval of glue for this
				 * database by setting client->query.gluedb.
				 */
				client->query.gluedb = db;
				client->query.isreferral = ISC_TRUE;
				/*
				 * We must ensure NOADDITIONAL is off,
				 * because the generation of
				 * additional data is required in
				 * delegations.
				 */
				client->query.attributes &=
					~NS_QUERYATTR_NOADDITIONAL;
				if (sigrdataset != NULL)
					sigrdatasetp = &sigrdataset;
				else
					sigrdatasetp = NULL;
				query_addrrset(client, &fname,
					       &rdataset, sigrdatasetp,
					       dbuf, DNS_SECTION_AUTHORITY);
				client->query.gluedb = NULL;
			} else {
				/*
				 * We might have a better answer or delegation
				 * in the cache.  We'll remember the current
				 * values of fname, rdataset, and sigrdataset.
				 * We'll then go looking for QNAME in the
				 * cache.  If we find something better, we'll
				 * use it instead.
				 */
				query_keepname(client, fname, dbuf);
				zdb = db;
				zfname = fname;
				fname = NULL;
				zrdataset = rdataset;
				rdataset = NULL;
				zsigrdataset = sigrdataset;
				sigrdataset = NULL;
				dns_db_detachnode(db, &node);
				version = NULL;
				db = NULL;
				dns_db_attach(client->view->cachedb, &db);
				is_zone = ISC_FALSE;
				goto db_find;
			}
		} else {
			if (zfname != NULL &&
			    !dns_name_issubdomain(fname, zfname)) {
				/*
				 * We've already got a delegation from
				 * authoritative data, and it is better
				 * than what we found in the cache.  Use
				 * it instead of the cache delegation.
				 */
				query_releasename(client, &fname);
				fname = zfname;
				zfname = NULL;
				/*
				 * We've already done query_keepname() on
				 * zfname, so we must set dbuf to NULL to
				 * prevent query_addrrset() from trying to
				 * call query_keepname() again.
				 */
				dbuf = NULL;
				query_putrdataset(client, &rdataset);
				if (sigrdataset != NULL)
					query_putrdataset(client,
							  &sigrdataset);
				rdataset = zrdataset;
				zrdataset = NULL;
				sigrdataset = zsigrdataset;
				zsigrdataset = NULL;
				/*
				 * We don't clean up zdb here because we
				 * may still need it.  It will get cleaned
				 * up by the main cleanup code.
				 */
			}

			if (RECURSIONOK(client)) {
				/*
				 * Recurse!
				 */
				if (type == dns_rdatatype_key)
					result = query_recurse(client, qtype,
							       NULL, NULL);
				else
					result = query_recurse(client, qtype,
							       fname, rdataset);
				if (result == ISC_R_SUCCESS)
					client->query.attributes |=
						NS_QUERYATTR_RECURSING;
				else
					QUERY_ERROR(DNS_R_SERVFAIL);
			} else {
				/*
				 * This is the best answer.
				 */
				client->query.attributes |=
					NS_QUERYATTR_CACHEGLUEOK;
				client->query.gluedb = zdb;
				client->query.isreferral = ISC_TRUE;
				/*
				 * We must ensure NOADDITIONAL is off,
				 * because the generation of
				 * additional data is required in
				 * delegations.
				 */
				client->query.attributes &=
					~NS_QUERYATTR_NOADDITIONAL;
				if (sigrdataset != NULL)
					sigrdatasetp = &sigrdataset;
				else
					sigrdatasetp = NULL;
				query_addrrset(client, &fname,
					       &rdataset, sigrdatasetp,
					       dbuf, DNS_SECTION_AUTHORITY);
				client->query.gluedb = NULL;
				client->query.attributes &=
					~NS_QUERYATTR_CACHEGLUEOK;
			}
		}
		goto cleanup;
	case DNS_R_EMPTYNAME:
		result = DNS_R_NXRRSET;
		/* FALLTHROUGH */
	case DNS_R_NXRRSET:
		INSIST(is_zone);
		if (dns_rdataset_isassociated(rdataset)) {
			/*
			 * If we've got a NXT record, we need to save the
			 * name now because we're going call query_addsoa()
			 * below, and it needs to use the name buffer.
			 */
			query_keepname(client, fname, dbuf);
		} else {
			/*
			 * We're not going to use fname, and need to release
			 * our hold on the name buffer so query_addsoa()
			 * may use it.
			 */
			query_releasename(client, &fname);
		}
		/*
		 * Add SOA.
		 */
		result = query_addsoa(client, db, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(result);
			goto cleanup;
		}
		/*
		 * Add NXT record if we found one.
		 */
		if (dns_rdataset_isassociated(rdataset)) {
			if (WANTDNSSEC(client))
				query_addrrset(client, &fname, &rdataset,
					       &sigrdataset,
					       NULL, DNS_SECTION_AUTHORITY);
		}
		goto cleanup;
	case DNS_R_EMPTYWILD:
		empty_wild = ISC_TRUE;
		/* FALLTHROUGH */
	case DNS_R_NXDOMAIN:
		INSIST(is_zone);
		if (dns_rdataset_isassociated(rdataset)) {
			/*
			 * If we've got a NXT record, we need to save the
			 * name now because we're going call query_addsoa()
			 * below, and it needs to use the name buffer.
			 */
			query_keepname(client, fname, dbuf);
		} else {
			/*
			 * We're not going to use fname, and need to release
			 * our hold on the name buffer so query_addsoa()
			 * may use it.
			 */
			query_releasename(client, &fname);
		}
		/*
		 * Add SOA.  If the query was for a SOA record force the
		 * ttl to zero so that it is possible for clients to find
		 * the containing zone of a arbitary name with a stub
		 * resolver and not have it cached.
		 */
		if (qtype == dns_rdatatype_soa)
			result = query_addsoa(client, db, ISC_TRUE);
		else
			result = query_addsoa(client, db, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(result);
			goto cleanup;
		}
		/*
		 * Add NXT record if we found one.
		 */
		if (dns_rdataset_isassociated(rdataset)) {
			if (WANTDNSSEC(client))
				query_addrrset(client, &fname, &rdataset,
					       &sigrdataset,
					       NULL, DNS_SECTION_AUTHORITY);
		}
		/*
		 * Set message rcode.
		 */
		if (empty_wild)
			client->message->rcode = dns_rcode_noerror;
		else
			client->message->rcode = dns_rcode_nxdomain;
		goto cleanup;
	case DNS_R_NCACHENXDOMAIN:
	case DNS_R_NCACHENXRRSET:
		INSIST(!is_zone);
		authoritative = ISC_FALSE;
		/*
		 * Set message rcode, if required.
		 */
		if (result == DNS_R_NCACHENXDOMAIN)
			client->message->rcode = dns_rcode_nxdomain;
		/*
		 * We don't call query_addrrset() because we don't need any
		 * of its extra features (and things would probably break!).
		 */
		query_keepname(client, fname, dbuf);
		dns_message_addname(client->message, fname,
				    DNS_SECTION_AUTHORITY);
		ISC_LIST_APPEND(fname->list, rdataset, link);
		fname = NULL;
		rdataset = NULL;
		goto cleanup;
	case DNS_R_CNAME:
		/*
		 * Keep a copy of the rdataset.  We have to do this because
		 * query_addrrset may clear 'rdataset' (to prevent the
		 * cleanup code from cleaning it up).
		 */
		trdataset = rdataset;
		/*
		 * Add the CNAME to the answer section.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		/*
		 * We set the PARTIALANSWER attribute so that if anything goes
		 * wrong later on, we'll return what we've got so far.
		 */
		client->query.attributes |= NS_QUERYATTR_PARTIALANSWER;
		/*
		 * Reset qname to be the target name of the CNAME and restart
		 * the query.
		 */
		tname = NULL;
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = dns_rdataset_first(trdataset);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_rdataset_current(trdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &cname, NULL);
		dns_rdata_reset(&rdata);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_name_init(tname, NULL);
		result = dns_name_dup(&cname.cname, client->mctx, tname);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			dns_rdata_freestruct(&cname);
			goto cleanup;
		}
		dns_rdata_freestruct(&cname);
		query_maybeputqname(client);
		client->query.qname = tname;
		want_restart = ISC_TRUE;
		goto addauth;
	case DNS_R_DNAME:
		/*
		 * Compare the current qname to the found name.  We need
		 * to know how many labels and bits are in common because
		 * we're going to have to split qname later on.
		 */
		namereln = dns_name_fullcompare(client->query.qname, fname,
						&order, &nlabels, &nbits);
		INSIST(namereln == dns_namereln_subdomain);
		/*
		 * Keep a copy of the rdataset.  We have to do this because
		 * query_addrrset may clear 'rdataset' (to prevent the
		 * cleanup code from cleaning it up).
		 */
		trdataset = rdataset;
		/*
		 * Add the DNAME to the answer section.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		/*
		 * We set the PARTIALANSWER attribute so that if anything goes
		 * wrong later on, we'll return what we've got so far.
		 */
		client->query.attributes |= NS_QUERYATTR_PARTIALANSWER;
		/*
		 * Get the target name of the DNAME.
		 */
		tname = NULL;
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = dns_rdataset_first(trdataset);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_rdataset_current(trdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dname, NULL);
		dns_rdata_reset(&rdata);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		dns_name_init(tname, NULL);
		dns_name_clone(&dname.dname, tname);
		dns_rdata_freestruct(&dname);
		/*
		 * Construct the new qname.
		 */
		dns_fixedname_init(&fixed);
		prefix = dns_fixedname_name(&fixed);
		result = dns_name_split(client->query.qname, nlabels, nbits,
					prefix, NULL);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		INSIST(fname == NULL);
		dbuf = query_getnamebuf(client);
		if (dbuf == NULL) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		fname = query_newname(client, dbuf, &b);
		if (fname == NULL) {
			dns_message_puttempname(client->message, &tname);
			goto cleanup;
		}
		result = dns_name_concatenate(prefix, tname, fname, NULL);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(client->message, &tname);
			if (result == ISC_R_NOSPACE) {
				/*
				 * RFC 2672, section 4.1, subsection 3c says
				 * we should return YXDOMAIN if the constructed
				 * name would be too long.
				 */
				client->message->rcode = dns_rcode_yxdomain;
			}
			goto cleanup;
		}
		query_keepname(client, fname, dbuf);
		/*
		 * Synthesize a CNAME for this DNAME.
		 *
		 * We want to synthesize a CNAME since if we don't
		 * then older software that doesn't understand DNAME
		 * will not chain like it should.
		 *
		 * We do not try to synthesize a signature because we hope
		 * that security aware servers will understand DNAME.  Also,
		 * even if we had an online key, making a signature
		 * on-the-fly is costly, and not really legitimate anyway
		 * since the synthesized CNAME is NOT in the zone.
		 */
		dns_name_init(tname, NULL);
		query_addcnamelike(client, client->query.qname, fname,
				   0, &tname, dns_rdatatype_cname);
		if (tname != NULL)
			dns_message_puttempname(client->message, &tname);
		/*
		 * Switch to the new qname and restart.
		 */
		query_maybeputqname(client);
		client->query.qname = fname;
		fname = NULL;
		want_restart = ISC_TRUE;
		goto addauth;
	default:
		/*
		 * Something has gone wrong.
		 */
		QUERY_ERROR(DNS_R_SERVFAIL);
		goto cleanup;
	}

	if (type == dns_rdatatype_any) {
		/*
		 * XXXRTH  Need to handle zonecuts with special case
		 * code.
		 */
		n = 0;
		rdsiter = NULL;
		result = dns_db_allrdatasets(db, node, version, 0, &rdsiter);
		if (result != ISC_R_SUCCESS) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
		/*
		 * Calling query_addrrset() with a non-NULL dbuf is going
		 * to either keep or release the name.  We don't want it to
		 * release fname, since we may have to call query_addrrset()
		 * more than once.  That means we have to call query_keepname()
		 * now, and pass a NULL dbuf to query_addrrset().
		 *
		 * If we do a query_addrrset() below, we must set fname to
		 * NULL before leaving this block, otherwise we might try to
		 * cleanup fname even though we're using it!
		 */
		query_keepname(client, fname, dbuf);
		tname = fname;
		result = dns_rdatasetiter_first(rdsiter);
		while (result == ISC_R_SUCCESS) {
			dns_rdatasetiter_current(rdsiter, rdataset);
			if ((qtype == dns_rdatatype_any ||
			     rdataset->type == qtype) && rdataset->type != 0) {
				query_addrrset(client,
					       fname != NULL ? &fname : &tname,
					       &rdataset, NULL,
					       NULL, DNS_SECTION_ANSWER);
				n++;
				INSIST(tname != NULL);
				/*
				 * rdataset is non-NULL only in certain pathological
				 * cases involving DNAMEs.
				 */
				if (rdataset != NULL)
					query_putrdataset(client, &rdataset);
				rdataset = query_newrdataset(client);
				if (rdataset == NULL)
					break;
			} else {
				/*
				 * We're not interested in this rdataset.
				 */
				dns_rdataset_disassociate(rdataset);
			}
			result = dns_rdatasetiter_next(rdsiter);
		}

		if (fname != NULL)
			dns_message_puttempname(client->message, &fname);

		if (n == 0) {
			/*
			 * We didn't match any rdatasets.
			 */
			if (qtype == dns_rdatatype_sig &&
			    result == ISC_R_NOMORE) {
				/*
				 * XXXRTH  If this is a secure zone and we
				 * didn't find any SIGs, we should generate
				 * an error unless we were searching for
				 * glue.  Ugh.
				 */
				/*
				 * We were searching for SIG records in
				 * a nonsecure zone.  Send a "no error,
				 * no data" response.
				 */
				/*
				 * Add SOA.
				 */
				result = query_addsoa(client, db, ISC_FALSE);
				if (result == ISC_R_SUCCESS)
					result = ISC_R_NOMORE;
			} else {
				/*
				 * Something went wrong.
				 */
				result = DNS_R_SERVFAIL;
			}
		}
		dns_rdatasetiter_destroy(&rdsiter);
		if (result != ISC_R_NOMORE) {
			QUERY_ERROR(DNS_R_SERVFAIL);
			goto cleanup;
		}
	} else {
		/*
		 * This is the "normal" case -- an ordinary question to which
		 * we know the answer.
		 */
		if (sigrdataset != NULL)
			sigrdatasetp = &sigrdataset;
		else
			sigrdatasetp = NULL;
		query_addrrset(client, &fname, &rdataset, sigrdatasetp, dbuf,
			       DNS_SECTION_ANSWER);
		/*
		 * We shouldn't ever fail to add 'rdataset'
		 * because it's already in the answer.
		 */
		INSIST(rdataset == NULL);
	}

 addauth:
	CTRACE("query_find: addauth");
	/*
	 * Add NS records to the authority section (if we haven't already
	 * added them to the answer section).
	 */
	if (!want_restart && !NOAUTHORITY(client)
	    ) {
		if (is_zone) {
			if (!((qtype == dns_rdatatype_ns ||
			       qtype == dns_rdatatype_any) &&
			      dns_name_equal(client->query.qname,
					     dns_db_origin(db))))
				query_addns(client, db);
		} else if (qtype != dns_rdatatype_ns) {
			if (fname != NULL)
				query_releasename(client, &fname);
			query_addbestns(client);
		}
	}

 cleanup:
	CTRACE("query_find: cleanup");
	/*
	 * General cleanup.
	 */
	if (rdataset != NULL)
		query_putrdataset(client, &rdataset);
	if (sigrdataset != NULL)
		query_putrdataset(client, &sigrdataset);
	if (fname != NULL)
		query_releasename(client, &fname);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (zdb != NULL) {
		query_putrdataset(client, &zrdataset);
		if (zsigrdataset != NULL)
			query_putrdataset(client, &zsigrdataset);
		if (zfname != NULL)
			query_releasename(client, &zfname);
		dns_db_detach(&zdb);
	}
	if (event != NULL)
		isc_event_free(ISC_EVENT_PTR(&event));

	/*
	 * AA bit.
	 */
	if (client->query.restarts == 0 && !authoritative) {
		/*
		 * We're not authoritative, so we must ensure the AA bit
		 * isn't set.
		 */
		client->message->flags &= ~DNS_MESSAGEFLAG_AA;
	}

	/*
	 * Restart the query?
	 */
	if (want_restart && client->query.restarts < MAX_RESTARTS) {
		client->query.restarts++;
		goto restart;
	}

	if (eresult != ISC_R_SUCCESS &&
	    (!PARTIALANSWER(client) || WANTRECURSION(client))) {
		/*
		 * If we don't have any answer to give the client,
		 * or if the client requested recursion and thus wanted
		 * the complete answer, send an error response.
		 */
		 query_error(client, eresult);
		 ns_client_detach(&client);
	} else if (!RECURSING(client)) {
		/*
		 * We are done.  Set up sortlist data for the message
		 * rendering code, make a final tweak to the AA bit if the
		 * auth-nxdomain config option says so, then render and
		 * send the response.
		 */
		setup_query_sortlist(client);

		if (client->message->rcode == dns_rcode_nxdomain &&
		    client->view->auth_nxdomain == ISC_TRUE)
			client->message->flags |= DNS_MESSAGEFLAG_AA;

		query_send(client);
		ns_client_detach(&client);
	}
	CTRACE("query_find: done");
}

static inline void
log_query(ns_client_t *client) {
	char namebuf[DNS_NAME_FORMATSIZE];
	char typename[DNS_RDATATYPE_FORMATSIZE];
	char classname[DNS_RDATACLASS_FORMATSIZE];
	dns_rdataset_t *rdataset;
	int level = ISC_LOG_INFO;

	if (! isc_log_wouldlog(ns_g_lctx, level))
		return;

	rdataset = ISC_LIST_HEAD(client->query.qname->list);
	INSIST(rdataset != NULL);
	dns_name_format(client->query.qname, namebuf, sizeof(namebuf));
	dns_rdataclass_format(rdataset->rdclass, classname, sizeof(classname));
	dns_rdatatype_format(rdataset->type, typename, sizeof(typename));

	ns_client_log(client, NS_LOGCATEGORY_QUERIES, NS_LOGMODULE_QUERY,
		      level, "query: %s %s %s", namebuf, classname, typename);
}

void
ns_query_start(ns_client_t *client) {
	isc_result_t result;
	dns_message_t *message = client->message;
	dns_rdataset_t *rdataset;
	ns_client_t *qclient;
	dns_rdatatype_t qtype;

	CTRACE("ns_query_start");

	/*
	 * Ensure that appropriate cleanups occur.
	 */
	client->next = query_next_callback;

	if ((message->flags & DNS_MESSAGEFLAG_RD) != 0)
		client->query.attributes |= NS_QUERYATTR_WANTRECURSION;
	
#ifdef ISC_RFC2535
	if ((client->extflags & DNS_MESSAGEEXTFLAG_DO) != 0 ||
	    (message->flags & DNS_MESSAGEFLAG_AD) != 0)
		client->query.attributes |= NS_QUERYATTR_WANTDNSSEC;
#endif
	
	if (client->view->minimalresponses)
		client->query.attributes |= (NS_QUERYATTR_NOAUTHORITY |
					     NS_QUERYATTR_NOADDITIONAL);

	if ((client->view->cachedb == NULL)
	    || (!client->view->additionalfromcache)) {
		/*
		 * We don't have a cache.  Turn off cache support and
		 * recursion.
		 */
		client->query.attributes &=
			~(NS_QUERYATTR_RECURSIONOK|NS_QUERYATTR_CACHEOK);
	} else if ((client->attributes & NS_CLIENTATTR_RA) == 0 ||
		   (message->flags & DNS_MESSAGEFLAG_RD) == 0) {
		/*
		 * If the client isn't allowed to recurse (due to
		 * "recursion no", the allow-recursion ACL, or the
		 * lack of a resolver in this view), or if it
		 * doesn't want recursion, turn recursion off.
		 */
		client->query.attributes &= ~NS_QUERYATTR_RECURSIONOK;
	}

	/*
	 * Get the question name.
	 */
	result = dns_message_firstname(message, DNS_SECTION_QUESTION);
	if (result != ISC_R_SUCCESS) {
		query_error(client, result);
		return;
	}
	dns_message_currentname(message, DNS_SECTION_QUESTION,
				&client->query.qname);
	client->query.origqname = client->query.qname;
	result = dns_message_nextname(message, DNS_SECTION_QUESTION);
	if (result != ISC_R_NOMORE) {
		if (result == ISC_R_SUCCESS) {
			/*
			 * There's more than one QNAME in the question
			 * section.
			 */
			query_error(client, DNS_R_FORMERR);
		} else
			query_error(client, result);
		return;
	}

	if (ns_g_server->log_queries)
		log_query(client);

	/*
	 * Check for multiple question queries, since edns1 is dead.
	 */
	if (message->counts[DNS_SECTION_QUESTION] > 1) {
		query_error(client, DNS_R_FORMERR);
		return;
	}

	/*
	 * Check for meta-queries like IXFR and AXFR.
	 */
	rdataset = ISC_LIST_HEAD(client->query.qname->list);
	INSIST(rdataset != NULL);
	qtype = rdataset->type;
	if (dns_rdatatype_ismeta(qtype)) {
		switch (qtype) {
		case dns_rdatatype_any:
			break; /* Let query_find handle it. */
		case dns_rdatatype_ixfr:
		case dns_rdatatype_axfr:
			ns_xfr_start(client, rdataset->type);
			return;
		case dns_rdatatype_maila:
		case dns_rdatatype_mailb:
			query_error(client, DNS_R_NOTIMP);
			return;
		case dns_rdatatype_tkey:
			result = dns_tkey_processquery(client->message,
						ns_g_server->tkeyctx,
						client->view->dynamickeys);
			if (result == ISC_R_SUCCESS)
				query_send(client);
			else
				query_error(client, result);
			return;
		default: /* TSIG, etc. */
			query_error(client, DNS_R_FORMERR);
			return;
		}
	}

	/*
	 * If the client has requested that DNSSEC checking be disabled,
	 * allow lookups to return pending data and instruct the resolver
	 * to return data before validation has completed.
	 */
	if (message->flags & DNS_MESSAGEFLAG_CD ||
	    qtype == dns_rdatatype_sig)
	{
		client->query.dboptions |= DNS_DBFIND_PENDINGOK;
		client->query.fetchoptions |= DNS_FETCHOPT_NOVALIDATE;
	}

	/*
	 * This is an ordinary query.
	 */
	result = dns_message_reply(message, ISC_TRUE);
	if (result != ISC_R_SUCCESS) {
		query_next(client, result);
		return;
	}

	/*
	 * Assume authoritative response until it is known to be
	 * otherwise.
	 */
	message->flags |= DNS_MESSAGEFLAG_AA;

	/*
	 * Set AD.  We must clear it if we add non-validated data to a
	 * response.
	 */
	if (WANTDNSSEC(client))
		message->flags |= DNS_MESSAGEFLAG_AD;

	/*
	 * Synthesize IPv6 responses if appropriate.
	 */
	if (RECURSIONOK(client) &&
	    (qtype == dns_rdatatype_aaaa || qtype == dns_rdatatype_ptr) &&
	    client->message->rdclass == dns_rdataclass_in &&
	    ns_client_checkacl(client, "v6 synthesis",
			       client->view->v6synthesisacl,
			       ISC_FALSE, ISC_LOG_DEBUG(9)) == ISC_R_SUCCESS)
	{
		if (qtype == dns_rdatatype_aaaa) {
			qclient = NULL;
			ns_client_attach(client, &qclient);
			synth_fwd_start(qclient);
			return;
		} else {
			INSIST(qtype == dns_rdatatype_ptr);
			 /* Must be 32 nibbles + "ip6" + "int" + root */
			if (dns_name_countlabels(client->query.qname) == 32 + 3 &&
			    dns_name_issubdomain(client->query.qname, &ip6int_name)) {
				qclient = NULL;
				ns_client_attach(client, &qclient);
				synth_rev_start(qclient);
				return;
			}
		}
	}

	qclient = NULL;
	ns_client_attach(client, &qclient);
	query_find(qclient, NULL, qtype);
}

/*
 * Generate a synthetic IPv6 forward mapping response for the current
 * query of 'client'.
 */
static void
synth_fwd_start(ns_client_t *client) {
	ns_client_log(client, NS_LOGCATEGORY_CLIENT, NS_LOGMODULE_QUERY,
		      ISC_LOG_DEBUG(5), "generating synthetic AAAA response");

	synth_fwd_startfind(client);
}

/*
 * Start an ADB find to get addresses, or more addresses, for 
 * a synthetic IPv6 forward mapping response.
 */
static void
synth_fwd_startfind(ns_client_t *client) {
	dns_adbfind_t *find = NULL;
	isc_result_t result;
	dns_fixedname_t target_fixed;
	dns_name_t *target;

	dns_fixedname_init(&target_fixed);
	target = dns_fixedname_name(&target_fixed);

 find_again:
	result = dns_adb_createfind(client->view->adb, client->task,
			    synth_fwd_finddone, client, client->query.qname,
			    dns_rootname, 
			    DNS_ADBFIND_WANTEVENT | DNS_ADBFIND_RETURNLAME |
			    DNS_ADBFIND_INET6, client->now,
			    target, 0, &find);

	ns_client_log(client, NS_LOGCATEGORY_CLIENT, NS_LOGMODULE_QUERY,
		      ISC_LOG_DEBUG(5), "find returned %s",
		      isc_result_totext(result));

	if (result == DNS_R_ALIAS) {
		dns_name_t *ptarget = NULL;
		dns_name_t *tname = NULL;
		isc_buffer_t *dbuf;
		isc_buffer_t b;

		/*
		 * Make a persistent copy of the 'target' name data in 'ptarget';
		 * it will become the new query name.
		 */
		dbuf = query_getnamebuf(client);
		if (dbuf == NULL)
			goto fail;
		ptarget = query_newname(client, dbuf, &b);
		if (ptarget == NULL)
			goto fail;
		dns_name_copy(target, ptarget, NULL);
		
		dns_adb_destroyfind(&find);

		/*
		 * Get another temporary name 'tname' for insertion into the
		 * response message.
		 */
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto fail;
		dns_name_init(tname, NULL);
		result = query_addcnamelike(client, client->query.qname,
					    ptarget, 0 /* XXX ttl */, &tname,
					    dns_rdatatype_cname);
		if (tname != NULL)
			dns_message_puttempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto fail;

		query_maybeputqname(client);
		client->query.qname = ptarget;
		query_keepname(client, ptarget, dbuf);
		ptarget = NULL;
		if (client->query.restarts < MAX_RESTARTS) {
			client->query.restarts++;
			goto find_again;
		} else {
			/*
			 * Probably a CNAME loop.  Reply with partial
			 * CNAME chain.
			 */
			result = ISC_R_SUCCESS;
			goto done;
		}
	} else if (result != ISC_R_SUCCESS) {
		if (find != NULL)
			dns_adb_destroyfind(&find);
		goto fail;
	}

	if ((find->options & DNS_ADBFIND_WANTEVENT) != 0) {
		ns_client_log(client, NS_LOGCATEGORY_CLIENT, NS_LOGMODULE_QUERY,
			      ISC_LOG_DEBUG(5), "find will send event");
	} else {
		synth_fwd_respond(client, find);
		dns_adb_destroyfind(&find);
	}
	return;

 fail:
	result = DNS_R_SERVFAIL;
 done:
	synth_finish(client, result);
}

/*
 * Handle an ADB finddone event generated as part of synthetic IPv6
 * forward mapping processing.
 */
static void
synth_fwd_finddone(isc_task_t *task, isc_event_t *ev) {
	ns_client_t *client = ev->ev_arg;
	dns_adbfind_t *find = ev->ev_sender;
	isc_eventtype_t evtype = ev->ev_type;

	UNUSED(task);

	ns_client_log(client, NS_LOGCATEGORY_CLIENT, NS_LOGMODULE_QUERY,
		      ISC_LOG_DEBUG(5), "got find event");

	if (evtype == DNS_EVENT_ADBNOMOREADDRESSES)
		synth_fwd_respond(client, find);
	else if (evtype == DNS_EVENT_ADBMOREADDRESSES)
		synth_fwd_startfind(client);
	else
		synth_finish(client, DNS_R_SERVFAIL);

	isc_event_free(&ev);
	dns_adb_destroyfind(&find);
	
}

/*
 * Generate a synthetic IPv6 forward mapping response based on
 * a completed ADB lookup.
 */
static void
synth_fwd_respond(ns_client_t *client, dns_adbfind_t *find) {
	dns_adbaddrinfo_t *ai;
	dns_name_t *tname = NULL;
	dns_rdataset_t *rdataset = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	isc_result_t result;

	result = dns_message_gettempname(client->message, &tname);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	dns_name_init(tname, NULL);
		
	result = dns_message_gettemprdatalist(client->message, &rdatalist);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	
	result = dns_message_gettemprdataset(client->message, &rdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	dns_rdataset_init(rdataset);

	ISC_LIST_INIT(rdatalist->rdata);

	rdatalist->type = dns_rdatatype_aaaa;
	rdatalist->covers = 0;
	rdatalist->rdclass = client->message->rdclass;
	rdatalist->ttl = 0;

	dns_name_clone(client->query.qname, tname);
	
	for (ai = ISC_LIST_HEAD(find->list);
	     ai != NULL;
	     ai = ISC_LIST_NEXT(ai, publink)) {
		dns_rdata_t *rdata = NULL;
		
		struct sockaddr_in6 *sin6 = &ai->sockaddr.type.sin6;
		/*
		 * Could it be useful to return IPv4 addresses as A records?
		 */
		if (sin6->sin6_family != AF_INET6)
			continue;

		result = dns_message_gettemprdata(client->message, &rdata);
		if (result != ISC_R_SUCCESS)
			goto cleanup;

		rdata->data = (unsigned char *) &sin6->sin6_addr;
		rdata->length = 16;
		rdata->rdclass = client->message->rdclass;
		rdata->type = dns_rdatatype_aaaa;
		ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	}

	dns_rdatalist_tordataset(rdatalist, rdataset);

	query_addrrset(client, &tname, &rdataset, NULL, NULL,
		       DNS_SECTION_ANSWER);

 cleanup:
	if (tname != NULL)
		dns_message_puttempname(client->message, &tname);

	if (rdataset != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		dns_message_puttemprdataset(client->message, &rdataset);
	}

	synth_finish(client, result);
}

/*
 * Finish synthetic IPv6 forward mapping processing.
 */
static void
synth_finish(ns_client_t *client, isc_result_t result) {
	if (result == ISC_R_SUCCESS)
		query_send(client);
	else
		query_error(client, result);
	ns_client_detach(&client);	
}

static signed char ascii2hex[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1
};

/*
 * Convert label 'i' of 'name' into its hexadecimal value, storing it
 * in '*hexp'.  If the label is not a valid hex nibble, return ISC_R_FAILURE.
 */
static isc_result_t
label2hex(dns_name_t *name, int i, int *hexp) {
	isc_region_t label;
	int hexval;
	dns_name_getlabel(name, i, &label);
	if (label.length != 2 || label.base[0] != '\001')
		return (ISC_R_FAILURE);
	hexval = ascii2hex[label.base[1]];
	if (hexval == -1)
		return (ISC_R_FAILURE);
	*hexp = hexval;
	return (ISC_R_SUCCESS);
}

/*
 * Convert the ip6.int name 'name' into the corresponding IPv6 address
 * in 'na'. 
 */
static isc_result_t
nibbles2netaddr(dns_name_t *name, isc_netaddr_t *na) {
	isc_result_t result;
	struct in6_addr ina6;
	unsigned char *addrdata = (unsigned char *) &ina6;
	int i;

	for (i = 0; i < 16; i++) {
		int hex0, hex1;
		result = label2hex(name, 2 * i, &hex0);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = label2hex(name, 2 * i + 1, &hex1);
		if (result != ISC_R_SUCCESS)
			return (result);
		addrdata[15-i] = (hex1 << 4) | hex0;
	}
	isc_netaddr_fromin6(na, &ina6);
	return (ISC_R_SUCCESS);
}

/*
 * Generate a synthetic IPv6 reverse mapping response for the current
 * query of 'client'.
 */
static void
synth_rev_start(ns_client_t *client) {
	isc_result_t result;
	dns_byaddr_t *byaddr_dummy = NULL;
	
	ns_client_log(client, NS_LOGCATEGORY_CLIENT, NS_LOGMODULE_QUERY,
		      ISC_LOG_DEBUG(5), "generating synthetic PTR response");

	result = nibbles2netaddr(client->query.qname, &client->query.synth.na);
	if (result != ISC_R_SUCCESS) {
		result = DNS_R_NXDOMAIN;
		goto cleanup;
	}

	/* Try IP6.ARPA first. */
	result = dns_byaddr_create(client->mctx,
				   &client->query.synth.na,
				   client->view,
				   DNS_BYADDROPT_IPV6NIBBLE,
				   client->task,
				   synth_rev_byaddrdone_arpa,
				   client, &byaddr_dummy);
	if (result == ISC_R_SUCCESS)
		return; /* Wait for completion event. */
 cleanup:
	synth_finish(client, result);
}

static void
synth_rev_byaddrdone_arpa(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	dns_byaddrevent_t *bevent = (dns_byaddrevent_t *)event;
	ns_client_t *client = event->ev_arg;
	dns_byaddr_t *byaddr = event->ev_sender;
	dns_byaddr_t *byaddr_dummy = NULL;	

	UNUSED(task);

	if (bevent->result == ISC_R_SUCCESS) {
		synth_rev_respond(client, bevent);
	} else {
		/* Try IP6.INT next. */
		result = dns_byaddr_create(client->mctx,
					   &client->query.synth.na,
					   client->view,
					   DNS_BYADDROPT_IPV6NIBBLE|
					   DNS_BYADDROPT_IPV6INT,
					   client->task,
					   synth_rev_byaddrdone_int,
					   client, &byaddr_dummy);
		if (result != ISC_R_SUCCESS)
			synth_finish(client, result);
	}
	dns_byaddr_destroy(&byaddr);
	isc_event_free(&event);
}

static void
synth_rev_byaddrdone_int(isc_task_t *task, isc_event_t *event) {
	dns_byaddrevent_t *bevent = (dns_byaddrevent_t *)event;
	ns_client_t *client = event->ev_arg;
	dns_byaddr_t *byaddr = event->ev_sender;
	
	UNUSED(task);

	if (bevent->result == ISC_R_SUCCESS) {
		synth_rev_respond(client, bevent);
	} else if (bevent->result == DNS_R_NCACHENXDOMAIN ||
		   bevent->result == DNS_R_NCACHENXRRSET ||
		   bevent->result == DNS_R_NXDOMAIN ||
		   bevent->result == DNS_R_NXRRSET) {
		/*
		 * We could give a NOERROR/NODATA response instead
		 * in some cases, but since there may be any combination
		 * of NXDOMAIN and NXRRSET results from the IP6.INT
		 * and IP6.ARPA lookups, it could still be wrong with
		 * respect to one or the other.
		 */
		synth_finish(client, DNS_R_NXDOMAIN);
	} else {
		synth_finish(client, bevent->result);
	}
	isc_event_free(&event);
	dns_byaddr_destroy(&byaddr);	
}

static void
synth_rev_respond(ns_client_t *client, dns_byaddrevent_t *bevent) {
	isc_result_t result = ISC_R_SUCCESS;
	dns_name_t *name;

	for (name = ISC_LIST_HEAD(bevent->names);
	     name != NULL;
	     name = ISC_LIST_NEXT(name, link))
	{
		dns_name_t *tname = NULL;
		
		/*
		 * Get a temporary name 'tname' for insertion into the
		 * response message.
		 */
		result = dns_message_gettempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto fail;
		dns_name_init(tname, NULL);

		result = query_addcnamelike(client, client->query.qname,
					    name, 0 /* XXX ttl */,
					    &tname, dns_rdatatype_ptr);
		if (tname != NULL)
			dns_message_puttempname(client->message, &tname);
		if (result != ISC_R_SUCCESS)
			goto fail;
	}
 fail:
	synth_finish(client, result);		
}
