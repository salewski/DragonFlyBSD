/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 * $FreeBSD: src/sys/boot/i386/libi386/bootinfo.c,v 1.35 2003/08/25 23:28:31 obrien Exp $
 * $DragonFly: src/sys/boot/i386/libi386/Attic/bootinfo.c,v 1.4 2004/06/25 05:37:58 dillon Exp $
 */

#include <stand.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

/*
 * Return a 'boothowto' value corresponding to the kernel arguments in
 * (kargs) and any relevant environment variables.
 */
static struct 
{
    const char	*ev;
    int		mask;
} howto_names[] = {
    {"boot_askname",	RB_ASKNAME},
    {"boot_cdrom",	RB_CDROM},
    {"boot_userconfig",	RB_CONFIG},
    {"boot_ddb",	RB_KDB},
    {"boot_gdb",	RB_GDB},
    {"boot_single",	RB_SINGLE},
    {"boot_verbose",	RB_VERBOSE},
    {"boot_vidcons",	RB_VIDEO},
    {"boot_serial",	RB_SERIAL},
    {NULL,	0}
};

int
bi_getboothowto(char *kargs)
{
    char	*cp;
    int		howto;
    int		active;
    int		i;
    
    /* Parse kargs */
    howto = 0;
    if (kargs  != NULL) {
	cp = kargs;
	active = 0;
	while (*cp != 0) {
	    if (!active && (*cp == '-')) {
		active = 1;
	    } else if (active)
		switch (*cp) {
		case 'a':
		    howto |= RB_ASKNAME;
		    break;
		case 'c':
		    howto |= RB_CONFIG;
		    break;
		case 'C':
		    howto |= RB_CDROM;
		    break;
		case 'd':
		    howto |= RB_KDB;
		    break;
		case 'D':
		    /* all available consoles become active */
		    howto &= ~(RB_MUTE|RB_VIDEO|RB_SERIAL);
		    break;
		case 'V':
		    howto |= RB_VIDEO;
		    break;
		case 'm':
		    howto |= RB_MUTE;
		    break;
		case 'g':
		    howto |= RB_GDB;
		    break;
		case 'h':
		    howto |= RB_SERIAL;
		    break;
		case 'p':
		    howto |= RB_PAUSE;
		    break;
		case 'r':
		    howto |= RB_DFLTROOT;
		    break;
		case 's':
		    howto |= RB_SINGLE;
		    break;
		case 'v':
		    howto |= RB_VERBOSE;
		    break;
		default:
		    active = 0;
		    break;
		}
	    cp++;
	}
    }
    /* get equivalents from the environment */
    for (i = 0; howto_names[i].ev != NULL; i++)
	if (getenv(howto_names[i].ev) != NULL)
	    howto |= howto_names[i].mask;
    if (!strcmp(getenv("console"), "comconsole"))
	howto |= RB_SERIAL;
    if (!strcmp(getenv("console"), "nullconsole"))
	howto |= RB_MUTE;
    if (!strcmp(getenv("console"), "vidconsole"))
	howto |= RB_VIDEO;
    return(howto);
}

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
vm_offset_t
bi_copyenv(vm_offset_t addr)
{
    struct env_var	*ep;
    
    /* traverse the environment */
    for (ep = environ; ep != NULL; ep = ep->ev_next) {
	i386_copyin(ep->ev_name, addr, strlen(ep->ev_name));
	addr += strlen(ep->ev_name);
	i386_copyin("=", addr, 1);
	addr++;
	if (ep->ev_value != NULL) {
	    i386_copyin(ep->ev_value, addr, strlen(ep->ev_value));
	    addr += strlen(ep->ev_value);
	}
	i386_copyin("", addr, 1);
	addr++;
    }
    i386_copyin("", addr, 1);
    addr++;
    return(addr);
}
