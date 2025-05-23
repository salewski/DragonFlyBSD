/******************************************************************************
 *
 * Name: actables.h - ACPI table management
 *       $Revision: 44 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/actables.h,v 1.1 2003/09/24 03:32:15 drhodus Exp $                                                               */

#ifndef __ACTABLES_H__
#define __ACTABLES_H__


/* Used in AcpiTbMapAcpiTable for size parameter if table header is to be used */

#define SIZE_IN_HEADER          0


ACPI_STATUS
AcpiTbHandleToObject (
    UINT16                  TableId,
    ACPI_TABLE_DESC         **TableDesc);

/*
 * tbconvrt - Table conversion routines
 */

ACPI_STATUS
AcpiTbConvertToXsdt (
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbConvertTableFadt (
    void);

ACPI_STATUS
AcpiTbBuildCommonFacs (
    ACPI_TABLE_DESC         *TableInfo);

UINT32
AcpiTbGetTableCount (
    RSDP_DESCRIPTOR         *RSDP,
    ACPI_TABLE_HEADER       *RSDT);

/*
 * tbget - Table "get" routines
 */

ACPI_STATUS
AcpiTbGetTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbGetTableHeader (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *ReturnHeader);

ACPI_STATUS
AcpiTbGetTableBody (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbGetThisTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbTableOverride (
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbGetTablePtr (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **TablePtrLoc);

ACPI_STATUS
AcpiTbVerifyRsdp (
    ACPI_POINTER            *Address);

void
AcpiTbGetRsdtAddress (
    ACPI_POINTER            *OutAddress);

ACPI_STATUS
AcpiTbValidateRsdt (
    ACPI_TABLE_HEADER       *TablePtr);

ACPI_STATUS
AcpiTbGetRequiredTables (
    void);

ACPI_STATUS
AcpiTbGetPrimaryTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbGetSecondaryTable (
    ACPI_POINTER            *Address,
    ACPI_STRING             Signature,
    ACPI_TABLE_DESC         *TableInfo);

/*
 * tbinstall - Table installation
 */

ACPI_STATUS
AcpiTbInstallTable (
    ACPI_TABLE_DESC         *TableInfo);

ACPI_STATUS
AcpiTbMatchSignature (
    char                    *Signature,
    ACPI_TABLE_DESC         *TableInfo,
    UINT8                   SearchType);

ACPI_STATUS
AcpiTbRecognizeTable (
    ACPI_TABLE_DESC         *TableInfo,
    UINT8                  SearchType);

ACPI_STATUS
AcpiTbInitTableDescriptor (
    ACPI_TABLE_TYPE         TableType,
    ACPI_TABLE_DESC         *TableInfo);


/*
 * tbremove - Table removal and deletion
 */

void
AcpiTbDeleteAcpiTables (
    void);

void
AcpiTbDeleteAcpiTable (
    ACPI_TABLE_TYPE         Type);

void
AcpiTbDeleteSingleTable (
    ACPI_TABLE_DESC         *TableDesc);

ACPI_TABLE_DESC *
AcpiTbUninstallTable (
    ACPI_TABLE_DESC         *TableDesc);

void
AcpiTbFreeAcpiTablesOfType (
    ACPI_TABLE_DESC         *TableInfo);


/*
 * tbrsd - RSDP, RSDT utilities
 */

ACPI_STATUS
AcpiTbGetTableRsdt (
    void);

UINT8 *
AcpiTbScanMemoryForRsdp (
    UINT8                   *StartAddress,
    UINT32                  Length);

ACPI_STATUS
AcpiTbFindRsdp (
    ACPI_TABLE_DESC         *TableInfo,
    UINT32                  Flags);


/*
 * tbutils - common table utilities
 */

ACPI_STATUS
AcpiTbFindTable (
    char                    *Signature,
    char                    *OemId,
    char                    *OemTableId,
    ACPI_TABLE_HEADER       **TablePtr);

ACPI_STATUS
AcpiTbVerifyTableChecksum (
    ACPI_TABLE_HEADER       *TableHeader);

UINT8
AcpiTbChecksum (
    void                    *Buffer,
    UINT32                  Length);

ACPI_STATUS
AcpiTbValidateTableHeader (
    ACPI_TABLE_HEADER       *TableHeader);


#endif /* __ACTABLES_H__ */
