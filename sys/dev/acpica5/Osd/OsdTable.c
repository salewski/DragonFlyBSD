/*-
 * Copyright (c) 2002 Mitsaru Iwasaki
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
 * $FreeBSD: src/sys/dev/acpica/Osd/OsdTable.c,v 1.7 2004/04/20 17:13:08 njl Exp $
 * $DragonFly: src/sys/dev/acpica5/Osd/OsdTable.c,v 1.2 2004/06/27 08:52:42 dillon Exp $
 */

/*
 * ACPI Table interfaces
 */

#include "acpi.h"

#include <sys/kernel.h>
#include <sys/linker.h>

#undef _COMPONENT
#define _COMPONENT      ACPI_TABLES

static char acpi_osname[128];
TUNABLE_STR("hw.acpi.osname", acpi_osname, sizeof(acpi_osname));

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal)
{
    if (InitVal == NULL || NewVal == NULL)
	return (AE_BAD_PARAMETER);

    *NewVal = NULL;
    if (strncmp(InitVal->Name, "_OS_", 4) == 0 && strlen(acpi_osname) > 0) {
	printf("ACPI: Overriding _OS definition with \"%s\"\n", acpi_osname);
	*NewVal = acpi_osname;
    }

    return (AE_OK);
}

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{
    caddr_t                 acpi_dsdt, p;

    if (ExistingTable == NULL || NewTable == NULL)
        return(AE_BAD_PARAMETER);

    *NewTable = NULL;
    if (strncmp(ExistingTable->Signature, "DSDT", 4) != 0)
        return(AE_OK);
    if ((acpi_dsdt = preload_search_by_type("acpi_dsdt")) == NULL)
        return(AE_OK);
    if ((p = preload_search_info(acpi_dsdt, MODINFO_ADDR)) == NULL)
        return(AE_OK);

    *NewTable = *(void **)p;
    printf("ACPI: DSDT was overridden.\n");
    return (AE_OK);
}
