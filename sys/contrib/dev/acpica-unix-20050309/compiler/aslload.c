/******************************************************************************
 *
 * Module Name: dswload - Dispatcher namespace load callbacks
 *              $Revision: 63 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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

#define __ASLLOAD_C__

#include "aslcompiler.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acnamesp.h"

#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslload")


/*******************************************************************************
 *
 * FUNCTION:    LdLoadNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform a walk of the parse tree that in turn loads all of the
 *              named ASL/AML objects into the namespace.  The namespace is
 *              constructed in order to resolve named references and references
 *              to named fields within resource templates/descriptors.
 *
 ******************************************************************************/

ACPI_STATUS
LdLoadNamespace (
    ACPI_PARSE_OBJECT       *RootOp)
{
    ACPI_WALK_STATE         *WalkState;


    DbgPrint (ASL_DEBUG_OUTPUT, "\nCreating namespace\n\n");

    /* Create a new walk state */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        return AE_NO_MEMORY;
    }

    /* Perform the walk of the parse tree */

    TrWalkParseTree (RootOp, ASL_WALK_VISIT_TWICE, LdNamespace1Begin,
                        LdNamespace1End, WalkState);

    /* Dump the namespace if debug is enabled */

    AcpiNsDumpTables (ACPI_NS_ALL, ACPI_UINT32_MAX);
    return AE_OK;
}


/*******************************************************************************
 *
 * FUNCTION:    LdLoadFieldElements
 *
 * PARAMETERS:  Op          - Parent node (Field)
 *              WalkState       - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter the named elements of the field (children of the parent)
 *              into the namespace.
 *
 ******************************************************************************/

ACPI_STATUS
LdLoadFieldElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Child = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Get the first named field element */

    switch (Op->Asl.AmlOpcode)
    {
    case AML_BANK_FIELD_OP:

        Child = UtGetArg (Op, 6);
        break;

    case AML_INDEX_FIELD_OP:

        Child = UtGetArg (Op, 5);
        break;

    case AML_FIELD_OP:

        Child = UtGetArg (Op, 4);
        break;

    default:
        /* No other opcodes should arrive here */
        return (AE_BAD_PARAMETER);
    }

    /* Enter all elements into the namespace */

    while (Child)
    {
        switch (Child->Asl.AmlOpcode)
        {
        case AML_INT_RESERVEDFIELD_OP:
        case AML_INT_ACCESSFIELD_OP:

            break;

        default:

            Status = AcpiNsLookup (WalkState->ScopeInfo, Child->Asl.Value.String,
                            ACPI_TYPE_LOCAL_REGION_FIELD, ACPI_IMODE_LOAD_PASS1,
                            ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE | ACPI_NS_ERROR_IF_FOUND,
                            NULL, &Node);
            if (ACPI_FAILURE (Status))
            {
                if (Status != AE_ALREADY_EXISTS)
                {
                     return (Status);
                }

                /*
                 * The name already exists in this scope
                 * But continue processing the elements
                 */
                AslError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Child, Child->Asl.Value.String);
            }
            else
            {
                Child->Asl.Node = Node;
                Node->Object = (ACPI_OPERAND_OBJECT *) Child;
            }
            break;
        }
        Child = Child->Asl.Next;
    }
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdLoadResourceElements
 *
 * PARAMETERS:  Op          - Parent node (Resource Descriptor)
 *              WalkState       - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter the named elements of the resource descriptor (children
 *              of the parent) into the namespace.
 *
 * NOTE: In the real AML namespace, these named elements never exist.  But
 *       we simply use the namespace here as a symbol table so we can look
 *       them up as they are referenced.
 *
 ******************************************************************************/

