/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2003  Internet Software Consortium.
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

/* $Id: rdata.h,v 1.51.2.5 2004/03/09 06:11:20 marka Exp $ */

#ifndef DNS_RDATA_H
#define DNS_RDATA_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Rdata
 *
 * Provides facilities for manipulating DNS rdata, including conversions to
 * and from wire format and text format.
 *
 * Given the large amount of rdata possible in a nameserver, it was important
 * to come up with a very efficient way of storing rdata, but at the same
 * time allow it to be manipulated.
 *
 * The decision was to store rdata in uncompressed wire format,
 * and not to make it a fully abstracted object; i.e. certain parts of the
 * server know rdata is stored that way.  This saves a lot of memory, and
 * makes adding rdata to messages easy.  Having much of the server know
 * the representation would be perilous, and we certainly don't want each
 * user of rdata to be manipulating such a low-level structure.  This is
 * where the rdata module comes in.  The module allows rdata handles to be
 * created and attached to uncompressed wire format regions.  All rdata
 * operations and conversions are done through these handles.
 *
 * Implementation Notes:
 *
 *	The routines in this module are expected to be synthesized by the
 *	build process from a set of source files, one per rdata type.  For
 *	portability, it's probably best that the building be done by a C
 *	program.  Adding a new rdata type will be a simple matter of adding
 *	a file to a directory and rebuilding the server.  *All* knowlege of
 *	the format of a particular rdata type is in this file.
 *
 * MP:
 *	Clients of this module must impose any required synchronization.
 *
 * Reliability:
 *	This module deals with low-level byte streams.  Errors in any of
 *	the functions are likely to crash the server or corrupt memory.
 *
 *	Rdata is typed, and the caller must know what type of rdata it has.
 *	A caller that gets this wrong could crash the server.
 *
 *	The fromstruct() and tostruct() routines use a void * pointer to
 *	represent the structure.  The caller must ensure that it passes a
 *	pointer to the appropriate type, or the server could crash or memory
 *	could be corrupted.
 *
 * Resources:
 *	None.
 *
 * Security:
 *
 *	*** WARNING ***
 *
 *	dns_rdata_fromwire() deals with raw network data.  An error in
 *	this routine could result in the failure or hijacking of the server.
 *
 * Standards:
 *	RFC 1035
 *	Draft EDNS0 (0)
 *	Draft EDNS1 (0)
 *	Draft Binary Labels (2)
 *	Draft Local Compression (1)
 *	<Various RFCs for particular types; these will be documented in the
 *	 sources files of the types.>
 *
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** RData
 *****
 ***** An 'rdata' is a handle to a binary region.  The handle has an RR
 ***** class and type, and the data in the binary region is in the format
 ***** of the given class and type.
 *****/

/***
 *** Types
 ***/

/*
 * Clients are strongly discouraged from using this type directly, with
 * the exception of the 'link' field which may be used directly for whatever
 * purpose the client desires.
 */
struct dns_rdata {
	unsigned char *			data;
	unsigned int			length;
	dns_rdataclass_t		rdclass;
	dns_rdatatype_t			type;
	unsigned int			flags;
	ISC_LINK(dns_rdata_t)		link;
};

#define DNS_RDATA_INIT { NULL, 0, 0, 0, 0, {(void*)(-1), (void *)(-1)}}

#define DNS_RDATA_UPDATE	0x0001		/* update pseudo record */

/*
 * Flags affecting rdata formatting style.  Flags 0xFFFF0000
 * are used by masterfile-level formatting and defined elsewhere.
 * See additional comments at dns_rdata_tofmttext().
 */

/* Split the rdata into multiple lines to try to keep it
 within the "width". */
#define DNS_STYLEFLAG_MULTILINE		0x00000001U

/* Output explanatory comments. */
#define DNS_STYLEFLAG_COMMENT		0x00000002U

/***
 *** Initialization
 ***/

void
dns_rdata_init(dns_rdata_t *rdata);
/*
 * Make 'rdata' empty.
 *
 * Requires:
 *	'rdata' is a valid rdata (i.e. not NULL, points to a struct dns_rdata)
 */

void
dns_rdata_reset(dns_rdata_t *rdata);
/*
 * Make 'rdata' empty.
 *
 * Requires:
 *	'rdata' is a previously initialized rdata and is not linked.
 */

void
dns_rdata_clone(const dns_rdata_t *src, dns_rdata_t *target);
/*
 * Clone 'target' from 'src'.
 *
 * Requires:
 *	'src' to be initialized.
 *	'target' to be initialized.
 */

