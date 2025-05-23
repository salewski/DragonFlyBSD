/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
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

/* $Id: db.h,v 1.67.2.4 2004/03/09 06:11:14 marka Exp $ */

#ifndef DNS_DB_H
#define DNS_DB_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS DB
 *
 * The DNS DB interface allows named rdatasets to be stored and retrieved.
 *
 * The dns_db_t type is like a "virtual class".  To actually use
 * DBs, an implementation of the class is required.
 *
 * XXX <more> XXX
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
 *	None.
 */

/*****
 ***** Imports
 *****/

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/ondestroy.h>
#include <isc/stdtime.h>

#include <dns/name.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types
 *****/

typedef struct dns_dbmethods {
	void		(*attach)(dns_db_t *source, dns_db_t **targetp);
	void		(*detach)(dns_db_t **dbp);
	isc_result_t	(*beginload)(dns_db_t *db, dns_addrdatasetfunc_t *addp,
				     dns_dbload_t **dbloadp);
	isc_result_t	(*endload)(dns_db_t *db, dns_dbload_t **dbloadp);
	isc_result_t	(*dump)(dns_db_t *db, dns_dbversion_t *version,
				const char *filename);
	void		(*currentversion)(dns_db_t *db,
					  dns_dbversion_t **versionp);
	isc_result_t	(*newversion)(dns_db_t *db,
				      dns_dbversion_t **versionp);
	void		(*attachversion)(dns_db_t *db, dns_dbversion_t *source,
					 dns_dbversion_t **targetp);
	void		(*closeversion)(dns_db_t *db,
					dns_dbversion_t **versionp,
					isc_boolean_t commit);
	isc_result_t	(*findnode)(dns_db_t *db, dns_name_t *name,
				    isc_boolean_t create,
				    dns_dbnode_t **nodep);
	isc_result_t	(*find)(dns_db_t *db, dns_name_t *name,
				dns_dbversion_t *version,
				dns_rdatatype_t type, unsigned int options,
				isc_stdtime_t now,
				dns_dbnode_t **nodep, dns_name_t *foundname,
				dns_rdataset_t *rdataset,
				dns_rdataset_t *sigrdataset);
	isc_result_t	(*findzonecut)(dns_db_t *db, dns_name_t *name,
				       unsigned int options, isc_stdtime_t now,
				       dns_dbnode_t **nodep,
				       dns_name_t *foundname,
				       dns_rdataset_t *rdataset,
				       dns_rdataset_t *sigrdataset);
	void		(*attachnode)(dns_db_t *db,
				      dns_dbnode_t *source,
				      dns_dbnode_t **targetp);
	void		(*detachnode)(dns_db_t *db,
				      dns_dbnode_t **targetp);
	isc_result_t	(*expirenode)(dns_db_t *db, dns_dbnode_t *node,
				      isc_stdtime_t now);
	void		(*printnode)(dns_db_t *db, dns_dbnode_t *node,
				     FILE *out);
	isc_result_t 	(*createiterator)(dns_db_t *db,
					  isc_boolean_t relative_names,
					  dns_dbiterator_t **iteratorp);
	isc_result_t	(*findrdataset)(dns_db_t *db, dns_dbnode_t *node,
					dns_dbversion_t *version,
					dns_rdatatype_t type,
					dns_rdatatype_t covers,
					isc_stdtime_t now,
					dns_rdataset_t *rdataset,
					dns_rdataset_t *sigrdataset);
	isc_result_t	(*allrdatasets)(dns_db_t *db, dns_dbnode_t *node,
					dns_dbversion_t *version,
					isc_stdtime_t now,
					dns_rdatasetiter_t **iteratorp);
	isc_result_t	(*addrdataset)(dns_db_t *db, dns_dbnode_t *node,
				       dns_dbversion_t *version,
				       isc_stdtime_t now,
				       dns_rdataset_t *rdataset,
				       unsigned int options,
				       dns_rdataset_t *addedrdataset);
	isc_result_t	(*subtractrdataset)(dns_db_t *db, dns_dbnode_t *node,
					    dns_dbversion_t *version,
					    dns_rdataset_t *rdataset,
					    unsigned int options,
					    dns_rdataset_t *newrdataset);
	isc_result_t	(*deleterdataset)(dns_db_t *db, dns_dbnode_t *node,
					  dns_dbversion_t *version,
					  dns_rdatatype_t type,
					  dns_rdatatype_t covers);
	isc_boolean_t	(*issecure)(dns_db_t *db);
	unsigned int	(*nodecount)(dns_db_t *db);
	isc_boolean_t	(*ispersistent)(dns_db_t *db);
	void		(*overmem)(dns_db_t *db, isc_boolean_t overmem);
	void		(*settask)(dns_db_t *db, isc_task_t *);
} dns_dbmethods_t;

