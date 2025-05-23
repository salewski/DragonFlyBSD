/*
 * Copyright (c) 1998 Kenneth D. Merry.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/systat/devs.c,v 1.4 1999/08/28 01:05:59 peter Exp $
 * $DragonFly: src/usr.bin/systat/devs.c,v 1.6 2004/12/22 11:01:49 joerg Exp $
 */
/*
 * Some code and ideas taken from the old disks.c.
 * static char sccsid[] = "@(#)disks.c	8.1 (Berkeley) 6/6/93";
 */
/*-
 * Copyright (c) 1980, 1992, 1993
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

#include <sys/types.h>
#include <sys/devicestat.h>

#include <string.h>
#include <devstat.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include "systat.h"
#include "extern.h"
#include "devs.h"

typedef enum {
	DS_MATCHTYPE_NONE,
	DS_MATCHTYPE_SPEC,
	DS_MATCHTYPE_PATTERN
} last_match_type;

last_match_type last_type;
struct device_selection *dev_select;
long generation;
int num_devices, num_selected;
int num_selections;
long select_generation;
struct devstat_match *matches = NULL;
int num_matches = 0;
char **specified_devices;
int num_devices_specified = 0;

static int dsmatchselect(char *args, devstat_select_mode select_mode,
			 int maxshowdevs, struct statinfo *s1);
static int dsselect(char *args, devstat_select_mode select_mode,
		    int maxshowdevs, struct statinfo *s1);

int
dsinit(int maxshowdevs, struct statinfo *s1, struct statinfo *s2,
       struct statinfo *s3)
{

	/*
	 * Make sure that the userland devstat version matches the kernel
	 * devstat version.  If not, exit and print a message informing 
	 * the user of his mistake.
	 */
	if (checkversion() < 0)
		errx(1, "%s", devstat_errbuf);

	generation = 0;
	num_devices = 0;
	num_selected = 0;
	num_selections = 0;
	select_generation = 0;
	last_type = DS_MATCHTYPE_NONE;

	if (getdevs(s1) == -1)
		errx(1, "%s", devstat_errbuf);

	num_devices = s1->dinfo->numdevs;
	generation = s1->dinfo->generation;

	dev_select = NULL;

	/*
	 * At this point, selectdevs will almost surely indicate that the
	 * device list has changed, so we don't look for return values of 0
	 * or 1.  If we get back -1, though, there is an error.
	 */
	if (selectdevs(&dev_select, &num_selected, &num_selections,
		       &select_generation, generation, s1->dinfo->devices,
		       num_devices, NULL, 0, NULL, 0, DS_SELECT_ADD,
		       maxshowdevs, 0) == -1)
		errx(1, "%s", devstat_errbuf);

	return(1);
}

int
dscmd(char *cmd, char *args, int maxshowdevs, struct statinfo *s1)
{
	int retval;

	if (prefix(cmd, "display") || prefix(cmd, "add"))
		return(dsselect(args, DS_SELECT_ADDONLY, maxshowdevs, s1));
	if (prefix(cmd, "ignore") || prefix(cmd, "delete"))
		return(dsselect(args, DS_SELECT_REMOVE, maxshowdevs, s1));
	if (prefix(cmd, "show") || prefix(cmd, "only"))
		return(dsselect(args, DS_SELECT_ONLY, maxshowdevs, s1));
	if (prefix(cmd, "type") || prefix(cmd, "match"))
		return(dsmatchselect(args, DS_SELECT_ONLY, maxshowdevs, s1));
	if (prefix(cmd, "refresh")) {
		retval = selectdevs(&dev_select, &num_selected, &num_selections,
				    &select_generation, generation,
				    s1->dinfo->devices, num_devices, 
				    (last_type == DS_MATCHTYPE_PATTERN) ?
					matches : NULL,
				    (last_type == DS_MATCHTYPE_PATTERN) ?
					num_matches : 0,
				    (last_type == DS_MATCHTYPE_SPEC) ?
					specified_devices : NULL, 
				    (last_type == DS_MATCHTYPE_SPEC) ?
					num_devices_specified : 0,
				    (last_type == DS_MATCHTYPE_NONE) ?
					DS_SELECT_ADD : DS_SELECT_ADDONLY,
				    maxshowdevs, 0);
		if (retval == -1) {
			warnx("%s", devstat_errbuf);
			return(0);
		} else if (retval == 1)
			return(2);
	}
	if (prefix(cmd, "drives")) {
		register int i;
		move(CMDLINE, 0);
		clrtoeol();
		for (i = 0; i < num_devices; i++) {
			printw("%s%d ", s1->dinfo->devices[i].device_name,
			       s1->dinfo->devices[i].unit_number);
		}
		return(1);
	}
	return(0);
}

