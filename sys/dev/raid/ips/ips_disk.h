/*-
 * Copyright (c) 2002 Adaptec Inc.
 * All rights reserved.
 *
 * Written by: David Jeffery
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
 * $FreeBSD: src/sys/dev/ips/ips_disk.h,v 1.2 2003/08/22 06:00:27 imp Exp $
 * $DragonFly: src/sys/dev/raid/ips/ips_disk.h,v 1.2 2004/10/06 02:12:31 y0netan1 Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/buf.h>
#include <sys/disk.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

#define IPS_MAX_IO_SIZE		0x10000

#define IPS_COMP_HEADS       	128
#define IPS_COMP_SECTORS     	32
#define IPS_NORM_HEADS       	254
#define IPS_NORM_SECTORS     	63

typedef struct ipsdisk_softc {
	device_t 	dev;
	int		unit;
	int		disk_number;
	u_int32_t 	state;
	struct disk 	ipsd_disk;
	dev_t		ipsd_dev_t;
	ips_softc_t	*sc;
	struct devstat	stats;
} ipsdisk_softc_t;
