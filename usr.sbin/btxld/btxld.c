/*
 * Copyright (c) 1998 Robert Nordier
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/btxld/btxld.c,v 1.4 2000/01/04 14:10:36 marcel Exp $
 * $DragonFly: src/usr.sbin/btxld/btxld.c,v 1.3 2004/08/19 21:38:30 joerg Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <a.out.h>

#include "btx.h"
#include "elfh.h"

#define BTX_PATH		"/sys/boot/i386/btx"

#define I_LDR	0		/* BTX loader */
#define I_BTX	1		/* BTX kernel */
#define I_CLNT	2		/* Client program */

#define F_BIN	0		/* Binary */
#define F_AOUT	1		/* ZMAGIC a.out */
#define F_ELF	2		/* 32-bit ELF */
#define F_CNT	3		/* Number of formats */

#define IMPURE	1		/* Writable text */
#define MAXU32	0xffffffff	/* Maximum unsigned 32-bit quantity */

#define align(x, y) (((x) + (y) - 1) & ~((y) - 1))

struct hdr {
    uint32_t fmt;		/* Format */
    uint32_t flags;		/* Bit flags */
    uint32_t size;		/* Size of file */
    uint32_t text;		/* Size of text segment */
    uint32_t data;		/* Size of data segment */
    uint32_t bss;		/* Size of bss segment */
    uint32_t org;		/* Program origin */
    uint32_t entry;		/* Program entry point */
};

static const char *const fmtlist[] = {"bin", "aout", "elf"};

static const char binfo[] =
    "kernel: ver=%u.%02u size=%x load=%x entry=%x map=%uM "
    "pgctl=%x:%x\n";
static const char cinfo[] =
    "client: fmt=%s size=%x text=%x data=%x bss=%x entry=%x\n";
static const char oinfo[] =
    "output: fmt=%s size=%x text=%x data=%x org=%x entry=%x\n";

static const char *lname =
    BTX_PATH "/btxldr/btxldr";	/* BTX loader */
static const char *bname =
    BTX_PATH "/btx/btx";	/* BTX kernel */
static const char *oname =
    "a.out";			/* Output filename */

static int ppage = -1;		/* First page present */
static int wpage = -1;		/* First page writable */

static unsigned int format; 	/* Output format */

static uint32_t centry; 	/* Client entry address */
static uint32_t lentry; 	/* Loader entry address */

static int Eflag;		/* Client entry option */

static int quiet;		/* Inhibit warnings */
static int verbose;		/* Display information */

static const char *tname;	/* Temporary output file */
static const char *fname;	/* Current input file */

static void cleanup(void);
static void btxld(const char *);
static void getbtx(int, struct btx_hdr *);
static void gethdr(int, struct hdr *);
static void puthdr(int, struct hdr *);
static void copy(int, int, size_t, off_t);
static size_t readx(int, void *, size_t, off_t);
static void writex(int, const void *, size_t);
static void seekx(int, off_t);
static unsigned int optfmt(const char *);
static uint32_t optaddr(const char *);
static int optpage(const char *, int);
static void Warn(const char *, const char *, ...);
static void usage(void);

/*
 * A link editor for BTX clients.
 */
int
main(int argc, char *argv[])
{
    int c;

    while ((c = getopt(argc, argv, "qvb:E:e:f:l:o:P:W:")) != -1)
	switch (c) {
	case 'q':
	    quiet = 1;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'b':
	    bname = optarg;
	    break;
	case 'E':
	    centry = optaddr(optarg);
	    Eflag = 1;
	    break;
	case 'e':
	    lentry = optaddr(optarg);
	    break;
	case 'f':
	    format = optfmt(optarg);
	    break;
	case 'l':
	    lname = optarg;
	    break;
	case 'o':
	    oname = optarg;
	    break;
	case 'P':
	    ppage = optpage(optarg, 1);
	    break;
	case 'W':
	    wpage = optpage(optarg, BTX_MAXCWR);
	    break;
	default:
	    usage();
	}
    argc -= optind;
    argv += optind;
    if (argc != 1)
	usage();
    atexit(cleanup);
    btxld(*argv);
    return 0;
}

/*
 * Clean up after errors.
 */