typedef isc_result_t
(*dns_dbcreatefunc_t)(isc_mem_t *mctx, dns_name_t *name,
		      dns_dbtype_t type, dns_rdataclass_t rdclass,
		      unsigned int argc, char *argv[], void *driverarg,
		      dns_db_t **dbp);
					
#define DNS_DB_MAGIC		ISC_MAGIC('D','N','S','D')
#define DNS_DB_VALID(db)	ISC_MAGIC_VALID(db, DNS_DB_MAGIC)

/*
 * This structure is actually just the common prefix of a DNS db
 * implementation's version of a dns_db_t.
 *
 * Direct use of this structure by clients is forbidden.  DB implementations
 * may change the structure.  'magic' must be DNS_DB_MAGIC for any of the
 * dns_db_ routines to work.  DB implementations must maintain all DB
 * invariants.
 */
struct dns_db {
	unsigned int			magic;
	unsigned int			impmagic;
	dns_dbmethods_t *		methods;
	isc_uint16_t			attributes;
	dns_rdataclass_t		rdclass;
	dns_name_t			origin;
	isc_ondestroy_t			ondest;
	isc_mem_t *			mctx;
};

#define DNS_DBATTR_CACHE		0x01
#define DNS_DBATTR_STUB			0x02

/*
 * Options that can be specified for dns_db_find().
 */
#define DNS_DBFIND_GLUEOK		0x01
#define DNS_DBFIND_VALIDATEGLUE		0x02
#define DNS_DBFIND_NOWILD		0x04
#define DNS_DBFIND_PENDINGOK		0x08
#define DNS_DBFIND_NOEXACT		0x10

/*
 * Options that can be specified for dns_db_addrdataset().
 */
#define DNS_DBADD_MERGE			0x01
#define DNS_DBADD_FORCE			0x02
#define DNS_DBADD_EXACT			0x04
#define DNS_DBADD_EXACTTTL		0x08

/*
 * Options that can be specified for dns_db_subtractrdataset().
 */
#define DNS_DBSUB_EXACT			0x01

/*****
 ***** Methods
 *****/

/***
 *** Basic DB Methods
 ***/

isc_result_t
dns_db_create(isc_mem_t *mctx, const char *db_type, dns_name_t *origin,
	      dns_dbtype_t type, dns_rdataclass_t rdclass,
	      unsigned int argc, char *argv[], dns_db_t **dbp);
/*
 * Create a new database using implementation 'db_type'.
 *
 * Notes:
 *	All names in the database must be subdomains of 'origin' and in class
 *	'rdclass'.  The database makes its own copy of the origin, so the
 *	caller may do whatever they like with 'origin' and its storage once the
 *	call returns.
 *
 *	DB implementation-specific parameters are passed using argc and argv.
 *
 * Requires:
 *
 *	dbp != NULL and *dbp == NULL
 *
 *	'origin' is a valid absolute domain name.
 *
 *	mctx is a valid memory context
 *
 * Ensures:
 *
 *	A copy of 'origin' has been made for the databases use, and the
 *	caller is free to do whatever they want with the name and storage
 *	associated with 'origin'.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *	ISC_R_NOTFOUND				db_type not found
 *
 *	Many other errors are possible, depending on what db_type was
 *	specified.
 */

void
dns_db_attach(dns_db_t *source, dns_db_t **targetp);
/*
 * Attach *targetp to source.
 *
 * Requires:
 *
 *	'source' is a valid database.
 *
 *	'targetp' points to a NULL dns_db_t *.
 *
 * Ensures:
 *
 *	*targetp is attached to source.
 */

