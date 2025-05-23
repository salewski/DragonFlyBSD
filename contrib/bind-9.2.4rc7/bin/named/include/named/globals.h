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

/* $Id: globals.h,v 1.59.2.1 2004/03/09 06:09:21 marka Exp $ */

#ifndef NAMED_GLOBALS_H
#define NAMED_GLOBALS_H 1

#include <isc/rwlock.h>
#include <isc/log.h>
#include <isc/net.h>

#include <isccfg/cfg.h>

#include <dns/zone.h>

#include <named/types.h>

#undef EXTERN
#undef INIT
#ifdef NS_MAIN
#define EXTERN
#define INIT(v)	= (v)
#else
#define EXTERN extern
#define INIT(v)
#endif

EXTERN isc_mem_t *		ns_g_mctx		INIT(NULL);
EXTERN unsigned int		ns_g_cpus		INIT(0);
EXTERN isc_taskmgr_t *		ns_g_taskmgr		INIT(NULL);
EXTERN dns_dispatchmgr_t *	ns_g_dispatchmgr	INIT(NULL);
EXTERN isc_entropy_t *		ns_g_entropy		INIT(NULL);
/*
 * XXXRTH  We're going to want multiple timer managers eventually.  One
 *         for really short timers, another for client timers, and one
 *         for zone timers.
 */
EXTERN isc_timermgr_t *		ns_g_timermgr		INIT(NULL);
EXTERN isc_socketmgr_t *	ns_g_socketmgr		INIT(NULL);
EXTERN cfg_parser_t *		ns_g_parser		INIT(NULL);
EXTERN const char *		ns_g_version		INIT(VERSION);
EXTERN in_port_t		ns_g_port		INIT(0);
EXTERN in_port_t		lwresd_g_listenport	INIT(0);

EXTERN ns_server_t *		ns_g_server		INIT(NULL);

EXTERN isc_boolean_t		ns_g_lwresdonly		INIT(ISC_FALSE);

/*
 * Logging.
 */
EXTERN isc_log_t *		ns_g_lctx		INIT(NULL);
EXTERN isc_logcategory_t *	ns_g_categories		INIT(NULL);
EXTERN isc_logmodule_t *	ns_g_modules		INIT(NULL);
EXTERN unsigned int		ns_g_debuglevel		INIT(0);

/*
 * Current configuration information.
 */
EXTERN cfg_obj_t *		ns_g_config		INIT(NULL);
EXTERN cfg_obj_t *		ns_g_defaults		INIT(NULL);
EXTERN const char *		ns_g_conffile		INIT(NS_SYSCONFDIR
							     "/named.conf");
EXTERN const char *		ns_g_keyfile		INIT(NS_SYSCONFDIR
							     "/rndc.key");
EXTERN const char *		lwresd_g_conffile	INIT(NS_SYSCONFDIR
							     "/lwresd.conf");
EXTERN const char *		lwresd_g_resolvconffile	INIT("/etc"
							     "/resolv.conf");
EXTERN isc_boolean_t		ns_g_conffileset	INIT(ISC_FALSE);
EXTERN isc_boolean_t		lwresd_g_useresolvconf	INIT(ISC_FALSE);

/*
 * Initial resource limits.
 */
EXTERN isc_resourcevalue_t	ns_g_initstacksize	INIT(0);
EXTERN isc_resourcevalue_t	ns_g_initdatasize	INIT(0);
EXTERN isc_resourcevalue_t	ns_g_initcoresize	INIT(0);
EXTERN isc_resourcevalue_t	ns_g_initopenfiles	INIT(0);

/*
 * Misc.
 */
EXTERN isc_boolean_t		ns_g_coreok		INIT(ISC_TRUE);
EXTERN const char *		ns_g_chrootdir		INIT(NULL);
EXTERN isc_boolean_t		ns_g_foreground		INIT(ISC_FALSE);
EXTERN isc_boolean_t		ns_g_logstderr		INIT(ISC_FALSE);

EXTERN const char *		ns_g_defaultpidfile 	INIT(NS_LOCALSTATEDIR
							     "/run/named.pid");
EXTERN const char *		lwresd_g_defaultpidfile INIT(NS_LOCALSTATEDIR
							    "/run/lwresd.pid");
EXTERN const char *		ns_g_username		INIT(NULL);

#undef EXTERN
#undef INIT

#endif /* NAMED_GLOBALS_H */