static int
dsmatchselect(char *args, devstat_select_mode select_mode, int maxshowdevs,
              struct statinfo *s1)
{
	char **tempstr;
	char *tstr[100];
	int num_args = 0;
	register int i;
	int retval = 0;

	/*
	 * Break the (pipe delimited) input string out into separate
	 * strings.
	 */
	for (tempstr = tstr, num_args  = 0;
	     (*tempstr = strsep(&args, "|")) != NULL && (num_args < 100);
	     num_args++)
		if (**tempstr != '\0')
			if (++tempstr >= &tstr[100])
				break;

	if (num_args > 99) {
		warnx("dsmatchselect: too many match arguments");
		return(0);
	}

	/*
	 * If we've gone through the matching code before, clean out
	 * previously used memory.
	 */
	if (num_matches > 0) {
		free(matches);
		matches = NULL;
		num_matches = 0;
	}

	for (i = 0; i < num_args; i++) {
		if (buildmatch(tstr[i], &matches, &num_matches) != 0) {	
			warnx("%s", devstat_errbuf);
			return(0);
		}
	}
	if (num_args > 0) {

		last_type = DS_MATCHTYPE_PATTERN;

		retval = selectdevs(&dev_select, &num_selected, &num_selections,
				    &select_generation, generation,
				    s1->dinfo->devices, num_devices, matches, 
				    num_matches, NULL, 0, select_mode,
				    maxshowdevs, 0);
		if (retval == -1)
			err(1, "device selection error");
		else if (retval == 1)
			return(2);
	}
	return(1);
}

static int
dsselect(char *args, devstat_select_mode select_mode, int maxshowdevs,
	 struct statinfo *s1)
{
	register char *cp;
	register int i;
	int retval = 0;

	/*
	 * If we've gone through this code before, free previously
	 * allocated resources.
	 */
	if (num_devices_specified > 0) {
		for (i = 0; i < num_devices_specified; i++)
			free(specified_devices[i]);
		free(specified_devices);
		specified_devices = NULL;
		num_devices_specified = 0;
	}

	/* do an initial malloc */
	specified_devices = (char **)malloc(sizeof(char *));

	cp = strchr(args, '\n');
	if (cp)
		*cp = '\0';
	for (;;) {
		for (cp = args; *cp && isspace(*cp); cp++)
			;
		args = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - args == 0)
			break;
		for (i = 0; i < num_devices; i++) {
			char tmpstr[80];

			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			if (strcmp(args, tmpstr) == 0) {
				
				num_devices_specified++;

				specified_devices =(char **)realloc(
						specified_devices,
						sizeof(char *) *
						num_devices_specified);
				specified_devices[num_devices_specified -1]=
					strdup(args);

				break;
			}
		}
		if (i >= num_devices)
			error("%s: unknown drive", args);
		args = cp;
	}

	if (num_devices_specified > 0) {
		last_type = DS_MATCHTYPE_SPEC;

		retval = selectdevs(&dev_select, &num_selected, &num_selections,
				    &select_generation, generation,
				    s1->dinfo->devices, num_devices, NULL, 0,
				    specified_devices, num_devices_specified,
				    select_mode, maxshowdevs, 0);
		if (retval == -1)
			err(1, "%s", devstat_errbuf);
		else if (retval == 1)
			return(2);
	}
	return(1);
}
