/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
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
 * $DragonFly: src/sys/boot/sparc64/boot1/boot1.c,v 1.1 2003/11/10 06:08:40 dillon Exp $
 */

#include <sys/param.h>
#include <sys/dirent.h>
#include <machine/elf.h>
#include <machine/stdarg.h>

#define _PATH_LOADER	"/boot/loader"
#define _PATH_KERNEL	"/boot/kernel/kernel"

#define BSIZEMAX	16384

typedef int putc_func_t(int c, void *arg);
typedef int32_t ofwh_t;

struct sp_data {
	char	*sp_buf;
	u_int	sp_len;
	u_int	sp_size;
};

static const char digits[] = "0123456789abcdef";

static char bootpath[128];
static char bootargs[128];

static ofwh_t bootdev;

static struct fs fs;
static ino_t inomap;
static char blkbuf[BSIZEMAX];
static unsigned int fsblks;

static uint32_t fs_off;

int main(int ac, char **av);

static void exit(int) __dead2;
static void load(const char *);
static int dskread(void *, u_int64_t, int);

static void usage(void);

static void bcopy(const void *src, void *dst, size_t len);
static void bzero(void *b, size_t len);

static int mount(const char *device);

static void panic(const char *fmt, ...) __dead2;
static int printf(const char *fmt, ...);
static int putchar(int c, void *arg);
static int vprintf(const char *fmt, va_list ap);
static int vsnprintf(char *str, size_t sz, const char *fmt, va_list ap);

static int __printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap);
static int __putc(int c, void *arg);
static int __puts(const char *s, putc_func_t *putc, void *arg);
static int __sputc(int c, void *arg);
static char *__uitoa(char *buf, u_int val, int base);
static char *__ultoa(char *buf, u_long val, int base);

/*
 * Open Firmware interface functions
 */
typedef u_int64_t	ofwcell_t;
typedef u_int32_t	u_ofwh_t;
typedef int (*ofwfp_t)(ofwcell_t []);
ofwfp_t ofw;			/* the prom Open Firmware entry */

void ofw_init(int, int, int, int, ofwfp_t);
ofwh_t ofw_finddevice(const char *);
ofwh_t ofw_open(const char *);
int ofw_getprop(ofwh_t, const char *, void *, size_t);
int ofw_read(ofwh_t, void *, size_t);
int ofw_write(ofwh_t, const void *, size_t);
int ofw_seek(ofwh_t, u_int64_t);
void ofw_exit(void) __dead2;

ofwh_t bootdevh;
ofwh_t stdinh, stdouth;

/*
 * This has to stay here, as the PROM seems to ignore the
 * entry point specified in the a.out header.  (or elftoaout is broken)
 */

void
ofw_init(int d, int d1, int d2, int d3, ofwfp_t ofwaddr)
{
	ofwh_t chosenh;
	char *av[16];
	char *p;
	int ac;

	ofw = ofwaddr;

	chosenh = ofw_finddevice("/chosen");
	ofw_getprop(chosenh, "stdin", &stdinh, sizeof(stdinh));
	ofw_getprop(chosenh, "stdout", &stdouth, sizeof(stdouth));
	ofw_getprop(chosenh, "bootargs", bootargs, sizeof(bootargs));
	ofw_getprop(chosenh, "bootpath", bootpath, sizeof(bootpath));

	bootargs[sizeof(bootargs) - 1] = '\0';
	bootpath[sizeof(bootpath) - 1] = '\0';

	ac = 0;
	p = bootargs;
	for (;;) {
		while (*p == ' ' && *p != '\0')
			p++;
		if (*p == '\0' || ac >= 16)
			break;
		av[ac++] = p;
		while (*p != ' ' && *p != '\0')
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}

	exit(main(ac, av));
}

ofwh_t
ofw_finddevice(const char *name)
{
	ofwcell_t args[] = {
		(ofwcell_t)"finddevice",
		1,
		1,
		(ofwcell_t)name,
		0
	};

	if ((*ofw)(args)) {
		printf("ofw_finddevice: name=\"%s\"\n", name);
		return (1);
	}
	return (args[4]);
}

int
ofw_getprop(ofwh_t ofwh, const char *name, void *buf, size_t len)
{
	ofwcell_t args[] = {
		(ofwcell_t)"getprop",
		4,
		1,
		(u_ofwh_t)ofwh,
		(ofwcell_t)name,
		(ofwcell_t)buf,
		len,
	0
	};

	if ((*ofw)(args)) {
		printf("ofw_getprop: ofwh=0x%x buf=%p len=%u\n",
			ofwh, buf, len);
		return (1);
	}
	return (0);
}

