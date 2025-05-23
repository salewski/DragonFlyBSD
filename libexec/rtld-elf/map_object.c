/*-
 * Copyright 1996-1998 John D. Polstra.
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
 * $FreeBSD: src/libexec/rtld-elf/map_object.c,v 1.7.2.2 2002/12/28 19:49:41 dillon Exp $
 * $DragonFly: src/libexec/rtld-elf/map_object.c,v 1.6 2005/03/22 22:56:36 davidxu Exp $
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtld.h"

static Elf_Ehdr *get_elf_header (int, const char *);
static int convert_prot(int);	/* Elf flags -> mmap protection */
static int convert_flags(int); /* Elf flags -> mmap flags */

/*
 * Map a shared object into memory.  The "fd" argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 * The "path" argument is a pathname that is used only for error messages.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
map_object(int fd, const char *path, const struct stat *sb)
{
    Obj_Entry *obj;
    Elf_Ehdr *hdr;
    int i;
    Elf_Phdr *phdr;
    Elf_Phdr *phlimit;
    Elf_Phdr **segs;
    int nsegs;
    Elf_Phdr *phdyn;
    Elf_Phdr *phphdr;
    Elf_Phdr *phinterp;
    Elf_Phdr *phtls;
    caddr_t mapbase;
    size_t mapsize;
    Elf_Off base_offset;
    Elf_Addr base_vaddr;
    Elf_Addr base_vlimit;
    caddr_t base_addr;
    Elf_Off data_offset;
    Elf_Addr data_vaddr;
    Elf_Addr data_vlimit;
    caddr_t data_addr;
    int data_prot;
    int data_flags;
    Elf_Addr clear_vaddr;
    caddr_t clear_addr;
    caddr_t clear_page;
    size_t nclear;
    Elf_Addr bss_vaddr;
    Elf_Addr bss_vlimit;
    caddr_t bss_addr;

    hdr = get_elf_header(fd, path);
    if (hdr == NULL)
	return NULL;

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) ((char *)hdr + hdr->e_phoff);
    phlimit = phdr + hdr->e_phnum;
    nsegs = -1;
    phdyn = phphdr = phinterp = phtls = NULL;
    segs = alloca(sizeof(segs[0]) * hdr->e_phnum);
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_INTERP:
	    phinterp = phdr;
	    break;

	case PT_LOAD:
	    segs[++nsegs] = phdr;
    	    if (segs[nsegs]->p_align < PAGE_SIZE) {
		_rtld_error("%s: PT_LOAD segment %d not page-aligned",
		    path, nsegs);
		return NULL;
	    }
	    break;

	case PT_PHDR:
	    phphdr = phdr;
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;
	case PT_TLS:
	    phtls = phdr;
	    break;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	_rtld_error("%s: object is not dynamically-linked", path);
	return NULL;
    }

    if (nsegs < 0) {
	_rtld_error("%s: too few PT_LOAD segments", path);
	return NULL;
    }

    /*
     * Map the entire address space of the object, to stake out our
     * contiguous region, and to establish the base address for relocation.
     */
    base_offset = trunc_page(segs[0]->p_offset);
    base_vaddr = trunc_page(segs[0]->p_vaddr);
    base_vlimit = round_page(segs[nsegs]->p_vaddr + segs[nsegs]->p_memsz);
    mapsize = base_vlimit - base_vaddr;
    base_addr = hdr->e_type == ET_EXEC ? (caddr_t) base_vaddr : NULL;

    mapbase = mmap(base_addr, mapsize, convert_prot(segs[0]->p_flags),
      convert_flags(segs[0]->p_flags), fd, base_offset);
    if (mapbase == (caddr_t) -1) {
	_rtld_error("%s: mmap of entire address space failed: %s",
	  path, strerror(errno));
	return NULL;
    }
    if (base_addr != NULL && mapbase != base_addr) {
	_rtld_error("%s: mmap returned wrong address: wanted %p, got %p",
	  path, base_addr, mapbase);
	munmap(mapbase, mapsize);
	return NULL;
    }

    for (i = 0; i <=  nsegs; i++) {
	/* Overlay the segment onto the proper region. */
	data_offset = trunc_page(segs[i]->p_offset);
	data_vaddr = trunc_page(segs[i]->p_vaddr);
	data_vlimit = round_page(segs[i]->p_vaddr + segs[i]->p_filesz);
	data_addr = mapbase + (data_vaddr - base_vaddr);
	data_prot = convert_prot(segs[i]->p_flags);
	data_flags = convert_flags(segs[i]->p_flags) | MAP_FIXED;
	/* Do not call mmap on the first segment - this is redundant */
	if (i && mmap(data_addr, data_vlimit - data_vaddr, data_prot,
	  data_flags, fd, data_offset) == (caddr_t) -1) {
	    _rtld_error("%s: mmap of data failed: %s", path, strerror(errno));
	    return NULL;
	}

	/* Clear any BSS in the last page of the segment. */
	clear_vaddr = segs[i]->p_vaddr + segs[i]->p_filesz;
	clear_addr = mapbase + (clear_vaddr - base_vaddr);
	clear_page = mapbase + (trunc_page(clear_vaddr) - base_vaddr);
	if ((nclear = data_vlimit - clear_vaddr) > 0) {
	    /* Make sure the end of the segment is writable */
	    if ((data_prot & PROT_WRITE) == 0) {
		if (mprotect(clear_page, PAGE_SIZE, data_prot|PROT_WRITE) < 0) {
			_rtld_error("%s: mprotect failed: %s", path,
			    strerror(errno));
			return NULL;
		}
	    }

	    memset(clear_addr, 0, nclear);

	    /*
	     * reset the data protection back, enable the segment to be
	     * coredumped since we modified it.
	     */
	    if ((data_prot & PROT_WRITE) == 0) {
		madvise(clear_page, PAGE_SIZE, MADV_CORE);
		mprotect(clear_page, PAGE_SIZE, data_prot);
	    }
	}

	/* Overlay the BSS segment onto the proper region. */
	bss_vaddr = data_vlimit;
	bss_vlimit = round_page(segs[i]->p_vaddr + segs[i]->p_memsz);
	bss_addr = mapbase +  (bss_vaddr - base_vaddr);
	if (bss_vlimit > bss_vaddr) {	/* There is something to do */
	    if (mmap(bss_addr, bss_vlimit - bss_vaddr, data_prot,
		MAP_PRIVATE|MAP_FIXED|MAP_ANON, -1, 0) == (caddr_t) -1) {
		    _rtld_error("%s: mmap of bss failed: %s", path,
			strerror(errno));
		return NULL;
	    }
	}
    }

    obj = obj_new();
    if (sb != NULL) {
	obj->dev = sb->st_dev;
	obj->ino = sb->st_ino;
    }
    obj->mapbase = mapbase;
    obj->mapsize = mapsize;
    obj->textsize = round_page(segs[0]->p_vaddr + segs[0]->p_memsz) -
      base_vaddr;
    obj->vaddrbase = base_vaddr;
    obj->relocbase = mapbase - base_vaddr;
    obj->dynamic = (const Elf_Dyn *) (obj->relocbase + phdyn->p_vaddr);
    if (hdr->e_entry != 0)
	obj->entry = (caddr_t) (obj->relocbase + hdr->e_entry);
    if (phphdr != NULL) {
	obj->phdr = (const Elf_Phdr *) (obj->relocbase + phphdr->p_vaddr);
	obj->phsize = phphdr->p_memsz;
    }
    if (phinterp != NULL)
	obj->interp = (const char *) (obj->relocbase + phinterp->p_vaddr);
    if (phtls != NULL) {
	tls_dtv_generation++;
	obj->tlsindex = ++tls_max_index;
	obj->tlssize = phtls->p_memsz;
	obj->tlsalign = phtls->p_align;
	obj->tlsinitsize = phtls->p_filesz;
	obj->tlsinit = mapbase + phtls->p_vaddr;
    }
    return obj;
}