static void
cleanup(void)
{
    if (tname)
	remove(tname);
}

/*
 * Read the input files; write the output file; display information.
 */
static void
btxld(const char *iname)
{
    char name[FILENAME_MAX];
    struct btx_hdr btx;
    struct hdr ihdr, ohdr;
    unsigned int ldr_size, cwr;
    int fdi[3], fdo, i;

    ldr_size = 0;

    for (i = I_LDR; i <= I_CLNT; i++) {
	fname = i == I_LDR ? lname : i == I_BTX ? bname : iname;
	if ((fdi[i] = open(fname, O_RDONLY)) == -1)
	    err(2, "%s", fname);
	switch (i) {
	case I_LDR:
	    gethdr(fdi[i], &ihdr);
	    if (ihdr.fmt != F_BIN)
		Warn(fname, "Loader format is %s; processing as %s",
		     fmtlist[ihdr.fmt], fmtlist[F_BIN]);
	    ldr_size = ihdr.size;
	    break;
	case I_BTX:
	    getbtx(fdi[i], &btx);
	    break;
	case I_CLNT:
	    gethdr(fdi[i], &ihdr);
	    if (ihdr.org && ihdr.org != BTX_PGSIZE)
		Warn(fname,
		     "Client origin is 0x%x; expecting 0 or 0x%x",
		     ihdr.org, BTX_PGSIZE);
	}
    }
    memset(&ohdr, 0, sizeof(ohdr));
    ohdr.fmt = format;
    ohdr.text = ldr_size;
    ohdr.data = btx.btx_textsz + ihdr.size;
    ohdr.org = lentry;
    ohdr.entry = lentry;
    cwr = 0;
    if (wpage > 0 || (wpage == -1 && !(ihdr.flags & IMPURE))) {
	if (wpage > 0)
	    cwr = wpage;
	else {
	    cwr = howmany(ihdr.text, BTX_PGSIZE);
	    if (cwr > BTX_MAXCWR)
		cwr = BTX_MAXCWR;
	}
    }
    if (ppage > 0 || (ppage && wpage && ihdr.org >= BTX_PGSIZE)) {
	btx.btx_flags |= BTX_MAPONE;
	if (!cwr)
	    cwr++;
    }
    btx.btx_pgctl -= cwr;
    btx.btx_entry = Eflag ? centry : ihdr.entry;
    if (snprintf(name, sizeof(name), "%s.tmp", oname) >= sizeof(name))
	errx(2, "%s: Filename too long", oname);
    if ((fdo = open(name, O_CREAT | O_TRUNC | O_WRONLY, 0666)) == -1)
	err(2, "%s", name);
    if (!(tname = strdup(name)))
	err(2, NULL);
    puthdr(fdo, &ohdr);
    for (i = I_LDR; i <= I_CLNT; i++) {
	fname = i == I_LDR ? lname : i == I_BTX ? bname : iname;
	switch (i) {
	case I_LDR:
	    copy(fdi[i], fdo, ldr_size, 0);
	    seekx(fdo, ohdr.size += ohdr.text);
	    break;
	case I_BTX:
	    writex(fdo, &btx, sizeof(btx));
	    copy(fdi[i], fdo, btx.btx_textsz - sizeof(btx),
		 sizeof(btx));
	    break;
	case I_CLNT:
	    copy(fdi[i], fdo, ihdr.size, 0);
	    if (ftruncate(fdo, ohdr.size += ohdr.data))
		err(2, "%s", tname);
	}
	if (close(fdi[i]))
	    err(2, "%s", fname);
    }
    if (close(fdo))
	err(2, "%s", tname);
    if (rename(tname, oname))
	err(2, "%s: Can't rename to %s", tname, oname);
    tname = NULL;
    if (verbose) {
	printf(binfo, btx.btx_majver, btx.btx_minver, btx.btx_textsz,
	       BTX_ORIGIN(btx), BTX_ENTRY(btx), BTX_MAPPED(btx) *
	       BTX_PGSIZE / 0x100000, !!(btx.btx_flags & BTX_MAPONE),
	       BTX_MAPPED(btx) - btx.btx_pgctl - BTX_PGBASE /
	       BTX_PGSIZE - BTX_MAPPED(btx) * 4 / BTX_PGSIZE);
	printf(cinfo, fmtlist[ihdr.fmt], ihdr.size, ihdr.text,
	       ihdr.data, ihdr.bss, ihdr.entry);
	printf(oinfo, fmtlist[ohdr.fmt], ohdr.size, ohdr.text,
	       ohdr.data, ohdr.org, ohdr.entry);
    }
}