void
dns_db_detach(dns_db_t **dbp);
/*
 * Detach *dbp from its database.
 *
 * Requires:
 *
 *	'dbp' points to a valid database.
 *
 * Ensures:
 *
 *	*dbp is NULL.
 *
 *	If '*dbp' is the last reference to the database,
 *
 *		All resources used by the database will be freed
 */

isc_result_t
dns_db_ondestroy(dns_db_t *db, isc_task_t *task, isc_event_t **eventp);
/*
 * Causes 'eventp' to be sent to be sent to 'task' when the database is
 * destroyed.
 *
 * Note; ownrship of the eventp is taken from the caller (and *eventp is
 * set to NULL). The sender field of the event is set to 'db' before it is
 * sent to the task.
 */

isc_boolean_t
dns_db_iscache(dns_db_t *db);
/*
 * Does 'db' have cache semantics?
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *	ISC_TRUE	'db' has cache semantics
 *	ISC_FALSE	otherwise
 */

isc_boolean_t
dns_db_iszone(dns_db_t *db);
/*
 * Does 'db' have zone semantics?
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *	ISC_TRUE	'db' has zone semantics
 *	ISC_FALSE	otherwise
 */

isc_boolean_t
dns_db_isstub(dns_db_t *db);
/*
 * Does 'db' have stub semantics?
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *	ISC_TRUE	'db' has zone semantics
 *	ISC_FALSE	otherwise
 */

isc_boolean_t
dns_db_issecure(dns_db_t *db);
/*
 * Is 'db' secure?
 *
 * Requires:
 *
 *	'db' is a valid database with zone semantics.
 *
 * Returns:
 *	ISC_TRUE	'db' is secure.
 *	ISC_FALSE	'db' is not secure.
 */

dns_name_t *
dns_db_origin(dns_db_t *db);
/*
 * The origin of the database.
 *
 * Note: caller must not try to change this name.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *
 *	The origin of the database.
 */

dns_rdataclass_t
dns_db_class(dns_db_t *db);
/*
 * The class of the database.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *
 *	The class of the database.
 */

isc_result_t
dns_db_beginload(dns_db_t *db, dns_addrdatasetfunc_t *addp,
		 dns_dbload_t **dbloadp);
/*
 * Begin loading 'db'.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	This is the first attempt to load 'db'.
 *
 *	addp != NULL && *addp == NULL
 *
 *	dbloadp != NULL && *dbloadp == NULL
 *
 * Ensures:
 *
 *	On success, *addp will be a valid dns_addrdatasetfunc_t suitable
 *	for loading 'db'.  *dbloadp will be a valid DB load context which
 *	should be used as 'arg' when *addp is called.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Other results are possible, depending upon the database
 *	implementation used, syntax errors in the master file, etc.
 */

isc_result_t
dns_db_endload(dns_db_t *db, dns_dbload_t **dbloadp);
/*
 * Finish loading 'db'.
 *
 * Requires:
 *
 *	'db' is a valid database that is being loaded.
 *
 *	dbloadp != NULL and *dbloadp is a valid database load context.
 *
 * Ensures:
 *
 *	*dbloadp == NULL
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Other results are possible, depending upon the database
 *	implementation used, syntax errors in the master file, etc.
 */

isc_result_t
dns_db_load(dns_db_t *db, const char *filename);
/*
 * Load master file 'filename' into 'db'.
 *
 * Notes:
 *	This routine is equivalent to calling
 *
 *		dns_db_beginload();
 *		dns_master_loadfile();
 *		dns_db_endload();
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	This is the first attempt to load 'db'.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Other results are possible, depending upon the database
 *	implementation used, syntax errors in the master file, etc.
 */

isc_result_t
dns_db_dump(dns_db_t *db, dns_dbversion_t *version, const char *filename);
/*
 * Dump version 'version' of 'db' to master file 'filename'.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'version' is a valid version.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Other results are possible, depending upon the database
 *	implementation used, OS file errors, etc.
 */

