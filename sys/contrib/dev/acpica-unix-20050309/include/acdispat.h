/******************************************************************************
 *
 * Name: acdispat.h - dispatcher (parser to interpreter interface)
 *       $Revision: 61 $
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


#ifndef _ACDISPAT_H_
#define _ACDISPAT_H_


#define NAMEOF_LOCAL_NTE    "__L0"
#define NAMEOF_ARG_NTE      "__A0"


/* Common interfaces */

ACPI_STATUS
AcpiDsObjStackPush (
    void                    *Object,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsObjStackPop (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState);

void *
AcpiDsObjStackGetValue (
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsObjStackPopObject (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState);


/* dsopcode - support for late evaluation */

ACPI_STATUS
AcpiDsExecuteArguments (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_NAMESPACE_NODE     *ScopeNode,
    UINT32                  AmlLength,
    UINT8                   *AmlStart);

ACPI_STATUS
AcpiDsGetBufferFieldArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsGetRegionArguments (
    ACPI_OPERAND_OBJECT     *RgnDesc);

ACPI_STATUS
AcpiDsGetBufferArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsGetPackageArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsInitBufferField (
    UINT16                  AmlOpcode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     *BufferDesc,
    ACPI_OPERAND_OBJECT     *OffsetDesc,
    ACPI_OPERAND_OBJECT     *LengthDesc,
    ACPI_OPERAND_OBJECT     *ResultDesc);

ACPI_STATUS
AcpiDsEvalBufferFieldOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsEvalRegionOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsEvalDataObjectOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsInitializeRegion (
    ACPI_HANDLE             ObjHandle);


/* dsctrl - Parser/Interpreter interface, control stack routines */


ACPI_STATUS
AcpiDsExecBeginControlOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsExecEndControlOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);


/* dsexec - Parser/Interpreter interface, method execution callbacks */


ACPI_STATUS
AcpiDsGetPredicateValue (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ResultObj);

ACPI_STATUS
AcpiDsExecBeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsExecEndOp (
    ACPI_WALK_STATE         *State);


/* dsfield - Parser/Interpreter interface for AML fields */

ACPI_STATUS
AcpiDsGetFieldNames (
    ACPI_CREATE_FIELD_INFO  *Info,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Arg);

ACPI_STATUS
AcpiDsCreateField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateBankField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateIndexField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateBufferField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsInitFieldObjects (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);


/* dsload - Parser/Interpreter interface, namespace load callbacks */

ACPI_STATUS
AcpiDsLoad1BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsLoad1EndOp (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsLoad2BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsLoad2EndOp (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsInitCallbacks (
    ACPI_WALK_STATE         *WalkState,
    UINT32                  PassNumber);


/* dsmthdat - method data (locals/args) */


ACPI_STATUS
AcpiDsStoreObjectToLocal (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *SrcDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataGetEntry (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     ***Node);

void
AcpiDsMethodDataDeleteAll (
    ACPI_WALK_STATE         *WalkState);

BOOLEAN
AcpiDsIsMethodValue (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_OBJECT_TYPE
AcpiDsMethodDataGetType (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataGetValue (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **DestDesc);

void
AcpiDsMethodDataDeleteValue (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataInitArgs (
    ACPI_OPERAND_OBJECT     **Params,
    UINT32                  MaxParamCount,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataGetNode (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     **Node);

void
AcpiDsMethodDataInit (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataSetValue (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_WALK_STATE         *WalkState);


/* dsmethod - Parser/Interpreter interface - control method parsing */

ACPI_STATUS
AcpiDsParseMethod (
    ACPI_HANDLE             ObjHandle);

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc);

ACPI_STATUS
AcpiDsTerminateControlMethod (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsBeginMethodExecution (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_NAMESPACE_NODE     *CallingMethodNode);


/* dsobj - Parser/Interpreter interface - object initialization and conversion */

ACPI_STATUS
AcpiDsInitOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AcpiDsInitializeObjects (
    ACPI_TABLE_DESC         *TableDesc,
    ACPI_NAMESPACE_NODE     *StartNode);

ACPI_STATUS
AcpiDsBuildInternalBufferObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  BufferLength,
    ACPI_OPERAND_OBJECT     **ObjDescPtr);

ACPI_STATUS
AcpiDsBuildInternalPackageObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *op,
    UINT32                  PackageLength,
    ACPI_OPERAND_OBJECT     **ObjDesc);

ACPI_STATUS
AcpiDsBuildInternalObject (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr);

ACPI_STATUS
AcpiDsInitObjectFromOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **ObjDesc);

ACPI_STATUS
AcpiDsCreateNode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op);


/* dsutils - Parser/Interpreter interface utility routines */

void
AcpiDsClearImplicitReturn (
    ACPI_WALK_STATE         *WalkState);

BOOLEAN
AcpiDsDoImplicitReturn (
    ACPI_OPERAND_OBJECT     *ReturnDesc,
    ACPI_WALK_STATE         *WalkState,
    BOOLEAN                 AddReference);

BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsDeleteResultIfNotUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ResultObj,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateOperand (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Arg,
    UINT32                  ArgsRemaining);

ACPI_STATUS
AcpiDsCreateOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *FirstArg);

ACPI_STATUS
AcpiDsResolveOperands (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsClearOperands (
    ACPI_WALK_STATE         *WalkState);


/*
 * dswscope - Scope Stack manipulation
 */

ACPI_STATUS
AcpiDsScopeStackPush (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT_TYPE        Type,
    ACPI_WALK_STATE         *WalkState);


ACPI_STATUS
AcpiDsScopeStackPop (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsScopeStackClear (
    ACPI_WALK_STATE         *WalkState);


/* dswstate - parser WALK_STATE management routines */

ACPI_WALK_STATE *
AcpiDsCreateWalkState (
    ACPI_OWNER_ID           OwnerId,
    ACPI_PARSE_OBJECT       *Origin,
    ACPI_OPERAND_OBJECT     *MthDesc,
    ACPI_THREAD_STATE       *Thread);

ACPI_STATUS
AcpiDsInitAmlWalk (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *MethodNode,
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    ACPI_PARAMETER_INFO     *Info,
    UINT32                  PassNumber);

ACPI_STATUS
AcpiDsObjStackDeleteAll (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsObjStackPopAndDelete (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsDeleteWalkState (
    ACPI_WALK_STATE         *WalkState);

ACPI_WALK_STATE *
AcpiDsPopWalkState (
    ACPI_THREAD_STATE       *Thread);

void
AcpiDsPushWalkState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_THREAD_STATE       *Thread);

ACPI_STATUS
AcpiDsResultStackPop (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultStackPush (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultStackClear (
    ACPI_WALK_STATE         *WalkState);

ACPI_WALK_STATE *
AcpiDsGetCurrentWalkState (
    ACPI_THREAD_STATE       *Thread);

#ifdef ACPI_ENABLE_OBJECT_CACHE
void
AcpiDsDeleteWalkStateCache (
    void);
#endif

ACPI_STATUS
AcpiDsResultInsert (
    void                    *Object,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultRemove (
    ACPI_OPERAND_OBJECT     **Object,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultPop (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultPush (
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultPopFromBottom (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState);

#endif /* _ACDISPAT_H_ */
