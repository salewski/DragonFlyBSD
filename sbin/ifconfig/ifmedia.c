/*	$NetBSD: ifconfig.c,v 1.34 1997/04/21 01:17:58 lukem Exp $	*/
/* $FreeBSD: src/sbin/ifconfig/ifmedia.c,v 1.6.2.3 2001/11/14 04:35:07 yar Exp $ */
/* $DragonFly: src/sbin/ifconfig/ifmedia.c,v 1.8 2005/03/06 05:01:59 dillon Exp $ */

/*
 * Copyright (c) 1997 Jason R. Thorpe.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

#ifndef IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS
  #define IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS {                       \
        { IFM_AUTO, "autoselect" },                                     \
        { IFM_IEEE80211_11A, "11a" },                                   \
        { IFM_IEEE80211_11B, "11b" },                                   \
        { IFM_IEEE80211_11G, "11g" },                                   \
        { IFM_IEEE80211_FH, "fh" },                                     \
        { 0, NULL },                                                    \
}
#endif

#ifndef IFM_SUBTYPE_IEEE80211_MODE_ALIASES
  #define IFM_SUBTYPE_IEEE80211_MODE_ALIASES {                            \
        { IFM_AUTO, "auto" },                                           \
        { 0, NULL },                                                    \
}
#endif

static void	domediaopt(const char *, int, int);
static int	get_media_subtype(int, const char *);
static int	get_media_mode(int, const char *);
static int	get_media_options(int, const char *);
static int	lookup_media_word(struct ifmedia_description *, const char *);
static void	print_media_word(int, int);
static void	print_media_word_ifconfig(int);

static struct ifmedia_description *get_toptype_desc(int);
static struct ifmedia_type_to_subtype *get_toptype_ttos(int);
static struct ifmedia_description *get_subtype_desc(int,
    struct ifmedia_type_to_subtype *ttos);

void
media_status(int s, struct rt_addrinfo *info __unused)
{
	struct ifmediareq ifmr;
	int *media_list, i;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA.
		 */
		return;
	}

	if (ifmr.ifm_count == 0) {
		warnx("%s: no media types?", name);
		return;
	}

	media_list = (int *)malloc(ifmr.ifm_count * sizeof(int));
	if (media_list == NULL)
		err(1, "malloc");
	ifmr.ifm_ulist = media_list;

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	printf("\tmedia: ");
	print_media_word(ifmr.ifm_current, 1);
	if (ifmr.ifm_active != ifmr.ifm_current) {
		putchar(' ');
		putchar('(');
		print_media_word(ifmr.ifm_active, 0);
		putchar(')');
	}

	putchar('\n');

	if (ifmr.ifm_status & IFM_AVALID) {
		printf("\tstatus: ");
		switch (IFM_TYPE(ifmr.ifm_active)) {
		case IFM_ETHER:
			if (ifmr.ifm_status & IFM_ACTIVE)
				printf("active");
			else
				printf("no carrier");
			break;

		case IFM_FDDI:
		case IFM_TOKEN:
			if (ifmr.ifm_status & IFM_ACTIVE)
				printf("inserted");
			else
				printf("no ring");
			break;

#if notyet
		case IFM_ATM:
			if (ifmr.ifm_status & IFM_ACTIVE)
				printf("active");
			else
				printf("no carrier");
			break;
#endif

		case IFM_IEEE80211:
			/* XXX: Different value for adhoc? */
			if (ifmr.ifm_status & IFM_ACTIVE)
				printf("associated");
			else
				printf("no carrier");
			break;
		}
		putchar('\n');
	}

	if (ifmr.ifm_count > 0 && supmedia) {
		printf("\tsupported media:\n");
		for (i = 0; i < ifmr.ifm_count; i++) {
			printf("\t\t");
			print_media_word_ifconfig(media_list[i]);
			putchar('\n');
		}
	}

	free(media_list);
}

