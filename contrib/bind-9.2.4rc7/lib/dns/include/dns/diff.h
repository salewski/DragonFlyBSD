/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: diff.h,v 1.4.2.1 2004/03/09 06:11:15 marka Exp $ */

#ifndef DNS_DIFF_H
#define DNS_DIFF_H 1

/*****
 ***** Module Info
 *****/

/*
 * A diff is a convenience type representing a list of changes to be
 * made to a database.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/magic.h>

#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/types.h>

/***
 *** Types
 ***/

/*
 * A dns_difftuple_t represents a single RR being added or deleted.
 * The RR type and class are in the 'rdata' member; the class is always
 * the real one, not a DynDNS meta-class, so that the rdatas can be
 * compared using dns_rdata_compare().  The TTL is significant
 * even for deletions, because a deletion/addition pair cannot
 * be canceled out if the TTL differs (it might be an explicit
 * TTL update).
 *
 * Tuples are also used to represent complete RRs with owner
 * names for a couple of other purposes, such as the
 * individual RRs of a "RRset exists (value dependent)"
 * prerequisite set.  In this case, op==DNS_DIFFOP_EXISTS,
 * and the TTL is ignored.
 */

typedef enum {
	DNS_DIFFOP_ADD,	        /* Add an RR. */
	DNS_DIFFOP_DEL,		/* Delete an RR. */
	DNS_DIFFOP_EXISTS	/* Assert RR existence. */
} dns_diffop_t;

typedef struct dns_difftuple dns_difftuple_t;

#define DNS_DIFFTUPLE_MAGIC	ISC_MAGIC('D','I','F','T')
#define DNS_DIFFTUPLE_VALID(t)	ISC_MAGIC_VALID(t, DNS_DIFFTUPLE_MAGIC)

struct dns_difftuple {
        unsigned int			magic;
	isc_mem_t			*mctx;
	dns_diffop_t			op;
	dns_name_t			name;
	dns_ttl_t			ttl;
	dns_rdata_t			rdata;
	ISC_LINK(dns_difftuple_t)	link;
	/* Variable-size name data and rdata follows. */
};

/*
 * A dns_diff_t represents a set of changes being applied to
 * a zone.  Diffs are also used to represent "RRset exists
 * (value dependent)" prerequisites.
 */
typedef struct dns_diff dns_diff_t;

#define DNS_DIFF_MAGIC		ISC_MAGIC('D','I','F','F')
#define DNS_DIFF_VALID(t)	ISC_MAGIC_VALID(t, DNS_DIFF_MAGIC)

struct dns_diff {
	unsigned int			magic;
	isc_mem_t *			mctx;
	ISC_LIST(dns_difftuple_t)	tuples;
};

/* Type of comparision function for sorting diffs. */
typedef int dns_diff_compare_func(const void *, const void *);

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

/**************************************************************************/
/*
 * Maniuplation of diffs and tuples.
 */

isc_result_t
dns_difftuple_create(isc_mem_t *mctx,
		     dns_diffop_t op, dns_name_t *name, dns_ttl_t ttl,
		     dns_rdata_t *rdata, dns_difftuple_t **tp);
/*
 * Create a tuple.  Deep copies are made of the name and rdata, so
 * they need not remain valid after the call.
 *
 * Requires:
 *	*tp != NULL && *tp == NULL.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *      ISC_R_NOMEMORY
 */

void
dns_difftuple_free(dns_difftuple_t **tp);
/*
 * Free a tuple.
 *
 * Requires:
 *       **tp is a valid tuple.
 *
 * Ensures:
 *       *tp == NULL
 *       All memory used by the tuple is freed.
 */

isc_result_t
dns_difftuple_copy(dns_difftuple_t *orig, dns_difftuple_t **copyp);
/*
 * Copy a tuple.
 *
 * Requires:
 * 	'orig' points to a valid tuple
 *	copyp != NULL && *copyp == NULL
 */

void
dns_diff_init(isc_mem_t *mctx, dns_diff_t *diff);
/*
 * Initialize a diff.
 *
 * Requires:
 *    'diff' points to an uninitialized dns_diff_t
 *    allocated by the caller.
 *
 * Ensures:
 *    '*diff' is a valid, empty diff.
 */

void
dns_diff_clear(dns_diff_t *diff);
/*
 * Clear a diff, destroying all its tuples.
 *
 * Requires:
 *    'diff' points to a valid dns_diff_t.
 *
 * Ensures:
 *     Any tuples in the diff are destroyed.
 *     The diff now empty, but it is still valid
 *     and may be reused without calling dns_diff_init
 *     again.  The only memory used is that of the
 *     dns_diff_t structure itself.
 *
 * Notes:
 *     Managing the memory of the dns_diff_t structure itself
 *     is the caller's responsibility.
 */

void
dns_diff_append(dns_diff_t *diff, dns_difftuple_t **tuple);
/*
 * Append a single tuple to a diff.
 *
 *	'diff' is a valid diff.
 * 	'*tuple' is a valid tuple.
 *
 * Ensures:
 *	*tuple is NULL.
 *	The tuple has been freed, or will be freed when the diff is cleared.
 */

void
dns_diff_appendminimal(dns_diff_t *diff, dns_difftuple_t **tuple);
/*
 * Append 'tuple' to 'diff', removing any duplicate
 * or conflicting updates as needed to create a minimal diff.
 *
 * Requires:
 *	'diff' is a minimal diff.
 *
 * Ensures:
 *	'diff' is still a minimal diff.
 *   	*tuple is NULL.
 *   	The tuple has been freed, or will be freed when the diff is cleared.
 *
 */

isc_result_t
dns_diff_sort(dns_diff_t *diff, dns_diff_compare_func *compare);
/*
 * Sort 'diff' in-place according to the comparison function 'compare'.
 */

isc_result_t
dns_diff_apply(dns_diff_t *diff, dns_db_t *db, dns_dbversion_t *ver);
/*
 * Apply 'diff' to the database 'db'.
 *
 * For efficiency, the diff should be sorted by owner name.
 * If it is not sorted, operation will still be correct,
 * but less efficient.
 *
 * Requires:
 *	*diff is a valid diff (possibly empty), containing
 *   	tuples of type DNS_DIFFOP_ADD and/or
 *   	For DNS_DIFFOP_DEL tuples, the TTL is ignored.
 *
 */

isc_result_t
dns_diff_load(dns_diff_t *diff, dns_addrdatasetfunc_t addfunc,
	      void *add_private);
/*
 * Like dns_diff_apply, but for use when loading a new database
 * instead of modifying an existing one.  This bypasses the
 * database transaction mechanisms.
 *
 * Requires:
 * 	'addfunc' is a valid dns_addradatasetfunc_t obtained from
 * 	dns_db_beginload()
 *
 *	'add_private' points to a corresponding dns_dbload_t *
 *      (XXX why is it a void pointer, then?)
 */

isc_result_t
dns_diff_print(dns_diff_t *diff, FILE *file);

/*
 * Print the differences to 'file' or if 'file' is NULL via the
 * logging system.
 *
 * Require:
 *	'diff' to be valid.
 *	'file' to refer to a open file or NULL.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_UNEXPECTED
 *	any error from dns_rdataset_totext()
 */

ISC_LANG_ENDDECLS

#endif /* DNS_DIFF_H */