/*
 * Read BTX file header.
 */
static void
getbtx(int fd, struct btx_hdr * btx)
{
    if (readx(fd, btx, sizeof(*btx), 0) != sizeof(*btx) ||
	btx->btx_magic[0] != BTX_MAG0 ||
	btx->btx_magic[1] != BTX_MAG1 ||
	btx->btx_magic[2] != BTX_MAG2)
	errx(1, "%s: Not a BTX kernel", fname);
}

/*
 * Get file size and read a.out or ELF header.
 */
static void
gethdr(int fd, struct hdr *hdr)
{
    struct stat sb;
    const struct exec *ex;
    const Elf32_Ehdr *ee;
    const Elf32_Phdr *ep;
    void *p;
    unsigned int fmt, x, n, i;

    memset(hdr, 0, sizeof(*hdr));
    if (fstat(fd, &sb))
	err(2, "%s", fname);
    if (sb.st_size > MAXU32)
	errx(1, "%s: Too big", fname);
    hdr->size = sb.st_size;
    if ((p = mmap(NULL, hdr->size, PROT_READ, MAP_SHARED, fd,
		  0)) == MAP_FAILED)
	err(2, "%s", fname);
    for (fmt = F_CNT - 1; !hdr->fmt && fmt; fmt--)
	switch (fmt) {
	case F_AOUT:
	    ex = p;
	    if (hdr->size >= sizeof(struct exec) && !N_BADMAG(*ex)) {
		hdr->fmt = fmt;
		x = N_GETMAGIC(*ex);
		if (x == OMAGIC || x == NMAGIC) {
		    if (x == NMAGIC)
			Warn(fname, "Treating %s NMAGIC as OMAGIC",
			     fmtlist[fmt]);
		    hdr->flags |= IMPURE;
		}
		hdr->text = ex->a_text;
		hdr->data = ex->a_data;
		hdr->bss = ex->a_bss;
		hdr->entry = ex->a_entry;
		if (ex->a_entry >= BTX_PGSIZE)
		    hdr->org = BTX_PGSIZE;
	    }
	    break;
	case F_ELF:
	    ee = p;
	    if (hdr->size >= sizeof(Elf32_Ehdr) && IS_ELF(*ee)) {
		hdr->fmt = fmt;
		for (n = i = 0; i < ee->e_phnum; i++) {
		    ep = (void *)((uint8_t *)p + ee->e_phoff +
				  ee->e_phentsize * i);
		    if (ep->p_type == PT_LOAD)
			switch (n++) {
			case 0:
			    hdr->text = ep->p_filesz;
			    hdr->org = ep->p_paddr;
			    if (ep->p_flags & PF_W)
				hdr->flags |= IMPURE;
			    break;
			case 1:
			    hdr->data = ep->p_filesz;
			    hdr->bss = ep->p_memsz - ep->p_filesz;
			    break;
			case 2:
			    Warn(fname,
				 "Ignoring extra %s PT_LOAD segments",
				 fmtlist[fmt]);
			}
		}
		hdr->entry = ee->e_entry;
	    }
	}
    if (munmap(p, hdr->size))
	err(2, "%s", fname);
}

/*
 * Write a.out or ELF header.
 */
