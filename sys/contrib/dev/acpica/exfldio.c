/******************************************************************************
 *
 * Module Name: exfldio - Aml Field I/O
 *              $Revision: 92 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/exfldio.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */


#define __EXFLDIO_C__

#include "acpi.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acevents.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exfldio")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSetupRegion
 *
 * PARAMETERS:  *ObjDesc                - Field to be read or written
 *              FieldDatumByteOffset    - Byte offset of this datum within the
 *                                        parent field
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Common processing for AcpiExExtractFromField and
 *              AcpiExInsertIntoField.  Initialize the
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSetupRegion (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  FieldDatumByteOffset)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *RgnDesc;


    ACPI_FUNCTION_TRACE_U32 ("ExSetupRegion", FieldDatumByteOffset);


    RgnDesc = ObjDesc->CommonField.RegionObj;

    /* We must have a valid region */

    if (ACPI_GET_OBJECT_TYPE (RgnDesc) != ACPI_TYPE_REGION)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Needed Region, found type %X (%s)\n",
            ACPI_GET_OBJECT_TYPE (RgnDesc),
            AcpiUtGetObjectTypeName (RgnDesc)));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /*
     * If the Region Address and Length have not been previously evaluated,
     * evaluate them now and save the results.
     */
    if (!(RgnDesc->Region.Flags & AOPOBJ_DATA_VALID))
    {
        Status = AcpiDsGetRegionArguments (RgnDesc);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    if (RgnDesc->Region.SpaceId == ACPI_ADR_SPACE_SMBUS)
    {
        /* SMBus has a non-linear address space */

        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Validate the request.  The entire request from the byte offset for a
     * length of one field datum (access width) must fit within the region.
     * (Region length is specified in bytes)
     */
    if (RgnDesc->Region.Length < (ObjDesc->CommonField.BaseByteOffset
                                    + FieldDatumByteOffset
                                    + ObjDesc->CommonField.AccessByteWidth))
    {
        if (RgnDesc->Region.Length < ObjDesc->CommonField.AccessByteWidth)
        {
            /*
             * This is the case where the AccessType (AccWord, etc.) is wider
             * than the region itself.  For example, a region of length one
             * byte, and a field with Dword access specified.
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Field [%4.4s] access width (%d bytes) too large for region [%4.4s] (length %X)\n",
                ObjDesc->CommonField.Node->Name.Ascii, ObjDesc->CommonField.AccessByteWidth,
                RgnDesc->Region.Node->Name.Ascii, RgnDesc->Region.Length));
        }

        /*
         * Offset rounded up to next multiple of field width
         * exceeds region length, indicate an error
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Field [%4.4s] Base+Offset+Width %X+%X+%X is beyond end of region [%4.4s] (length %X)\n",
            ObjDesc->CommonField.Node->Name.Ascii, ObjDesc->CommonField.BaseByteOffset,
            FieldDatumByteOffset, ObjDesc->CommonField.AccessByteWidth,
            RgnDesc->Region.Node->Name.Ascii, RgnDesc->Region.Length));

        return_ACPI_STATUS (AE_AML_REGION_LIMIT);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExAccessRegion
 *
 * PARAMETERS:  *ObjDesc                - Field to be read
 *              FieldDatumByteOffset    - Byte offset of this datum within the
 *                                        parent field
 *              *Value                  - Where to store value (must at least
 *                                        the size of ACPI_INTEGER)
 *              Function                - Read or Write flag plus other region-
 *                                        dependent flags
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read or Write a single field datum to an Operation Region.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExAccessRegion (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  FieldDatumByteOffset,
    ACPI_INTEGER            *Value,
    UINT32                  Function)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *RgnDesc;
    ACPI_PHYSICAL_ADDRESS   Address;


    ACPI_FUNCTION_TRACE ("ExAccessRegion");


    /*
     * Ensure that the region operands are fully evaluated and verify
     * the validity of the request
     */
    Status = AcpiExSetupRegion (ObjDesc, FieldDatumByteOffset);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * The physical address of this field datum is:
     *
     * 1) The base of the region, plus
     * 2) The base offset of the field, plus
     * 3) The current offset into the field
     */
    RgnDesc = ObjDesc->CommonField.RegionObj;
    Address = RgnDesc->Region.Address
                + ObjDesc->CommonField.BaseByteOffset
                + FieldDatumByteOffset;

    if ((Function & ACPI_IO_MASK) == ACPI_READ)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD, "[READ]"));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD, "[WRITE]"));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_BFIELD,
        " Region[%s-%X] Access %X Base:Off %X:%X at %8.8X%8.8X\n",
        AcpiUtGetRegionName (RgnDesc->Region.SpaceId),
        RgnDesc->Region.SpaceId,
        ObjDesc->CommonField.AccessByteWidth,
        ObjDesc->CommonField.BaseByteOffset,
        FieldDatumByteOffset,
        ACPI_HIDWORD (Address), ACPI_LODWORD (Address)));

    /* Invoke the appropriate AddressSpace/OpRegion handler */

    Status = AcpiEvAddressSpaceDispatch (RgnDesc, Function,
                    Address, ACPI_MUL_8 (ObjDesc->CommonField.AccessByteWidth), Value);

    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_IMPLEMENTED)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Region %s(%X) not implemented\n",
                AcpiUtGetRegionName (RgnDesc->Region.SpaceId),
                RgnDesc->Region.SpaceId));
        }
        else if (Status == AE_NOT_EXIST)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Region %s(%X) has no handler\n",
                AcpiUtGetRegionName (RgnDesc->Region.SpaceId),
                RgnDesc->Region.SpaceId));
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExRegisterOverflow
 *
 * PARAMETERS:  *ObjDesc                - Register(Field) to be written
 *              Value                   - Value to be stored
 *
 * RETURN:      TRUE if value overflows the field, FALSE otherwise
 *
 * DESCRIPTION: Check if a value is out of range of the field being written.
 *              Used to check if the values written to Index and Bank registers
 *              are out of range.  Normally, the value is simply truncated
 *              to fit the field, but this case is most likely a serious
 *              coding error in the ASL.
 *
 ******************************************************************************/

