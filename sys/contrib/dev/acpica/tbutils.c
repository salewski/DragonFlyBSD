/******************************************************************************
 *
 * Module Name: tbutils - Table manipulation utilities
 *              $Revision: 58 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/tbutils.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */

#define __TBUTILS_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbutils")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbHandleToObject
 *
 * PARAMETERS:  TableId             - Id for which the function is searching
 *              TableDesc           - Pointer to return the matching table
 *                                      descriptor.
 *
 * RETURN:      Search the tables to find one with a matching TableId and
 *              return a pointer to that table descriptor.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbHandleToObject (
    UINT16                  TableId,
    ACPI_TABLE_DESC         **TableDesc)
{
    UINT32                  i;
    ACPI_TABLE_DESC         *ListHead;


    ACPI_FUNCTION_NAME ("TbHandleToObject");


    for (i = 0; i < ACPI_TABLE_MAX; i++)
    {
        ListHead = &AcpiGbl_AcpiTables[i];
        do
        {
            if (ListHead->TableId == TableId)
            {
                *TableDesc = ListHead;
                return (AE_OK);
            }

            ListHead = ListHead->Next;

        } while (ListHead != &AcpiGbl_AcpiTables[i]);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "TableId=%X does not exist\n", TableId));
    return (AE_BAD_PARAMETER);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateTableHeader
 *
 * PARAMETERS:  TableHeader         - Logical pointer to the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check an ACPI table header for validity
 *
 * NOTE:  Table pointers are validated as follows:
 *          1) Table pointer must point to valid physical memory
 *          2) Signature must be 4 ASCII chars, even if we don't recognize the
 *             name
 *          3) Table must be readable for length specified in the header
 *          4) Table checksum must be valid (with the exception of the FACS
 *              which has no checksum because it contains variable fields)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbValidateTableHeader (
    ACPI_TABLE_HEADER       *TableHeader)
{
    ACPI_NAME               Signature;


    ACPI_FUNCTION_NAME ("TbValidateTableHeader");


    /* Verify that this is a valid address */

    if (!AcpiOsReadable (TableHeader, sizeof (ACPI_TABLE_HEADER)))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Cannot read table header at %p\n", TableHeader));
        return (AE_BAD_ADDRESS);
    }

    /* Ensure that the signature is 4 ASCII characters */

    ACPI_MOVE_UNALIGNED32_TO_32 (&Signature, TableHeader->Signature);
    if (!AcpiUtValidAcpiName (Signature))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Table signature at %p [%p] has invalid characters\n",
            TableHeader, &Signature));

        ACPI_REPORT_WARNING (("Invalid table signature found: [%4.4s]\n",
            (char *) &Signature));
        ACPI_DUMP_BUFFER (TableHeader, sizeof (ACPI_TABLE_HEADER));
        return (AE_BAD_SIGNATURE);
    }

    /* Validate the table length */

    if (TableHeader->Length < sizeof (ACPI_TABLE_HEADER))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Invalid length in table header %p name %4.4s\n",
            TableHeader, (char *) &Signature));

        ACPI_REPORT_WARNING (("Invalid table header length (0x%X) found\n",
            (UINT32) TableHeader->Length));
        ACPI_DUMP_BUFFER (TableHeader, sizeof (ACPI_TABLE_HEADER));
        return (AE_BAD_HEADER);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbVerifyTableChecksum
 *
 * PARAMETERS:  *TableHeader            - ACPI table to verify
 *
 * RETURN:      8 bit checksum of table
 *
 * DESCRIPTION: Does an 8 bit checksum of table and returns status.  A correct
 *              table should have a checksum of 0.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbVerifyTableChecksum (
    ACPI_TABLE_HEADER       *TableHeader)
{
    UINT8                   Checksum;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("TbVerifyTableChecksum");


    /* Compute the checksum on the table */

    Checksum = AcpiTbChecksum (TableHeader, TableHeader->Length);

    /* Return the appropriate exception */

    if (Checksum)
    {
        ACPI_REPORT_WARNING (("Invalid checksum in table [%4.4s] (%02X, sum %02X is not zero)\n",
            TableHeader->Signature, (UINT32) TableHeader->Checksum, (UINT32) Checksum));

        Status = AE_BAD_CHECKSUM;
    }
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbChecksum
 *
 * PARAMETERS:  Buffer              - Buffer to checksum
 *              Length              - Size of the buffer
 *
 * RETURNS      8 bit checksum of buffer
 *
 * DESCRIPTION: Computes an 8 bit checksum of the buffer(length) and returns it.
 *
 ******************************************************************************/

UINT8
AcpiTbChecksum (
    void                    *Buffer,
    UINT32                  Length)
{
    const UINT8             *limit;
    const UINT8             *rover;
    UINT8                   sum = 0;


    if (Buffer && Length)
    {
        /*  Buffer and Length are valid   */

        limit = (UINT8 *) Buffer + Length;

        for (rover = Buffer; rover < limit; rover++)
        {
            sum = (UINT8) (sum + *rover);
        }
    }
    return (sum);
}