static void
puthdr(int fd, struct hdr *hdr)
{
    struct exec ex;
    struct elfh eh;

    switch (hdr->fmt) {
    case F_AOUT:
	memset(&ex, 0, sizeof(ex));
	N_SETMAGIC(ex, ZMAGIC, MID_ZERO, 0);
	hdr->text = N_ALIGN(ex, hdr->text);
	ex.a_text = hdr->text;
	hdr->data = N_ALIGN(ex, hdr->data);
	ex.a_data = hdr->data;
	ex.a_entry = hdr->entry;
	writex(fd, &ex, sizeof(ex));
	hdr->size = N_ALIGN(ex, sizeof(ex));
	seekx(fd, hdr->size);
	break;
    case F_ELF:
	eh = elfhdr;
	eh.e.e_entry = hdr->entry;
	eh.p[0].p_vaddr = eh.p[0].p_paddr = hdr->org;
	eh.p[0].p_filesz = eh.p[0].p_memsz = hdr->text;
	eh.p[1].p_offset = eh.p[0].p_offset + eh.p[0].p_filesz;
	eh.p[1].p_vaddr = eh.p[1].p_paddr = align(eh.p[0].p_paddr +
						  eh.p[0].p_memsz, 4);
	eh.p[1].p_filesz = eh.p[1].p_memsz = hdr->data;
	eh.sh[2].sh_addr = eh.p[0].p_vaddr;
	eh.sh[2].sh_offset = eh.p[0].p_offset;
	eh.sh[2].sh_size = eh.p[0].p_filesz;
	eh.sh[3].sh_addr = eh.p[1].p_vaddr;
	eh.sh[3].sh_offset = eh.p[1].p_offset;
	eh.sh[3].sh_size = eh.p[1].p_filesz;
	writex(fd, &eh, sizeof(eh));
	hdr->size = sizeof(eh);
    }
}

/*
 * Safe copy from input file to output file.
 */
static void
copy(int fdi, int fdo, size_t nbyte, off_t offset)
{
    char buf[8192];
    size_t n;

    while (nbyte) {
	if ((n = sizeof(buf)) > nbyte)
	    n = nbyte;
	if (readx(fdi, buf, n, offset) != n)
	    errx(2, "%s: Short read", fname);
	writex(fdo, buf, n);
	nbyte -= n;
	offset = -1;
    }
}

/*
 * Safe read from input file.
 */
static size_t
readx(int fd, void *buf, size_t nbyte, off_t offset)
{
    ssize_t n;

    if (offset != -1 && lseek(fd, offset, SEEK_SET) != offset)
	err(2, "%s", fname);
    if ((n = read(fd, buf, nbyte)) == -1)
	err(2, "%s", fname);
    return n;
}

/*
 * Safe write to output file.
 */
static void
writex(int fd, const void *buf, size_t nbyte)
{
    ssize_t n;

    if ((n = write(fd, buf, nbyte)) == -1)
	err(2, "%s", tname);
    if (n != nbyte)
	errx(2, "%s: Short write", tname);
}

/*
 * Safe seek in output file.
 */
static void
seekx(int fd, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
	err(2, "%s", tname);
}

/*
 * Convert an option argument to a format code.
 */
static unsigned int
optfmt(const char *arg)
{
    unsigned int i;

    for (i = 0; i < F_CNT && strcmp(arg, fmtlist[i]); i++);
    if (i == F_CNT)
	errx(1, "%s: Unknown format", arg);
    return i;
}

/*
 * Convert an option argument to an address.
 */
static uint32_t
optaddr(const char *arg)
{
    char *s;
    unsigned long x;

    errno = 0;
    x = strtoul(arg, &s, 0);
    if (errno || !*arg || *s || x > MAXU32)
	errx(1, "%s: Illegal address", arg);
    return x;
}

/*
 * Convert an option argument to a page number.
 */
static int
optpage(const char *arg, int hi)
{
    char *s;
    long x;

    errno = 0;
    x = strtol(arg, &s, 0);
    if (errno || !*arg || *s || x < 0 || x > hi)
	errx(1, "%s: Illegal page number", arg);
    return x;
}

/*
 * Display a warning.
 */
static void
Warn(const char *locus, const char *fmt, ...)
{
    va_list ap;
    char *s;

    if (!quiet) {
	asprintf(&s, "%s: Warning: %s", locus, fmt);
	va_start(ap, fmt);
	vwarnx(s, ap);
	va_end(ap);
	free(s);
    }
}

/*
 * Display usage information.
 */
static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n",
    "usage: btxld [-qv] [-b file] [-E address] [-e address] [-f format]",
    "             [-l file] [-o filename] [-P page] [-W page] file");
    exit(1);
}