void
setmedia(const char *val, int d __unused, int s,
	 const struct afswtch *afp __unused)
{
	struct ifmediareq ifmr;
	int first_type, subtype;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	ifmr.ifm_count = 1;
	ifmr.ifm_ulist = &first_type;
	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		/*
		 * If we get E2BIG, the kernel is telling us
		 * that there are more, so we can ignore it.
		 */
		if (errno != E2BIG)
			err(1, "SIOCGIFMEDIA");
	}

	if (ifmr.ifm_count == 0)
		errx(1, "%s: no media types?", name);

	/*
	 * We are primarily concerned with the top-level type.
	 * However, "current" may be only IFM_NONE, so we just look
	 * for the top-level type in the first "supported type"
	 * entry.
	 *
	 * (I'm assuming that all supported media types for a given
	 * interface will be the same top-level type..)
	 */
	subtype = get_media_subtype(IFM_TYPE(first_type), val);

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = (ifmr.ifm_current & ~(IFM_NMASK|IFM_TMASK)) |
	    IFM_TYPE(first_type) | subtype;

	if (ioctl(s, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
		err(1, "SIOCSIFMEDIA (media)");
}

void
setmediaopt(const char *val, int d __unused, int s,
	    const struct afswtch *afp __unused)
{

	domediaopt(val, 0, s);
}

void
unsetmediaopt(const char *val, int d __unused, int s,
	      const struct afswtch *afp __unused)
{

	domediaopt(val, 1, s);
}

static void
domediaopt(const char *val, int clear, int s)
{
	struct ifmediareq ifmr;
	int *mwords, options;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	/*
	 * We must go through the motions of reading all
	 * supported media because we need to know both
	 * the current media type and the top-level type.
	 */

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	if (ifmr.ifm_count == 0)
		errx(1, "%s: no media types?", name);

	mwords = (int *)malloc(ifmr.ifm_count * sizeof(int));
	if (mwords == NULL)
		err(1, "malloc");

	ifmr.ifm_ulist = mwords;
	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	options = get_media_options(IFM_TYPE(mwords[0]), val);

	free(mwords);

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = ifmr.ifm_current;
	if (clear)
		ifr.ifr_media &= ~options;
	else
		ifr.ifr_media |= options;

	if (ioctl(s, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
		err(1, "SIOCSIFMEDIA (mediaopt)");
}


void
setmediamode(const char *val, int d __unused, int s,
	const struct afswtch *afp __unused)
{
	struct ifmediareq ifmr;
	int *mwords, mode;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	/*
	 * We must go through the motions of reading all
	 * supported media because we need to know both
	 * the current media type and the top-level type.
	 */

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	if (ifmr.ifm_count == 0)
		errx(1, "%s: no media types?", name);

	mwords = (int *)malloc(ifmr.ifm_count * sizeof(int));
	if (mwords == NULL)
		err(1, "malloc");

	ifmr.ifm_ulist = mwords;
	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		err(1, "SIOCGIFMEDIA");

	mode = get_media_mode(IFM_TYPE(mwords[0]), val);

	free(mwords);

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = (ifmr.ifm_current & ~IFM_MMASK) | mode;

	if (ioctl(s, SIOCSIFMEDIA, (caddr_t)&ifr) < 0)
		err(1, "SIOCSIFMEDIA (mode)");
}

/**********************************************************************
 * A good chunk of this is duplicated from sys/net/ifmedia.c
 **********************************************************************/

static struct ifmedia_description ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_ethernet_descriptions[] =
    IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_ethernet_aliases[] =
    IFM_SUBTYPE_ETHERNET_ALIASES;

static struct ifmedia_description ifm_subtype_ethernet_option_descriptions[] =
    IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_tokenring_descriptions[] =
    IFM_SUBTYPE_TOKENRING_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_tokenring_aliases[] =
    IFM_SUBTYPE_TOKENRING_ALIASES;

static struct ifmedia_description ifm_subtype_tokenring_option_descriptions[] =
    IFM_SUBTYPE_TOKENRING_OPTION_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_fddi_descriptions[] =
    IFM_SUBTYPE_FDDI_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_fddi_aliases[] =
    IFM_SUBTYPE_FDDI_ALIASES;

static struct ifmedia_description ifm_subtype_fddi_option_descriptions[] =
    IFM_SUBTYPE_FDDI_OPTION_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_ieee80211_descriptions[] =
    IFM_SUBTYPE_IEEE80211_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_ieee80211_aliases[] =
    IFM_SUBTYPE_IEEE80211_ALIASES;

static struct ifmedia_description ifm_subtype_ieee80211_option_descriptions[] =
    IFM_SUBTYPE_IEEE80211_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ieee80211_mode_descriptions[] =
    IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ieee80211_mode_aliases[] =
    IFM_SUBTYPE_IEEE80211_MODE_ALIASES;

#if notyet
static struct ifmedia_description ifm_subtype_atm_descriptions[] =
    IFM_SUBTYPE_ATM_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_atm_aliases[] =
    IFM_SUBTYPE_ATM_ALIASES;

static struct ifmedia_description ifm_subtype_atm_option_descriptions[] =
    IFM_SUBTYPE_ATM_OPTION_DESCRIPTIONS;
#endif

static struct ifmedia_description ifm_subtype_shared_descriptions[] =
    IFM_SUBTYPE_SHARED_DESCRIPTIONS;

static struct ifmedia_description ifm_subtype_shared_aliases[] =
    IFM_SUBTYPE_SHARED_ALIASES;

static struct ifmedia_description ifm_shared_option_descriptions[] =
    IFM_SHARED_OPTION_DESCRIPTIONS;

struct ifmedia_type_to_subtype {
	struct {
		struct ifmedia_description *desc;
		int alias;
	} subtypes[5];
	struct {
		struct ifmedia_description *desc;
		int alias;
	} options[3];
	struct {
		struct ifmedia_description *desc;
		int alias;
	} modes[3];
};

/* must be in the same order as IFM_TYPE_DESCRIPTIONS */
static struct ifmedia_type_to_subtype ifmedia_types_to_subtypes[] = {
	{
		{
			{ &ifm_subtype_shared_descriptions[0], 0 },
			{ &ifm_subtype_shared_aliases[0], 1 },
			{ &ifm_subtype_ethernet_descriptions[0], 0 },
			{ &ifm_subtype_ethernet_aliases[0], 1 },
			{ NULL, 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0], 0 },
			{ &ifm_subtype_ethernet_option_descriptions[0], 0 },
			{ NULL, 0 },
		},
		{
			{ NULL, 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0], 0 },
			{ &ifm_subtype_shared_aliases[0], 1 },
			{ &ifm_subtype_tokenring_descriptions[0], 0 },
			{ &ifm_subtype_tokenring_aliases[0], 1 },
			{ NULL, 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0], 0 },
			{ &ifm_subtype_tokenring_option_descriptions[0], 0 },
			{ NULL, 0 },
		},
		{
			{ NULL, 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0], 0 },
			{ &ifm_subtype_shared_aliases[0], 1 },
			{ &ifm_subtype_fddi_descriptions[0], 0 },
			{ &ifm_subtype_fddi_aliases[0], 1 },
			{ NULL, 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0], 0 },
			{ &ifm_subtype_fddi_option_descriptions[0], 0 },
			{ NULL, 0 },
		},
		{
			{ NULL, 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0], 0 },
			{ &ifm_subtype_shared_aliases[0], 1 },
			{ &ifm_subtype_ieee80211_descriptions[0], 0 },
			{ &ifm_subtype_ieee80211_aliases[0], 1 },
			{ NULL, 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0], 0 },
			{ &ifm_subtype_ieee80211_option_descriptions[0], 0 },
			{ NULL, 0 },
		},
		{
			{ &ifm_subtype_ieee80211_mode_descriptions[0], 0 },
			{ &ifm_subtype_ieee80211_mode_aliases[0], 0 },
			{ NULL, 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0], 0 },
			{ &ifm_subtype_shared_aliases[0], 1 },