BOOLEAN
AcpiExRegisterOverflow (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_INTEGER            Value)
{

    if (ObjDesc->CommonField.BitLength >= ACPI_INTEGER_BIT_SIZE)
    {
        /*
         * The field is large enough to hold the maximum integer, so we can
         * never overflow it.
         */
        return (FALSE);
    }

    if (Value >= ((ACPI_INTEGER) 1 << ObjDesc->CommonField.BitLength))
    {
        /*
         * The Value is larger than the maximum value that can fit into
         * the register.
         */
        return (TRUE);
    }

    /* The Value will fit into the field with no truncation */

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExFieldDatumIo
 *
 * PARAMETERS:  *ObjDesc                - Field to be read
 *              FieldDatumByteOffset    - Byte offset of this datum within the
 *                                        parent field
 *              *Value                  - Where to store value (must be 64 bits)
 *              ReadWrite               - Read or Write flag
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read or Write a single datum of a field.  The FieldType is
 *              demultiplexed here to handle the different types of fields
 *              (BufferField, RegionField, IndexField, BankField)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExFieldDatumIo (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT32                  FieldDatumByteOffset,
    ACPI_INTEGER            *Value,
    UINT32                  ReadWrite)
{
    ACPI_STATUS             Status;
    ACPI_INTEGER            LocalValue;


    ACPI_FUNCTION_TRACE_U32 ("ExFieldDatumIo", FieldDatumByteOffset);


    if (ReadWrite == ACPI_READ)
    {
        if (!Value)
        {
            LocalValue = 0;
            Value = &LocalValue;  /* To support reads without saving return value */
        }

        /* Clear the entire return buffer first, [Very Important!] */

        *Value = 0;
    }

    /*
     * The four types of fields are:
     *
     * BufferFields - Read/write from/to a Buffer
     * RegionFields - Read/write from/to a Operation Region.
     * BankFields   - Write to a Bank Register, then read/write from/to an OpRegion
     * IndexFields  - Write to an Index Register, then read/write from/to a Data Register
     */
    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_BUFFER_FIELD:
        /*
         * If the BufferField arguments have not been previously evaluated,
         * evaluate them now and save the results.
         */
        if (!(ObjDesc->Common.Flags & AOPOBJ_DATA_VALID))
        {
            Status = AcpiDsGetBufferFieldArguments (ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        if (ReadWrite == ACPI_READ)
        {
            /*
             * Copy the data from the source buffer.
             * Length is the field width in bytes.
             */
            ACPI_MEMCPY (Value, (ObjDesc->BufferField.BufferObj)->Buffer.Pointer
                            + ObjDesc->BufferField.BaseByteOffset
                            + FieldDatumByteOffset,
                            ObjDesc->CommonField.AccessByteWidth);
        }
        else
        {
            /*
             * Copy the data to the target buffer.
             * Length is the field width in bytes.
             */
            ACPI_MEMCPY ((ObjDesc->BufferField.BufferObj)->Buffer.Pointer
                    + ObjDesc->BufferField.BaseByteOffset
                    + FieldDatumByteOffset,
                    Value, ObjDesc->CommonField.AccessByteWidth);
        }

        Status = AE_OK;
        break;


    case ACPI_TYPE_LOCAL_BANK_FIELD:

        /* Ensure that the BankValue is not beyond the capacity of the register */

        if (AcpiExRegisterOverflow (ObjDesc->BankField.BankObj,
                                    (ACPI_INTEGER) ObjDesc->BankField.Value))
        {
            return_ACPI_STATUS (AE_AML_REGISTER_LIMIT);
        }

        /*
         * For BankFields, we must write the BankValue to the BankRegister
         * (itself a RegionField) before we can access the data.
         */
        Status = AcpiExInsertIntoField (ObjDesc->BankField.BankObj,
                                &ObjDesc->BankField.Value,
                                sizeof (ObjDesc->BankField.Value));
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /*
         * Now that the Bank has been selected, fall through to the
         * RegionField case and write the datum to the Operation Region
         */

        /*lint -fallthrough */


    case ACPI_TYPE_LOCAL_REGION_FIELD:
        /*
         * For simple RegionFields, we just directly access the owning
         * Operation Region.
         */
        Status = AcpiExAccessRegion (ObjDesc, FieldDatumByteOffset, Value,
                        ReadWrite);
        break;


    case ACPI_TYPE_LOCAL_INDEX_FIELD:


        /* Ensure that the IndexValue is not beyond the capacity of the register */

        if (AcpiExRegisterOverflow (ObjDesc->IndexField.IndexObj,
                                    (ACPI_INTEGER) ObjDesc->IndexField.Value))
        {
            return_ACPI_STATUS (AE_AML_REGISTER_LIMIT);
        }

        /* Write the index value to the IndexRegister (itself a RegionField) */

        Status = AcpiExInsertIntoField (ObjDesc->IndexField.IndexObj,
                                &ObjDesc->IndexField.Value,
                                sizeof (ObjDesc->IndexField.Value));
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        if (ReadWrite == ACPI_READ)
        {
            /* Read the datum from the DataRegister */

            Status = AcpiExExtractFromField (ObjDesc->IndexField.DataObj,
                            Value, ObjDesc->CommonField.AccessByteWidth);
        }
        else
        {
            /* Write the datum to the Data register */

            Status = AcpiExInsertIntoField (ObjDesc->IndexField.DataObj,
                            Value, ObjDesc->CommonField.AccessByteWidth);
        }
        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%p, Wrong object type - %s\n",
            ObjDesc, AcpiUtGetObjectTypeName (ObjDesc)));
        Status = AE_AML_INTERNAL;
        break;
    }

    if (ACPI_SUCCESS (Status))
    {
        if (ReadWrite == ACPI_READ)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD, "Value Read=%8.8X%8.8X\n",
                                ACPI_HIDWORD (*Value), ACPI_LODWORD (*Value)));
        }
        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD, "Value Written=%8.8X%8.8X\n",
                                ACPI_HIDWORD (*Value), ACPI_LODWORD (*Value)));
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExWriteWithUpdateRule
 *
 * PARAMETERS:  *ObjDesc            - Field to be set
 *              Value               - Value to store
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Apply the field update rule to a field write
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExWriteWithUpdateRule (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_INTEGER            Mask,
    ACPI_INTEGER            FieldValue,
    UINT32                  FieldDatumByteOffset)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_INTEGER            MergedValue;
    ACPI_INTEGER            CurrentValue;


    ACPI_FUNCTION_TRACE_U32 ("ExWriteWithUpdateRule", Mask);


    /* Start with the new bits  */

    MergedValue = FieldValue;

    /* If the mask is all ones, we don't need to worry about the update rule */

    if (Mask != ACPI_INTEGER_MAX)
    {
        /* Decode the update rule */

        switch (ObjDesc->CommonField.FieldFlags & AML_FIELD_UPDATE_RULE_MASK)
        {
        case AML_FIELD_UPDATE_PRESERVE:
            /*
             * Check if update rule needs to be applied (not if mask is all
             * ones)  The left shift drops the bits we want to ignore.
             */
            if ((~Mask << (ACPI_MUL_8 (sizeof (Mask)) -
                           ACPI_MUL_8 (ObjDesc->CommonField.AccessByteWidth))) != 0)
            {
                /*
                 * Read the current contents of the byte/word/dword containing
                 * the field, and merge with the new field value.
                 */
                Status = AcpiExFieldDatumIo (ObjDesc, FieldDatumByteOffset,
                                &CurrentValue, ACPI_READ);
                MergedValue |= (CurrentValue & ~Mask);
            }
            break;

        case AML_FIELD_UPDATE_WRITE_AS_ONES:

            /* Set positions outside the field to all ones */

            MergedValue |= ~Mask;
            break;

        case AML_FIELD_UPDATE_WRITE_AS_ZEROS:

            /* Set positions outside the field to all zeros */

            MergedValue &= Mask;
            break;

        default:
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "WriteWithUpdateRule: Unknown UpdateRule setting: %X\n",
                (ObjDesc->CommonField.FieldFlags & AML_FIELD_UPDATE_RULE_MASK)));
            return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
        }
    }

    /* Write the merged value */

    Status = AcpiExFieldDatumIo (ObjDesc, FieldDatumByteOffset,
                    &MergedValue, ACPI_WRITE);

    ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD,
        "Mask %8.8X%8.8X DatumOffset %X Value %8.8X%8.8X, MergedValue %8.8X%8.8X\n",
        ACPI_HIDWORD (Mask), ACPI_LODWORD (Mask),
        FieldDatumByteOffset,
        ACPI_HIDWORD (FieldValue), ACPI_LODWORD (FieldValue),
        ACPI_HIDWORD (MergedValue),ACPI_LODWORD (MergedValue)));

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExGetBufferDatum
 *
 * PARAMETERS:  Datum               - Where the Datum is returned
 *              Buffer              - Raw field buffer
 *              ByteGranularity     - 1/2/4/8 Granularity of the field
 *                                    (aka Datum Size)
 *              Offset              - Datum offset into the buffer
 *
 * RETURN:      none
 *
 * DESCRIPTION: Get a datum from the buffer according to the buffer field
 *              byte granularity
 *
 ******************************************************************************/