/***
 *** Version Methods
 ***/

void
dns_db_currentversion(dns_db_t *db, dns_dbversion_t **versionp);
/*
 * Open the current version for reading.
 *
 * Requires:
 *
 *	'db' is a valid database with zone semantics.
 *
 *	versionp != NULL && *verisonp == NULL
 *
 * Ensures:
 *
 *	On success, '*versionp' is attached to the current version.
 *
 */

isc_result_t
dns_db_newversion(dns_db_t *db, dns_dbversion_t **versionp);
/*
 * Open a new version for reading and writing.
 *
 * Requires:
 *
 *	'db' is a valid database with zone semantics.
 *
 *	versionp != NULL && *verisonp == NULL
 *
 * Ensures:
 *
 *	On success, '*versionp' is attached to the current version.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

void
dns_db_attachversion(dns_db_t *db, dns_dbversion_t *source,
		     dns_dbversion_t **targetp);
/*
 * Attach '*targetp' to 'source'.
 *
 * Requires:
 *
 *	'db' is a valid database with zone semantics.
 *
 *	source is a valid open version
 *
 *	targetp != NULL && *targetp == NULL
 *
 * Ensures:
 *
 *	'*targetp' is attached to source.
 */

void
dns_db_closeversion(dns_db_t *db, dns_dbversion_t **versionp,
		    isc_boolean_t commit);
/*
 * Close version '*versionp'.
 *
 * Note: if '*versionp' is a read-write version and 'commit' is ISC_TRUE,
 * then all changes made in the version will take effect, otherwise they
 * will be rolled back.  The value if 'commit' is ignored for read-only
 * versions.
 *
 * Requires:
 *
 *	'db' is a valid database with zone semantics.
 *
 *	'*versionp' refers to a valid version.
 *
 *	If committing a writable version, then there must be no other
 *	outstanding references to the version (e.g. an active rdataset
 *	iterator).
 *
 * Ensures:
 *
 *	*versionp == NULL
 *
 *	If *versionp is a read-write version, and commit is ISC_TRUE, then
 *	the version will become the current version.  If !commit, then all
 *	changes made in the version will be undone, and the version will
 *	not become the current version.
 */

/***
 *** Node Methods
 ***/

isc_result_t
dns_db_findnode(dns_db_t *db, dns_name_t *name, isc_boolean_t create,
		dns_dbnode_t **nodep);