static Elf_Ehdr *
get_elf_header (int fd, const char *path)
{
    static union {
	Elf_Ehdr hdr;
	char buf[PAGE_SIZE];
    } u;
    ssize_t nbytes;

    if ((nbytes = read(fd, u.buf, PAGE_SIZE)) == -1) {
	_rtld_error("%s: read error: %s", path, strerror(errno));
	return NULL;
    }

    /* Make sure the file is valid */
    if (nbytes < (ssize_t)sizeof(Elf_Ehdr) || !IS_ELF(u.hdr)) {
	_rtld_error("%s: invalid file format", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_CLASS] != ELF_TARG_CLASS
      || u.hdr.e_ident[EI_DATA] != ELF_TARG_DATA) {
	_rtld_error("%s: unsupported file layout", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT
      || u.hdr.e_version != EV_CURRENT) {
	_rtld_error("%s: unsupported file version", path);
	return NULL;
    }
    if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
	_rtld_error("%s: unsupported file type", path);
	return NULL;
    }
    if (u.hdr.e_machine != ELF_TARG_MACH) {
	_rtld_error("%s: unsupported machine", path);
	return NULL;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (u.hdr.e_phentsize != sizeof(Elf_Phdr)) {
	_rtld_error(
	  "%s: invalid shared object: e_phentsize != sizeof(Elf_Phdr)", path);
	return NULL;
    }
    if (u.hdr.e_phoff + u.hdr.e_phnum * sizeof(Elf_Phdr) > (size_t)nbytes) {
	_rtld_error("%s: program header too large", path);
	return NULL;
    }

    return (&u.hdr);
}

void
obj_free(Obj_Entry *obj)
{
    Objlist_Entry *elm;

    if (obj->tls_done) {
	free_tls_offset(obj);
    }
    free(obj->origin_path);
    free(obj->path);
    while (obj->needed != NULL) {
	Needed_Entry *needed = obj->needed;
	obj->needed = needed->next;
	free(needed);
    }
    while (!STAILQ_EMPTY(&obj->dldags)) {
	elm = STAILQ_FIRST(&obj->dldags);
	STAILQ_REMOVE_HEAD(&obj->dldags, link);
	free(elm);
    }
    while (!STAILQ_EMPTY(&obj->dagmembers)) {
	elm = STAILQ_FIRST(&obj->dagmembers);
	STAILQ_REMOVE_HEAD(&obj->dagmembers, link);
	free(elm);
    }
    free(obj);
}

Obj_Entry *
obj_new(void)
{
    Obj_Entry *obj;

    obj = CNEW(Obj_Entry);
    STAILQ_INIT(&obj->dldags);
    STAILQ_INIT(&obj->dagmembers);
    return obj;
}

/*
 * Given a set of ELF protection flags, return the corresponding protection
 * flags for MMAP.
 */
static int
convert_prot(int elfflags)
{
    int prot = 0;
    if (elfflags & PF_R)
	prot |= PROT_READ;
    if (elfflags & PF_W)
	prot |= PROT_WRITE;
    if (elfflags & PF_X)
	prot |= PROT_EXEC;
    return prot;
}

static int
convert_flags(int elfflags)
{
    int flags = MAP_PRIVATE; /* All mappings are private */

    /*
     * Readonly mappings are marked "MAP_NOCORE", because they can be
     * reconstructed by a debugger.
     */
    if (!(elfflags & PF_W))
	flags |= MAP_NOCORE;
    return flags;
}