/***
 *** Comparisons
 ***/

int
dns_rdata_compare(const dns_rdata_t *rdata1, const dns_rdata_t *rdata2);
/*
 * Determine the relative ordering under the DNSSEC order relation of
 * 'rdata1' and 'rdata2'.
 *
 * Requires:
 *
 *	'rdata1' is a valid, non-empty rdata
 *
 *	'rdata2' is a valid, non-empty rdata
 *
 * Returns:
 *	< 0		'rdata1' is less than 'rdata2'
 *	0		'rdata1' is equal to 'rdata2'
 *	> 0		'rdata1' is greater than 'rdata2'
 */

/***
 *** Conversions
 ***/

void
dns_rdata_fromregion(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, isc_region_t *r);
/*
 * Make 'rdata' refer to region 'r'.
 *
 * Requires:
 *
 *	The data in 'r' is properly formatted for whatever type it is.
 */

void
dns_rdata_toregion(const dns_rdata_t *rdata, isc_region_t *r);
/*
 * Make 'r' refer to 'rdata'.
 */

isc_result_t
dns_rdata_fromwire(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_buffer_t *source,
		   dns_decompress_t *dctx,
		   isc_boolean_t downcase,
		   isc_buffer_t *target);
/*
 * Copy the possibly-compressed rdata at source into the target region.
 *
 * Notes:
 *	Name decompression policy is controlled by 'dctx'.
 *
 *	If 'downcase' is true, any uppercase letters in domain names in
 * 	'source' will be downcased when they are copied into 'target'.
 *
 * Requires:
 *
 *	'rdclass' and 'type' are valid.
 *
 *	'source' is a valid buffer, and the active region of 'source'
 *	references the rdata to be processed.
 *
 *	'target' is a valid buffer.
 *
 *	'dctx' is a valid decompression context.
 *
 * Ensures:
 *
 *	If result is success:
 *	 	If 'rdata' is not NULL, it is attached to the target.
 *
 *		The conditions dns_name_fromwire() ensures for names hold
 *		for all names in the rdata.
 *
 *		The current location in source is advanced, and the used space
 *		in target is updated.
 *
 * Result:
 *	Success
 *	<Any non-success status from dns_name_fromwire()>
 *	<Various 'Bad Form' class failures depending on class and type>
 *	Bad Form: Input too short
 *	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_towire(dns_rdata_t *rdata, dns_compress_t *cctx,
		 isc_buffer_t *target);
/*
 * Convert 'rdata' into wire format, compressing it as specified by the
 * compression context 'cctx', and storing the result in 'target'.
 *
 * Notes:
 *	If the compression context allows global compression, then the
 *	global compression table may be updated.
 *
 * Requires:
 *	'rdata' is a valid, non-empty rdata
 *
 *	target is a valid buffer
 *
 *	Any offsets specified in a global compression table are valid
 *	for target.
 *
 * Ensures:
 *	If the result is success:
 *		The used space in target is updated.
 *
 * Returns:
 *	Success
 *	<Any non-success status from dns_name_towire()>
 *	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_fromtext(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		   dns_rdatatype_t type, isc_lex_t *lexer, dns_name_t *origin,
		   isc_boolean_t downcase, isc_mem_t *mctx,
		   isc_buffer_t *target, dns_rdatacallbacks_t *callbacks);
/*
 * Convert the textual representation of a DNS rdata into uncompressed wire
 * form stored in the target region.  Tokens constituting the text of the rdata
 * are taken from 'lexer'.
 *
 * Notes:
 *	Relative domain names in the rdata will have 'origin' appended to them.
 *	A NULL origin implies "origin == dns_rootname".
 *
 *	If 'downcase' is true, any uppercase letters in domain names in
 * 	'source' will be downcased when they are copied into 'target'.
 *
 * Requires:
 *
 *	'rdclass' and 'type' are valid.
 *
 *	'lexer' is a valid isc_lex_t.
 *
 *	'mctx' is a valid isc_mem_t.
 *
 *	'target' is a valid region.
 *
 *	'origin' if non NULL it must be absolute.
 *	
 *	'callbacks' to be NULL or callbacks->warn and callbacks->error be
 *	initialized.
 *
 * Ensures:
 *	If result is success:
 *	 	If 'rdata' is not NULL, it is attached to the target.
 *
 *		The conditions dns_name_fromtext() ensures for names hold
 *		for all names in the rdata.
 *
 *		The used space in target is updated.
 *
 * Result:
 *	Success
 *	<Translated result codes from isc_lex_gettoken>
 *	<Various 'Bad Form' class failures depending on class and type>
 *	Bad Form: Input too short
 *	Resource Limit: Not enough space
 *	Resource Limit: Not enough memory
 */

