/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/lib/libdisk/disklabel.c,v 1.5.2.3 2001/05/13 21:01:38 jkh Exp $
 * $DragonFly: src/lib/libdisk/Attic/disklabel.c,v 1.2 2003/06/17 04:26:49 dillon Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/disklabel.h>
#include "libdisk.h"

struct disklabel *
read_disklabel(int fd, daddr_t block, u_long sector_size)
{
	struct disklabel *dp;

	dp = (struct disklabel *) read_block(fd, block, sector_size);
	if (dp->d_magic != DISKMAGIC)
		return 0;
	if (dp->d_magic2 != DISKMAGIC)
		return 0;
	if (dkcksum(dp) != 0)
		return 0;
	return dp;
}
