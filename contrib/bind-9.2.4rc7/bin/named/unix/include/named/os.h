/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* $Id: os.h,v 1.14.2.5 2004/03/09 06:09:24 marka Exp $ */

#ifndef NS_OS_H
#define NS_OS_H 1

#include <isc/types.h>

void
ns_os_init(const char *progname);

void
ns_os_daemonize(void);

void
ns_os_opendevnull(void);

void
ns_os_closedevnull(void);

void
ns_os_chroot(const char *root);

void
ns_os_inituserinfo(const char *username);

void
ns_os_changeuser(void);

void
ns_os_minprivs(void);

void
ns_os_writepidfile(const char *filename, isc_boolean_t first_time);

void
ns_os_shutdown(void);

void
ns_os_tzset(void);

#endif /* NS_OS_H */