void
AcpiExGetBufferDatum(
    ACPI_INTEGER            *Datum,
    void                    *Buffer,
    UINT32                  ByteGranularity,
    UINT32                  Offset)
{

    ACPI_FUNCTION_ENTRY ();


    switch (ByteGranularity)
    {
    case ACPI_FIELD_BYTE_GRANULARITY:

        *Datum = ((UINT8 *) Buffer) [Offset];
        break;

    case ACPI_FIELD_WORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED16_TO_32 (Datum, &(((UINT16 *) Buffer) [Offset]));
        break;

    case ACPI_FIELD_DWORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED32_TO_32 (Datum, &(((UINT32 *) Buffer) [Offset]));
        break;

    case ACPI_FIELD_QWORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED64_TO_64 (Datum, &(((UINT64 *) Buffer) [Offset]));
        break;

    default:
        /* Should not get here */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSetBufferDatum
 *
 * PARAMETERS:  MergedDatum         - Value to store
 *              Buffer              - Receiving buffer
 *              ByteGranularity     - 1/2/4/8 Granularity of the field
 *                                    (aka Datum Size)
 *              Offset              - Datum offset into the buffer
 *
 * RETURN:      none
 *
 * DESCRIPTION: Store the merged datum to the buffer according to the
 *              byte granularity
 *
 ******************************************************************************/

void
AcpiExSetBufferDatum (
    ACPI_INTEGER            MergedDatum,
    void                    *Buffer,
    UINT32                  ByteGranularity,
    UINT32                  Offset)
{

    ACPI_FUNCTION_ENTRY ();


    switch (ByteGranularity)
    {
    case ACPI_FIELD_BYTE_GRANULARITY:

        ((UINT8 *) Buffer) [Offset] = (UINT8) MergedDatum;
        break;

    case ACPI_FIELD_WORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED16_TO_16 (&(((UINT16 *) Buffer)[Offset]), &MergedDatum);
        break;

    case ACPI_FIELD_DWORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED32_TO_32 (&(((UINT32 *) Buffer)[Offset]), &MergedDatum);
        break;

    case ACPI_FIELD_QWORD_GRANULARITY:

        ACPI_MOVE_UNALIGNED64_TO_64 (&(((UINT64 *) Buffer)[Offset]), &MergedDatum);
        break;

    default:
        /* Should not get here */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExExtractFromField
 *
 * PARAMETERS:  *ObjDesc            - Field to be read
 *              *Value              - Where to store value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve the value of the given field
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExExtractFromField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength)
{
    ACPI_STATUS             Status;
    UINT32                  FieldDatumByteOffset;
    UINT32                  DatumOffset;
    ACPI_INTEGER            PreviousRawDatum;
    ACPI_INTEGER            ThisRawDatum = 0;
    ACPI_INTEGER            MergedDatum = 0;
    UINT32                  ByteFieldLength;
    UINT32                  DatumCount;


    ACPI_FUNCTION_TRACE ("ExExtractFromField");


    /*
     * The field must fit within the caller's buffer
     */
    ByteFieldLength = ACPI_ROUND_BITS_UP_TO_BYTES (ObjDesc->CommonField.BitLength);
    if (ByteFieldLength > BufferLength)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD,
            "Field size %X (bytes) too large for buffer (%X)\n",
            ByteFieldLength, BufferLength));

        return_ACPI_STATUS (AE_BUFFER_OVERFLOW);
    }

    /* Convert field byte count to datum count, round up if necessary */

    DatumCount = ACPI_ROUND_UP_TO (ByteFieldLength,
                              ObjDesc->CommonField.AccessByteWidth);

    ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD,
        "ByteLen=%X, DatumLen=%X, ByteGran=%X\n",
        ByteFieldLength, DatumCount,ObjDesc->CommonField.AccessByteWidth));

    /*
     * Clear the caller's buffer (the whole buffer length as given)
     * This is very important, especially in the cases where a byte is read,
     * but the buffer is really a UINT32 (4 bytes).
     */
    ACPI_MEMSET (Buffer, 0, BufferLength);

    /* Read the first raw datum to prime the loop */

    FieldDatumByteOffset = 0;
    DatumOffset= 0;

    Status = AcpiExFieldDatumIo (ObjDesc, FieldDatumByteOffset,
                    &PreviousRawDatum, ACPI_READ);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /* We might actually be done if the request fits in one datum */

    if ((DatumCount == 1) &&
        (ObjDesc->CommonField.Flags & AOPOBJ_SINGLE_DATUM))
    {
        /* 1) Shift the valid data bits down to start at bit 0 */

        MergedDatum = (PreviousRawDatum >> ObjDesc->CommonField.StartFieldBitOffset);

        /* 2) Mask off any upper unused bits (bits not part of the field) */

        if (ObjDesc->CommonField.EndBufferValidBits)
        {
            MergedDatum &= ACPI_MASK_BITS_ABOVE (ObjDesc->CommonField.EndBufferValidBits);
        }

        /* Store the datum to the caller buffer */

        AcpiExSetBufferDatum (MergedDatum, Buffer, ObjDesc->CommonField.AccessByteWidth,
                DatumOffset);

        return_ACPI_STATUS (AE_OK);
    }


    /* We need to get more raw data to complete one or more field data */

    while (DatumOffset < DatumCount)
    {
        FieldDatumByteOffset += ObjDesc->CommonField.AccessByteWidth;

        /*
         * If the field is aligned on a byte boundary, we don't want
         * to perform a final read, since this would potentially read
         * past the end of the region.
         *
         * We could just split the aligned and non-aligned cases since the
         * aligned case is so very simple, but this would require more code.
         */
        if ((ObjDesc->CommonField.StartFieldBitOffset != 0)  ||
            ((ObjDesc->CommonField.StartFieldBitOffset == 0) &&
            (DatumOffset < (DatumCount -1))))
        {
            /*
             * Get the next raw datum, it contains some or all bits
             * of the current field datum
             */
            Status = AcpiExFieldDatumIo (ObjDesc, FieldDatumByteOffset,
                            &ThisRawDatum, ACPI_READ);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        /*
         * Create the (possibly) merged datum to be stored to the caller buffer
         */
        if (ObjDesc->CommonField.StartFieldBitOffset == 0)
        {
            /* Field is not skewed and we can just copy the datum */

            MergedDatum = PreviousRawDatum;
        }
        else
        {
            /*
             * Put together the appropriate bits of the two raw data to make a
             * single complete field datum
             *
             * 1) Normalize the first datum down to bit 0
             */
            MergedDatum = (PreviousRawDatum >> ObjDesc->CommonField.StartFieldBitOffset);

            /* 2) Insert the second datum "above" the first datum */

            MergedDatum |= (ThisRawDatum << ObjDesc->CommonField.DatumValidBits);

            if ((DatumOffset >= (DatumCount -1)))
            {
                /*
                 * This is the last iteration of the loop.  We need to clear
                 * any unused bits (bits that are not part of this field) that
                 * came from the last raw datum before we store the final
                 * merged datum into the caller buffer.
                 */
                if (ObjDesc->CommonField.EndBufferValidBits)
                {
                    MergedDatum &=
                        ACPI_MASK_BITS_ABOVE (ObjDesc->CommonField.EndBufferValidBits);
                }
            }
        }

        /*
         * Store the merged field datum in the caller's buffer, according to
         * the granularity of the field (size of each datum).
         */
        AcpiExSetBufferDatum (MergedDatum, Buffer,
                ObjDesc->CommonField.AccessByteWidth, DatumOffset);

        /*
         * Save the raw datum that was just acquired since it may contain bits
         * of the *next* field datum.  Update offsets
         */
        PreviousRawDatum = ThisRawDatum;
        DatumOffset++;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExInsertIntoField
 *
 * PARAMETERS:  *ObjDesc            - Field to be set
 *              Buffer              - Value to store
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the value into the given field
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExInsertIntoField (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    void                    *Buffer,
    UINT32                  BufferLength)
{
    ACPI_STATUS             Status;
    UINT32                  FieldDatumByteOffset;
    UINT32                  DatumOffset;
    ACPI_INTEGER            Mask;
    ACPI_INTEGER            MergedDatum;
    ACPI_INTEGER            PreviousRawDatum;
    ACPI_INTEGER            ThisRawDatum;
    UINT32                  ByteFieldLength;
    UINT32                  DatumCount;


    ACPI_FUNCTION_TRACE ("ExInsertIntoField");


    /*
     * Incoming buffer must be at least as long as the field, we do not
     * allow "partial" field writes.  We do not care if the buffer is
     * larger than the field, this typically happens when an integer is
     * written to a field that is actually smaller than an integer.
     */
    ByteFieldLength = ACPI_ROUND_BITS_UP_TO_BYTES (ObjDesc->CommonField.BitLength);
    if (BufferLength < ByteFieldLength)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD, "Buffer length %X too small for field %X\n",
            BufferLength, ByteFieldLength));

        return_ACPI_STATUS (AE_BUFFER_OVERFLOW);
    }

    /* Convert byte count to datum count, round up if necessary */

    DatumCount = ACPI_ROUND_UP_TO (ByteFieldLength, ObjDesc->CommonField.AccessByteWidth);

    ACPI_DEBUG_PRINT ((ACPI_DB_BFIELD,
        "ByteLen=%X, DatumLen=%X, ByteGran=%X\n",
        ByteFieldLength, DatumCount, ObjDesc->CommonField.AccessByteWidth));

    /*
     * Break the request into up to three parts (similar to an I/O request):
     * 1) non-aligned part at start
     * 2) aligned part in middle
     * 3) non-aligned part at the end
     */
    FieldDatumByteOffset = 0;
    DatumOffset= 0;

    /* Get a single datum from the caller's buffer */

    AcpiExGetBufferDatum (&PreviousRawDatum, Buffer,
            ObjDesc->CommonField.AccessByteWidth, DatumOffset);

    /*
     * Part1:
     * Write a partial field datum if field does not begin on a datum boundary
     * Note: The code in this section also handles the aligned case
     *
     * Construct Mask with 1 bits where the field is, 0 bits elsewhere
     * (Only the bottom 5 bits of BitLength are valid for a shift operation)
     *
     * Mask off bits that are "below" the field (if any)
     */
    Mask = ACPI_MASK_BITS_BELOW (ObjDesc->CommonField.StartFieldBitOffset);

    /* If the field fits in one datum, may need to mask upper bits */

    if ((ObjDesc->CommonField.Flags & AOPOBJ_SINGLE_DATUM) &&
         ObjDesc->CommonField.EndFieldValidBits)
    {
        /* There are bits above the field, mask them off also */

        Mask &= ACPI_MASK_BITS_ABOVE (ObjDesc->CommonField.EndFieldValidBits);
    }

    /* Shift and mask the value into the field position */

    MergedDatum = (PreviousRawDatum << ObjDesc->CommonField.StartFieldBitOffset);
    MergedDatum &= Mask;

    /* Apply the update rule (if necessary) and write the datum to the field */

    Status = AcpiExWriteWithUpdateRule (ObjDesc, Mask, MergedDatum,
                        FieldDatumByteOffset);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* If the entire field fits within one datum, we are done. */

    if ((DatumCount == 1) &&
       (ObjDesc->CommonField.Flags & AOPOBJ_SINGLE_DATUM))
    {
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Part2:
     * Write the aligned data.
     *
     * We don't need to worry about the update rule for these data, because
     * all of the bits in each datum are part of the field.
     *
     * The last datum must be special cased because it might contain bits
     * that are not part of the field -- therefore the "update rule" must be
     * applied in Part3 below.
     */
    while (DatumOffset < DatumCount)
    {
        DatumOffset++;
        FieldDatumByteOffset += ObjDesc->CommonField.AccessByteWidth;

        /*
         * Get the next raw buffer datum.  It may contain bits of the previous
         * field datum
         */
        AcpiExGetBufferDatum (&ThisRawDatum, Buffer,
                ObjDesc->CommonField.AccessByteWidth, DatumOffset);

        /* Create the field datum based on the field alignment */

        if (ObjDesc->CommonField.StartFieldBitOffset != 0)
        {
            /*
             * Put together appropriate bits of the two raw buffer data to make
             * a single complete field datum
             */
            MergedDatum =
                (PreviousRawDatum >> ObjDesc->CommonField.DatumValidBits) |
                (ThisRawDatum << ObjDesc->CommonField.StartFieldBitOffset);
        }
        else
        {
            /* Field began aligned on datum boundary */

            MergedDatum = ThisRawDatum;
        }

        /*
         * Special handling for the last datum if the field does NOT end on
         * a datum boundary.  Update Rule must be applied to the bits outside
         * the field.
         */
        if (DatumOffset == DatumCount)
        {
            /*
             * If there are dangling non-aligned bits, perform one more merged write
             * Else - field is aligned at the end, no need for any more writes
             */
            if (ObjDesc->CommonField.EndFieldValidBits)
            {
                /*
                 * Part3:
                 * This is the last datum and the field does not end on a datum boundary.
                 * Build the partial datum and write with the update rule.
                 *
                 * Mask off the unused bits above (after) the end-of-field
                 */
                Mask = ACPI_MASK_BITS_ABOVE (ObjDesc->CommonField.EndFieldValidBits);
                MergedDatum &= Mask;

                /* Write the last datum with the update rule */

                Status = AcpiExWriteWithUpdateRule (ObjDesc, Mask, MergedDatum,
                                    FieldDatumByteOffset);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }
            }
        }
        else
        {
            /* Normal case -- write the completed datum */

            Status = AcpiExFieldDatumIo (ObjDesc, FieldDatumByteOffset,
                            &MergedDatum, ACPI_WRITE);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        /*
         * Save the most recent datum since it may contain bits of the *next*
         * field datum.  Update current byte offset.
         */
        PreviousRawDatum = ThisRawDatum;
    }

    return_ACPI_STATUS (Status);
}


