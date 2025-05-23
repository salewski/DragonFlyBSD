/* Created by ./makestatetext on Wed Jan 5 10:05:30 CST 2000. Do not edit */

/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $FreeBSD: src/sys/dev/vinum/statetexts.h,v 1.9 2000/02/29 06:07:01 grog Exp $
 * $DragonFly: src/sys/dev/raid/vinum/statetexts.h,v 1.2 2003/06/17 04:28:33 dillon Exp $
 */

/* Drive state texts */
char *drivestatetext[] =
{
    "unallocated",
    "referenced",
    "down",
    "up",
};

/* Subdisk state texts */
char *sdstatetext[] =
{
    "unallocated",
    "uninit",
    "referenced",
    "init",
    "empty",
    "initializing",
    "initialized",
    "obsolete",
    "stale",
    "crashed",
    "down",
    "reviving",
    "reborn",
    "up",
};

/* Plex state texts */
char *plexstatetext[] =
{
    "unallocated",
    "referenced",
    "init",
    "faulty",
    "down",
    "initializing",
    "corrupt",
    "degraded",
    "flaky",
    "up",
};

/* Volume state texts */
char *volstatetext[] =
{
    "unallocated",
    "uninit",
    "down",
    "up",
};