#if notyet
			{ &ifm_subtype_atm_descriptions[0], 0 },
			{ &ifm_subtype_atm_aliases[0], 1 },
#endif
			{ NULL, 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0], 0 },
#if notyet
			{ &ifm_subtype_atm_option_descriptions[0], 0 },
#endif
			{ NULL, 0 },
		},
		{
			{ NULL, 0 },
		},
	},
};

static int
get_media_subtype(int type, const char *val)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	int rval, i;

	/* Find the top-level interface type. */
	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++)
		if (type == desc->ifmt_word)
			break;
	if (desc->ifmt_string == NULL)
		errx(1, "unknown media type 0x%x", type);

	for (i = 0; ttos->subtypes[i].desc != NULL; i++) {
		rval = lookup_media_word(ttos->subtypes[i].desc, val);
		if (rval != -1)
			return (rval);
	}
	errx(1, "unknown media subtype: %s", val);
	/*NOTREACHED*/
}

static int
get_media_mode(int type, const char *val)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	int rval, i;

	/* Find the top-level interface type. */
	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++)
		if (type == desc->ifmt_word)
			break;
	if (desc->ifmt_string == NULL)
		errx(1, "unknown media mode 0x%x", type);

	for (i = 0; ttos->modes[i].desc != NULL; i++) {
		rval = lookup_media_word(ttos->modes[i].desc, val);
		if (rval != -1)
			return (rval);
	}
	return -1;
}

static int
get_media_options(int type, const char *val)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	char *optlist, *optptr;
	int option = 0, i, rval = 0;

	/* We muck with the string, so copy it. */
	optlist = strdup(val);
	if (optlist == NULL)
		err(1, "strdup");

	/* Find the top-level interface type. */
	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++)
		if (type == desc->ifmt_word)
			break;
	if (desc->ifmt_string == NULL)
		errx(1, "unknown media type 0x%x", type);

	/*
	 * Look up the options in the user-provided comma-separated
	 * list.
	 */
	optptr = optlist;
	for (; (optptr = strtok(optptr, ",")) != NULL; optptr = NULL) {
		for (i = 0; ttos->options[i].desc != NULL; i++) {
			option = lookup_media_word(ttos->options[i].desc, optptr);
			if (option != -1)
				break;
		}
		if (option == 0)
			errx(1, "unknown option: %s", optptr);
		rval |= option;
	}

	free(optlist);
	return (rval);
}

