/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 * $FreeBSD: src/sys/boot/i386/boot2/boot2.c,v 1.64 2003/08/25 23:28:31 obrien Exp $
 * $DragonFly: src/sys/boot/i386/boot2/Attic/boot2.c,v 1.13 2004/07/27 19:37:17 dillon Exp $
 */
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/diskmbr.h>
#include <sys/dirent.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>

#include <stdarg.h>

#include <a.out.h>

#include <btxv86.h>

#include "boot2.h"
#include "lib.h"
#include "../bootasm.h"

#define SECOND		18	/* Circa that many ticks in a second. */

#define RBX_ASKNAME	0x0	/* -a */
#define RBX_SINGLE	0x1	/* -s */
#define RBX_DFLTROOT	0x5	/* -r */
#define RBX_KDB 	0x6	/* -d */
#define RBX_CONFIG	0xa	/* -c */
#define RBX_VERBOSE	0xb	/* -v */
#define RBX_SERIAL	0xc	/* -h */
#define RBX_CDROM	0xd	/* -C */
#define RBX_GDB 	0xf	/* -g */
#define RBX_MUTE	0x10	/* -m */
#define RBX_PAUSE	0x12	/* -p */
#define RBX_NOINTR	0x1c	/* -n */
#define RBX_VIDEO	0x1d	/* -V */
#define RBX_PROBEKBD	0x1e	/* -P */
/* 0x1f is reserved for the historical RB_BOOTINFO option */

#define RBF_MUTE	(1 << RBX_MUTE)
#define RBF_SERIAL	(1 << RBX_SERIAL)
#define RBF_VIDEO	(1 << RBX_VIDEO)

/* pass: -a, -s, -r, -d, -c, -v, -h, -C, -g, -m, -p, -V */
#define RBX_MASK	0x2005ffff

#define PATH_CONFIG	"/boot.config"
#define PATH_BOOT3	"/boot/loader"
#define PATH_KERNEL	"/kernel"

#define NDEV		3
#define MEM_BASE	0x12
#define MEM_EXT 	0x15
#define V86_CY(x)	((x) & 1)
#define V86_ZR(x)	((x) & 0x40)

#define DRV_HARD	0x80
#define DRV_MASK	0x7f

#define TYPE_AD		0
#define TYPE_DA		1
#define TYPE_MAXHARD	TYPE_DA
#define TYPE_FD		2

#define NOPT		12

#define INVALID_S	"Bad %s\n"

extern uint32_t _end;

static const char optstr[NOPT] = { "VhaCgmnPprsv" };
static const unsigned char flags[NOPT] = {
    RBX_VIDEO,
    RBX_SERIAL,
    RBX_ASKNAME,
    RBX_CDROM,
    RBX_GDB,
    RBX_MUTE,
    RBX_NOINTR,
    RBX_PROBEKBD,
    RBX_PAUSE,
    RBX_DFLTROOT,
    RBX_SINGLE,
    RBX_VERBOSE
};

static const char *const dev_nm[NDEV] = {"ad", "da", "fd"};
static const unsigned char dev_maj[NDEV] = {30, 4, 2};

static struct dsk {
    unsigned drive;
    unsigned type;
    unsigned unit;
    unsigned slice;
    unsigned part;
    unsigned start;
    int init;
} dsk;
static char cmd[512];
static char kname[1024];
static uint32_t opts;
static struct bootinfo bootinfo;

void exit(int);
static void load(void);
static int parse(void);
static int xfsread(ino_t, void *, size_t);
static int dskread(void *, unsigned, unsigned);
static void printf(const char *,...);
static void putchar(int);
static uint32_t memsize(void);
static int drvread(void *, unsigned, unsigned);
static int keyhit(unsigned);
static void xputc(int);
static int xgetc(int);
static int getc(int);

