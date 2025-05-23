
/******************************************************************************
 *
 * Module Name: exregion - ACPI default OpRegion (address space) handlers
 *              $Revision: 82 $
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
/* $DragonFly: src/sys/contrib/dev/acpica/Attic/exregion.c,v 1.1 2003/09/24 03:32:16 drhodus Exp $                                                               */


#define __EXREGION_C__

#include "acpi.h"
#include "acinterp.h"


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exregion")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemMemorySpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System Memory address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemMemorySpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    void                    *LogicalAddrPtr = NULL;
    ACPI_MEM_SPACE_CONTEXT  *MemInfo = RegionContext;
    UINT32                  Length;
    ACPI_SIZE               WindowSize;
#ifndef _HW_ALIGNMENT_SUPPORT
    UINT32                  Remainder;
#endif

    ACPI_FUNCTION_TRACE ("ExSystemMemorySpaceHandler");


    /* Validate and translate the bit width */

    switch (BitWidth)
    {
    case 8:
        Length = 1;
        break;

    case 16:
        Length = 2;
        break;

    case 32:
        Length = 4;
        break;

    case 64:
        Length = 8;
        break;

    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Invalid SystemMemory width %d\n",
            BitWidth));
        return_ACPI_STATUS (AE_AML_OPERAND_VALUE);
    }


#ifndef _HW_ALIGNMENT_SUPPORT
    /*
     * Hardware does not support non-aligned data transfers, we must verify
     * the request.
     */
    (void) AcpiUtShortDivide ((ACPI_INTEGER *) &Address, Length, NULL, &Remainder);
    if (Remainder != 0)
    {
        return_ACPI_STATUS (AE_AML_ALIGNMENT);
    }
