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

/* $Id: zoneconf.c,v 1.87.2.6 2004/03/09 06:09:20 marka Exp $ */

#include <config.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdatatype.h>
#include <dns/ssu.h>
#include <dns/zone.h>

#include <named/config.h>
#include <named/globals.h>
#include <named/log.h>
#include <named/server.h>
#include <named/zoneconf.h>

/*
 * These are BIND9 server defaults, not necessarily identical to the
 * library defaults defined in zone.c.
 */
#define RETERR(x) do { \
	isc_result_t _r = (x); \
	if (_r != ISC_R_SUCCESS) \
		return (_r); \
	} while (0)

/*
 * Convenience function for configuring a single zone ACL.
 */
static isc_result_t
configure_zone_acl(cfg_obj_t *zconfig, cfg_obj_t *vconfig, cfg_obj_t *config,
		   const char *aclname, ns_aclconfctx_t *actx,
		   dns_zone_t *zone, 
		   void (*setzacl)(dns_zone_t *, dns_acl_t *),
		   void (*clearzacl)(dns_zone_t *))
{
	isc_result_t result;
	cfg_obj_t *maps[4];
	cfg_obj_t *aclobj = NULL;
	int i = 0;
	dns_acl_t *dacl = NULL;

	if (zconfig != NULL)
		maps[i++] = cfg_tuple_get(zconfig, "options");
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		cfg_obj_t *options = NULL;
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i] = NULL;

	result = ns_config_get(maps, aclname, &aclobj);
	if (aclobj == NULL) {
		(*clearzacl)(zone);
		return (ISC_R_SUCCESS);
	}

	result = ns_acl_fromconfig(aclobj, config, actx,
				   dns_zone_getmctx(zone), &dacl);
	if (result != ISC_R_SUCCESS)
		return (result);
	(*setzacl)(zone, dacl);
	dns_acl_detach(&dacl);
	return (ISC_R_SUCCESS);
}

/*
 * Parse the zone update-policy statement.
 */
static isc_result_t
configure_zone_ssutable(cfg_obj_t *zconfig, dns_zone_t *zone) {
	cfg_obj_t *updatepolicy = NULL;
	cfg_listelt_t *element, *element2;
	dns_ssutable_t *table = NULL;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	isc_result_t result;

	(void)cfg_map_get(zconfig, "update-policy", &updatepolicy);
	if (updatepolicy == NULL)
		return (ISC_R_SUCCESS);

	result = dns_ssutable_create(mctx, &table);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(updatepolicy);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *stmt = cfg_listelt_value(element);
		cfg_obj_t *mode = cfg_tuple_get(stmt, "mode");
		cfg_obj_t *identity = cfg_tuple_get(stmt, "identity");
		cfg_obj_t *matchtype = cfg_tuple_get(stmt, "matchtype");
		cfg_obj_t *dname = cfg_tuple_get(stmt, "name");
		cfg_obj_t *typelist = cfg_tuple_get(stmt, "types");
		char *str;
		isc_boolean_t grant = ISC_FALSE;
		unsigned int mtype = DNS_SSUMATCHTYPE_NAME;
		dns_fixedname_t fname, fident;
		isc_buffer_t b;
		dns_rdatatype_t *types;
		unsigned int i, n;

		str = cfg_obj_asstring(mode);
		if (strcasecmp(str, "grant") == 0)
			grant = ISC_TRUE;
		else if (strcasecmp(str, "deny") == 0)
			grant = ISC_FALSE;
		else
			INSIST(0);

		str = cfg_obj_asstring(matchtype);
		if (strcasecmp(str, "name") == 0)
			mtype = DNS_SSUMATCHTYPE_NAME;
		else if (strcasecmp(str, "subdomain") == 0)
			mtype = DNS_SSUMATCHTYPE_SUBDOMAIN;
		else if (strcasecmp(str, "wildcard") == 0)
			mtype = DNS_SSUMATCHTYPE_WILDCARD;
		else if (strcasecmp(str, "self") == 0)
			mtype = DNS_SSUMATCHTYPE_SELF;
		else
			INSIST(0);

		dns_fixedname_init(&fident);
		str = cfg_obj_asstring(identity);
		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(dns_fixedname_name(&fident), &b,
					   dns_rootname, ISC_FALSE, NULL);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
				    "'%s' is not a valid name", str);
			goto cleanup;
		}

		dns_fixedname_init(&fname);
		str = cfg_obj_asstring(dname);
		isc_buffer_init(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(dns_fixedname_name(&fname), &b,
					   dns_rootname, ISC_FALSE, NULL);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
				    "'%s' is not a valid name", str);
			goto cleanup;
		}

		n = ns_config_listcount(typelist);
		if (n == 0)
			types = NULL;
		else {
			types = isc_mem_get(mctx, n * sizeof(dns_rdatatype_t));
			if (types == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
		}

		i = 0;
		for (element2 = cfg_list_first(typelist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			cfg_obj_t *typeobj;
			isc_textregion_t r;

			INSIST(i < n);

			typeobj = cfg_listelt_value(element2);
			str = cfg_obj_asstring(typeobj);
			r.base = str;
			r.length = strlen(str);

			result = dns_rdatatype_fromtext(&types[i++], &r);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "'%s' is not a valid type", str);
				isc_mem_put(mctx, types,
					    n * sizeof(dns_rdatatype_t));
				goto cleanup;
			}
		}
		INSIST(i == n);

		result = dns_ssutable_addrule(table, grant,
					      dns_fixedname_name(&fident),
					      mtype,
					      dns_fixedname_name(&fname),
					      n, types);
		if (types != NULL)
			isc_mem_put(mctx, types, n * sizeof(dns_rdatatype_t));
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}

	}

	result = ISC_R_SUCCESS;
	dns_zone_setssutable(zone, table);

 cleanup:
	dns_ssutable_detach(&table);
	return (result);
}