static int
lookup_media_word(struct ifmedia_description *desc, const char *val)
{

	for (; desc->ifmt_string != NULL; desc++)
		if (strcasecmp(desc->ifmt_string, val) == 0)
			return (desc->ifmt_word);

	return (-1);
}

static struct ifmedia_description *get_toptype_desc(int ifmw)
{
	struct ifmedia_description *desc;

	for (desc = ifm_type_descriptions; desc->ifmt_string != NULL; desc++)
		if (IFM_TYPE(ifmw) == desc->ifmt_word)
			break;

	return desc;
}

static struct ifmedia_type_to_subtype *get_toptype_ttos(int ifmw)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;

	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++)
		if (IFM_TYPE(ifmw) == desc->ifmt_word)
			break;

	return ttos;
}

static struct ifmedia_description *get_subtype_desc(int ifmw, 
    struct ifmedia_type_to_subtype *ttos)
{
	int i;
	struct ifmedia_description *desc;

	for (i = 0; ttos->subtypes[i].desc != NULL; i++) {
		if (ttos->subtypes[i].alias)
			continue;
		for (desc = ttos->subtypes[i].desc;
		    desc->ifmt_string != NULL; desc++) {
			if (IFM_SUBTYPE(ifmw) == desc->ifmt_word)
				return desc;
		}
	}

	return NULL;
}

static struct ifmedia_description *get_mode_desc(int ifmw, 
    struct ifmedia_type_to_subtype *ttos)
{
	int i;
	struct ifmedia_description *desc;

	for (i = 0; ttos->modes[i].desc != NULL; i++) {
		if (ttos->modes[i].alias)
			continue;
		for (desc = ttos->modes[i].desc;
		    desc->ifmt_string != NULL; desc++) {
			if (IFM_MODE(ifmw) == desc->ifmt_word)
				return desc;
		}
	}

	return NULL;
}

static void
print_media_word(int ifmw, int print_toptype)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	int seen_option = 0, i;

	/* Find the top-level interface type. */
	desc = get_toptype_desc(ifmw);
	ttos = get_toptype_ttos(ifmw);
	if (desc->ifmt_string == NULL) {
		printf("<unknown type>");
		return;
	} else if (print_toptype) {
		printf("%s", desc->ifmt_string);
	}

	/*
	 * Don't print the top-level type; it's not like we can
	 * change it, or anything.
	 */

	/* Find subtype. */
	desc = get_subtype_desc(ifmw, ttos);
	if (desc != NULL)
		goto got_subtype;

	/* Falling to here means unknown subtype. */
	printf("<unknown subtype>");
	return;

 got_subtype:
	if (print_toptype)
		putchar(' ');

	printf("%s", desc->ifmt_string);

	if (print_toptype) {
		desc = get_mode_desc(ifmw, ttos);
		if (desc != NULL && strcasecmp("autoselect", desc->ifmt_string))
			printf(" mode %s", desc->ifmt_string);
	}

	/* Find options. */
	for (i = 0; ttos->options[i].desc != NULL; i++) {
		if (ttos->options[i].alias)
			continue;
		for (desc = ttos->options[i].desc;
		    desc->ifmt_string != NULL; desc++) {
			if (ifmw & desc->ifmt_word) {
				if (seen_option == 0)
					printf(" <");
				printf("%s%s", seen_option++ ? "," : "",
				    desc->ifmt_string);
			}
		}
	}
	printf("%s", seen_option ? ">" : "");
}

static void
print_media_word_ifconfig(int ifmw)
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	int i;

	/* Find the top-level interface type. */
	desc = get_toptype_desc(ifmw);
	ttos = get_toptype_ttos(ifmw);
	if (desc->ifmt_string == NULL) {
		printf("<unknown type>");
		return;
	}

	/*
	 * Don't print the top-level type; it's not like we can
	 * change it, or anything.
	 */

	/* Find subtype. */
	desc = get_subtype_desc(ifmw, ttos);
	if (desc != NULL)
		goto got_subtype;

	/* Falling to here means unknown subtype. */
	printf("<unknown subtype>");
	return;

 got_subtype:
	printf("media %s", desc->ifmt_string);

	desc = get_mode_desc(ifmw, ttos);
	if (desc != NULL)
		printf(" mode %s", desc->ifmt_string);

	/* Find options. */
	for (i = 0; ttos->options[i].desc != NULL; i++) {
		if (ttos->options[i].alias)
			continue;
		for (desc = ttos->options[i].desc;
		    desc->ifmt_string != NULL; desc++) {
			if (ifmw & desc->ifmt_word) {
				printf(" mediaopt %s", desc->ifmt_string);
			}
		}
	}
}

/**********************************************************************
 * ...until here.
 **********************************************************************/
