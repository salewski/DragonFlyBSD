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
 * $FreeBSD: src/sys/boot/i386/libi386/bootinfo64.c,v 1.36 2003/08/25 23:28:31 obrien Exp $
 * $DragonFly: src/sys/boot/i386/libi386/Attic/bootinfo64.c,v 1.1 2003/11/10 06:08:36 dillon Exp $
 */

#include <stand.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <machine/bootinfo.h>
#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceded by a 32-bit identifier and a 32-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ARGS	(variable)		module parameters (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define COPY32(v, a, c) {			\
    u_int32_t	x = (v);			\
    if (c)					\
	i386_copyin(&x, a, sizeof(x));		\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(strlen(s) + 1, a, c);		\
    if (c)					\
	i386_copyin(s, a, strlen(s) + 1);	\
    a += roundup(strlen(s) + 1, sizeof(u_int64_t));\
}

#define MOD_NAME(a, s, c)	MOD_STR(MODINFO_NAME, a, s, c)
#define MOD_TYPE(a, s, c)	MOD_STR(MODINFO_TYPE, a, s, c)
#define MOD_ARGS(a, s, c)	MOD_STR(MODINFO_ARGS, a, s, c)

#define MOD_VAR(t, a, s, c) {			\
    COPY32(t, a, c);				\
    COPY32(sizeof(s), a, c);			\
    if (c)					\
	i386_copyin(&s, a, sizeof(s));		\
    a += roundup(sizeof(s), sizeof(u_int64_t));	\
}

#define MOD_ADDR(a, s, c)	MOD_VAR(MODINFO_ADDR, a, s, c)
#define MOD_SIZE(a, s, c)	MOD_VAR(MODINFO_SIZE, a, s, c)

#define MOD_METADATA(a, mm, c) {		\
    COPY32(MODINFO_METADATA | mm->md_type, a, c); \
    COPY32(mm->md_size, a, c);			\
    if (c)					\
	i386_copyin(mm->md_data, a, mm->md_size); \
    a += roundup(mm->md_size, sizeof(u_int64_t));\
}

#define MOD_END(a, c) {				\
    COPY32(MODINFO_END, a, c);			\
    COPY32(0, a, c);				\
}

static vm_offset_t
bi_copymodules64(vm_offset_t addr)
{
    struct preloaded_file	*fp;
    struct file_metadata	*md;
    int				c;
    u_int64_t			v;

    c = addr != 0;
    /* start with the first module on the list, should be the kernel */
    for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {

	MOD_NAME(addr, fp->f_name, c);	/* this field must come first */
	MOD_TYPE(addr, fp->f_type, c);
	if (fp->f_args)
	    MOD_ARGS(addr, fp->f_args, c);
	v = fp->f_addr;
	MOD_ADDR(addr, v, c);
	v = fp->f_size;
	MOD_SIZE(addr, v, c);
	for (md = fp->f_metadata; md != NULL; md = md->md_next)
	    if (!(md->md_type & MODINFOMD_NOCOPY))
		MOD_METADATA(addr, md, c);
    }
    MOD_END(addr, c);
    return(addr);
}

/*
 * Load the information expected by an i386 kernel.
 *
 * - The 'boothowto' argument is constructed
 * - The 'bootdev' argument is constructed
 * - The 'bootinfo' struct is constructed, and copied into the kernel space.
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load64(char *args, vm_offset_t *modulep, vm_offset_t *kernendp)
{
    struct preloaded_file	*xp, *kfp;
    struct i386_devdesc		*rootdev;
    struct file_metadata	*md;
    vm_offset_t			addr;
    u_int64_t			kernend;
    u_int64_t			envp;
    vm_offset_t			size;
    char			*rootdevname;
    int				i, howto;
    char			*kernelname;
    const char			*kernelpath;

    howto = bi_getboothowto(args);

    /* 
     * Allow the environment variable 'rootdev' to override the supplied device 
     * This should perhaps go to MI code and/or have $rootdev tested/set by
     * MI code before launching the kernel.
     */
    rootdevname = getenv("rootdev");
    i386_getdev((void **)(&rootdev), rootdevname, NULL);
    if (rootdev == NULL) {		/* bad $rootdev/$currdev */
	printf("can't determine root device\n");
	return(EINVAL);
    }

    /* Try reading the /etc/fstab file to select the root device */
    getrootmount(i386_fmtdev((void *)rootdev));

    /* find the last module in the chain */
    addr = 0;
    for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
	if (addr < (xp->f_addr + xp->f_size))
	    addr = xp->f_addr + xp->f_size;
    }
    /* pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    /* copy our environment */
    envp = addr;
    addr = bi_copyenv(addr);

    /* pad to a page boundary */
    addr = roundup(addr, PAGE_SIZE);

    kfp = file_findfile(NULL, "elf kernel");
    if (kfp == NULL)
      kfp = file_findfile(NULL, "elf64 kernel");
    if (kfp == NULL)
	panic("can't find kernel file");
    kernend = 0;	/* fill it in later */
    file_addmetadata(kfp, MODINFOMD_HOWTO, sizeof howto, &howto);
    file_addmetadata(kfp, MODINFOMD_ENVP, sizeof envp, &envp);
    file_addmetadata(kfp, MODINFOMD_KERNEND, sizeof kernend, &kernend);
    bios_addsmapdata(kfp);

    /* Figure out the size and location of the metadata */
    *modulep = addr;
    size = bi_copymodules64(0);
    kernend = roundup(addr + size, PAGE_SIZE);
    *kernendp = kernend;

    /* patch MODINFOMD_KERNEND */
    md = file_findmetadata(kfp, MODINFOMD_KERNEND);
    bcopy(&kernend, md->md_data, sizeof kernend);

    /* copy module list and metadata */
    (void)bi_copymodules64(addr);

    return(0);
}