/*
 * Find the node with name 'name'.
 *
 * Notes:
 *	If 'create' is ISC_TRUE and no node with name 'name' exists, then
 *	such a node will be created.
 *
 *	This routine is for finding or creating a node with the specified
 *	name.  There are no partial matches.  It is not suitable for use
 *	in building responses to ordinary DNS queries; clients which wish
 *	to do that should use dns_db_find() instead.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'name' is a valid, non-empty, absolute name.
 *
 *	nodep != NULL && *nodep == NULL
 *
 * Ensures:
 *
 *	On success, *nodep is attached to the node with name 'name'.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOTFOUND			If !create and name not found.
 *	ISC_R_NOMEMORY		        Can only happen if create is ISC_TRUE.
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_find(dns_db_t *db, dns_name_t *name, dns_dbversion_t *version,
	    dns_rdatatype_t type, unsigned int options, isc_stdtime_t now,
	    dns_dbnode_t **nodep, dns_name_t *foundname,
	    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
/*
 * Find the best match for 'name' and 'type' in version 'version' of 'db'.
 *
 * Notes:
 *
 *	If type == dns_rdataset_any, then rdataset will not be bound.
 *
 *	If 'options' does not have DNS_DBFIND_GLUEOK set, then no glue will
 *	be returned.  For zone databases, glue is as defined in RFC 2181.
 *	For cache databases, glue is any rdataset with a trust of
 *	dns_trust_glue.
 *
 *	If 'options' does not have DNS_DBFIND_PENDINGOK set, then no
 *	pending data will be returned.  This option is only meaningful for
 *	cache databases.
 *
 *	If the DNS_DBFIND_NOWILD option is set, then wildcard matching will
 *	be disabled.  This option is only meaningful for zone databases.
 *
 *	To respond to a query for SIG records, the caller should create a
 *	rdataset iterator and extract the signatures from each rdataset.
 *
 *	Making queries of type ANY with DNS_DBFIND_GLUEOK is not recommended,
 *	because the burden of determining whether a given rdataset is valid
 *	glue or not falls upon the caller.
 *
 *	The 'now' field is ignored if 'db' is a zone database.  If 'db' is a
 *	cache database, an rdataset will not be found unless it expires after
 *	'now'.  Any ANY query will not match unless at least one rdataset at
 *	the node expires after 'now'.  If 'now' is zero, then the current time
 *	will be used.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'type' is not SIG, or a meta-RR type other than 'ANY' (e.g. 'OPT').
 *
 *	'nodep' is NULL, or nodep is a valid pointer and *nodep == NULL.
 *
 *	'foundname' is a valid name with a dedicated buffer.
 *
 *	'rdataset' is NULL, or is a valid unassociated rdataset.
 *
 * Ensures:
 *	On a non-error completion:
 *
 *		If nodep != NULL, then it is bound to the found node.
 *
 *		If foundname != NULL, then it contains the full name of the
 *		found node.
 *
 *		If rdataset != NULL and type != dns_rdatatype_any, then
 *		rdataset is bound to the found rdataset.
 *
 * Returns:
 *
 *	Non-error results are:
 *
 *		ISC_R_SUCCESS			The desired node and type were
 *						found.
 *
 *		DNS_R_GLUE			The desired node and type were
 *						found, but are glue.  This
 *						result can only occur if
 *						the DNS_DBFIND_GLUEOK option
 *						is set.  This result can only
 *						occur if 'db' is a zone
 *						database.  If type ==
 *						dns_rdatatype_any, then the
 *						node returned may contain, or
 *						consist entirely of invalid
 *						glue (i.e. data occluded by a
 *						zone cut).  The caller must
 *						take care not to return invalid
 *						glue to a client.
 *
 *		DNS_R_DELEGATION		The data requested is beneath
 *						a zone cut.  node, foundname,
 *						and rdataset reference the
 *						NS RRset of the zone cut.
 *						If 'db' is a cache database,
 *						then this is the deepest known
 *						delegation.
 *
 *		DNS_R_ZONECUT			type == dns_rdatatype_any, and
 *						the desired node is a zonecut.
 *						The caller must take care not
 *						to return inappropriate glue
 *						to a client.  This result can
 *						only occur if 'db' is a zone
 *						database and DNS_DBFIND_GLUEOK
 *						is set.
 *
 *		DNS_R_DNAME			The data requested is beneath
 *						a DNAME.  node, foundname,
 *						and rdataset reference the
 *						DNAME RRset.
 *
 *		DNS_R_CNAME			The rdataset requested was not
 *						found, but there is a CNAME
 *						at the desired name.  node,
 *						foundname, and rdataset
 *						reference the CNAME RRset.
 *
 *		DNS_R_NXDOMAIN			The desired name does not
 *						exist.
 *
 *		DNS_R_NXRRSET			The desired name exists, but
 *						the desired type does not.
 *
 *		ISC_R_NOTFOUND			The desired name does not
 *						exist, and no delegation could
 *						be found.  This result can only
 *						occur if 'db' is a cache
 *						database.  The caller should
 *						use its nameserver(s) of last
 *						resort (e.g. root hints).
 *
 *		DNS_R_NCACHENXDOMAIN		The desired name does not
 *						exist.  'node' is bound to the
 *						cache node with the desired
 *						name, and 'rdataset' contains
 *						the negative caching proof.
 *
 *		DNS_R_NCACHENXRRSET		The desired type does not
 *						exist.  'node' is bound to the
 *						cache node with the desired
 *						name, and 'rdataset' contains
 *						the negative caching proof.
 *
 *	Error results:
 *
 *		ISC_R_NOMEMORY
 *
 *		DNS_R_BADDB			Data that is required to be
 *						present in the DB, e.g. an NXT
 *						record in a secure zone, is not
 *						present.
 *
 *		Other results are possible, and should all be treated as
 *		errors.
 */

