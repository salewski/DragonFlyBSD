/* $DragonFly: src/sys/contrib/dev/acpica/Attic/hwacpi.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */
/******************************************************************************
 *
 * Module Name: hwacpi - ACPI Hardware Initialization/Mode Interface
 *              $Revision: 62 $
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

#define __HWACPI_C__

#include "acpi.h"


#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwacpi")


/******************************************************************************
 *
 * FUNCTION:    AcpiHwInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize and validate various ACPI registers
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwInitialize (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("HwInitialize");


    /* We must have the ACPI tables by the time we get here */

    if (!AcpiGbl_FADT)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "A FADT is not loaded\n"));

        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /* Sanity check the FADT for valid values */

    Status = AcpiUtValidateFadt ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwSetMode
 *
 * PARAMETERS:  Mode            - SYS_MODE_ACPI or SYS_MODE_LEGACY
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transitions the system into the requested mode.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwSetMode (
    UINT32                  Mode)
{

    ACPI_STATUS             Status;
    UINT32                  Retry;


    ACPI_FUNCTION_TRACE ("HwSetMode");

    /*
     * ACPI 2.0 clarified that if SMI_CMD in FADT is zero,
     * system does not support mode transition.
     */
    if (!AcpiGbl_FADT->SmiCmd)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No SMI_CMD in FADT, mode transition failed.\n"));
        return_ACPI_STATUS (AE_NO_HARDWARE_RESPONSE);
    }

    /*
     * ACPI 2.0 clarified the meaning of ACPI_ENABLE and ACPI_DISABLE
     * in FADT: If it is zero, enabling or disabling is not supported.
     * As old systems may have used zero for mode transition,
     * we make sure both the numbers are zero to determine these
     * transitions are not supported.
     */
    if (!AcpiGbl_FADT->AcpiEnable && !AcpiGbl_FADT->AcpiDisable)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "No mode transition supported in this system.\n"));
        return_ACPI_STATUS (AE_OK);
    }

    switch (Mode)
    {
    case ACPI_SYS_MODE_ACPI:

        /* BIOS should have disabled ALL fixed and GP events */

        Status = AcpiOsWritePort (AcpiGbl_FADT->SmiCmd,
                        (ACPI_INTEGER) AcpiGbl_FADT->AcpiEnable, 8);
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Attempting to enable ACPI mode\n"));
        break;

    case ACPI_SYS_MODE_LEGACY:

        /*
         * BIOS should clear all fixed status bits and restore fixed event
         * enable bits to default
         */
        Status = AcpiOsWritePort (AcpiGbl_FADT->SmiCmd,
                    (ACPI_INTEGER) AcpiGbl_FADT->AcpiDisable, 8);
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                    "Attempting to enable Legacy (non-ACPI) mode\n"));
        break;

    default:
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Some hardware takes a LONG time to switch modes. Give them 3 sec to
     * do so, but allow faster systems to proceed more quickly.
     */
    Retry = 3000;
    while (Retry)
    {
        Status = AE_NO_HARDWARE_RESPONSE;

        if (AcpiHwGetMode() == Mode)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Mode %X successfully enabled\n", Mode));
            Status = AE_OK;
            break;
        }
        AcpiOsStall(1000);
        Retry--;
    }

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiHwGetMode
 *
 * PARAMETERS:  none
 *
 * RETURN:      SYS_MODE_ACPI or SYS_MODE_LEGACY
 *
 * DESCRIPTION: Return current operating state of system.  Determined by
 *              querying the SCI_EN bit.
 *
 ******************************************************************************/

UINT32
AcpiHwGetMode (void)
{
    ACPI_STATUS             Status;
    UINT32                  Value;


    ACPI_FUNCTION_TRACE ("HwGetMode");

    Status = AcpiGetRegister (ACPI_BITREG_SCI_ENABLE, &Value, ACPI_MTX_LOCK);
    if (ACPI_FAILURE (Status))
    {
        return_VALUE (ACPI_SYS_MODE_LEGACY);
    }

    if (Value)
    {
        return_VALUE (ACPI_SYS_MODE_ACPI);
    }
    else
    {
        return_VALUE (ACPI_SYS_MODE_LEGACY);
    }
}
