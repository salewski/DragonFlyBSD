
/******************************************************************************
 *
 * Module Name: exresnte - AML Interpreter object resolution
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/exresnte.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */

#define __EXRESNTE_C__

#include "acpi.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exresnte")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveNodeToValue
 *
 * PARAMETERS:  ObjectPtr       - Pointer to a location that contains
 *                                a pointer to a NS node, and will receive a
 *                                pointer to the resolved object.
 *              WalkState       - Current state.  Valid only if executing AML
 *                                code.  NULL if simply resolving an object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Resolve a Namespace node to a valued object
 *
 * Note: for some of the data types, the pointer attached to the Node
 * can be either a pointer to an actual internal object or a pointer into the
 * AML stream itself.  These types are currently:
 *
 *      ACPI_TYPE_INTEGER
 *      ACPI_TYPE_STRING
 *      ACPI_TYPE_BUFFER
 *      ACPI_TYPE_MUTEX
 *      ACPI_TYPE_PACKAGE
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveNodeToValue (
    ACPI_NAMESPACE_NODE     **ObjectPtr,
    ACPI_WALK_STATE         *WalkState)

{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *SourceDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OBJECT_TYPE        EntryType;


    ACPI_FUNCTION_TRACE ("ExResolveNodeToValue");


    /*
     * The stack pointer points to a ACPI_NAMESPACE_NODE (Node).  Get the
     * object that is attached to the Node.
     */
    Node       = *ObjectPtr;
    SourceDesc = AcpiNsGetAttachedObject (Node);
    EntryType  = AcpiNsGetType ((ACPI_HANDLE) Node);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Entry=%p SourceDesc=%p [%s]\n",
         Node, SourceDesc, AcpiUtGetTypeName (EntryType)));

    if (EntryType == ACPI_TYPE_LOCAL_ALIAS)
    {
        /* There is always exactly one level of indirection */

        Node       = (ACPI_NAMESPACE_NODE *) Node->Object;
        SourceDesc = AcpiNsGetAttachedObject (Node);
        EntryType  = AcpiNsGetType ((ACPI_HANDLE) Node);
        *ObjectPtr = Node;
    }

    /*
     * Several object types require no further processing:
     * 1) Devices rarely have an attached object, return the Node
     * 2) Method locals and arguments have a pseudo-Node
     */
    if (EntryType == ACPI_TYPE_DEVICE ||
        (Node->Flags & (ANOBJ_METHOD_ARG | ANOBJ_METHOD_LOCAL)))
    {
        return_ACPI_STATUS (AE_OK);
    }

    if (!SourceDesc)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No object attached to node %p\n",
            Node));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    /*
     * Action is based on the type of the Node, which indicates the type
     * of the attached object or pointer
     */
    switch (EntryType)
    {
    case ACPI_TYPE_PACKAGE:

        if (ACPI_GET_OBJECT_TYPE (SourceDesc) != ACPI_TYPE_PACKAGE)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Object not a Package, type %s\n",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        Status = AcpiDsGetPackageArguments (SourceDesc);
        if (ACPI_SUCCESS (Status))
        {
            /* Return an additional reference to the object */

            ObjDesc = SourceDesc;
            AcpiUtAddReference (ObjDesc);
        }
        break;


    case ACPI_TYPE_BUFFER:

        if (ACPI_GET_OBJECT_TYPE (SourceDesc) != ACPI_TYPE_BUFFER)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Object not a Buffer, type %s\n",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        Status = AcpiDsGetBufferArguments (SourceDesc);
        if (ACPI_SUCCESS (Status))
        {
            /* Return an additional reference to the object */

            ObjDesc = SourceDesc;
            AcpiUtAddReference (ObjDesc);
        }
        break;


    case ACPI_TYPE_STRING:

        if (ACPI_GET_OBJECT_TYPE (SourceDesc) != ACPI_TYPE_STRING)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Object not a String, type %s\n",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;


    case ACPI_TYPE_INTEGER:

        if (ACPI_GET_OBJECT_TYPE (SourceDesc) != ACPI_TYPE_INTEGER)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Object not a Integer, type %s\n",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;


    case ACPI_TYPE_BUFFER_FIELD:
    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "FieldRead Node=%p SourceDesc=%p Type=%X\n",
            Node, SourceDesc, EntryType));

        Status = AcpiExReadDataFromField (WalkState, SourceDesc, &ObjDesc);
        break;

    /*
     * For these objects, just return the object attached to the Node
     */
    case ACPI_TYPE_MUTEX:
    case ACPI_TYPE_METHOD:
    case ACPI_TYPE_POWER:
    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_THERMAL:
    case ACPI_TYPE_EVENT:
    case ACPI_TYPE_REGION:

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;


    /* TYPE_ANY is untyped, and thus there is no object associated with it */

    case ACPI_TYPE_ANY:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Untyped entry %p, no attached object!\n",
            Node));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);  /* Cannot be AE_TYPE */


    case ACPI_TYPE_LOCAL_REFERENCE:

        /* No named references are allowed here */

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unsupported Reference opcode %X\n",
            SourceDesc->Reference.Opcode));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);


    /* Default case is for unknown types */

    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Node %p - Unknown object type %X\n",
            Node, EntryType));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);

    } /* switch (EntryType) */


    /* Put the object descriptor on the stack */

    *ObjectPtr = (void *) ObjDesc;
    return_ACPI_STATUS (Status);
}