isc_result_t
dns_db_findzonecut(dns_db_t *db, dns_name_t *name,
		   unsigned int options, isc_stdtime_t now,
		   dns_dbnode_t **nodep, dns_name_t *foundname,
		   dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
/*
 * Find the deepest known zonecut which encloses 'name' in 'db'.
 *
 * Notes:
 *
 *	If the DNS_DBFIND_NOEXACT option is set, then the zonecut returned
 *	(if any) will be the deepest known ancestor of 'name'.
 *
 *	If 'now' is zero, then the current time will be used.
 *
 * Requires:
 *
 *	'db' is a valid database with cache semantics.
 *
 *	'nodep' is NULL, or nodep is a valid pointer and *nodep == NULL.
 *
 *	'foundname' is a valid name with a dedicated buffer.
 *
 *	'rdataset' is NULL, or is a valid unassociated rdataset.
 *
 * Ensures:
 *	On a non-error completion:
 *
 *		If nodep != NULL, then it is bound to the found node.
 *
 *		If foundname != NULL, then it contains the full name of the
 *		found node.
 *
 *		If rdataset != NULL and type != dns_rdatatype_any, then
 *		rdataset is bound to the found rdataset.
 *
 * Returns:
 *
 *	Non-error results are:
 *
 *		ISC_R_SUCCESS
 *
 *		ISC_R_NOTFOUND
 *
 *		Other results are possible, and should all be treated as
 *		errors.
 */

void
dns_db_attachnode(dns_db_t *db, dns_dbnode_t *source, dns_dbnode_t **targetp);
/*
 * Attach *targetp to source.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'source' is a valid node.
 *
 *	'targetp' points to a NULL dns_node_t *.
 *
 * Ensures:
 *
 *	*targetp is attached to source.
 */

void
dns_db_detachnode(dns_db_t *db, dns_dbnode_t **nodep);
/*
 * Detach *nodep from its node.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'nodep' points to a valid node.
 *
 * Ensures:
 *
 *	*nodep is NULL.
 */

isc_result_t
dns_db_expirenode(dns_db_t *db, dns_dbnode_t *node, isc_stdtime_t now);
/*
 * Mark as stale all records at 'node' which expire at or before 'now'.
 *
 * Note: if 'now' is zero, then the current time will be used.
 *
 * Requires:
 *
 *	'db' is a valid cache database.
 *
 *	'node' is a valid node.
 */

void
dns_db_printnode(dns_db_t *db, dns_dbnode_t *node, FILE *out);
/*
 * Print a textual representation of the contents of the node to
 * 'out'.
 *
 * Note: this function is intended for debugging, not general use.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 */

/***
 *** DB Iterator Creation
 ***/

isc_result_t
dns_db_createiterator(dns_db_t *db, isc_boolean_t relative_names,
		      dns_dbiterator_t **iteratorp);
/*
 * Create an iterator for version 'version' of 'db'.
 *
 * Notes:
 *
 *	If 'relative_names' is ISC_TRUE, then node names returned by the
 *	iterator will be relative to the iterator's current origin.  If
 *	ISC_FALSE, then the node names will be absolute.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	iteratorp != NULL && *iteratorp == NULL
 *
 * Ensures:
 *
 *	On success, *iteratorp will be a valid database iterator.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 */

/***
 *** Rdataset Methods
 ***/

/*
 * XXXRTH  Should we check for glue and pending data in dns_db_findrdataset()?
 */

isc_result_t
dns_db_findrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		    dns_rdatatype_t type, dns_rdatatype_t covers,
		    isc_stdtime_t now, dns_rdataset_t *rdataset,
		    dns_rdataset_t *sigrdataset);
