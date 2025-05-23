
/******************************************************************************
 *
 * Module Name: exoparg3 - AML execution - opcodes with 3 arguments
 *              $Revision: 17 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/exoparg3.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */

#define __EXOPARG3_C__

#include "acpi.h"
#include "acinterp.h"
#include "acparser.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exoparg3")


/*!
 * Naming convention for AML interpreter execution routines.
 *
 * The routines that begin execution of AML opcodes are named with a common
 * convention based upon the number of arguments, the number of target operands,
 * and whether or not a value is returned:
 *
 *      AcpiExOpcode_xA_yT_zR
 *
 * Where:
 *
 * xA - ARGUMENTS:    The number of arguments (input operands) that are
 *                    required for this opcode type (1 through 6 args).
 * yT - TARGETS:      The number of targets (output operands) that are required
 *                    for this opcode type (0, 1, or 2 targets).
 * zR - RETURN VALUE: Indicates whether this opcode type returns a value
 *                    as the function return (0 or 1).
 *
 * The AcpiExOpcode* functions are called via the Dispatcher component with
 * fully resolved operands.
!*/


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_3A_0T_0R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Triadic operator (3 operands)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_3A_0T_0R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_SIGNAL_FATAL_INFO  *Fatal;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_3A_0T_0R", AcpiPsGetOpcodeName (WalkState->Opcode));


    switch (WalkState->Opcode)
    {

    case AML_FATAL_OP:          /* Fatal (FatalType  FatalCode  FatalArg)    */

        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "FatalOp: Type %X Code %X Arg %X <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n",
            (UINT32) Operand[0]->Integer.Value, (UINT32) Operand[1]->Integer.Value,
            (UINT32) Operand[2]->Integer.Value));


        Fatal = ACPI_MEM_ALLOCATE (sizeof (ACPI_SIGNAL_FATAL_INFO));
        if (Fatal)
        {
            Fatal->Type     = (UINT32) Operand[0]->Integer.Value;
            Fatal->Code     = (UINT32) Operand[1]->Integer.Value;
            Fatal->Argument = (UINT32) Operand[2]->Integer.Value;
        }

        /*
         * Always signal the OS!
         */
        Status = AcpiOsSignal (ACPI_SIGNAL_FATAL, Fatal);

        /* Might return while OS is shutting down, just continue */

        ACPI_MEM_FREE (Fatal);
        break;


    default:

        ACPI_REPORT_ERROR (("AcpiExOpcode_3A_0T_0R: Unknown opcode %X\n",
                WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


Cleanup:

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_3A_1T_1R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Triadic operator (3 operands)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_3A_1T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    char                    *Buffer;
    ACPI_STATUS             Status = AE_OK;
    ACPI_NATIVE_UINT        Index;
    ACPI_SIZE               Length;


    ACPI_FUNCTION_TRACE_STR ("ExOpcode_3A_1T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    switch (WalkState->Opcode)
    {
    case AML_MID_OP:        /* Mid  (Source[0], Index[1], Length[2], Result[3]) */

        /*
         * Create the return object.  The Source operand is guaranteed to be
         * either a String or a Buffer, so just use its type.
         */
        ReturnDesc = AcpiUtCreateInternalObject (ACPI_GET_OBJECT_TYPE (Operand[0]));
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* Get the Integer values from the objects */

        Index = (ACPI_NATIVE_UINT) Operand[1]->Integer.Value;
        Length = (ACPI_SIZE) Operand[2]->Integer.Value;

        /*
         * If the index is beyond the length of the String/Buffer, or if the
         * requested length is zero, return a zero-length String/Buffer
         */
        if ((Index < Operand[0]->String.Length) &&
            (Length > 0))
        {
            /* Truncate request if larger than the actual String/Buffer */

            if ((Index + Length) >
                Operand[0]->String.Length)
            {
                Length = (ACPI_SIZE) Operand[0]->String.Length - Index;
            }

            /* Allocate a new buffer for the String/Buffer */

            Buffer = ACPI_MEM_CALLOCATE ((ACPI_SIZE) Length + 1);
            if (!Buffer)
            {
                Status = AE_NO_MEMORY;
                goto Cleanup;
            }

            /* Copy the portion requested */

            ACPI_MEMCPY (Buffer, Operand[0]->String.Pointer + Index,
                         Length);

            /* Set the length of the new String/Buffer */

            ReturnDesc->String.Pointer = Buffer;
            ReturnDesc->String.Length = (UINT32) Length;
        }
        break;


    default:

        ACPI_REPORT_ERROR (("AcpiExOpcode_3A_0T_0R: Unknown opcode %X\n",
                WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }

    /* Store the result in the target */

    Status = AcpiExStore (ReturnDesc, Operand[3], WalkState);

Cleanup:

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    /* Set the return object and exit */

    if (!WalkState->ResultObj)
    {
        WalkState->ResultObj = ReturnDesc;
    }
    return_ACPI_STATUS (Status);
}


