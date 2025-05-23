/*
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/pccard/pccardc/rdmap.c,v 1.12 1999/08/28 01:17:33 peter Exp $
 * $DragonFly: src/usr.sbin/pccard/pccardc/Attic/rdmap.c,v 1.3 2003/08/08 04:18:46 dillon Exp $
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <bus/pccard/cardinfo.h>
#include <bus/pccard/cis.h>

static void
dump_io(fd, nio)
	int     fd, nio;
{
	struct io_desc io;
	int     i;

	for (i = 0; i < nio; i++) {
		io.window = i;
		if (ioctl(fd, PIOCGIO, &io))
			err(1, "ioctl (PIOCGIO)");
		printf("I/O %d: flags 0x%03x port 0x%3x size %d bytes\n",
		    io.window, io.flags, io.start, io.size);
	}
}

static void
dump_mem(fd, nmem)
	int     fd, nmem;
{
	struct mem_desc mem;
	int     i;

	for (i = 0; i < nmem; i++) {
		mem.window = i;
		if (ioctl(fd, PIOCGMEM, &mem))
			err(1, "ioctl (PIOCGMEM)");
		printf("Mem %d: flags 0x%03x host %p card %04lx size %d bytes\n",
		    mem.window, mem.flags, mem.start, mem.card, mem.size);
	}
}

static void
scan(slot)
	int     slot;
{
	int     fd;
	char    name[64];
	struct slotstate st;

	sprintf(name, CARD_DEVICE, slot);
	fd = open(name, O_RDONLY);
	if (fd < 0)
		return;
	if (ioctl(fd, PIOCGSTATE, &st))
		err(1, "ioctl (PIOCGSTATE)");
/*
	if (st.state == filled)
 */
	{
		dump_mem(fd, st.maxmem);
		dump_io(fd, st.maxio);
	}
	close(fd);
}

int
rdmap_main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     node;

	for (node = 0; node < 8; node++)
		scan(node);
	exit(0);
}