ofwh_t
ofw_open(const char *path)
{
	ofwcell_t args[] = {
		(ofwcell_t)"open",
		1,
		1,
		(ofwcell_t)path,
		0
	};

	if ((*ofw)(args)) {
		printf("ofw_open: path=\"%s\"\n", path);
		return (-1);
	}
	return (args[4]);
}

int
ofw_close(ofwh_t devh)
{
	ofwcell_t args[] = {
		(ofwcell_t)"close",
		1,
		0,
		(u_ofwh_t)devh
	};

	if ((*ofw)(args)) {
		printf("ofw_close: devh=0x%x\n", devh);
		return (1);
	}
	return (0);
}

int
ofw_read(ofwh_t devh, void *buf, size_t len)
{
	ofwcell_t args[] = {
		(ofwcell_t)"read",
		4,
		1,
		(u_ofwh_t)devh,
		(ofwcell_t)buf,
		len,
		0
	};

	if ((*ofw)(args)) {
		printf("ofw_read: devh=0x%x buf=%p len=%u\n", devh, buf, len);
		return (1);
	}
	return (0);
}

int
ofw_write(ofwh_t devh, const void *buf, size_t len)
{
	ofwcell_t args[] = {
		(ofwcell_t)"write",
		3,
		1,
		(u_ofwh_t)devh,
		(ofwcell_t)buf,
		len,
		0
	};

	if ((*ofw)(args)) {
		printf("ofw_write: devh=0x%x buf=%p len=%u\n", devh, buf, len);
		return (1);
	}
	return (0);
}

int
ofw_seek(ofwh_t devh, u_int64_t off)
{
	ofwcell_t args[] = {
		(ofwcell_t)"seek",
		4,
		1,
		(u_ofwh_t)devh,
		off >> 32,
		off,
		0
	};

	if ((*ofw)(args)) {
		printf("ofw_seek: devh=0x%x off=0x%lx\n", devh, off);
		return (1);
	}
	return (0);
}

void
ofw_exit(void)
{
	ofwcell_t args[3];

	args[0] = (ofwcell_t)"exit";
	args[1] = 0;
	args[2] = 0;

	for (;;)
		(*ofw)(args);
}

static void
bcopy(const void *src, void *dst, size_t len)
{
	const char *s = src;
	char *d = dst;

	while (len-- != 0)
		*d++ = *s++;
}

static void
memcpy(void *dst, const void *src, size_t len)
{
	bcopy(src, dst, len);
}

static void
bzero(void *b, size_t len)
{
	char *p = b;

	while (len-- != 0)
		*p++ = 0;
}

static int
strcmp(const char *s1, const char *s2)
{
	for (; *s1 == *s2 && *s1; s1++, s2++)
		;
	return ((u_char)*s1 - (u_char)*s2);
}

#include "ufsread.c"

int
main(int ac, char **av)
{
	const char *path;
	int i;

	path = _PATH_LOADER;
	for (i = 0; i < ac; i++) {
		switch (av[i][0]) {
		case '-':
			switch (av[i][1]) {
			default:
				usage();
			}
			break;
		default:
			path = av[i];
			break;
		}
	}

	printf(" \n>> FreeBSD/sparc64 boot block\n"
	"   Boot path:   %s\n"
	"   Boot loader: %s\n", bootpath, path);

	if (mount(bootpath) == -1)
		panic("mount");

	load(path);
	return (1);
}

static void
usage(void)
{

	printf("usage: boot device [/path/to/loader]\n");
	exit(1);
}

static void
exit(int code)
{

	ofw_exit();
}

static struct dmadat __dmadat;

static int
mount(const char *device)
{

	dmadat = &__dmadat;
	if ((bootdev = ofw_open(device)) == -1) {
		printf("mount: can't open device\n");
		return (-1);
	}
	if (fsread(0, NULL, 0)) {
		printf("mount: can't read superblock\n");
		return (-1);
	}
	return (0);
}

static void
load(const char *fname)
{
	Elf64_Ehdr eh;
	Elf64_Phdr ph;
	caddr_t p;
	ino_t ino;
	int i;

	if ((ino = lookup(fname)) == 0) {
		printf("File %s not found\n", fname);
		return;
	}
	if (fsread(ino, &eh, sizeof(eh)) != sizeof(eh)) {
		printf("Can't read elf header\n");
		return;
	}
	if (!IS_ELF(eh)) {
		printf("Not an ELF file\n");
		return;
	}
	for (i = 0; i < eh.e_phnum; i++) {
		fs_off = eh.e_phoff + i * eh.e_phentsize;
		if (fsread(ino, &ph, sizeof(ph)) != sizeof(ph)) {
			printf("Can't read program header %d\n", i);
			return;
		}
		if (ph.p_type != PT_LOAD)
			continue;
		fs_off = ph.p_offset;
		p = (caddr_t)ph.p_vaddr;
		if (fsread(ino, p, ph.p_filesz) != ph.p_filesz) {
			printf("Can't read content of section %d\n", i);
			return;
		}
		if (ph.p_filesz != ph.p_memsz)
			bzero(p + ph.p_filesz, ph.p_memsz - ph.p_filesz);
	}
	ofw_close(bootdev);
	(*(void (*)(int, int, int, int, ofwfp_t))eh.e_entry)(0, 0, 0, 0, ofw);
}