ACPI_STATUS
LdLoadResourceElements (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *InitializerOp = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /*
     * Enter the resouce name into the namespace
     * This opens a scope
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Asl.Namepath,
                    ACPI_TYPE_LOCAL_RESOURCE, ACPI_IMODE_LOAD_PASS1, ACPI_NS_NO_UPSEARCH,
                    WalkState, &Node);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * Now enter the predefined fields, for easy lookup when referenced
     * by the source ASL
     */
    InitializerOp = ASL_GET_CHILD_NODE (Op);
    while (InitializerOp)
    {

        if (InitializerOp->Asl.ExternalName)
        {
            Status = AcpiNsLookup (WalkState->ScopeInfo,
                            InitializerOp->Asl.ExternalName,
                            ACPI_TYPE_LOCAL_RESOURCE_FIELD,
                            ACPI_IMODE_LOAD_PASS1, ACPI_NS_NO_UPSEARCH | ACPI_NS_DONT_OPEN_SCOPE,
                            NULL, &Node);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /*
             * Store the field offset in the namespace node so it
             * can be used when the field is referenced
             */
            Node->OwnerId = (UINT16) InitializerOp->Asl.Value.Integer;
            InitializerOp->Asl.Node = Node;
            Node->Object = (ACPI_OPERAND_OBJECT *) InitializerOp;

            /* Pass thru the field type (Bitfield or Bytefield) */

            if (InitializerOp->Asl.CompileFlags & NODE_IS_BIT_OFFSET)
            {
                Node->Flags |= ANOBJ_IS_BIT_OFFSET;
            }
        }
        InitializerOp = ASL_GET_PEER_NODE (InitializerOp);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LdNamespace1Begin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the parse tree walk.  If this
 *              is a named AML opcode, enter into the namespace
 *
 ******************************************************************************/

ACPI_STATUS
LdNamespace1Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_OBJECT_TYPE        ActualObjectType = ACPI_TYPE_ANY;
    char                    *Path;
    UINT32                  Flags = ACPI_NS_NO_UPSEARCH;
    ACPI_PARSE_OBJECT       *Arg;
    UINT32                  i;


    ACPI_FUNCTION_NAME ("LdNamespace1Begin");
    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Op %p [%s]\n",
        Op, Op->Asl.ParseOpName));


    /*
     * We are only interested in opcodes that have an associated name
     * (or multiple names)
     */
    switch (Op->Asl.AmlOpcode)
    {
    case AML_BANK_FIELD_OP:
    case AML_INDEX_FIELD_OP:
    case AML_FIELD_OP:

        Status = LdLoadFieldElements (Op, WalkState);
        return (Status);

    default:

        /* All other opcodes go below */
        break;
    }

    /* Check if this object has already been installed in the namespace */

    if (Op->Asl.Node)
    {
        return (AE_OK);
    }

    Path = Op->Asl.Namepath;
    if (!Path)
    {
        return (AE_OK);
    }

    /* Map the raw opcode into an internal object type */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_NAME:

        Arg = Op->Asl.Child;        /* Get the NameSeg/NameString node */
        Arg = Arg->Asl.Next;        /* First peer is the object to be associated with the name */

        /* Get the data type associated with the named object, not the name itself */

        /* Log2 loop to convert from Btype (binary) to Etype (encoded) */

        ObjectType = 1;
        for (i = 1; i < Arg->Asl.AcpiBtype; i *= 2)
        {
            ObjectType++;
        }
        break;


    case PARSEOP_EXTERNAL:

        /*
         * "External" simply enters a name and type into the namespace.
         * We must be careful to not open a new scope, however, no matter
         * what type the external name refers to (e.g., a method)
         *
         * first child is name, next child is ObjectType
         */
        ActualObjectType = (UINT8) Op->Asl.Child->Asl.Next->Asl.Value.Integer;
        ObjectType = ACPI_TYPE_ANY;
        break;


    case PARSEOP_DEFAULT_ARG:

        if(Op->Asl.CompileFlags == NODE_IS_RESOURCE_DESC)
        {
            Status = LdLoadResourceElements (Op, WalkState);
            goto Exit;
        }

        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
        break;


    case PARSEOP_SCOPE:

        /*
         * The name referenced by Scope(Name) must already exist at this point.
         * In other words, forward references for Scope() are not supported.
         * The only real reason for this is that the MS interpreter cannot
         * handle this case.  Perhaps someday this case can go away.
         */
        Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
                        ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT, WalkState, &(Node));
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_NOT_FOUND)
            {
                /* The name was not found, go ahead and create it */

                Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_LOCAL_SCOPE,
                                ACPI_IMODE_LOAD_PASS1, Flags, WalkState, &(Node));

                /*
                 * However, this is an error -- primarily because the MS
                 * interpreter can't handle a forward reference from the
                 * Scope() operator.
                 */
                AslError (ASL_ERROR, ASL_MSG_NOT_FOUND, Op, Op->Asl.ExternalName);
                AslError (ASL_ERROR, ASL_MSG_SCOPE_FWD_REF, Op, Op->Asl.ExternalName);
                goto FinishNode;
            }

            AslCoreSubsystemError (Op, Status, "Failure from lookup\n", FALSE);
            goto Exit;
        }

        /* We found a node with this name, now check the type */

        switch (Node->Type)
        {
        case ACPI_TYPE_LOCAL_SCOPE:
        case ACPI_TYPE_DEVICE:
        case ACPI_TYPE_POWER:
        case ACPI_TYPE_PROCESSOR:
        case ACPI_TYPE_THERMAL:

            /* These are acceptable types - they all open a new scope */
            break;

        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_STRING:
        case ACPI_TYPE_BUFFER:

            /*
             * These types we will allow, but we will change the type.  This
             * enables some existing code of the form:
             *
             *  Name (DEB, 0)
             *  Scope (DEB) { ... }
             *
             * Which is used to workaround the fact that the MS interpreter
             * does not allow Scope() forward references.
             */
            sprintf (MsgBuffer, "%s [%s], changing type to [Scope]",
                Op->Asl.ExternalName, AcpiUtGetTypeName (Node->Type));
            AslError (ASL_REMARK, ASL_MSG_SCOPE_TYPE, Op, MsgBuffer);

            /*
             * Switch the type to scope, open the new scope
             */
            Node->Type = ACPI_TYPE_LOCAL_SCOPE;
            Status = AcpiDsScopeStackPush (Node, ACPI_TYPE_LOCAL_SCOPE, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;

        default:

            /*
             * All other types are an error
             */
            sprintf (MsgBuffer, "%s [%s]", Op->Asl.ExternalName, AcpiUtGetTypeName (Node->Type));
            AslError (ASL_ERROR, ASL_MSG_SCOPE_TYPE, Op, MsgBuffer);

            /*
             * However, switch the type to be an actual scope so
             * that compilation can continue without generating a whole
             * cascade of additional errors.  Open the new scope.
             */
            Node->Type = ACPI_TYPE_LOCAL_SCOPE;
            Status = AcpiDsScopeStackPush (Node, ACPI_TYPE_LOCAL_SCOPE, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;
        }

        Status = AE_OK;
        goto FinishNode;


    default:

        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
        break;
    }


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Loading name: %s, (%s)\n",
            Op->Asl.ExternalName, AcpiUtGetTypeName (ObjectType)));

    /* The name must not already exist */

    Flags |= ACPI_NS_ERROR_IF_FOUND;

    /*
     * Enter the named type into the internal namespace.  We enter the name
     * as we go downward in the parse tree.  Any necessary subobjects that involve
     * arguments to the opcode must be created as we go back up the parse tree later.
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                    ACPI_IMODE_LOAD_PASS1, Flags, WalkState, &(Node));
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* The name already exists in this scope */

            if (Node->Type == ACPI_TYPE_LOCAL_SCOPE)
            {
                Node->Type = (UINT8) ObjectType;
                Status = AE_OK;
            }
            else
            {
                AslError (ASL_ERROR, ASL_MSG_NAME_EXISTS, Op, Op->Asl.ExternalName);
                Status = AE_OK;
                goto Exit;
            }
        }
        else
        {
            AslCoreSubsystemError (Op, Status, "Failure from lookup %s\n", FALSE);
            goto Exit;
        }
    }


