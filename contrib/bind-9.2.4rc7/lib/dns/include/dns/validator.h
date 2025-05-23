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

/* $Id: validator.h,v 1.18.2.1 2004/03/09 06:11:24 marka Exp $ */

#ifndef DNS_VALIDATOR_H
#define DNS_VALIDATOR_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Validator
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
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	RFCs:	1034, 1035, 2181, 2535, <TBS>
 *	Drafts:	<TBS>
 */

#include <isc/lang.h>
#include <isc/event.h>
#include <isc/mutex.h>

#include <dns/types.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h> /* for dns_rdata_sig_t */

#include <dst/dst.h>

/*
 * A dns_validatorevent_t is sent when a 'validation' completes.
 *
 * 'name', 'rdataset', 'sigrdataset', and 'message' are the values that were
 * supplied when dns_validator_create() was called.  They are returned to the
 * caller so that they may be freed.
 */
typedef struct dns_validatorevent {
	ISC_EVENT_COMMON(struct dns_validatorevent);
	dns_validator_t *		validator;
	isc_result_t			result;
	dns_name_t *			name;
	dns_rdatatype_t			type;
	dns_rdataset_t *		rdataset;
	dns_rdataset_t *		sigrdataset;
	dns_message_t *			message;
} dns_validatorevent_t;


/*
 * A validator object represents a validation in procgress.
 *
 * Clients are strongly discouraged from using this type directly, with
 * the exception of the 'link' field, which may be used directly for
 * whatever purpose the client desires.
 */
struct dns_validator {
	/* Unlocked. */
	unsigned int			magic;
	isc_mutex_t			lock;
	dns_view_t *			view;
	/* Locked by lock. */
	unsigned int			options;
	unsigned int			attributes;
	dns_validatorevent_t *		event;
	dns_fetch_t *			fetch;
	dns_validator_t *		keyvalidator;
	dns_validator_t *		authvalidator;
	dns_keytable_t *		keytable;
	dns_keynode_t *			keynode;
	dst_key_t *			key;
	dns_rdata_sig_t *		siginfo;
	isc_task_t *			task;
	isc_taskaction_t		action;
	void *				arg;
	unsigned int			labels;
	dns_rdataset_t *		currentset;
	isc_boolean_t			seensig;
	dns_rdataset_t *		keyset;
	dns_rdataset_t			frdataset;
	dns_rdataset_t			fsigrdataset;
	ISC_LINK(dns_validator_t)	link;
};

ISC_LANG_BEGINDECLS

isc_result_t
dns_validator_create(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset,
		     dns_message_t *message, unsigned int options,
		     isc_task_t *task, isc_taskaction_t action, void *arg,
		     dns_validator_t **validatorp);
/*
 * Start a DNSSEC validation.
 *
 * This validates a response to the question given by
 * 'name' and 'type'.
 *
 * To validate a positive response, the response data is
 * given by 'rdataset' and 'sigrdataset'.  If 'sigrdataset'
 * is NULL, the data is presumed insecure and an attempt
 * is made to prove its insecurity by finding the appropriate
 * null key.
 *
 * The complete response message may be given in 'message',
 * to make available any authority section NXTs that may be
 * needed for validation of a response resulting from a
 * wildcard expansion (though no such wildcard validation
 * is implemented yet).  If the complete response message
 * is not available, 'message' is NULL.
 *
 * To validate a negative response, the complete negative response
 * message is given in 'message'.  The 'rdataset', and
 * 'sigrdataset' arguments must be NULL, but the 'name' and 'type'
 * arguments must be provided.
 *
 * The validation is performed in the context of 'view'.
 * 'options' must be zero.
 *
 * When the validation finishes, a dns_validatorevent_t with
 * the given 'action' and 'arg' are sent to 'task'.
 * Its 'result' field will be ISC_R_SUCCESS iff the
 * response was successfully proven to be either secure or
 * part of a known insecure domain.
 */

void
dns_validator_cancel(dns_validator_t *validator);
/*
 * Cancel a DNSSEC validation in progress.
 *
 * Requires:
 *	'validator' points to a valid DNSSEC validator, which
 *	may or may not already have completed.
 *
 * Ensures:
 *	It the validator has not already sent its completion
 *	event, it will send it with result code ISC_R_CANCELED.
 */

void
dns_validator_destroy(dns_validator_t **validatorp);
/*
 * Destroy a DNSSEC validator.
 *
 * Requires:
 *	'*validatorp' points to a valid DNSSEC validator.
 * 	The validator must have completed and sent its completion
 * 	event.
 *
 * Ensures:
 *	All resources used by the validator are freed.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_VALIDATOR_H */