static int
dskread(void *buf, u_int64_t lba, int nblk)
{
	/*
	 * The OpenFirmware should open the correct partition for us.
	 * That means, if we read from offset zero on an open instance handle,
	 * we should read from offset zero of that partition.
	 */
	ofw_seek(bootdev, lba * DEV_BSIZE);
	ofw_read(bootdev, buf, nblk * DEV_BSIZE);
	return (0);
}

static void
panic(const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	printf("panic: %s\n", buf);
	va_end(ap);

	exit(1);
}

static int
printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);
	return (ret);
}

static int
putchar(int c, void *arg)
{
	char buf;

	if (c == '\n') {
		buf = '\r';
		ofw_write(stdouth, &buf, 1);
	}
	buf = c;
	ofw_write(stdouth, &buf, 1);
	return (1);
}

static int
vprintf(const char *fmt, va_list ap)
{
	int ret;

	ret = __printf(fmt, putchar, 0, ap);
	return (ret);
}

static int
vsnprintf(char *str, size_t sz, const char *fmt, va_list ap)
{
	struct sp_data sp;
	int ret;

	sp.sp_buf = str;
	sp.sp_len = 0;
	sp.sp_size = sz;
	ret = __printf(fmt, __sputc, &sp, ap);
	return (ret);
}

static int
__printf(const char *fmt, putc_func_t *putc, void *arg, va_list ap)
{
	char buf[(sizeof(long) * 8) + 1];
	char *nbuf;
	u_long ul;
	u_int ui;
	int lflag;
	int sflag;
	char *s;
	int pad;
	int ret;
	int c;

	nbuf = &buf[sizeof buf - 1];
	ret = 0;
	while ((c = *fmt++) != 0) {
		if (c != '%') {
			ret += putc(c, arg);
			continue;
		}
		lflag = 0;
		sflag = 0;
		pad = 0;
reswitch:	c = *fmt++;
		switch (c) {
		case '#':
			sflag = 1;
			goto reswitch;
		case '%':
			ret += putc('%', arg);
			break;
		case 'c':
			c = va_arg(ap, int);
			ret += putc(c, arg);
			break;
		case 'd':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, int);
				if (ui < (int)ui) {
					ui = -ui;
					ret += putc('-', arg);
				}
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = (u_long)va_arg(ap, long);
				if (ul < (long)ul) {
					ul = -ul;
					ret += putc('-', arg);
				}
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'l':
			lflag = 1;
			goto reswitch;
		case 'o':
			if (lflag == 0) {
				ui = (u_int)va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 8);
			} else {
				ul = (u_long)va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 8);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'p':
			ul = (u_long)va_arg(ap, void *);
			s = __ultoa(nbuf, ul, 16);
			ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case 's':
			s = va_arg(ap, char *);
			ret += __puts(s, putc, arg);
			break;
		case 'u':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 10);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 10);
			}
			ret += __puts(s, putc, arg);
			break;
		case 'x':
			if (lflag == 0) {
				ui = va_arg(ap, u_int);
				s = __uitoa(nbuf, ui, 16);
			} else {
				ul = va_arg(ap, u_long);
				s = __ultoa(nbuf, ul, 16);
			}
			if (sflag)
				ret += __puts("0x", putc, arg);
			ret += __puts(s, putc, arg);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			pad = pad * 10 + c - '0';
			goto reswitch;
		default:
			break;
		}
	}
	return (ret);
}

static int
__sputc(int c, void *arg)
{
	struct sp_data *sp;

	sp = arg;
	if (sp->sp_len < sp->sp_size)
		sp->sp_buf[sp->sp_len++] = c;
	sp->sp_buf[sp->sp_len] = '\0';
	return (1);
}

static int
__puts(const char *s, putc_func_t *putc, void *arg)
{
	const char *p;
	int ret;

	ret = 0;
	for (p = s; *p != '\0'; p++)
		ret += putc(*p, arg);
	return (ret);
}

static char *
__uitoa(char *buf, u_int ui, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ui % base];
	while ((ui /= base) != 0);
	return (p);
}

static char *
__ultoa(char *buf, u_long ul, int base)
{
	char *p;

	p = buf;
	*p = '\0';
	do
		*--p = digits[ul % base];
	while ((ul /= base) != 0);
	return (p);
}
