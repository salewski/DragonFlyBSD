/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $Id: dbtable.h,v 1.16.2.1 2004/03/09 06:11:15 marka Exp $ */

#ifndef DNS_DBTABLE_H
#define DNS_DBTABLE_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS DB Tables
 *
 * XXX <TBS> XXX
 *
 * MP:
 *	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	None.
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

#include <isc/lang.h>

#include <dns/types.h>

#define DNS_DBTABLEFIND_NOEXACT		0x01

ISC_LANG_BEGINDECLS

isc_result_t
dns_dbtable_create(isc_mem_t *mctx, dns_rdataclass_t rdclass,
		   dns_dbtable_t **dbtablep);
/*
 * Make a new dbtable of class 'rdclass'
 *
 * Requires:
 *	mctx != NULL
 * 	dbtablep != NULL && *dptablep == NULL
 *	'rdclass' is a valid class
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 */

void
dns_dbtable_attach(dns_dbtable_t *source, dns_dbtable_t **targetp);
/*
 * Attach '*targetp' to 'source'.
 *
 * Requires:
 *
 *	'source' is a valid dbtable.
 *
 *	'targetp' points to a NULL dns_dbtable_t *.
 *
 * Ensures:
 *
 *	*targetp is attached to source.
 */

void
dns_dbtable_detach(dns_dbtable_t **dbtablep);
/*
 * Detach *dbtablep from its dbtable.
 *
 * Requires:
 *
 *	'*dbtablep' points to a valid dbtable.
 *
 * Ensures:
 *
 *	*dbtablep is NULL.
 *
 *	If '*dbtablep' is the last reference to the dbtable,
 *
 *		All resources used by the dbtable will be freed
 */

isc_result_t
dns_dbtable_add(dns_dbtable_t *dbtable, dns_db_t *db);
/*
 * Add 'db' to 'dbtable'.
 *
 * Requires:
 *	'dbtable' is a valid dbtable.
 *
 *	'db' is a valid database with the same class as 'dbtable'
 */

void
dns_dbtable_remove(dns_dbtable_t *dbtable, dns_db_t *db);
/*
 * Remove 'db' from 'dbtable'.
 *
 * Requires:
 *	'db' was previously added to 'dbtable'.
 */

void
dns_dbtable_adddefault(dns_dbtable_t *dbtable, dns_db_t *db);
/*
 * Use 'db' as the result of a dns_dbtable_find() if no better match is
 * available.
 */

void
dns_dbtable_getdefault(dns_dbtable_t *dbtable, dns_db_t **db);
/*
 * Get the 'db' used as the result of a dns_dbtable_find()
 * if no better match is available.
 */

void
dns_dbtable_removedefault(dns_dbtable_t *dbtable);
/*
 * Remove the default db from 'dbtable'.
 */

isc_result_t
dns_dbtable_find(dns_dbtable_t *dbtable, dns_name_t *name,
		 unsigned int options, dns_db_t **dbp);
/*
 * Find the deepest match to 'name' in the dbtable, and return it
 *
 * Notes:
 *	If the DNS_DBTABLEFIND_NOEXACT option is set, the best partial
 *	match (if any) to 'name' will be returned.
 *
 * Returns:  ISC_R_SUCCESS		on success
 *	     <something else>		no default and match
 */

ISC_LANG_ENDDECLS

#endif /* DNS_DBTABLE_H */