static void 
memcpy(void *d, const void *s, int len)
{
    char *dd = d;
    const char *ss = s;

#if 0
    if (dd < ss) {
	while (--len >= 0)
	    *dd++ = *ss++;
    } else {
#endif
	while (--len >= 0)
	    dd[len] = ss[len];
#if 0
    }
#endif
}

static inline int
strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2 && *s1; s1++, s2++);
    return (unsigned char)*s1 - (unsigned char)*s2;
}

#include "ufsread.c"

static int
xfsread(ino_t inode, void *buf, size_t nbyte)
{
    if ((size_t)fsread(inode, buf, nbyte) != nbyte) {
	printf(INVALID_S, "format");
	return -1;
    }
    return 0;
}

static inline uint32_t
memsize(void)
{
    v86.addr = MEM_EXT;
    v86.eax = 0x8800;
    v86int();
    return v86.eax;
}

static inline void
getstr(void)
{
    char *s;
    int c;

    s = cmd;
    for (;;) {
	switch (c = xgetc(0)) {
	case 0:
	    break;
	case '\177':
	case '\b':
	    if (s > cmd) {
		s--;
		printf("\b \b");
	    }
	    break;
	case '\n':
	case '\r':
	    *s = 0;
	    return;
	default:
	    if (s - cmd < sizeof(cmd) - 1)
		*s++ = c;
	    putchar(c);
	}
    }
}

static inline void
putc(int c)
{
    v86.addr = 0x10;
    v86.eax = 0xe00 | (c & 0xff);
    v86.ebx = 0x7;
    v86int();
}

int
main(void)
{
    int autoboot;
    ino_t ino;

    dmadat = (void *)(roundup2(__base + (int32_t)&_end, 0x10000) - __base);
    v86.ctl = V86_FLAGS;
    dsk.drive = *(uint8_t *)PTOV(MEM_BTX_USR_ARG);
    dsk.type = dsk.drive & DRV_HARD ? TYPE_AD : TYPE_FD;
    dsk.unit = dsk.drive & DRV_MASK;
    dsk.slice = *(uint8_t *)PTOV(MEM_BTX_USR_ARG + 1) + 1;
    bootinfo.bi_version = BOOTINFO_VERSION;
    bootinfo.bi_size = sizeof(bootinfo);
    bootinfo.bi_basemem = 0;	/* XXX will be filled by loader or kernel */
    bootinfo.bi_extmem = memsize();
    bootinfo.bi_memsizes_valid++;

    /* Process configuration file */

    autoboot = 1;

    if ((ino = lookup(PATH_CONFIG)))
	fsread(ino, cmd, sizeof(cmd));

    if (cmd[0]) {
	printf("%s: %s", PATH_CONFIG, cmd);
	if (parse())
	    autoboot = 0;
	/* Do not process this command twice */
	*cmd = 0;
    }

    /*
     * Setup our (serial) console after processing the config file.  If
     * the initialization fails, don't try to use the serial port.  This
     * can happen if the serial port is unmaped (happens on new laptops a lot).
     */
    if ((opts & (RBF_MUTE|RBF_SERIAL|RBF_VIDEO)) == 0)
	opts |= RBF_SERIAL|RBF_VIDEO;
    if (opts & RBF_SERIAL) {
	if (sio_init())
	    opts = RBF_VIDEO;
    }


    /*
     * Try to exec stage 3 boot loader. If interrupted by a keypress,
     * or in case of failure, try to load a kernel directly instead.
     */

    if (autoboot && !*kname) {
	memcpy(kname, PATH_BOOT3, sizeof(PATH_BOOT3));
	if (!keyhit(3*SECOND)) {
	    load();
	    memcpy(kname, PATH_KERNEL, sizeof(PATH_KERNEL));
	}
    }

    /* Present the user with the boot2 prompt. */

    for (;;) {
	printf("\nDragonFly boot\n"
	       "%u:%s(%u,%c)%s: ",
	       dsk.drive & DRV_MASK, dev_nm[dsk.type], dsk.unit,
	       'a' + dsk.part, kname);
	if (!autoboot || keyhit(5*SECOND))
	    getstr();
	else
	    putchar('\n');
	autoboot = 0;
	if (cmd[0] == 0 || parse())
	    putchar('\a');
	else
	    load();
    }
}

