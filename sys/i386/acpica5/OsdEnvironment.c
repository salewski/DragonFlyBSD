/*-
 * Copyright (c) 2000,2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/i386/acpica/OsdEnvironment.c,v 1.10 2004/05/06 02:18:58 njl Exp $
 * $DragonFly: src/sys/i386/acpica5/Attic/OsdEnvironment.c,v 1.2 2004/06/27 08:52:45 dillon Exp $
 */

/*
 * 6.1 : Environmental support
 */
#include <sys/types.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>

#include "acpi.h"

static u_long i386_acpi_root;

SYSCTL_ULONG(_machdep, OID_AUTO, acpi_root, CTLFLAG_RD, &i386_acpi_root, 0,
	     "The physical address of the RSDP");

ACPI_STATUS
AcpiOsInitialize(void)
{
	return(0);
}

ACPI_STATUS
AcpiOsTerminate(void)
{
	return(0);
}

ACPI_STATUS
AcpiOsGetRootPointer(UINT32 Flags, ACPI_POINTER *RsdpPhysicalAddress)
{
	ACPI_POINTER ptr;
	ACPI_STATUS status;

	if (i386_acpi_root == 0) {
		/*
		 * The loader passes the physical address at which it found the
		 * RSDP in a hint.  We could recover this rather than searching
		 * manually here.
		 */
		status = AcpiFindRootPointer(Flags, &ptr);
		if (status == AE_OK)
			i386_acpi_root = ptr.Pointer.Physical;
	} else
		status = AE_OK;

	RsdpPhysicalAddress->PointerType = ACPI_PHYSICAL_POINTER;
	RsdpPhysicalAddress->Pointer.Physical = i386_acpi_root;
	return (status);
}
