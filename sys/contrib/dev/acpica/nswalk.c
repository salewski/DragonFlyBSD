/******************************************************************************
 *
 * Module Name: nswalk - Functions for walking the ACPI namespace
 *              $Revision: 34 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/nswalk.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */


#define __NSWALK_C__

#include "acpi.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nswalk")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetNextNode
 *
 * PARAMETERS:  Type                - Type of node to be searched for
 *              ParentNode          - Parent node whose children we are
 *                                     getting
 *              ChildNode           - Previous child that was found.
 *                                    The NEXT child will be returned
 *
 * RETURN:      ACPI_NAMESPACE_NODE - Pointer to the NEXT child or NULL if
 *                                    none is found.
 *
 * DESCRIPTION: Return the next peer node within the namespace.  If Handle
 *              is valid, Scope is ignored.  Otherwise, the first node
 *              within Scope is returned.
 *
 ******************************************************************************/

ACPI_NAMESPACE_NODE *
AcpiNsGetNextNode (
    ACPI_OBJECT_TYPE        Type,
    ACPI_NAMESPACE_NODE     *ParentNode,
    ACPI_NAMESPACE_NODE     *ChildNode)
{
    ACPI_NAMESPACE_NODE     *NextNode = NULL;


    ACPI_FUNCTION_ENTRY ();


    if (!ChildNode)
    {
        /* It's really the parent's _scope_ that we want */

        if (ParentNode->Child)
        {
            NextNode = ParentNode->Child;
        }
    }

    else
    {
        /* Start search at the NEXT node */

        NextNode = AcpiNsGetNextValidNode (ChildNode);
    }

    /* If any type is OK, we are done */

    if (Type == ACPI_TYPE_ANY)
    {
        /* NextNode is NULL if we are at the end-of-list */

        return (NextNode);
    }

    /* Must search for the node -- but within this scope only */

    while (NextNode)
    {
        /* If type matches, we are done */

        if (NextNode->Type == Type)
        {
            return (NextNode);
        }

        /* Otherwise, move on to the next node */

        NextNode = AcpiNsGetNextValidNode (NextNode);
    }

    /* Not found */

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsWalkNamespace
 *
 * PARAMETERS:  Type                - ACPI_OBJECT_TYPE to search for
 *              StartNode           - Handle in namespace where search begins
 *              MaxDepth            - Depth to which search is to reach
 *              UnlockBeforeCallback- Whether to unlock the NS before invoking
 *                                    the callback routine
 *              UserFunction        - Called when an object of "Type" is found
 *              Context             - Passed to user function
 *              ReturnValue         - from the UserFunction if terminated early.
 *                                    Otherwise, returns NULL.
 * RETURNS:     Status
 *
 * DESCRIPTION: Performs a modified depth-first walk of the namespace tree,
 *              starting (and ending) at the node specified by StartHandle.
 *              The UserFunction is called whenever a node that matches
 *              the type parameter is found.  If the user function returns
 *              a non-zero value, the search is terminated immediately and this
 *              value is returned to the caller.
 *
 *              The point of this procedure is to provide a generic namespace
 *              walk routine that can be called from multiple places to
 *              provide multiple services;  the User Function can be tailored
 *              to each task, whether it is a print function, a compare
 *              function, etc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsWalkNamespace (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             StartNode,
    UINT32                  MaxDepth,
    BOOLEAN                 UnlockBeforeCallback,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_NAMESPACE_NODE     *ParentNode;
    ACPI_OBJECT_TYPE        ChildType;
    UINT32                  Level;


    ACPI_FUNCTION_TRACE ("NsWalkNamespace");


    /* Special case for the namespace Root Node */

    if (StartNode == ACPI_ROOT_OBJECT)
    {
        StartNode = AcpiGbl_RootNode;
    }

    /* Null child means "get first node" */

    ParentNode  = StartNode;
    ChildNode   = 0;
    ChildType   = ACPI_TYPE_ANY;
    Level       = 1;

    /*
     * Traverse the tree of nodes until we bubble back up to where we
     * started. When Level is zero, the loop is done because we have
     * bubbled up to (and passed) the original parent handle (StartEntry)
     */
    while (Level > 0)
    {
        /* Get the next node in this scope.  Null if not found */

        Status = AE_OK;
        ChildNode = AcpiNsGetNextNode (ACPI_TYPE_ANY, ParentNode, ChildNode);
        if (ChildNode)
        {
            /*
             * Found node, Get the type if we are not
             * searching for ANY
             */
            if (Type != ACPI_TYPE_ANY)
            {
                ChildType = ChildNode->Type;
            }

            if (ChildType == Type)
            {
                /*
                 * Found a matching node, invoke the user
                 * callback function
                 */
                if (UnlockBeforeCallback)
                {
                    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
                    if (ACPI_FAILURE (Status))
                    {
                        return_ACPI_STATUS (Status);
                    }
                }

                Status = UserFunction (ChildNode, Level,
                                        Context, ReturnValue);

                if (UnlockBeforeCallback)
                {
                    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
                    if (ACPI_FAILURE (Status))
                    {
                        return_ACPI_STATUS (Status);
                    }
                }

                switch (Status)
                {
                case AE_OK:
                case AE_CTRL_DEPTH:

                    /* Just keep going */
                    break;

                case AE_CTRL_TERMINATE:

                    /* Exit now, with OK status */

                    return_ACPI_STATUS (AE_OK);

                default:

                    /* All others are valid exceptions */

                    return_ACPI_STATUS (Status);
                }
            }

            /*
             * Depth first search:
             * Attempt to go down another level in the namespace
             * if we are allowed to.  Don't go any further if we
             * have reached the caller specified maximum depth
             * or if the user function has specified that the
             * maximum depth has been reached.
             */
            if ((Level < MaxDepth) && (Status != AE_CTRL_DEPTH))
            {
                if (AcpiNsGetNextNode (ACPI_TYPE_ANY, ChildNode, 0))
                {
                    /*
                     * There is at least one child of this
                     * node, visit the onde
                     */
                    Level++;
                    ParentNode    = ChildNode;
                    ChildNode     = 0;
                }
            }
        }
        else
        {
            /*
             * No more children of this node (AcpiNsGetNextNode
             * failed), go back upwards in the namespace tree to
             * the node's parent.
             */
            Level--;
            ChildNode = ParentNode;
            ParentNode = AcpiNsGetParentNode (ParentNode);
        }
    }

    /* Complete walk, not terminated by user function */

    return_ACPI_STATUS (AE_OK);
}