#endif

    /*
     * Does the request fit into the cached memory mapping?
     * Is 1) Address below the current mapping? OR
     *    2) Address beyond the current mapping?
     */
    if ((Address < MemInfo->MappedPhysicalAddress) ||
        (((ACPI_INTEGER) Address + Length) >
            ((ACPI_INTEGER) MemInfo->MappedPhysicalAddress + MemInfo->MappedLength)))
    {
        /*
         * The request cannot be resolved by the current memory mapping;
         * Delete the existing mapping and create a new one.
         */
        if (MemInfo->MappedLength)
        {
            /* Valid mapping, delete it */

            AcpiOsUnmapMemory (MemInfo->MappedLogicalAddress,
                                MemInfo->MappedLength);
        }

        /*
         * Don't attempt to map memory beyond the end of the region, and
         * constrain the maximum mapping size to something reasonable.
         */
        WindowSize = (ACPI_SIZE) ((MemInfo->Address + MemInfo->Length) - Address);
        if (WindowSize > ACPI_SYSMEM_REGION_WINDOW_SIZE)
        {
            WindowSize = ACPI_SYSMEM_REGION_WINDOW_SIZE;
        }

        /* Create a new mapping starting at the address given */

        Status = AcpiOsMapMemory (Address, WindowSize,
                                    (void **) &MemInfo->MappedLogicalAddress);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not map memory at %8.8X%8.8X, size %X\n",
                ACPI_HIDWORD (Address), ACPI_LODWORD (Address), (UINT32) WindowSize));
            MemInfo->MappedLength = 0;
            return_ACPI_STATUS (Status);
        }

        /* Save the physical address and mapping size */

        MemInfo->MappedPhysicalAddress = Address;
        MemInfo->MappedLength = WindowSize;
    }

    /*
     * Generate a logical pointer corresponding to the address we want to
     * access
     */
    LogicalAddrPtr = MemInfo->MappedLogicalAddress +
                    ((ACPI_INTEGER) Address - (ACPI_INTEGER) MemInfo->MappedPhysicalAddress);

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "SystemMemory %d (%d width) Address=%8.8X%8.8X\n", Function, BitWidth,
        ACPI_HIDWORD (Address), ACPI_LODWORD (Address)));

   /*
    * Perform the memory read or write
    *
    * Note: For machines that do not support non-aligned transfers, the target
    * address was checked for alignment above.  We do not attempt to break the
    * transfer up into smaller (byte-size) chunks because the AML specifically
    * asked for a transfer width that the hardware may require.
    */
    switch (Function)
    {
    case ACPI_READ:

        *Value = 0;
        switch (BitWidth)
        {
        case 8:
            *Value = (ACPI_INTEGER) *((UINT8 *) LogicalAddrPtr);
            break;

        case 16:
            *Value = (ACPI_INTEGER) *((UINT16 *) LogicalAddrPtr);
            break;

        case 32:
            *Value = (ACPI_INTEGER) *((UINT32 *) LogicalAddrPtr);
            break;

#if ACPI_MACHINE_WIDTH != 16
        case 64:
            *Value = (ACPI_INTEGER) *((UINT64 *) LogicalAddrPtr);
            break;
#endif
        default:
            /* BitWidth was already validated */
            break;
        }
        break;

    case ACPI_WRITE:

        switch (BitWidth)
        {
        case 8:
            *(UINT8 *) LogicalAddrPtr = (UINT8) *Value;
            break;

        case 16:
            *(UINT16 *) LogicalAddrPtr = (UINT16) *Value;
            break;

        case 32:
            *(UINT32 *) LogicalAddrPtr = (UINT32) *Value;
            break;

#if ACPI_MACHINE_WIDTH != 16
        case 64:
            *(UINT64 *) LogicalAddrPtr = (UINT64) *Value;
            break;
#endif

        default:
            /* BitWidth was already validated */
            break;
        }
        break;

    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemIoSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the System IO address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemIoSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExSystemIoSpaceHandler");


    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "SystemIO %d (%d width) Address=%8.8X%8.8X\n", Function, BitWidth,
        ACPI_HIDWORD (Address), ACPI_LODWORD (Address)));

    /* Decode the function parameter */

    switch (Function)
    {
    case ACPI_READ:

        *Value = 0;
        Status = AcpiOsReadPort ((ACPI_IO_ADDRESS) Address, Value, BitWidth);
        break;

    case ACPI_WRITE:

        Status = AcpiOsWritePort ((ACPI_IO_ADDRESS) Address, *Value, BitWidth);
        break;

    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExPciConfigSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI Config address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExPciConfigSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PCI_ID             *PciId;
    UINT16                  PciRegister;


    ACPI_FUNCTION_TRACE ("ExPciConfigSpaceHandler");


    /*
     *  The arguments to AcpiOs(Read|Write)PciConfiguration are:
     *
     *  PciSegment  is the PCI bus segment range 0-31
     *  PciBus      is the PCI bus number range 0-255
     *  PciDevice   is the PCI device number range 0-31
     *  PciFunction is the PCI device function number
     *  PciRegister is the Config space register range 0-255 bytes
     *
     *  Value - input value for write, output address for read
     *
     */
    PciId       = (ACPI_PCI_ID *) RegionContext;
    PciRegister = (UINT16) (UINT32) Address;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "PciConfig %d (%d) Seg(%04x) Bus(%04x) Dev(%04x) Func(%04x) Reg(%04x)\n",
        Function, BitWidth, PciId->Segment, PciId->Bus, PciId->Device,
        PciId->Function, PciRegister));

    switch (Function)
    {
    case ACPI_READ:

        *Value = 0;
        Status = AcpiOsReadPciConfiguration (PciId, PciRegister, Value, BitWidth);
        break;

    case ACPI_WRITE:

        Status = AcpiOsWritePciConfiguration (PciId, PciRegister, *Value, BitWidth);
        break;

    default:

        Status = AE_BAD_PARAMETER;
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExCmosSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the CMOS address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExCmosSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExCmosSpaceHandler");


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExPciBarSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the PCI BarTarget address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExPciBarSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExPciBarSpaceHandler");


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDataTableSpaceHandler
 *
 * PARAMETERS:  Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              BitWidth            - Field width in bits (8, 16, or 32)
 *              Value               - Pointer to in or out value
 *              HandlerContext      - Pointer to Handler's context
 *              RegionContext       - Pointer to context specific to the
 *                                    accessed region
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Handler for the Data Table address space (Op Region)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDataTableSpaceHandler (
    UINT32                  Function,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  BitWidth,
    ACPI_INTEGER            *Value,
    void                    *HandlerContext,
    void                    *RegionContext)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  ByteWidth = ACPI_DIV_8 (BitWidth);
    UINT32                  i;
    char                    *LogicalAddrPtr;


    ACPI_FUNCTION_TRACE ("ExDataTableSpaceHandler");


    LogicalAddrPtr = ACPI_PHYSADDR_TO_PTR (Address);


   /* Perform the memory read or write */

    switch (Function)
    {
    case ACPI_READ:

        for (i = 0; i < ByteWidth; i++)
        {
            ((char *) Value) [i] = LogicalAddrPtr[i];
        }
        break;

    case ACPI_WRITE:
    default:

        return_ACPI_STATUS (AE_SUPPORT);
    }

    return_ACPI_STATUS (Status);
}