isc_result_t
dns_rdata_totext(dns_rdata_t *rdata, dns_name_t *origin, isc_buffer_t *target);
/*
 * Convert 'rdata' into text format, storing the result in 'target'.
 * The text will consist of a single line, with fields separated by
 * single spaces.
 *
 * Notes:
 *	If 'origin' is not NULL, then any names in the rdata that are
 *	subdomains of 'origin' will be made relative it.
 *
 *	XXX Do we *really* want to support 'origin'?  I'm inclined towards "no"
 *	at the moment.
 *
 * Requires:
 *
 *	'rdata' is a valid, non-empty rdata
 *
 *	'origin' is NULL, or is a valid name
 *
 *	'target' is a valid text buffer
 *
 * Ensures:
 *	If the result is success:
 *
 *		The used space in target is updated.
 *
 * Returns:
 *	Success
 *	<Any non-success status from dns_name_totext()>
 *	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_tofmttext(dns_rdata_t *rdata, dns_name_t *origin, unsigned int flags,
		    unsigned int width, char *linebreak, isc_buffer_t *target);
/*
 * Like dns_rdata_totext, but do formatted output suitable for
 * database dumps.  This is intended for use by dns_db_dump();
 * library users are discouraged from calling it directly.
 *
 * If (flags & DNS_STYLEFLAG_MULTILINE) != 0, attempt to stay
 * within 'width' by breaking the text into multiple lines.
 * The string 'linebreak' is inserted between lines, and parentheses
 * are added when necessary.  Because RRs contain unbreakable elements
 * such as domain names whose length is variable, unpredictable, and
 * potentially large, there is no guarantee that the lines will
 * not exceed 'width' anyway.
 *
 * If (flags & DNS_STYLEFLAG_MULTILINE) == 0, the rdata is always
 * printed as a single line, and no parentheses are used.
 * The 'width' and 'linebreak' arguments are ignored.
 *
 * If (flags & DNS_STYLEFLAG_COMMENT) != 0, output explanatory
 * comments next to things like the SOA timer fields.  Some
 * comments (e.g., the SOA ones) are only printed when multiline
 * output is selected.
 */

isc_result_t
dns_rdata_fromstruct(dns_rdata_t *rdata, dns_rdataclass_t rdclass,
		     dns_rdatatype_t type, void *source, isc_buffer_t *target);
/*
 * Convert the C structure representation of an rdata into uncompressed wire
 * format in 'target'.
 *
 * XXX  Should we have a 'size' parameter as a sanity check on target?
 *
 * Requires:
 *
 *	'rdclass' and 'type' are valid.
 *
 *	'source' points to a valid C struct for the class and type.
 *
 *	'target' is a valid buffer.
 *
 *	All structure pointers to memory blocks should be NULL if their
 *	corresponding length values are zero.
 *
 * Ensures:
 *	If result is success:
 *	 	If 'rdata' is not NULL, it is attached to the target.
 *
 *		The used space in 'target' is updated.
 *
 * Result:
 *	Success
 *	<Various 'Bad Form' class failures depending on class and type>
 *	Resource Limit: Not enough space
 */

isc_result_t
dns_rdata_tostruct(dns_rdata_t *rdata, void *target, isc_mem_t *mctx);
/*
 * Convert an rdata into its C structure representation.
 *
 * If 'mctx' is NULL then 'rdata' must persist while 'target' is being used.
 *
 * If 'mctx' is non NULL then memory will be allocated if required.
 *
 * Requires:
 *
 *	'rdata' is a valid, non-empty rdata.
 *
 *	'target' to point to a valid pointer for the type and class.
 *
 * Result:
 *	Success
 *	Resource Limit: Not enough memory
 */

void
dns_rdata_freestruct(void *source);
/*
 * Free dynamic memory attached to 'source' (if any).
 *
 * Requires:
 *
 *	'source' to point to the structure previously filled in by
 *	dns_rdata_tostruct().
 */

isc_boolean_t
dns_rdatatype_ismeta(dns_rdatatype_t type);
/*
 * Return true iff the rdata type 'type' is a meta-type
 * like ANY or AXFR.
 */

isc_boolean_t
dns_rdatatype_issingleton(dns_rdatatype_t type);
/*
 * Return true iff the rdata type 'type' is a singleton type,
 * like CNAME or SOA.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 *
 */