/* XXX - Needed for btxld to link the boot2 binary; do not remove. */
void
exit(int x)
{
}

static void
load(void)
{
    union {
	struct exec ex;
	Elf32_Ehdr eh;
    } hdr;
    Elf32_Phdr ep[2];
    Elf32_Shdr es[2];
    caddr_t p;
    ino_t ino;
    uint32_t addr, x;
    int fmt, i, j;

    if (!(ino = lookup(kname))) {
	if (!ls)
	    printf("No %s\n", kname);
	return;
    }
    if (xfsread(ino, &hdr, sizeof(hdr)))
	return;
    if (N_GETMAGIC(hdr.ex) == ZMAGIC)
	fmt = 0;
    else if (IS_ELF(hdr.eh))
	fmt = 1;
    else {
	printf(INVALID_S, "format");
	return;
    }
    if (fmt == 0) {
	addr = hdr.ex.a_entry & 0xffffff;
	p = PTOV(addr);
	fs_off = PAGE_SIZE;
	if (xfsread(ino, p, hdr.ex.a_text))
	    return;
	p += roundup2(hdr.ex.a_text, PAGE_SIZE);
	if (xfsread(ino, p, hdr.ex.a_data))
	    return;
	p += hdr.ex.a_data + roundup2(hdr.ex.a_bss, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	memcpy(p, &hdr.ex.a_syms, sizeof(hdr.ex.a_syms));
	p += sizeof(hdr.ex.a_syms);
	if (hdr.ex.a_syms) {
	    if (xfsread(ino, p, hdr.ex.a_syms))
		return;
	    p += hdr.ex.a_syms;
	    if (xfsread(ino, p, sizeof(int)))
		return;
	    x = *(uint32_t *)p;
	    p += sizeof(int);
	    x -= sizeof(int);
	    if (xfsread(ino, p, x))
		return;
	    p += x;
	}
    } else {
	fs_off = hdr.eh.e_phoff;
	for (j = i = 0; i < hdr.eh.e_phnum && j < 2; i++) {
	    if (xfsread(ino, ep + j, sizeof(ep[0])))
		return;
	    if (ep[j].p_type == PT_LOAD)
		j++;
	}
	for (i = 0; i < 2; i++) {
	    p = PTOV(ep[i].p_paddr & 0xffffff);
	    fs_off = ep[i].p_offset;
	    if (xfsread(ino, p, ep[i].p_filesz))
		return;
	}
	p += roundup2(ep[1].p_memsz, PAGE_SIZE);
	bootinfo.bi_symtab = VTOP(p);
	if (hdr.eh.e_shnum == hdr.eh.e_shstrndx + 3) {
	    fs_off = hdr.eh.e_shoff + sizeof(es[0]) *
		(hdr.eh.e_shstrndx + 1);
	    if (xfsread(ino, &es, sizeof(es)))
		return;
	    for (i = 0; i < 2; i++) {
		memcpy(p, &es[i].sh_size, sizeof(es[i].sh_size));
		p += sizeof(es[i].sh_size);
		fs_off = es[i].sh_offset;
		if (xfsread(ino, p, es[i].sh_size))
		    return;
		p += es[i].sh_size;
	    }
	}
	addr = hdr.eh.e_entry & 0xffffff;
    }
    bootinfo.bi_esymtab = VTOP(p);
    bootinfo.bi_kernelname = VTOP(kname);
    bootinfo.bi_bios_dev = dsk.drive;
    __exec((caddr_t)addr, opts & RBX_MASK,
	   MAKEBOOTDEV(dev_maj[dsk.type], 0, dsk.slice, dsk.unit, dsk.part),
	   0, 0, 0, VTOP(&bootinfo));
}

static int
parse()
{
    char *arg = cmd;
    char *p, *q;
    unsigned int drv;
    int c, i;

    while ((c = *arg++)) {
	if (c == ' ' || c == '\t' || c == '\n')
	    continue;
	for (p = arg; *p && *p != '\n' && *p != ' ' && *p != '\t'; p++)
	    ;
	if (*p)
	    *p++ = 0;
	if (c == '-') {
	    while ((c = *arg++)) {
		for (i = NOPT - 1; i >= 0; --i) {
		    if (optstr[i] == c) {
			opts ^= 1 << flags[i];
			goto ok;
		    }
		}
		return(-1);
		ok: ;	/* ugly but save space */
	    }
	    if (opts & (1 << RBX_PROBEKBD)) {
		i = *(uint8_t *)PTOV(0x496) & 0x10;
		if (!i) {
		    printf("NO KB\n");
		    opts |= RBF_VIDEO | RBF_SERIAL;
		}
		opts &= ~(1 << RBX_PROBEKBD);
	    }
	} else {
	    for (q = arg--; *q && *q != '('; q++);
	    if (*q) {
		drv = -1;
		if (arg[1] == ':') {
		    drv = *arg - '0';
		    if (drv > 9)
			return (-1);
		    arg += 2;
		}
		if (q - arg != 2)
		    return -1;
		for (i = 0; arg[0] != dev_nm[i][0] ||
			    arg[1] != dev_nm[i][1]; i++)
		    if (i == NDEV - 1)
			return -1;
		dsk.type = i;
		arg += 3;
		dsk.unit = *arg - '0';
		if (arg[1] != ',' || dsk.unit > 9)
		    return -1;
		arg += 2;
		dsk.slice = WHOLE_DISK_SLICE;
		if (arg[1] == ',') {
		    dsk.slice = *arg - '0' + 1;
		    if (dsk.slice > NDOSPART)
			return -1;
		    arg += 2;
		}
		if (arg[1] != ')')
		    return -1;
		dsk.part = *arg - 'a';
		if (dsk.part > 7)
		    return (-1);
		arg += 2;
		if (drv == -1)
		    drv = dsk.unit;
		dsk.drive = (dsk.type <= TYPE_MAXHARD
			     ? DRV_HARD : 0) + drv;
		dsk_meta = 0;
	    }
	    if ((i = p - arg - !*(p - 1))) {
		if ((size_t)i >= sizeof(kname))
		    return -1;
		memcpy(kname, arg, i + 1);
	    }
	}
	arg = p;
    }
    return 0;
}

static int
dskread(void *buf, unsigned lba, unsigned nblk)
{
    struct dos_partition *dp;
    struct disklabel *d;
    char *sec;
    unsigned sl, i;

    if (!dsk_meta) {
	sec = dmadat->secbuf;
	dsk.start = 0;
	if (drvread(sec, DOSBBSECTOR, 1))
	    return -1;
	dp = (void *)(sec + DOSPARTOFF);
	sl = dsk.slice;
	if (sl < BASE_SLICE) {
	    for (i = 0; i < NDOSPART; i++)
		if (dp[i].dp_typ == DOSPTYP_386BSD &&
		    (dp[i].dp_flag & 0x80 || sl < BASE_SLICE)) {
		    sl = BASE_SLICE + i;
		    if (dp[i].dp_flag & 0x80 ||
			dsk.slice == COMPATIBILITY_SLICE)
			break;
		}
	    if (dsk.slice == WHOLE_DISK_SLICE)
		dsk.slice = sl;
	}
	if (sl != WHOLE_DISK_SLICE) {
	    if (sl != COMPATIBILITY_SLICE)
		dp += sl - BASE_SLICE;
	    if (dp->dp_typ != DOSPTYP_386BSD) {
		printf(INVALID_S, "slice");
		return -1;
	    }
	    dsk.start = dp->dp_start;
	}
	if (drvread(sec, dsk.start + LABELSECTOR, 1))
		return -1;
	d = (void *)(sec + LABELOFFSET);
	if (d->d_magic != DISKMAGIC || d->d_magic2 != DISKMAGIC) {
	    if (dsk.part != RAW_PART) {
		printf(INVALID_S, "label");
		return -1;
	    }
	} else {
	    if (!dsk.init) {
		if (d->d_type == DTYPE_SCSI)
		    dsk.type = TYPE_DA;
		dsk.init++;
	    }
	    if (dsk.part >= d->d_npartitions ||
		!d->d_partitions[dsk.part].p_size) {
		printf(INVALID_S, "partition");
		return -1;
	    }
	    dsk.start += d->d_partitions[dsk.part].p_offset;
	    dsk.start -= d->d_partitions[RAW_PART].p_offset;
	}
    }
    return drvread(buf, dsk.start + lba, nblk);
}

static void
printf(const char *fmt,...)
{
    va_list ap;
    char buf[10];
    char *s;
    unsigned u;
    int c;

    va_start(ap, fmt);
    while ((c = *fmt++)) {
	if (c == '%') {
	    c = *fmt++;
	    switch (c) {
	    case 'c':
		putchar(va_arg(ap, int));
		continue;
	    case 's':
		for (s = va_arg(ap, char *); *s; s++)
		    putchar(*s);
		continue;
	    case 'u':
		u = va_arg(ap, unsigned);
		s = buf;
		do
		    *s++ = '0' + u % 10U;
		while (u /= 10U);
		while (--s >= buf)
		    putchar(*s);
		continue;
	    }
	}
	putchar(c);
    }
    va_end(ap);
    return;
}

static void
putchar(int c)
{
    if (c == '\n')
	xputc('\r');
    xputc(c);
}

static int
drvread(void *buf, unsigned lba, unsigned nblk)
{
    static unsigned c = 0x2d5c7c2f;	/* twiddle */

    c = (c << 8) | (c >> 24);
    xputc(c);
    xputc('\b');
    v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
    v86.addr = XREADORG;		/* call to xread in boot1 */
    v86.es = VTOPSEG(buf);
    v86.eax = lba;
    v86.ebx = VTOPOFF(buf);
    v86.ecx = lba >> 16;
    v86.edx = nblk << 8 | dsk.drive;
    v86int();
    v86.ctl = V86_FLAGS;
    if (V86_CY(v86.efl)) {
	printf("error %u lba %u\n", v86.eax >> 8 & 0xff, lba);
	return -1;
    }
    return 0;
}

static int
keyhit(unsigned ticks)
{
    uint32_t t0, t1;

    if (opts & 1 << RBX_NOINTR)
	return 0;
    t0 = 0;
    for (;;) {
	if (xgetc(1))
	    return 1;
	t1 = *(uint32_t *)PTOV(0x46c);
	if (!t0)
	    t0 = t1;
	if (t1 < t0 || t1 >= t0 + ticks)
	    return 0;
    }
}

static void
xputc(int c)
{
    if (opts & RBF_VIDEO)
	putc(c);
    if (opts & RBF_SERIAL)
	sio_putc(c);
}

static int
xgetc(int fn)
{
    if (opts & 1 << RBX_NOINTR)
	return 0;
    for (;;) {
	if ((opts & RBF_VIDEO) && getc(1))
	    return fn ? 1 : getc(0);
	if ((opts & RBF_SERIAL) && sio_ischar())
	    return fn ? 1 : sio_getc();
	if (fn)
	    return 0;
    }
}

static int
getc(int fn)
{
    v86.addr = 0x16;
    v86.eax = fn << 8;
    v86int();
    return fn == 0 ? v86.eax & 0xff : !V86_ZR(v86.efl);
}
