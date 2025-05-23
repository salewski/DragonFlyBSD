/*******************************************************************************
 *
 * Module Name: nsxfobj - Public interfaces to the ACPI subsystem
 *                         ACPI Object oriented interfaces
 *              $Revision: 116 $
 *
 ******************************************************************************/

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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/nsxfobj.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */


#define __NSXFOBJ_C__

#include "acpi.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsxfobj")

/*******************************************************************************
 *
 * FUNCTION:    AcpiGetType
 *
 * PARAMETERS:  Handle          - Handle of object whose type is desired
 *              *RetType        - Where the type will be placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This routine returns the type associatd with a particular handle
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetType (
    ACPI_HANDLE             Handle,
    ACPI_OBJECT_TYPE        *RetType)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Parameter Validation */

    if (!RetType)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * Special case for the predefined Root Node
     * (return type ANY)
     */
    if (Handle == ACPI_ROOT_OBJECT)
    {
        *RetType = ACPI_TYPE_ANY;
        return (AE_OK);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Convert and validate the handle */

    Node = AcpiNsMapHandleToNode (Handle);
    if (!Node)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return (AE_BAD_PARAMETER);
    }

    *RetType = Node->Type;


    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetParent
 *
 * PARAMETERS:  Handle          - Handle of object whose parent is desired
 *              RetHandle       - Where the parent handle will be placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Returns a handle to the parent of the object represented by
 *              Handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetParent (
    ACPI_HANDLE             Handle,
    ACPI_HANDLE             *RetHandle)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    if (!RetHandle)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Special case for the predefined Root Node (no parent) */

    if (Handle == ACPI_ROOT_OBJECT)
    {
        return (AE_NULL_ENTRY);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Convert and validate the handle */

    Node = AcpiNsMapHandleToNode (Handle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Get the parent entry */

    *RetHandle =
        AcpiNsConvertEntryToHandle (AcpiNsGetParentNode (Node));

    /* Return exception if parent is null */

    if (!AcpiNsGetParentNode (Node))
    {
        Status = AE_NULL_ENTRY;
    }


UnlockAndExit:

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetNextObject
 *
 * PARAMETERS:  Type            - Type of object to be searched for
 *              Parent          - Parent object whose children we are getting
 *              LastChild       - Previous child that was found.
 *                                The NEXT child will be returned
 *              RetHandle       - Where handle to the next object is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the next peer object within the namespace.  If Handle is
 *              valid, Scope is ignored.  Otherwise, the first object within
 *              Scope is returned.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetNextObject (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             Parent,
    ACPI_HANDLE             Child,
    ACPI_HANDLE             *RetHandle)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_NAMESPACE_NODE     *ParentNode = NULL;
    ACPI_NAMESPACE_NODE     *ChildNode = NULL;


    /* Parameter validation */

    if (Type > ACPI_TYPE_EXTERNAL_MAX)
    {
        return (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* If null handle, use the parent */

    if (!Child)
    {
        /* Start search at the beginning of the specified scope */

        ParentNode = AcpiNsMapHandleToNode (Parent);
        if (!ParentNode)
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }
    }
    else
    {
        /* Non-null handle, ignore the parent */
        /* Convert and validate the handle */

        ChildNode = AcpiNsMapHandleToNode (Child);
        if (!ChildNode)
        {
            Status = AE_BAD_PARAMETER;
            goto UnlockAndExit;
        }
    }

    /* Internal function does the real work */

    Node = AcpiNsGetNextNode (Type, ParentNode, ChildNode);
    if (!Node)
    {
        Status = AE_NOT_FOUND;
        goto UnlockAndExit;
    }

    if (RetHandle)
    {
        *RetHandle = AcpiNsConvertEntryToHandle (Node);
    }


UnlockAndExit:

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}