/*
 * Convert a config file zone type into a server zone type.
 */
static inline dns_zonetype_t
zonetype_fromconfig(cfg_obj_t *map) {
	cfg_obj_t *obj = NULL;
	isc_result_t result;

	result = cfg_map_get(map, "type", &obj);
	INSIST(result == ISC_R_SUCCESS);
	return (ns_config_getzonetype(obj));
}

/*
 * Helper function for strtoargv().  Pardon the gratuitous recursion.
 */
static isc_result_t
strtoargvsub(isc_mem_t *mctx, char *s, unsigned int *argcp,
	     char ***argvp, unsigned int n)
{
	isc_result_t result;
	
	/* Discard leading whitespace. */
	while (*s == ' ' || *s == '\t')
		s++;
	
	if (*s == '\0') {
		/* We have reached the end of the string. */
		*argcp = n;
		*argvp = isc_mem_get(mctx, n * sizeof(char *));
		if (*argvp == NULL)
			return (ISC_R_NOMEMORY);
	} else {
		char *p = s;
		while (*p != ' ' && *p != '\t' && *p != '\0')
			p++;
		if (*p != '\0')
			*p++ = '\0';

		result = strtoargvsub(mctx, p, argcp, argvp, n + 1);
		if (result != ISC_R_SUCCESS)
			return (result);
		(*argvp)[n] = s;
	}
	return (ISC_R_SUCCESS);
}

/*
 * Tokenize the string "s" into whitespace-separated words,
 * return the number of words in '*argcp' and an array
 * of pointers to the words in '*argvp'.  The caller
 * must free the array using isc_mem_put().  The string
 * is modified in-place.
 */
static isc_result_t
strtoargv(isc_mem_t *mctx, char *s, unsigned int *argcp, char ***argvp) {
	return (strtoargvsub(mctx, s, argcp, argvp, 0));
}

