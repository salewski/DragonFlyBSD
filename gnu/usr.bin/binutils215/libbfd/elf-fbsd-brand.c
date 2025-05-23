/*-
 * Copyright (c) 2002, 2003 David E. O'Brien
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
 * $DragonFly: src/gnu/usr.bin/binutils215/libbfd/Attic/elf-fbsd-brand.c,v 1.1 2004/12/20 13:14:45 asmodai Exp $
 */

#include "bfd.h"
#include "sysdep.h"
#include "elf-bfd.h"

/* The kernel recognizes executables as valid only if they carry a
   "FreeBSD" label in the ELF header.  So we put this label on all
   executables and (for simplicity) also all other object files.  */

static void elf_fbsd_post_process_headers
  PARAMS ((bfd *, struct bfd_link_info *));

static void
elf_fbsd_post_process_headers (abfd, link_info)
     bfd * abfd;
     struct bfd_link_info * link_info ATTRIBUTE_UNUSED;
{
  Elf_Internal_Ehdr * i_ehdrp;	/* ELF file header, internal form.  */

  i_ehdrp = elf_elfheader (abfd);

  /* Put an ABI label supported by FreeBSD >= 4.1.  */
  i_ehdrp->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
}