/*
 * Search for an rdataset of type 'type' at 'node' that are in version
 * 'version' of 'db'.  If found, make 'rdataset' refer to it.
 *
 * Notes:
 *
 *	If 'version' is NULL, then the current version will be used.
 *
 *	Care must be used when using this routine to build a DNS response:
 *	'node' should have been found with dns_db_find(), not
 *	dns_db_findnode().  No glue checking is done.  No checking for
 *	pending data is done.
 *
 *	The 'now' field is ignored if 'db' is a zone database.  If 'db' is a
 *	cache database, an rdataset will not be found unless it expires after
 *	'now'.  If 'now' is zero, then the current time will be used.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 *
 *	'rdataset' is a valid, disassociated rdataset.
 *
 *	'sigrdataset' is a valid, disassociated rdataset, or it is NULL.
 *
 *	If 'covers' != 0, 'type' must be SIG.
 *
 *	'type' is not a meta-RR type such as 'ANY' or 'OPT'.
 *
 * Ensures:
 *
 *	On success, 'rdataset' is associated with the found rdataset.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOTFOUND
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_allrdatasets(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		    isc_stdtime_t now, dns_rdatasetiter_t **iteratorp);
/*
 * Make '*iteratorp' an rdataset iteratator for all rdatasets at 'node' in
 * version 'version' of 'db'.
 *
 * Notes:
 *
 *	If 'version' is NULL, then the current version will be used.
 *
 *	The 'now' field is ignored if 'db' is a zone database.  If 'db' is a
 *	cache database, an rdataset will not be found unless it expires after
 *	'now'.  Any ANY query will not match unless at least one rdataset at
 *	the node expires after 'now'.  If 'now' is zero, then the current time
 *	will be used.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 *
 *	iteratorp != NULL && *iteratorp == NULL
 *
 * Ensures:
 *
 *	On success, '*iteratorp' is a valid rdataset iterator.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	ISC_R_NOTFOUND
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_addrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		   isc_stdtime_t now, dns_rdataset_t *rdataset,
		   unsigned int options, dns_rdataset_t *addedrdataset);
/*
 * Add 'rdataset' to 'node' in version 'version' of 'db'.
 *
 * Notes:
 *
 *	If the database has zone semantics, the DNS_DBADD_MERGE option is set,
 *	and an rdataset of the same type as 'rdataset' already exists at
 *	'node' then the contents of 'rdataset' will be merged with the existing
 *	rdataset.  If the option is not set, then rdataset will replace any
 *	existing rdataset of the same type.  If not merging and the
 *	DNS_DBADD_FORCE option is set, then the data will update the database
 *	without regard to trust levels.  If not forcing the data, then the
 *	rdataset will only be added if its trust level is >= the trust level of
 *	any existing rdataset.  Forcing is only meaningful for cache databases.
 *	If DNS_DBADD_EXACT is set then there must be no rdata in common between
 *	the old and new rdata sets.  If DNS_DBADD_EXACTTTL is set then both
 *	the old and new rdata sets must have the same ttl.
 *
 *	The 'now' field is ignored if 'db' is a zone database.  If 'db' is
 *	a cache database, then the added rdataset will expire no later than
 *	now + rdataset->ttl.
 *
 *	If 'addedrdataset' is not NULL, then it will be attached to the
 *	resulting new rdataset in the database, or to the existing data if
 *	the existing data was better.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 *
 *	'rdataset' is a valid, associated rdataset with the same class
 *	as 'db'.
 *
 *	'addedrdataset' is NULL, or a valid, unassociated rdataset.
 *
 *	The database has zone semantics and 'version' is a valid
 *	read-write version, or the database has cache semantics
 *	and version is NULL.
 *
 *	If the database has cache semantics, the DNS_DBADD_MERGE option must
 *	not be set.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	DNS_R_UNCHANGED			The operation did not change anything.
 *	ISC_R_NOMEMORY
 *	DNS_R_NOTEXACT
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_subtractrdataset(dns_db_t *db, dns_dbnode_t *node,
			dns_dbversion_t *version, dns_rdataset_t *rdataset,
			unsigned int options, dns_rdataset_t *newrdataset);
/*
 * Remove any rdata in 'rdataset' from 'node' in version 'version' of
 * 'db'.
 *
 * Notes:
 *
 *	If 'newrdataset' is not NULL, then it will be attached to the
 *	resulting new rdataset in the database, unless the rdataset has
 *	become nonexistent.  If DNS_DBSUB_EXACT is set then all elements
 *	of 'rdataset' must exist at 'node'.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 *
 *	'rdataset' is a valid, associated rdataset with the same class
 *	as 'db'.
 *
 *	'newrdataset' is NULL, or a valid, unassociated rdataset.
 *
 *	The database has zone semantics and 'version' is a valid
 *	read-write version.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	DNS_R_UNCHANGED			The operation did not change anything.
 *	DNS_R_NXRRSET			All rdata of the same type as those
 *					in 'rdataset' have been deleted.
 *	DNS_R_NOTEXACT			Some part of 'rdataset' did not
 *					exist and DNS_DBSUB_EXACT was set.
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_deleterdataset(dns_db_t *db, dns_dbnode_t *node,
		      dns_dbversion_t *version, dns_rdatatype_t type,
		      dns_rdatatype_t covers);
/*
 * Make it so that no rdataset of type 'type' exists at 'node' in version
 * version 'version' of 'db'.
 *
 * Notes:
 *
 *	If 'type' is dns_rdatatype_any, then no rdatasets will exist in
 *	'version' (provided that the dns_db_deleterdataset() isn't followed
 *	by one or more dns_db_addrdataset() calls).
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 *	'node' is a valid node.
 *
 *	The database has zone semantics and 'version' is a valid
 *	read-write version, or the database has cache semantics
 *	and version is NULL.
 *
 *	'type' is not a meta-RR type, except for dns_rdatatype_any, which is
 *	allowed.
 *
 *	If 'covers' != 0, 'type' must be SIG.
 *
 * Returns:
 *
 *	ISC_R_SUCCESS
 *	DNS_R_UNCHANGED			No rdatasets of 'type' existed before
 *					the operation was attempted.
 *
 *	Other results are possible, depending upon the database
 *	implementation used.
 */

