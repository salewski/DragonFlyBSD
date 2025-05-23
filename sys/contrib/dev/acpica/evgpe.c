/******************************************************************************
 *
 * Module Name: evgpe - General Purpose Event handling and dispatch
 *              $Revision: 12 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/evgpe.c,v 1.1 2003/09/24 03:32:15 drhodus Exp $                                                               */

#include "acpi.h"
#include "acevents.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evgpe")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGetGpeEventInfo
 *
 * PARAMETERS:  GpeNumber       - Raw GPE number
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Returns the EventInfo struct
 *              associated with this GPE.
 *
 * TBD: this function will go away when full support of GPE block devices
 *      is implemented!
 *
 ******************************************************************************/

ACPI_GPE_EVENT_INFO *
AcpiEvGetGpeEventInfo (
    UINT32                  GpeNumber)
{
    ACPI_GPE_BLOCK_INFO     *GpeBlock;


    /* Examine GPE Block 0 */

    GpeBlock = AcpiGbl_GpeBlockListHead;
    if (!GpeBlock)
    {
        return (NULL);
    }

    if ((GpeNumber >= GpeBlock->BlockBaseNumber) &&
        (GpeNumber < GpeBlock->BlockBaseNumber + (GpeBlock->RegisterCount * 8)))
    {
        return (&GpeBlock->EventInfo[GpeNumber - GpeBlock->BlockBaseNumber]);
    }

    /* Examine GPE Block 1 */

    GpeBlock = GpeBlock->Next;
    if (!GpeBlock)
    {
        return (NULL);
    }

    if ((GpeNumber >= GpeBlock->BlockBaseNumber) &&
        (GpeNumber < GpeBlock->BlockBaseNumber + (GpeBlock->RegisterCount * 8)))
    {
        return (&GpeBlock->EventInfo[GpeNumber - GpeBlock->BlockBaseNumber]);
    }

    return (NULL);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeDetect
 *
 * PARAMETERS:  None
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Detect if any GP events have occurred.  This function is
 *              executed at interrupt level.
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDetect (void)
{
    UINT32                  IntStatus = ACPI_INTERRUPT_NOT_HANDLED;
    UINT32                  i;
    UINT32                  j;
    UINT8                   EnabledStatusByte;
    UINT8                   BitMask;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    UINT32                  InValue;
    ACPI_STATUS             Status;
    ACPI_GPE_BLOCK_INFO     *GpeBlock;


    ACPI_FUNCTION_NAME ("EvGpeDetect");


    /* Examine all GPE blocks attached to this interrupt level */

    GpeBlock = AcpiGbl_GpeBlockListHead;
    while (GpeBlock)
    {
        /*
         * Read all of the 8-bit GPE status and enable registers
         * in this GPE block, saving all of them.
         * Find all currently active GP events.
         */
        for (i = 0; i < GpeBlock->RegisterCount; i++)
        {
            /* Get the next status/enable pair */

            GpeRegisterInfo = &GpeBlock->RegisterInfo[i];

            Status = AcpiHwLowLevelRead (ACPI_GPE_REGISTER_WIDTH, &InValue,
                        &GpeRegisterInfo->StatusAddress, 0);
            GpeRegisterInfo->Status = (UINT8) InValue;
            if (ACPI_FAILURE (Status))
            {
                return (ACPI_INTERRUPT_NOT_HANDLED);
            }

            Status = AcpiHwLowLevelRead (ACPI_GPE_REGISTER_WIDTH, &InValue,
                        &GpeRegisterInfo->EnableAddress, 0);
            GpeRegisterInfo->Enable = (UINT8) InValue;
            if (ACPI_FAILURE (Status))
            {
                return (ACPI_INTERRUPT_NOT_HANDLED);
            }

            ACPI_DEBUG_PRINT ((ACPI_DB_INTERRUPTS,
                "GPE block at %8.8X%8.8X - Values: Enable %02X Status %02X\n",
                ACPI_HIDWORD (ACPI_GET_ADDRESS (GpeRegisterInfo->EnableAddress.Address)),
                ACPI_LODWORD (ACPI_GET_ADDRESS (GpeRegisterInfo->EnableAddress.Address)),
                GpeRegisterInfo->Enable,
                GpeRegisterInfo->Status));

            /* First check if there is anything active at all in this register */

            EnabledStatusByte = (UINT8) (GpeRegisterInfo->Status &
                                         GpeRegisterInfo->Enable);
            if (!EnabledStatusByte)
            {
                /* No active GPEs in this register, move on */

                continue;
            }

            /* Now look at the individual GPEs in this byte register */

            for (j = 0, BitMask = 1; j < ACPI_GPE_REGISTER_WIDTH; j++, BitMask <<= 1)
            {
                /* Examine one GPE bit */

                if (EnabledStatusByte & BitMask)
                {
                    /*
                     * Found an active GPE. Dispatch the event to a handler
                     * or method.
                     */
                    IntStatus |= AcpiEvGpeDispatch (
                                    &GpeBlock->EventInfo[(i * ACPI_GPE_REGISTER_WIDTH) +j]);
                }
            }
        }

        GpeBlock = GpeBlock->Next;
    }

    return (IntStatus);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAsynchExecuteGpeMethod
 *
 * PARAMETERS:  GpeEventInfo - Info for this GPE
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform the actual execution of a GPE control method.  This
 *              function is called from an invocation of AcpiOsQueueForExecution
 *              (and therefore does NOT execute at interrupt level) so that
 *              the control method itself is not executed in the context of
 *              the SCI interrupt handler.
 *
 ******************************************************************************/

static void ACPI_SYSTEM_XFACE
AcpiEvAsynchExecuteGpeMethod (
    void                    *Context)
{
    ACPI_GPE_EVENT_INFO     *GpeEventInfo = (void *) Context;
    UINT32                  GpeNumber = 0;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvAsynchExecuteGpeMethod");


    /*
     * Take a snapshot of the GPE info for this level - we copy the
     * info to prevent a race condition with RemoveHandler.
     */
    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    Status = AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    if (GpeEventInfo->MethodNode)
    {
        /*
         * Invoke the GPE Method (_Lxx, _Exx):
         * (Evaluate the _Lxx/_Exx control method that corresponds to this GPE.)
         */
        Status = AcpiNsEvaluateByHandle (GpeEventInfo->MethodNode, NULL, NULL);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("%s while evaluating method [%4.4s] for GPE[%2.2X]\n",
                AcpiFormatException (Status),
                GpeEventInfo->MethodNode->Name.Ascii, GpeNumber));
        }
    }

    if (GpeEventInfo->Type & ACPI_EVENT_LEVEL_TRIGGERED)
    {
        /*
         * GPE is level-triggered, we clear the GPE status bit after handling
         * the event.
         */
        Status = AcpiHwClearGpe (GpeEventInfo);
        if (ACPI_FAILURE (Status))
        {
            return_VOID;
        }
    }

    /* Enable this GPE */

    (void) AcpiHwEnableGpe (GpeEventInfo);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeDispatch
 *
 * PARAMETERS:  GpeEventInfo   - info for this GPE
 *
 * RETURN:      INTERRUPT_HANDLED or INTERRUPT_NOT_HANDLED
 *
 * DESCRIPTION: Dispatch a General Purpose Event to either a function (e.g. EC)
 *              or method (e.g. _Lxx/_Exx) handler.  This function executes
 *              at interrupt level.
 *
 ******************************************************************************/

UINT32
AcpiEvGpeDispatch (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo)
{
    UINT32                  GpeNumber = 0; /* TBD: remove */
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("EvGpeDispatch");


    /*
     * If edge-triggered, clear the GPE status bit now.  Note that
     * level-triggered events are cleared after the GPE is serviced.
     */
    if (GpeEventInfo->Type & ACPI_EVENT_EDGE_TRIGGERED)
    {
        Status = AcpiHwClearGpe (GpeEventInfo);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: Unable to clear GPE[%2.2X]\n",
                GpeNumber));
            return_VALUE (ACPI_INTERRUPT_NOT_HANDLED);
        }
    }

    /*
     * Dispatch the GPE to either an installed handler, or the control
     * method associated with this GPE (_Lxx or _Exx).
     * If a handler exists, we invoke it and do not attempt to run the method.
     * If there is neither a handler nor a method, we disable the level to
     * prevent further events from coming in here.
     */
    if (GpeEventInfo->Handler)
    {
        /* Invoke the installed handler (at interrupt level) */

        GpeEventInfo->Handler (GpeEventInfo->Context);
    }
    else if (GpeEventInfo->MethodNode)
    {
        /*
         * Disable GPE, so it doesn't keep firing before the method has a
         * chance to run.
         */
        Status = AcpiHwDisableGpe (GpeEventInfo);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: Unable to disable GPE[%2.2X]\n",
                GpeNumber));
            return_VALUE (ACPI_INTERRUPT_NOT_HANDLED);
        }

        /*
         * Execute the method associated with the GPE.
         */
        if (ACPI_FAILURE (AcpiOsQueueForExecution (OSD_PRIORITY_GPE,
                                AcpiEvAsynchExecuteGpeMethod,
                                GpeEventInfo)))
        {
            ACPI_REPORT_ERROR ((
                "AcpiEvGpeDispatch: Unable to queue handler for GPE[%2.2X], event is disabled\n",
                GpeNumber));
        }
    }
    else
    {
        /* No handler or method to run! */

        ACPI_REPORT_ERROR ((
            "AcpiEvGpeDispatch: No handler or method for GPE[%2.2X], disabling event\n",
            GpeNumber));

        /*
         * Disable the GPE.  The GPE will remain disabled until the ACPI
         * Core Subsystem is restarted, or the handler is reinstalled.
         */
        Status = AcpiHwDisableGpe (GpeEventInfo);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: Unable to disable GPE[%2.2X]\n",
                GpeNumber));
            return_VALUE (ACPI_INTERRUPT_NOT_HANDLED);
        }
    }

    /*
     * It is now safe to clear level-triggered evnets.
     */
    if (GpeEventInfo->Type & ACPI_EVENT_LEVEL_TRIGGERED)
    {
        Status = AcpiHwClearGpe (GpeEventInfo);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("AcpiEvGpeDispatch: Unable to clear GPE[%2.2X]\n",
                GpeNumber));
            return_VALUE (ACPI_INTERRUPT_NOT_HANDLED);
        }
    }

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