FinishNode:
    /*
     * Point the parse node to the new namespace node, and point
     * the Node back to the original Parse node
     */
    Op->Asl.Node = Node;
    Node->Object = (ACPI_OPERAND_OBJECT *) Op;

    /* Set the actual data type if appropriate (EXTERNAL term only) */

    if (ActualObjectType != ACPI_TYPE_ANY)
    {
        Node->Type = (UINT8) ActualObjectType;
        Node->OwnerId = ASL_EXTERNAL_METHOD;
    }

    if (Op->Asl.ParseOpcode == PARSEOP_METHOD)
    {
        /*
         * Get the method argument count from "Extra" and store
         * it in the OwnerId field of the namespace node
         */
        Node->OwnerId = (UINT16) Op->Asl.Extra;
    }

Exit:
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    LdNamespace1End
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the loading of the namespace,
 *              We only need to worry about managing the scope stack here.
 *
 ******************************************************************************/

ACPI_STATUS
LdNamespace1End (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = (ACPI_WALK_STATE *) Context;
    ACPI_OBJECT_TYPE        ObjectType;


    ACPI_FUNCTION_NAME ("LdNamespace1End");


    /* We are only interested in opcodes that have an associated name */

    if (!Op->Asl.Namepath)
    {
        return (AE_OK);
    }

    /* Get the type to determine if we should pop the scope */

    if ((Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG) &&
        (Op->Asl.CompileFlags == NODE_IS_RESOURCE_DESC))
    {
        /* TBD: Merge into AcpiDsMapNamedOpcodeToDataType */

        ObjectType = ACPI_TYPE_LOCAL_RESOURCE;
    }
    else
    {
        ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);
    }

    /* Pop the scope stack */

    if (AcpiNsOpensScope (ObjectType))
    {

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "(%s): Popping scope for Op [%s] %p\n",
            AcpiUtGetTypeName (ObjectType), Op->Asl.ParseOpName, Op));

        AcpiDsScopeStackPop (WalkState);
    }

    return (AE_OK);
}