isc_result_t
dns_db_getsoaserial(dns_db_t *db, dns_dbversion_t *ver, isc_uint32_t *serialp);
/*
 * Get the current SOA serial number from a zone database.
 *
 * Requires:
 *      'db' is a valid database with zone semantics.
 *      'ver' is a valid version.
 */

void
dns_db_overmem(dns_db_t *db, isc_boolean_t overmem);
/*
 * Enable / disable agressive cache cleaning.
 */

unsigned int
dns_db_nodecount(dns_db_t *db);
/*
 * Count the number of nodes in 'db'.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 * 	The number of nodes in the database
 */

void
dns_db_settask(dns_db_t *db, isc_task_t *task);
/*
 * If task is set then the final detach maybe performed asynchronously.
 *
 * Requires:
 *	'db' is a valid database.
 *	'task' to be valid or NULL.
 */

isc_boolean_t
dns_db_ispersistent(dns_db_t *db);
/*
 * Is 'db' persistent?  A persistent database does not need to be loaded
 * from disk or written to disk.
 *
 * Requires:
 *
 *	'db' is a valid database.
 *
 * Returns:
 *	ISC_TRUE	'db' is persistent.
 *	ISC_FALSE	'db' is not persistent.
 */

isc_result_t
dns_db_register(const char *name, dns_dbcreatefunc_t create, void *driverarg,
		isc_mem_t *mctx, dns_dbimplementation_t **dbimp);

/*
 * Register a new database implementation and add it to the list of
 * supported implementations.
 *
 * Requires:
 *
 * 	'name' is not NULL
 * 	'order' is a valid function pointer
 * 	'mctx' is a valid memory context
 * 	dbimp != NULL && *dbimp == NULL
 *
 * Returns:
 * 	ISC_R_SUCCESS	The registration succeeded
 * 	ISC_R_NOMEMORY	Out of memory
 * 	ISC_R_EXISTS	A database implementation with the same name exists
 *
 * Ensures:
 *
 *	*dbimp points to an opaque structure which must be passed to
 *	dns_db_unregister().
 */

void
dns_db_unregister(dns_dbimplementation_t **dbimp);
/*
 * Remove a database implementation from the the list of supported
 * implementations.  No databases of this type can be active when this
 * is called.
 *
 * Requires:
 * 	dbimp != NULL && *dbimp == NULL
 *
 * Ensures:
 *
 * 	Any memory allocated in *dbimp will be freed.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_DB_H */