isc_result_t
ns_zone_configure(cfg_obj_t *config, cfg_obj_t *vconfig, cfg_obj_t *zconfig,
		  ns_aclconfctx_t *ac, dns_zone_t *zone)
{
	isc_result_t result;
	char *zname;
	dns_rdataclass_t zclass;
	dns_rdataclass_t vclass;
	cfg_obj_t *maps[5];
	cfg_obj_t *zoptions = NULL;
	cfg_obj_t *options = NULL;
	cfg_obj_t *obj;
	const char *filename = NULL;
	dns_notifytype_t notifytype = dns_notifytype_yes;
	isc_sockaddr_t *addrs;
	dns_name_t **keynames;
	isc_uint32_t count;
	char *cpval;
	unsigned int dbargc;
	char **dbargv;
	static char default_dbtype[] = "rbt";
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	dns_dialuptype_t dialup = dns_dialuptype_no;
	dns_zonetype_t ztype;
	int i;

	i = 0;
	if (zconfig != NULL) {
		zoptions = cfg_tuple_get(zconfig, "options");
		maps[i++] = zoptions;
	}
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		(void)cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i++] = ns_g_defaults;
	maps[i++] = NULL;

	if (vconfig != NULL)
		RETERR(ns_config_getclass(cfg_tuple_get(vconfig, "class"),
					  dns_rdataclass_in, &vclass));
	else
		vclass = dns_rdataclass_in;

	/*
	 * Configure values common to all zone types.
	 */

	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));

	RETERR(ns_config_getclass(cfg_tuple_get(zconfig, "class"),
				  vclass, &zclass));
	dns_zone_setclass(zone, zclass);

	ztype = zonetype_fromconfig(zoptions);
	dns_zone_settype(zone, ztype);

	obj = NULL;
	result = cfg_map_get(zoptions, "database", &obj);
	if (result == ISC_R_SUCCESS)
		cpval = cfg_obj_asstring(obj);
	else
		cpval = default_dbtype;
	RETERR(strtoargv(mctx, cpval, &dbargc, &dbargv));
	/*
	 * ANSI C is strange here.  There is no logical reason why (char **)
	 * cannot be promoted automatically to (const char * const *) by the
	 * compiler w/o generating a warning.
	 */
	RETERR(dns_zone_setdbtype(zone, dbargc, (const char * const *)dbargv));
	isc_mem_put(mctx, dbargv, dbargc * sizeof(*dbargv));

	obj = NULL;
	result = cfg_map_get(zoptions, "file", &obj);
	if (result == ISC_R_SUCCESS)
		filename = cfg_obj_asstring(obj);
	RETERR(dns_zone_setfile(zone, filename));

	if (ztype == dns_zone_slave)
		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  "allow-notify", ac, zone,
					  dns_zone_setnotifyacl,
					  dns_zone_clearnotifyacl));
	/*
	 * XXXAG This probably does not make sense for stubs.
	 */
	RETERR(configure_zone_acl(zconfig, vconfig, config,
				  "allow-query", ac, zone,
				  dns_zone_setqueryacl,
				  dns_zone_clearqueryacl));

	obj = NULL;
	result = ns_config_get(maps, "dialup", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isboolean(obj)) {
		if (cfg_obj_asboolean(obj))
			dialup = dns_dialuptype_yes;
		else
			dialup = dns_dialuptype_no;
	} else {
		char *dialupstr = cfg_obj_asstring(obj);
		if (strcasecmp(dialupstr, "notify") == 0)
			dialup = dns_dialuptype_notify;
		else if (strcasecmp(dialupstr, "notify-passive") == 0)
			dialup = dns_dialuptype_notifypassive;
		else if (strcasecmp(dialupstr, "refresh") == 0)
			dialup = dns_dialuptype_refresh;
		else if (strcasecmp(dialupstr, "passive") == 0)
			dialup = dns_dialuptype_passive;
		else
			INSIST(0);
	}
	dns_zone_setdialup(zone, dialup);

	obj = NULL;
	result = ns_config_get(maps, "zone-statistics", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zone_setstatistics(zone, cfg_obj_asboolean(obj));

	/*
	 * Configure master functionality.  This applies
	 * to primary masters (type "master") and slaves
	 * acting as masters (type "slave"), but not to stubs.
	 */
	if (ztype != dns_zone_stub) {
		obj = NULL;
		result = ns_config_get(maps, "notify", &obj);
		INSIST(result == ISC_R_SUCCESS);
		if (cfg_obj_isboolean(obj)) {
			if (cfg_obj_asboolean(obj))
				notifytype = dns_notifytype_yes;
			else
				notifytype = dns_notifytype_no;
		} else {
			char *notifystr = cfg_obj_asstring(obj);
			if (strcasecmp(notifystr, "explicit") == 0)
				notifytype = dns_notifytype_explicit;
			else
				INSIST(0);
		}
		dns_zone_setnotifytype(zone, notifytype);

		obj = NULL;
		result = ns_config_get(maps, "also-notify", &obj);
		if (result == ISC_R_SUCCESS) {
			isc_sockaddr_t *addrs = NULL;
			isc_uint32_t addrcount;
			result = ns_config_getiplist(config, obj, 0, mctx,
						     &addrs, &addrcount);
			if (result != ISC_R_SUCCESS)
				return (result);
			result = dns_zone_setalsonotify(zone, addrs,
							addrcount);
			ns_config_putiplist(mctx, &addrs, addrcount);
			if (result != ISC_R_SUCCESS)
				return (result);
		} else
			RETERR(dns_zone_setalsonotify(zone, NULL, 0));

		obj = NULL;
		result = ns_config_get(maps, "notify-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setnotifysrc4(zone, cfg_obj_assockaddr(obj));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "notify-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setnotifysrc6(zone, cfg_obj_assockaddr(obj));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  "allow-transfer", ac, zone,
					  dns_zone_setxfracl,
					  dns_zone_clearxfracl));

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-time-out", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxxfrout(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-idle-out", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setidleout(zone, cfg_obj_asuint32(obj) * 60);
	}

	/*
	 * Configure update-related options.  These apply to
	 * primary masters only.
	 */
	if (ztype == dns_zone_master) {
		dns_acl_t *updateacl;
		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  "allow-update", ac, zone,
					  dns_zone_setupdateacl,
					  dns_zone_clearupdateacl));
		
		updateacl = dns_zone_getupdateacl(zone);
		if (updateacl != NULL  && dns_acl_isinsecure(updateacl))
			isc_log_write(ns_g_lctx, DNS_LOGCATEGORY_SECURITY,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "zone '%s' allows updates by IP "
				      "address, which is insecure",
				      zname);
		
		RETERR(configure_zone_ssutable(zoptions, zone));

		obj = NULL;
		result = ns_config_get(maps, "sig-validity-interval", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setsigvalidityinterval(zone,
						cfg_obj_asuint32(obj) * 86400);
	} else if (ztype == dns_zone_slave) {
		RETERR(configure_zone_acl(zconfig, vconfig, config,
					  "allow-update-forwarding", ac, zone,
					  dns_zone_setforwardacl,
					  dns_zone_clearforwardacl));
	}

	/*
	 * Configure slave functionality.
	 */
	switch (ztype) {
	case dns_zone_slave:
	case dns_zone_stub:
		obj = NULL;
		result = cfg_map_get(zoptions, "masters", &obj);
		if (obj != NULL) {
			addrs = NULL;
			keynames = NULL;
			RETERR(ns_config_getipandkeylist(config, obj, mctx,
							 &addrs, &keynames,
							 &count));
			result = dns_zone_setmasterswithkeys(zone, addrs,
							     keynames, count);
			ns_config_putipandkeylist(mctx, &addrs, &keynames,
						  count);
		} else
			result = dns_zone_setmasters(zone, NULL, 0);
		RETERR(result);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-time-in", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxxfrin(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-transfer-idle-in", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setidlein(zone, cfg_obj_asuint32(obj) * 60);

		obj = NULL;
		result = ns_config_get(maps, "max-refresh-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxrefreshtime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "min-refresh-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setminrefreshtime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "max-retry-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setmaxretrytime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "min-retry-time", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setminretrytime(zone, cfg_obj_asuint32(obj));

		obj = NULL;
		result = ns_config_get(maps, "transfer-source", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setxfrsource4(zone, cfg_obj_assockaddr(obj));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		obj = NULL;
		result = ns_config_get(maps, "transfer-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		dns_zone_setxfrsource6(zone, cfg_obj_assockaddr(obj));
		ns_add_reserved_dispatch(ns_g_server, cfg_obj_assockaddr(obj));

		break;

	default:
		break;
	}

	return (ISC_R_SUCCESS);
}

isc_boolean_t
ns_zone_reusable(dns_zone_t *zone, cfg_obj_t *zconfig) {
	cfg_obj_t *zoptions = NULL;
	cfg_obj_t *obj = NULL;
	const char *cfilename;
	const char *zfilename;

	zoptions = cfg_tuple_get(zconfig, "options");

	if (zonetype_fromconfig(zoptions) != dns_zone_gettype(zone))
		return (ISC_FALSE);

	obj = NULL;
	(void)cfg_map_get(zoptions, "file", &obj);
	if (obj != NULL)
		cfilename = cfg_obj_asstring(obj);
	else
		cfilename = NULL;
	zfilename = dns_zone_getfile(zone);
	if (!((cfilename == NULL && zfilename == NULL) ||
	      (cfilename != NULL && zfilename != NULL &&
	       strcmp(cfilename, zfilename) == 0)))
	    return (ISC_FALSE);

	return (ISC_TRUE);
}