isc_boolean_t
dns_rdataclass_ismeta(dns_rdataclass_t rdclass);
/*
 * Return true iff the rdata class 'rdclass' is a meta-class
 * like ANY or NONE.
 */

isc_boolean_t
dns_rdatatype_isdnssec(dns_rdatatype_t type);
/*
 * Return true iff 'type' is one of the DNSSEC
 * rdata types that may exist alongside a CNAME record.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 */

isc_boolean_t
dns_rdatatype_iszonecutauth(dns_rdatatype_t type);
/*
 * Return true iff rdata of type 'type' is considered authoritative
 * data (not glue) in the NXT chain when it occurs in the parent zone
 * at a zone cut.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 *
 */

isc_boolean_t
dns_rdatatype_isknown(dns_rdatatype_t type);
/*
 * Return true iff the rdata type 'type' is known.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 *
 */


isc_result_t
dns_rdata_additionaldata(dns_rdata_t *rdata, dns_additionaldatafunc_t add,
			 void *arg);
/*
 * Call 'add' for each name and type from 'rdata' which is subject to
 * additional section processing.
 *
 * Requires:
 *
 *	'rdata' is a valid, non-empty rdata.
 *
 *	'add' is a valid dns_additionalfunc_t.
 *
 * Ensures:
 *
 *	If successful, then add() will have been called for each name
 *	and type subject to additional section processing.
 *
 *	If add() returns something other than ISC_R_SUCCESS, that result
 *	will be returned as the result of dns_rdata_additionaldata().
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *
 *	Many other results are possible if not successful.
 */

isc_result_t
dns_rdata_digest(dns_rdata_t *rdata, dns_digestfunc_t digest, void *arg);
/*
 * Send 'rdata' in DNSSEC canonical form to 'digest'.
 *
 * Note:
 *	'digest' may be called more than once by dns_rdata_digest().  The
 *	concatenation of all the regions, in the order they were given
 *	to 'digest', will be the DNSSEC canonical form of 'rdata'.
 *
 * Requires:
 *
 *	'rdata' is a valid, non-empty rdata.
 *
 *	'digest' is a valid dns_digestfunc_t.
 *
 * Ensures:
 *
 *	If successful, then all of the rdata's data has been sent, in
 *	DNSSEC canonical form, to 'digest'.
 *
 *	If digest() returns something other than ISC_R_SUCCESS, that result
 *	will be returned as the result of dns_rdata_digest().
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *
 *	Many other results are possible if not successful.
 */

isc_boolean_t
dns_rdatatype_questiononly(dns_rdatatype_t type);
/*
 * Return true iff rdata of type 'type' can only appear in the question
 * section of a properly formatted message.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 *
 */

isc_boolean_t
dns_rdatatype_notquestion(dns_rdatatype_t type);
/*
 * Return true iff rdata of type 'type' can not appear in the question
 * section of a properly formatted message.
 *
 * Requires:
 * 	'type' is a valid rdata type.
 *
 */

unsigned int
dns_rdatatype_attributes(dns_rdatatype_t rdtype);
/*
 * Return attributes for the given type.
 *
 * Requires:
 *	'rdtype' are known.
 *
 * Returns:
 *	a bitmask consisting of the following flags.
 */

/* only one may exist for a name */
#define DNS_RDATATYPEATTR_SINGLETON		0x00000001U
/* requires no other data be present */
#define DNS_RDATATYPEATTR_EXCLUSIVE		0x00000002U
/* Is a meta type */
#define DNS_RDATATYPEATTR_META			0x00000004U
/* Is a DNSSEC type, like SIG or NXT */
#define DNS_RDATATYPEATTR_DNSSEC		0x00000008U
/* Is a zone cut authority type */
#define DNS_RDATATYPEATTR_ZONECUTAUTH		0x00000010U
/* Is reserved (unusable) */
#define DNS_RDATATYPEATTR_RESERVED		0x00000020U
/* Is an unknown type */
#define DNS_RDATATYPEATTR_UNKNOWN		0x00000040U
/* Is META, and can only be in a question section */
#define DNS_RDATATYPEATTR_QUESTIONONLY		0x00000080U
/* is META, and can NOT be in a question section */
#define DNS_RDATATYPEATTR_NOTQUESTION		0x00000100U

dns_rdatatype_t
dns_rdata_covers(dns_rdata_t *rdata);
/*
 * Return the rdatatype that this type covers.
 *
 * Requires:
 *	'rdata' is a valid, non-empty rdata.
 *
 *	'rdata' is a type that covers other rdata types.
 *
 * Returns:
 *	The type covered.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATA_H */
