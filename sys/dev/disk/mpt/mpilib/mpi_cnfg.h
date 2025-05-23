/* $FreeBSD: src/sys/dev/mpt/mpilib/mpi_cnfg.h,v 1.1.2.2 2002/09/01 23:08:06 mjacob Exp $ */
/* $DragonFly: src/sys/dev/disk/mpt/mpilib/mpi_cnfg.h,v 1.2 2003/06/17 04:28:28 dillon Exp $ */
/*
 * Copyright (c) 2000, 2001 by LSI Logic Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *           Name:  MPI_CNFG.H
 *          Title:  MPI Config message, structures, and Pages
 *  Creation Date:  July 27, 2000
 *
 *    MPI Version:  01.02.05
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-08-00  01.00.02  Added _PAGEVERSION definitions for all pages.
 *                      Added FcPhLowestVersion, FcPhHighestVersion, Reserved2
 *                      fields to FC_DEVICE_0 page, updated the page version.
 *                      Changed _FREE_RUNNING_CLOCK to _PACING_TRANSFERS in
 *                      SCSI_PORT_0, SCSI_DEVICE_0 and SCSI_DEVICE_1 pages
 *                      and updated the page versions.
 *                      Added _RESPONSE_ID_MASK definition to SCSI_PORT_1
 *                      page and updated the page version.
 *                      Added Information field and _INFO_PARAMS_NEGOTIATED
 *                      definitionto SCSI_DEVICE_0 page.
 *  06-22-00  01.00.03  Removed batch controls from LAN_0 page and updated the
 *                      page version.
 *                      Added BucketsRemaining to LAN_1 page, redefined the
 *                      state values, and updated the page version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *                      SCSI_DEVICE_0 and SCSI_DEVICE_1 pages.
 *  06-30-00  01.00.04  Added MaxReplySize to LAN_1 page and updated the page
 *                      version.
 *                      Moved FC_DEVICE_0 PageAddress description to spec.
 *  07-27-00  01.00.05  Corrected the SubsystemVendorID and SubsystemID field
 *                      widths in IOC_0 page and updated the page version.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Added Manufacturing pages, IO Unit Page 2, SCSI SPI
 *                      Port Page 2, FC Port Page 4, FC Port Page 5
 *  11-15-00  01.01.02  Interim changes to match proposals
 *  12-04-00  01.01.03  Config page changes to match MPI rev 1.00.01.
 *  12-05-00  01.01.04  Modified config page actions.
 *  01-09-01  01.01.05  Added defines for page address formats.
 *                      Data size for Manufacturing pages 2 and 3 no longer
 *                      defined here.
 *                      Io Unit Page 2 size is fixed at 4 adapters and some
 *                      flags were changed.
 *                      SCSI Port Page 2 Device Settings modified.
 *                      New fields added to FC Port Page 0 and some flags
 *                      cleaned up.
 *                      Removed impedance flash from FC Port Page 1.
 *                      Added FC Port pages 6 and 7.
 *  01-25-01  01.01.06  Added MaxInitiators field to FcPortPage0.
 *  01-29-01  01.01.07  Changed some defines to make them 32 character unique.
 *                      Added some LinkType defines for FcPortPage0.
 *  02-20-01  01.01.08  Started using MPI_POINTER.
 *  02-27-01  01.01.09  Replaced MPI_CONFIG_PAGETYPE_SCSI_LUN with
 *                      MPI_CONFIG_PAGETYPE_RAID_VOLUME.
 *                      Added definitions and structures for IOC Page 2 and
 *                      RAID Volume Page 2.
 *  03-27-01  01.01.10  Added CONFIG_PAGE_FC_PORT_8 and CONFIG_PAGE_FC_PORT_9.
 *                      CONFIG_PAGE_FC_PORT_3 now supports persistent by DID.
 *                      Added VendorId and ProductRevLevel fields to
 *                      RAIDVOL2_IM_PHYS_ID struct.
 *                      Modified values for MPI_FCPORTPAGE0_FLAGS_ATTACH_
 *                      defines to make them compatible to MPI version 1.0.
 *                      Added structure offset comments.
 *  04-09-01  01.01.11  Added some new defines for the PageAddress field and
 *                      removed some obsolete ones.
 *                      Added IO Unit Page 3.
 *                      Modified defines for Scsi Port Page 2.
 *                      Modified RAID Volume Pages.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      Added SepID and SepBus to RVP2 IMPhysicalDisk struct.
 *                      Added defines for the SEP bits in RVP2 VolumeSettings.
 *                      Modified the DeviceSettings field in RVP2 to use the
 *                      proper structure.
 *                      Added defines for SES, SAF-TE, and cross channel for
 *                      IOCPage2 CapabilitiesFlags.
 *                      Removed define for MPI_IOUNITPAGE2_FLAGS_RAID_DISABLE.
 *                      Removed define for
 *                      MPI_SCSIPORTPAGE2_PORT_FLAGS_PARITY_ENABLE.
 *                      Added define for MPI_CONFIG_PAGEATTR_RO_PERSISTENT.
 *  08-29-01 01.02.02   Fixed value for MPI_MANUFACTPAGE_DEVID_53C1035.
 *                      Added defines for MPI_FCPORTPAGE1_FLAGS_HARD_ALPA_ONLY
 *                      and MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY.
 *                      Removed MPI_SCSIPORTPAGE0_CAP_PACING_TRANSFERS,
 *                      MPI_SCSIDEVPAGE0_NP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_RP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_CONF_PPR_ALLOWED.
 *                      Added defines for MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED
 *                      and MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED.
 *                      Added OnBusTimerValue to CONFIG_PAGE_SCSI_PORT_1.
 *                      Added rejected bits to SCSI Device Page 0 Information.
 *                      Increased size of ALPA array in FC Port Page 2 by one
 *                      and removed a one byte reserved field.
 *  09-28-01 01.02.03   Swapped NegWireSpeedLow and NegWireSpeedLow in
 *                      CONFIG_PAGE_LAN_1 to match preferred 64-bit ordering.
 *                      Added structures for Manufacturing Page 4, IO Unit
 *                      Page 3, IOC Page 3, IOC Page 4, RAID Volume Page 0, and
 *                      RAID PhysDisk Page 0.
 *  10-04-01 01.02.04   Added define for MPI_CONFIG_PAGETYPE_RAID_PHYSDISK.
 *                      Modified some of the new defines to make them 32
 *                      character unique.
 *                      Modified how variable length pages (arrays) are defined.
 *                      Added generic defines for hot spare pools and RAID
 *                      volume types.
 *  11-01-01 01.02.05   Added define for MPI_IOUNITPAGE1_DISABLE_IR.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_CNFG_H
#define MPI_CNFG_H


/*****************************************************************************
*
*       C o n f i g    M e s s a g e    a n d    S t r u c t u r e s
*
*****************************************************************************/

typedef struct _CONFIG_PAGE_HEADER
{
    U8                      PageVersion;                /* 00h */
    U8                      PageLength;                 /* 01h */
    U8                      PageNumber;                 /* 02h */
    U8                      PageType;                   /* 03h */
} fCONFIG_PAGE_HEADER, MPI_POINTER PTR_CONFIG_PAGE_HEADER,
  ConfigPageHeader_t, MPI_POINTER pConfigPageHeader_t;

typedef union _CONFIG_PAGE_HEADER_UNION
{
   ConfigPageHeader_t  Struct;
   U8                  Bytes[4];
   U16                 Word16[2];
   U32                 Word32;
} ConfigPageHeaderUnion, MPI_POINTER pConfigPageHeaderUnion,
  fCONFIG_PAGE_HEADER_UNION, MPI_POINTER PTR_CONFIG_PAGE_HEADER_UNION;


/****************************************************************************
*   PageType field values
****************************************************************************/
#define MPI_CONFIG_PAGEATTR_READ_ONLY               (0x00)
#define MPI_CONFIG_PAGEATTR_CHANGEABLE              (0x10)
#define MPI_CONFIG_PAGEATTR_PERSISTENT              (0x20)
#define MPI_CONFIG_PAGEATTR_RO_PERSISTENT           (0x30)
#define MPI_CONFIG_PAGEATTR_MASK                    (0xF0)

#define MPI_CONFIG_PAGETYPE_IO_UNIT                 (0x00)
#define MPI_CONFIG_PAGETYPE_IOC                     (0x01)
#define MPI_CONFIG_PAGETYPE_BIOS                    (0x02)
#define MPI_CONFIG_PAGETYPE_SCSI_PORT               (0x03)
#define MPI_CONFIG_PAGETYPE_SCSI_DEVICE             (0x04)
#define MPI_CONFIG_PAGETYPE_FC_PORT                 (0x05)
#define MPI_CONFIG_PAGETYPE_FC_DEVICE               (0x06)
#define MPI_CONFIG_PAGETYPE_LAN                     (0x07)
#define MPI_CONFIG_PAGETYPE_RAID_VOLUME             (0x08)
#define MPI_CONFIG_PAGETYPE_MANUFACTURING           (0x09)
#define MPI_CONFIG_PAGETYPE_RAID_PHYSDISK           (0x0A)
#define MPI_CONFIG_PAGETYPE_MASK                    (0x0F)

#define MPI_CONFIG_TYPENUM_MASK                     (0x0FFF)


/****************************************************************************
*   PageAddress field values
****************************************************************************/
#define MPI_SCSI_PORT_PGAD_PORT_MASK                (0x000000FF)

#define MPI_SCSI_DEVICE_TARGET_ID_MASK              (0x000000FF)
#define MPI_SCSI_DEVICE_TARGET_ID_SHIFT             (0)
#define MPI_SCSI_DEVICE_BUS_MASK                    (0x0000FF00)
#define MPI_SCSI_DEVICE_BUS_SHIFT                   (8)

#define MPI_FC_PORT_PGAD_PORT_MASK                  (0xF0000000)
#define MPI_FC_PORT_PGAD_PORT_SHIFT                 (28)
#define MPI_FC_PORT_PGAD_FORM_MASK                  (0x0F000000)
#define MPI_FC_PORT_PGAD_FORM_INDEX                 (0x01000000)
#define MPI_FC_PORT_PGAD_INDEX_MASK                 (0x0000FFFF)
#define MPI_FC_PORT_PGAD_INDEX_SHIFT                (0)

#define MPI_FC_DEVICE_PGAD_PORT_MASK                (0xF0000000)
#define MPI_FC_DEVICE_PGAD_PORT_SHIFT               (28)
#define MPI_FC_DEVICE_PGAD_FORM_MASK                (0x0F000000)
#define MPI_FC_DEVICE_PGAD_FORM_NEXT_DID            (0x00000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_MASK             (0xF0000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_SHIFT            (28)
#define MPI_FC_DEVICE_PGAD_ND_DID_MASK              (0x00FFFFFF)
#define MPI_FC_DEVICE_PGAD_ND_DID_SHIFT             (0)
#define MPI_FC_DEVICE_PGAD_FORM_BUS_TID             (0x01000000)
#define MPI_FC_DEVICE_PGAD_BT_BUS_MASK              (0x0000FF00)
#define MPI_FC_DEVICE_PGAD_BT_BUS_SHIFT             (8)
#define MPI_FC_DEVICE_PGAD_BT_TID_MASK              (0x000000FF)
#define MPI_FC_DEVICE_PGAD_BT_TID_SHIFT             (0)

#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_MASK          (0x000000FF)
#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_SHIFT         (0)



/****************************************************************************
*   Config Request Message
****************************************************************************/
typedef struct _MSG_CONFIG
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[8];               /* 0Ch */
    fCONFIG_PAGE_HEADER      Header;                     /* 14h */
    U32                     PageAddress;                /* 18h */
    SGE_IO_UNION            PageBufferSGE;              /* 1Ch */
} MSG_CONFIG, MPI_POINTER PTR_MSG_CONFIG,
  Config_t, MPI_POINTER pConfig_t;


/****************************************************************************
*   Action field values
****************************************************************************/
#define MPI_CONFIG_ACTION_PAGE_HEADER               (0x00)
#define MPI_CONFIG_ACTION_PAGE_READ_CURRENT         (0x01)
#define MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT        (0x02)
#define MPI_CONFIG_ACTION_PAGE_DEFAULT              (0x03)
#define MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM          (0x04)
#define MPI_CONFIG_ACTION_PAGE_READ_DEFAULT         (0x05)
#define MPI_CONFIG_ACTION_PAGE_READ_NVRAM           (0x06)


/* Config Reply Message */
typedef struct _MSG_CONFIG_REPLY
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[2];               /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    fCONFIG_PAGE_HEADER      Header;                     /* 14h */
} MSG_CONFIG_REPLY, MPI_POINTER PTR_MSG_CONFIG_REPLY,
  ConfigReply_t, MPI_POINTER pConfigReply_t;



/*****************************************************************************
*
*               C o n f i g u r a t i o n    P a g e s
*
*****************************************************************************/

/****************************************************************************
*   Manufacturing Config pages
****************************************************************************/
#define MPI_MANUFACTPAGE_DEVICEID_FC909             (0x0621)
#define MPI_MANUFACTPAGE_DEVICEID_FC919             (0x0624)
#define MPI_MANUFACTPAGE_DEVICEID_FC929             (0x0622)
#define MPI_MANUFACTPAGE_DEVICEID_FC919X            (0x0628)
#define MPI_MANUFACTPAGE_DEVICEID_FC929X            (0x0626)
#define MPI_MANUFACTPAGE_DEVID_53C1030              (0x0030)
#define MPI_MANUFACTPAGE_DEVID_53C1030ZC            (0x0031)
#define MPI_MANUFACTPAGE_DEVID_1030_53C1035         (0x0032)
#define MPI_MANUFACTPAGE_DEVID_1030ZC_53C1035       (0x0033)
#define MPI_MANUFACTPAGE_DEVID_53C1035              (0x0040)
#define MPI_MANUFACTPAGE_DEVID_53C1035ZC            (0x0041)

typedef struct _CONFIG_PAGE_MANUFACTURING_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      ChipName[16];               /* 04h */
    U8                      ChipRevision[8];            /* 14h */
    U8                      BoardName[16];              /* 1Ch */
    U8                      BoardAssembly[16];          /* 2Ch */
    U8                      BoardTracerNumber[16];      /* 3Ch */

} fCONFIG_PAGE_MANUFACTURING_0, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_0,
  ManufacturingPage0_t, MPI_POINTER pManufacturingPage0_t;

#define MPI_MANUFACTURING0_PAGEVERSION                 (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      VPD[256];                   /* 04h */
} fCONFIG_PAGE_MANUFACTURING_1, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_1,
  ManufacturingPage1_t, MPI_POINTER pManufacturingPage1_t;

#define MPI_MANUFACTURING1_PAGEVERSION                 (0x00)


typedef struct _MPI_CHIP_REVISION_ID
{
    U16 DeviceID;                                       /* 00h */
    U8  PCIRevisionID;                                  /* 02h */
    U8  Reserved;                                       /* 03h */
} MPI_CHIP_REVISION_ID, MPI_POINTER PTR_MPI_CHIP_REVISION_ID,
  MpiChipRevisionId_t, MPI_POINTER pMpiChipRevisionId_t;


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_2_HW_SETTINGS_WORDS
#define MPI_MAN_PAGE_2_HW_SETTINGS_WORDS    (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_2
{
    fCONFIG_PAGE_HEADER      Header;                                 /* 00h */
    MPI_CHIP_REVISION_ID    ChipId;                                 /* 04h */
    U32                     HwSettings[MPI_MAN_PAGE_2_HW_SETTINGS_WORDS];/* 08h */
} fCONFIG_PAGE_MANUFACTURING_2, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_2,
  ManufacturingPage2_t, MPI_POINTER pManufacturingPage2_t;

#define MPI_MANUFACTURING2_PAGEVERSION                  (0x00)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_3_INFO_WORDS
#define MPI_MAN_PAGE_3_INFO_WORDS           (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_3
{
    fCONFIG_PAGE_HEADER                  Header;                     /* 00h */
    MPI_CHIP_REVISION_ID                ChipId;                     /* 04h */
    U32                                 Info[MPI_MAN_PAGE_3_INFO_WORDS];/* 08h */
} fCONFIG_PAGE_MANUFACTURING_3, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_3,
  ManufacturingPage3_t, MPI_POINTER pManufacturingPage3_t;

#define MPI_MANUFACTURING3_PAGEVERSION                  (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_4
{
    fCONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             Reserved1;          /* 04h */
    U8                              InfoOffset0;        /* 08h */
    U8                              InfoSize0;          /* 09h */
    U8                              InfoOffset1;        /* 0Ah */
    U8                              InfoSize1;          /* 0Bh */
    U8                              InquirySize;        /* 0Ch */
    U8                              Reserved2;          /* 0Dh */
    U16                             Reserved3;          /* 0Eh */
    U8                              InquiryData[56];    /* 10h */
    U32                             ISVolumeSettings;   /* 48h */
    U32                             IMEVolumeSettings;  /* 4Ch */
    U32                             IMVolumeSettings;   /* 50h */
} fCONFIG_PAGE_MANUFACTURING_4, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_4,
  ManufacturingPage4_t, MPI_POINTER pManufacturingPage4_t;

#define MPI_MANUFACTURING4_PAGEVERSION                  (0x00)


/****************************************************************************
*   IO Unit Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IO_UNIT_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     UniqueValue;                /* 04h */
} fCONFIG_PAGE_IO_UNIT_0, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_0,
  IOUnitPage0_t, MPI_POINTER pIOUnitPage0_t;

#define MPI_IOUNITPAGE0_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_IO_UNIT_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
} fCONFIG_PAGE_IO_UNIT_1, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_1,
  IOUnitPage1_t, MPI_POINTER pIOUnitPage1_t;

#define MPI_IOUNITPAGE1_PAGEVERSION                     (0x00)

/* IO Unit Page 1 Flags defines */

#define MPI_IOUNITPAGE1_MULTI_FUNCTION                  (0x00000000)
#define MPI_IOUNITPAGE1_SINGLE_FUNCTION                 (0x00000001)
#define MPI_IOUNITPAGE1_MULTI_PATHING                   (0x00000002)
#define MPI_IOUNITPAGE1_SINGLE_PATHING                  (0x00000000)
#define MPI_IOUNITPAGE1_DISABLE_IR                      (0x00000040)
#define MPI_IOUNITPAGE1_FORCE_32                        (0x00000080)


typedef struct _MPI_ADAPTER_INFO
{
    U8      PciBusNumber;                               /* 00h */
    U8      PciDeviceAndFunctionNumber;                 /* 01h */
    U16     AdapterFlags;                               /* 02h */
} MPI_ADAPTER_INFO, MPI_POINTER PTR_MPI_ADAPTER_INFO,
  MpiAdapterInfo_t, MPI_POINTER pMpiAdapterInfo_t;

#define MPI_ADAPTER_INFO_FLAGS_EMBEDDED                 (0x0001)
#define MPI_ADAPTER_INFO_FLAGS_INIT_STATUS              (0x0002)

typedef struct _CONFIG_PAGE_IO_UNIT_2
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     BiosVersion;                /* 08h */
    MPI_ADAPTER_INFO        AdapterOrder[4];            /* 0Ch */
} fCONFIG_PAGE_IO_UNIT_2, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_2,
  IOUnitPage2_t, MPI_POINTER pIOUnitPage2_t;

#define MPI_IOUNITPAGE2_PAGEVERSION                     (0x00)

#define MPI_IOUNITPAGE2_FLAGS_PAUSE_ON_ERROR            (0x00000002)
#define MPI_IOUNITPAGE2_FLAGS_VERBOSE_ENABLE            (0x00000004)
#define MPI_IOUNITPAGE2_FLAGS_COLOR_VIDEO_DISABLE       (0x00000008)
#define MPI_IOUNITPAGE2_FLAGS_DONT_HOOK_INT_40          (0x00000010)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX
#define MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX     (1)
#endif

typedef struct _CONFIG_PAGE_IO_UNIT_3
{
    fCONFIG_PAGE_HEADER      Header;                                   /* 00h */
    U8                      GPIOCount;                                /* 04h */
    U8                      Reserved1;                                /* 05h */
    U16                     Reserved2;                                /* 06h */
    U16                     GPIOVal[MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX]; /* 08h */
} fCONFIG_PAGE_IO_UNIT_3, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_3,
  IOUnitPage3_t, MPI_POINTER pIOUnitPage3_t;

#define MPI_IOUNITPAGE3_PAGEVERSION                     (0x01)

#define MPI_IOUNITPAGE3_GPIO_FUNCTION_MASK              (0xFC)
#define MPI_IOUNITPAGE3_GPIO_FUNCTION_SHIFT             (2)
#define MPI_IOUNITPAGE3_GPIO_SETTING_OFF                (0x00)
#define MPI_IOUNITPAGE3_GPIO_SETTING_ON                 (0x01)


/****************************************************************************
*   IOC Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IOC_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     TotalNVStore;               /* 04h */
    U32                     FreeNVStore;                /* 08h */
    U16                     VendorID;                   /* 0Ch */
    U16                     DeviceID;                   /* 0Eh */
    U8                      RevisionID;                 /* 10h */
    U8                      Reserved[3];                /* 11h */
    U32                     ClassCode;                  /* 14h */
    U16                     SubsystemVendorID;          /* 18h */
    U16                     SubsystemID;                /* 1Ah */
} fCONFIG_PAGE_IOC_0, MPI_POINTER PTR_CONFIG_PAGE_IOC_0,
  IOCPage0_t, MPI_POINTER pIOCPage0_t;

#define MPI_IOCPAGE0_PAGEVERSION                        (0x01)


typedef struct _CONFIG_PAGE_IOC_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     CoalescingTimeout;          /* 08h */
    U8                      CoalescingDepth;            /* 0Ch */
    U8                      Reserved[3];                /* 0Dh */
} fCONFIG_PAGE_IOC_1, MPI_POINTER PTR_CONFIG_PAGE_IOC_1,
  IOCPage1_t, MPI_POINTER pIOCPage1_t;

#define MPI_IOCPAGE1_PAGEVERSION                        (0x00)

#define MPI_IOCPAGE1_REPLY_COALESCING                   (0x00000001)


typedef struct _CONFIG_PAGE_IOC_2_RAID_VOL
{
    U8                          VolumeID;               /* 00h */
    U8                          VolumeBus;              /* 01h */
    U8                          VolumeIOC;              /* 02h */
    U8                          VolumePageNumber;       /* 03h */
    U8                          VolumeType;             /* 04h */
    U8                          Reserved2;              /* 05h */
    U16                         Reserved3;              /* 06h */
} fCONFIG_PAGE_IOC_2_RAID_VOL, MPI_POINTER PTR_CONFIG_PAGE_IOC_2_RAID_VOL,
  ConfigPageIoc2RaidVol_t, MPI_POINTER pConfigPageIoc2RaidVol_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_2_RAID_VOLUME_MAX
#define MPI_IOC_PAGE_2_RAID_VOLUME_MAX      (1)
#endif

typedef struct _CONFIG_PAGE_IOC_2
{
    fCONFIG_PAGE_HEADER          Header;                              /* 00h */
    U32                         CapabilitiesFlags;                   /* 04h */
    U8                          NumActiveVolumes;                    /* 08h */
    U8                          MaxVolumes;                          /* 09h */
    U8                          NumActivePhysDisks;                  /* 0Ah */
    U8                          MaxPhysDisks;                        /* 0Bh */
    fCONFIG_PAGE_IOC_2_RAID_VOL  RaidVolume[MPI_IOC_PAGE_2_RAID_VOLUME_MAX];/* 0Ch */
} fCONFIG_PAGE_IOC_2, MPI_POINTER PTR_CONFIG_PAGE_IOC_2,
  IOCPage2_t, MPI_POINTER pIOCPage2_t;

#define MPI_IOCPAGE2_PAGEVERSION                        (0x01)

/* IOC Page 2 Capabilities flags */

#define MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT               (0x00000001)
#define MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT              (0x00000002)
#define MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT               (0x00000004)
#define MPI_IOCPAGE2_CAP_FLAGS_SES_SUPPORT              (0x20000000)
#define MPI_IOCPAGE2_CAP_FLAGS_SAFTE_SUPPORT            (0x40000000)
#define MPI_IOCPAGE2_CAP_FLAGS_CROSS_CHANNEL_SUPPORT    (0x80000000)

/* IOC Page 2 Volume RAID Type values, also used in RAID Volume pages */

#define MPI_RAID_VOL_TYPE_IS                        (0x00)
#define MPI_RAID_VOL_TYPE_IME                       (0x01)
#define MPI_RAID_VOL_TYPE_IM                        (0x02)


typedef struct _IOC_3_PHYS_DISK
{
    U8                          PhysDiskID;             /* 00h */
    U8                          PhysDiskBus;            /* 01h */
    U8                          PhysDiskIOC;            /* 02h */
    U8                          PhysDiskNum;            /* 03h */
} IOC_3_PHYS_DISK, MPI_POINTER PTR_IOC_3_PHYS_DISK,
  Ioc3PhysDisk_t, MPI_POINTER pIoc3PhysDisk_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_3_PHYSDISK_MAX
#define MPI_IOC_PAGE_3_PHYSDISK_MAX         (1)
#endif

typedef struct _CONFIG_PAGE_IOC_3
{
    fCONFIG_PAGE_HEADER          Header;                                /* 00h */
    U8                          NumPhysDisks;                          /* 04h */
    U8                          Reserved1;                             /* 05h */
    U16                         Reserved2;                             /* 06h */
    IOC_3_PHYS_DISK             PhysDisk[MPI_IOC_PAGE_3_PHYSDISK_MAX]; /* 08h */
} fCONFIG_PAGE_IOC_3, MPI_POINTER PTR_CONFIG_PAGE_IOC_3,
  IOCPage3_t, MPI_POINTER pIOCPage3_t;

#define MPI_IOCPAGE3_PAGEVERSION                        (0x00)


typedef struct _IOC_4_SEP
{
    U8                          SEPTargetID;            /* 00h */
    U8                          SEPBus;                 /* 01h */
    U16                         Reserved;               /* 02h */
} IOC_4_SEP, MPI_POINTER PTR_IOC_4_SEP,
  Ioc4Sep_t, MPI_POINTER pIoc4Sep_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_4_SEP_MAX
#define MPI_IOC_PAGE_4_SEP_MAX              (1)
#endif

typedef struct _CONFIG_PAGE_IOC_4
{
    fCONFIG_PAGE_HEADER          Header;                         /* 00h */
    U8                          ActiveSEP;                      /* 04h */
    U8                          MaxSEP;                         /* 05h */
    U16                         Reserved1;                      /* 06h */
    IOC_4_SEP                   SEP[MPI_IOC_PAGE_4_SEP_MAX];    /* 08h */
} fCONFIG_PAGE_IOC_4, MPI_POINTER PTR_CONFIG_PAGE_IOC_4,
  IOCPage4_t, MPI_POINTER pIOCPage4_t;

#define MPI_IOCPAGE4_PAGEVERSION                        (0x00)


/****************************************************************************
*   SCSI Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_PORT_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Capabilities;               /* 04h */
    U32                     PhysicalInterface;          /* 08h */
} fCONFIG_PAGE_SCSI_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_0,
  SCSIPortPage0_t, MPI_POINTER pSCSIPortPage0_t;

#define MPI_SCSIPORTPAGE0_PAGEVERSION                   (0x01)

#define MPI_SCSIPORTPAGE0_CAP_IU                        (0x00000001)
#define MPI_SCSIPORTPAGE0_CAP_DT                        (0x00000002)
#define MPI_SCSIPORTPAGE0_CAP_QAS                       (0x00000004)
#define MPI_SCSIPORTPAGE0_CAP_MIN_SYNC_PERIOD_MASK      (0x0000FF00)
#define MPI_SCSIPORTPAGE0_CAP_MAX_SYNC_OFFSET_MASK      (0x00FF0000)
#define MPI_SCSIPORTPAGE0_CAP_WIDE                      (0x20000000)
#define MPI_SCSIPORTPAGE0_CAP_AIP                       (0x80000000)

#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_TYPE_MASK          (0x00000003)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_HVD                (0x01)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_SE                 (0x02)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_LVD                (0x03)


typedef struct _CONFIG_PAGE_SCSI_PORT_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Configuration;              /* 04h */
    U32                     OnBusTimerValue;            /* 08h */
} fCONFIG_PAGE_SCSI_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_1,
  SCSIPortPage1_t, MPI_POINTER pSCSIPortPage1_t;

#define MPI_SCSIPORTPAGE1_PAGEVERSION                   (0x02)

#define MPI_SCSIPORTPAGE1_CFG_PORT_SCSI_ID_MASK         (0x000000FF)
#define MPI_SCSIPORTPAGE1_CFG_PORT_RESPONSE_ID_MASK     (0xFFFF0000)


typedef struct _MPI_DEVICE_INFO
{
    U8      Timeout;                                    /* 00h */
    U8      SyncFactor;                                 /* 01h */
    U16     DeviceFlags;                                /* 02h */
} MPI_DEVICE_INFO, MPI_POINTER PTR_MPI_DEVICE_INFO,
  MpiDeviceInfo_t, MPI_POINTER pMpiDeviceInfo_t;

typedef struct _CONFIG_PAGE_SCSI_PORT_2
{
    fCONFIG_PAGE_HEADER  Header;                         /* 00h */
    U32                 PortFlags;                      /* 04h */
    U32                 PortSettings;                   /* 08h */
    MPI_DEVICE_INFO     DeviceSettings[16];             /* 0Ch */
} fCONFIG_PAGE_SCSI_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_2,
  SCSIPortPage2_t, MPI_POINTER pSCSIPortPage2_t;

#define MPI_SCSIPORTPAGE2_PAGEVERSION                       (0x01)

#define MPI_SCSIPORTPAGE2_PORT_FLAGS_SCAN_HIGH_TO_LOW       (0x00000001)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_AVOID_SCSI_RESET       (0x00000004)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_ALTERNATE_CHS          (0x00000008)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_TERMINATION_DISABLE    (0x00000010)

#define MPI_SCSIPORTPAGE2_PORT_HOST_ID_MASK                 (0x0000000F)
#define MPI_SCSIPORTPAGE2_PORT_MASK_INIT_HBA                (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_DISABLE_INIT_HBA             (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_INIT_HBA                (0x00000010)
#define MPI_SCSIPORTPAGE2_PORT_OS_INIT_HBA                  (0x00000020)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_OS_INIT_HBA             (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_REMOVABLE_MEDIA              (0x000000C0)
#define MPI_SCSIPORTPAGE2_PORT_SPINUP_DELAY_MASK            (0x00000F00)
#define MPI_SCSIPORTPAGE2_PORT_MASK_NEGO_MASTER_SETTINGS    (0x00003000)
#define MPI_SCSIPORTPAGE2_PORT_NEGO_MASTER_SETTINGS         (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_NONE_MASTER_SETTINGS         (0x00001000)
#define MPI_SCSIPORTPAGE2_PORT_ALL_MASTER_SETTINGS          (0x00003000)

#define MPI_SCSIPORTPAGE2_DEVICE_DISCONNECT_ENABLE          (0x0001)
#define MPI_SCSIPORTPAGE2_DEVICE_ID_SCAN_ENABLE             (0x0002)
#define MPI_SCSIPORTPAGE2_DEVICE_LUN_SCAN_ENABLE            (0x0004)
#define MPI_SCSIPORTPAGE2_DEVICE_TAG_QUEUE_ENABLE           (0x0008)
#define MPI_SCSIPORTPAGE2_DEVICE_WIDE_DISABLE               (0x0010)
#define MPI_SCSIPORTPAGE2_DEVICE_BOOT_CHOICE                (0x0020)


/****************************************************************************
*   SCSI Target Device Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_DEVICE_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     NegotiatedParameters;       /* 04h */
    U32                     Information;                /* 08h */
} fCONFIG_PAGE_SCSI_DEVICE_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_0,
  SCSIDevicePage0_t, MPI_POINTER pSCSIDevicePage0_t;

#define MPI_SCSIDEVPAGE0_PAGEVERSION                    (0x02)

#define MPI_SCSIDEVPAGE0_NP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE0_NP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE0_NP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE0_NP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE0_NP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE0_INFO_PARAMS_NEGOTIATED         (0x00000001)
#define MPI_SCSIDEVPAGE0_INFO_SDTR_REJECTED             (0x00000002)
#define MPI_SCSIDEVPAGE0_INFO_WDTR_REJECTED             (0x00000004)
#define MPI_SCSIDEVPAGE0_INFO_PPR_REJECTED              (0x00000008)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     RequestedParameters;        /* 04h */
    U32                     Reserved;                   /* 08h */
    U32                     Configuration;              /* 0Ch */
} fCONFIG_PAGE_SCSI_DEVICE_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_1,
  SCSIDevicePage1_t, MPI_POINTER pSCSIDevicePage1_t;

#define MPI_SCSIDEVPAGE1_PAGEVERSION                    (0x03)

#define MPI_SCSIDEVPAGE1_RP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE1_RP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE1_RP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE1_RP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE1_RP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE1_DV_LVD_DRIVE_STRENGTH_MASK     (0x00000003)
#define MPI_SCSIDEVPAGE1_DV_SE_SLEW_RATE_MASK           (0x00000300)

#define MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED           (0x00000002)
#define MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED           (0x00000004)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_2
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     DomainValidation;           /* 04h */
    U32                     ParityPipeSelect;           /* 08h */
    U32                     DataPipeSelect;             /* 0Ch */
} fCONFIG_PAGE_SCSI_DEVICE_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_2,
  SCSIDevicePage2_t, MPI_POINTER pSCSIDevicePage2_t;

#define MPI_SCSIDEVPAGE2_PAGEVERSION                    (0x00)

#define MPI_SCSIDEVPAGE2_DV_ISI_ENABLE                  (0x00000010)
#define MPI_SCSIDEVPAGE2_DV_SECONDARY_DRIVER_ENABLE     (0x00000020)
#define MPI_SCSIDEVPAGE2_DV_SLEW_RATE_CTRL              (0x00000380)
#define MPI_SCSIDEVPAGE2_DV_PRIM_DRIVE_STR_CTRL         (0x00001C00)
#define MPI_SCSIDEVPAGE2_DV_SECOND_DRIVE_STR_CTRL       (0x0000E000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_ST                    (0x10000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_ST                    (0x20000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_DT                    (0x40000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_DT                    (0x80000000)

#define MPI_SCSIDEVPAGE2_PPS_PPS_MASK                   (0x00000003)

#define MPI_SCSIDEVPAGE2_DPS_BIT_0_PL_SELECT_MASK       (0x00000003)
#define MPI_SCSIDEVPAGE2_DPS_BIT_1_PL_SELECT_MASK       (0x0000000C)
#define MPI_SCSIDEVPAGE2_DPS_BIT_2_PL_SELECT_MASK       (0x00000030)
#define MPI_SCSIDEVPAGE2_DPS_BIT_3_PL_SELECT_MASK       (0x000000C0)
#define MPI_SCSIDEVPAGE2_DPS_BIT_4_PL_SELECT_MASK       (0x00000300)
#define MPI_SCSIDEVPAGE2_DPS_BIT_5_PL_SELECT_MASK       (0x00000C00)
#define MPI_SCSIDEVPAGE2_DPS_BIT_6_PL_SELECT_MASK       (0x00003000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_7_PL_SELECT_MASK       (0x0000C000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_8_PL_SELECT_MASK       (0x00030000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_9_PL_SELECT_MASK       (0x000C0000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_10_PL_SELECT_MASK      (0x00300000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_11_PL_SELECT_MASK      (0x00C00000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_12_PL_SELECT_MASK      (0x03000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_13_PL_SELECT_MASK      (0x0C000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_14_PL_SELECT_MASK      (0x30000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_15_PL_SELECT_MASK      (0xC0000000)


/****************************************************************************
*   FC Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_FC_PORT_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U8                      MPIPortNumber;              /* 08h */
    U8                      LinkType;                   /* 09h */
    U8                      PortState;                  /* 0Ah */
    U8                      Reserved;                   /* 0Bh */
    U32                     PortIdentifier;             /* 0Ch */
    U64                     WWNN;                       /* 10h */
    U64                     WWPN;                       /* 18h */
    U32                     SupportedServiceClass;      /* 20h */
    U32                     SupportedSpeeds;            /* 24h */
    U32                     CurrentSpeed;               /* 28h */
    U32                     MaxFrameSize;               /* 2Ch */
    U64                     FabricWWNN;                 /* 30h */
    U64                     FabricWWPN;                 /* 38h */
    U32                     DiscoveredPortsCount;       /* 40h */
    U32                     MaxInitiators;              /* 44h */
} fCONFIG_PAGE_FC_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_0,
  FCPortPage0_t, MPI_POINTER pFCPortPage0_t;

#define MPI_FCPORTPAGE0_PAGEVERSION                     (0x01)

#define MPI_FCPORTPAGE0_FLAGS_PROT_MASK                 (0x0000000F)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_INIT             (MPI_PORTFACTS_PROTOCOL_INITIATOR)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_TARG             (MPI_PORTFACTS_PROTOCOL_TARGET)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LAN                  (MPI_PORTFACTS_PROTOCOL_LAN)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LOGBUSADDR           (MPI_PORTFACTS_PROTOCOL_LOGBUSADDR)

#define MPI_FCPORTPAGE0_FLAGS_ALIAS_ALPA_SUPPORTED      (0x00000010)
#define MPI_FCPORTPAGE0_FLAGS_ALIAS_WWN_SUPPORTED       (0x00000020)
#define MPI_FCPORTPAGE0_FLAGS_FABRIC_WWN_VALID          (0x00000030)

#define MPI_FCPORTPAGE0_FLAGS_ATTACH_TYPE_MASK          (0x00000F00)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_NO_INIT            (0x00000000)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_POINT_TO_POINT     (0x00000100)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PRIVATE_LOOP       (0x00000200)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_FABRIC_DIRECT      (0x00000400)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PUBLIC_LOOP        (0x00000800)

#define MPI_FCPORTPAGE0_LTYPE_RESERVED                  (0x00)
#define MPI_FCPORTPAGE0_LTYPE_OTHER                     (0x01)
#define MPI_FCPORTPAGE0_LTYPE_UNKNOWN                   (0x02)
#define MPI_FCPORTPAGE0_LTYPE_COPPER                    (0x03)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1300               (0x04)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1500               (0x05)
#define MPI_FCPORTPAGE0_LTYPE_50_LASER_MULTI            (0x06)
#define MPI_FCPORTPAGE0_LTYPE_50_LED_MULTI              (0x07)
#define MPI_FCPORTPAGE0_LTYPE_62_LASER_MULTI            (0x08)
#define MPI_FCPORTPAGE0_LTYPE_62_LED_MULTI              (0x09)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_LONG_WAVE           (0x0A)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_SHORT_WAVE          (0x0B)
#define MPI_FCPORTPAGE0_LTYPE_LASER_SHORT_WAVE          (0x0C)
#define MPI_FCPORTPAGE0_LTYPE_LED_SHORT_WAVE            (0x0D)
#define MPI_FCPORTPAGE0_LTYPE_1300_LONG_WAVE            (0x0E)
#define MPI_FCPORTPAGE0_LTYPE_1500_LONG_WAVE            (0x0F)

#define MPI_FCPORTPAGE0_PORTSTATE_UNKNOWN               (0x01)      /*(SNIA)HBA_PORTSTATE_UNKNOWN       1 Unknown */
#define MPI_FCPORTPAGE0_PORTSTATE_ONLINE                (0x02)      /*(SNIA)HBA_PORTSTATE_ONLINE        2 Operational */
#define MPI_FCPORTPAGE0_PORTSTATE_OFFLINE               (0x03)      /*(SNIA)HBA_PORTSTATE_OFFLINE       3 User Offline */
#define MPI_FCPORTPAGE0_PORTSTATE_BYPASSED              (0x04)      /*(SNIA)HBA_PORTSTATE_BYPASSED      4 Bypassed */
#define MPI_FCPORTPAGE0_PORTSTATE_DIAGNOST              (0x05)      /*(SNIA)HBA_PORTSTATE_DIAGNOSTICS   5 In diagnostics mode */
#define MPI_FCPORTPAGE0_PORTSTATE_LINKDOWN              (0x06)      /*(SNIA)HBA_PORTSTATE_LINKDOWN      6 Link Down */
#define MPI_FCPORTPAGE0_PORTSTATE_ERROR                 (0x07)      /*(SNIA)HBA_PORTSTATE_ERROR         7 Port Error */
#define MPI_FCPORTPAGE0_PORTSTATE_LOOPBACK              (0x08)      /*(SNIA)HBA_PORTSTATE_LOOPBACK      8 Loopback */

#define MPI_FCPORTPAGE0_SUPPORT_CLASS_1                 (0x00000001)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_2                 (0x00000002)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_3                 (0x00000004)

#define MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED             (0x00000001) /* (SNIA)HBA_PORTSPEED_1GBIT 1  1 GBit/sec  */
#define MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED             (0x00000002) /* (SNIA)HBA_PORTSPEED_2GBIT 2  2 GBit/sec  */
#define MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED            (0x00000004) /* (SNIA)HBA_PORTSPEED_10GBIT 4 10 GBit/sec */

#define MPI_FCPORTPAGE0_CURRENT_SPEED_1GBIT             MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_2GBIT             MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_10GBIT            MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED


typedef struct _CONFIG_PAGE_FC_PORT_1
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U64                     NoSEEPROMWWNN;              /* 08h */
    U64                     NoSEEPROMWWPN;              /* 10h */
    U8                      HardALPA;                   /* 18h */
    U8                      LinkConfig;                 /* 19h */
    U8                      TopologyConfig;             /* 1Ah */
    U8                      Reserved;                   /* 1Bh */
} fCONFIG_PAGE_FC_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_1,
  FCPortPage1_t, MPI_POINTER pFCPortPage1_t;

#define MPI_FCPORTPAGE1_PAGEVERSION                     (0x02)

#define MPI_FCPORTPAGE1_FLAGS_EXT_FCP_STATUS_EN         (0x08000000)
#define MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY     (0x04000000)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_DID               (0x00000001)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_WWN               (0x00000000)

#define MPI_FCPORTPAGE1_FLAGS_PROT_MASK                 (0xF0000000)
#define MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT                (28)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_INIT             ((U32)MPI_PORTFACTS_PROTOCOL_INITIATOR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_TARG             ((U32)MPI_PORTFACTS_PROTOCOL_TARGET << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LAN                  ((U32)MPI_PORTFACTS_PROTOCOL_LAN << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LOGBUSADDR           ((U32)MPI_PORTFACTS_PROTOCOL_LOGBUSADDR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)

#define MPI_FCPORTPAGE1_HARD_ALPA_NOT_USED              (0xFF)

#define MPI_FCPORTPAGE1_LCONFIG_SPEED_MASK              (0x0F)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_1GIG              (0x00)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_2GIG              (0x01)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_4GIG              (0x02)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_10GIG             (0x03)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_AUTO              (0x0F)

#define MPI_FCPORTPAGE1_TOPOLOGY_MASK                   (0x0F)
#define MPI_FCPORTPAGE1_TOPOLOGY_NLPORT                 (0x01)
#define MPI_FCPORTPAGE1_TOPOLOGY_NPORT                  (0x02)
#define MPI_FCPORTPAGE1_TOPOLOGY_AUTO                   (0x0F)


typedef struct _CONFIG_PAGE_FC_PORT_2
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      NumberActive;               /* 04h */
    U8                      ALPA[127];                  /* 05h */
} fCONFIG_PAGE_FC_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_2,
  FCPortPage2_t, MPI_POINTER pFCPortPage2_t;

#define MPI_FCPORTPAGE2_PAGEVERSION                     (0x01)


typedef struct _WWN_FORMAT
{
    U64                     WWNN;                       /* 00h */
    U64                     WWPN;                       /* 08h */
} WWN_FORMAT, MPI_POINTER PTR_WWN_FORMAT,
  WWNFormat, MPI_POINTER pWWNFormat;

typedef union _FC_PORT_PERSISTENT_PHYSICAL_ID
{
    WWN_FORMAT              WWN;
    U32                     Did;
} FC_PORT_PERSISTENT_PHYSICAL_ID, MPI_POINTER PTR_FC_PORT_PERSISTENT_PHYSICAL_ID,
  PersistentPhysicalId_t, MPI_POINTER pPersistentPhysicalId_t;

typedef struct _FC_PORT_PERSISTENT
{
    FC_PORT_PERSISTENT_PHYSICAL_ID  PhysicalIdentifier; /* 00h */
    U8                              TargetID;           /* 10h */
    U8                              Bus;                /* 11h */
    U16                             Flags;              /* 12h */
} FC_PORT_PERSISTENT, MPI_POINTER PTR_FC_PORT_PERSISTENT,
  PersistentData_t, MPI_POINTER pPersistentData_t;

#define MPI_PERSISTENT_FLAGS_SHIFT                      (16)
#define MPI_PERSISTENT_FLAGS_ENTRY_VALID                (0x0001)
#define MPI_PERSISTENT_FLAGS_SCAN_ID                    (0x0002)
#define MPI_PERSISTENT_FLAGS_SCAN_LUNS                  (0x0004)
#define MPI_PERSISTENT_FLAGS_BOOT_DEVICE                (0x0008)
#define MPI_PERSISTENT_FLAGS_BY_DID                     (0x0080)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_FC_PORT_PAGE_3_ENTRY_MAX
#define MPI_FC_PORT_PAGE_3_ENTRY_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_FC_PORT_3
{
    fCONFIG_PAGE_HEADER      Header;                                 /* 00h */
    FC_PORT_PERSISTENT      Entry[MPI_FC_PORT_PAGE_3_ENTRY_MAX];    /* 04h */
} fCONFIG_PAGE_FC_PORT_3, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_3,
  FCPortPage3_t, MPI_POINTER pFCPortPage3_t;

#define MPI_FCPORTPAGE3_PAGEVERSION                     (0x01)


typedef struct _CONFIG_PAGE_FC_PORT_4
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     PortFlags;                  /* 04h */
    U32                     PortSettings;               /* 08h */
} fCONFIG_PAGE_FC_PORT_4, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_4,
  FCPortPage4_t, MPI_POINTER pFCPortPage4_t;

#define MPI_FCPORTPAGE4_PAGEVERSION                     (0x00)

#define MPI_FCPORTPAGE4_PORT_FLAGS_ALTERNATE_CHS        (0x00000008)

#define MPI_FCPORTPAGE4_PORT_MASK_INIT_HBA              (0x00000030)
#define MPI_FCPORTPAGE4_PORT_DISABLE_INIT_HBA           (0x00000000)
#define MPI_FCPORTPAGE4_PORT_BIOS_INIT_HBA              (0x00000010)
#define MPI_FCPORTPAGE4_PORT_OS_INIT_HBA                (0x00000020)
#define MPI_FCPORTPAGE4_PORT_BIOS_OS_INIT_HBA           (0x00000030)
#define MPI_FCPORTPAGE4_PORT_REMOVABLE_MEDIA            (0x000000C0)
#define MPI_FCPORTPAGE4_PORT_SPINUP_DELAY_MASK          (0x00000F00)


typedef struct _CONFIG_PAGE_FC_PORT_5_ALIAS_INFO
{
    U8      Flags;                                      /* 00h */
    U8      AliasAlpa;                                  /* 01h */
    U16     Reserved;                                   /* 02h */
    U64     AliasWWNN;                                  /* 04h */
    U64     AliasWWPN;                                  /* 0Ch */
} fCONFIG_PAGE_FC_PORT_5_ALIAS_INFO,
  MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_5_ALIAS_INFO,
  FcPortPage5AliasInfo_t, MPI_POINTER pFcPortPage5AliasInfo_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_FC_PORT_PAGE_5_ALIAS_MAX
#define MPI_FC_PORT_PAGE_5_ALIAS_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_FC_PORT_5
{
    fCONFIG_PAGE_HEADER                  Header;                     /* 00h */
    fCONFIG_PAGE_FC_PORT_5_ALIAS_INFO    AliasInfo[MPI_FC_PORT_PAGE_5_ALIAS_MAX];/* 04h */
} fCONFIG_PAGE_FC_PORT_5, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_5,
  FCPortPage5_t, MPI_POINTER pFCPortPage5_t;

#define MPI_FCPORTPAGE5_PAGEVERSION                     (0x00)

#define MPI_FCPORTPAGE5_FLAGS_ALIAS_ALPA_VALID          (0x01)
#define MPI_FCPORTPAGE5_FLAGS_ALIAS_WWN_VALID           (0x02)


typedef struct _CONFIG_PAGE_FC_PORT_6
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U64                     TimeSinceReset;             /* 08h */
    U64                     TxFrames;                   /* 10h */
    U64                     RxFrames;                   /* 18h */
    U64                     TxWords;                    /* 20h */
    U64                     RxWords;                    /* 28h */
    U64                     LipCount;                   /* 30h */
    U64                     NosCount;                   /* 38h */
    U64                     ErrorFrames;                /* 40h */
    U64                     DumpedFrames;               /* 48h */
    U64                     LinkFailureCount;           /* 50h */
    U64                     LossOfSyncCount;            /* 58h */
    U64                     LossOfSignalCount;          /* 60h */
    U64                     PrimativeSeqErrCount;       /* 68h */
    U64                     InvalidTxWordCount;         /* 70h */
    U64                     InvalidCrcCount;            /* 78h */
    U64                     FcpInitiatorIoCount;        /* 80h */
} fCONFIG_PAGE_FC_PORT_6, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_6,
  FCPortPage6_t, MPI_POINTER pFCPortPage6_t;

#define MPI_FCPORTPAGE6_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_7
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U8                      PortSymbolicName[256];      /* 08h */
} fCONFIG_PAGE_FC_PORT_7, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_7,
  FCPortPage7_t, MPI_POINTER pFCPortPage7_t;

#define MPI_FCPORTPAGE7_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_8
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     BitVector[8];               /* 04h */
} fCONFIG_PAGE_FC_PORT_8, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_8,
  FCPortPage8_t, MPI_POINTER pFCPortPage8_t;

#define MPI_FCPORTPAGE8_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_9
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U64                     GlobalWWPN;                 /* 08h */
    U64                     GlobalWWNN;                 /* 10h */
    U32                     UnitType;                   /* 18h */
    U32                     PhysicalPortNumber;         /* 1Ch */
    U32                     NumAttachedNodes;           /* 20h */
    U16                     IPVersion;                  /* 24h */
    U16                     UDPPortNumber;              /* 26h */
    U8                      IPAddress[16];              /* 28h */
    U16                     Reserved1;                  /* 38h */
    U16                     TopologyDiscoveryFlags;     /* 3Ah */
} fCONFIG_PAGE_FC_PORT_9, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_9,
  FCPortPage9_t, MPI_POINTER pFCPortPage9_t;

#define MPI_FCPORTPAGE9_PAGEVERSION                     (0x00)


/****************************************************************************
*   FC Device Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_FC_DEVICE_0
{
    fCONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     WWNN;                       /* 04h */
    U64                     WWPN;                       /* 0Ch */
    U32                     PortIdentifier;             /* 14h */
    U8                      Protocol;                   /* 18h */
    U8                      Flags;                      /* 19h */
    U16                     BBCredit;                   /* 1Ah */
    U16                     MaxRxFrameSize;             /* 1Ch */
    U8                      Reserved1;                  /* 1Eh */
    U8                      PortNumber;                 /* 1Fh */
    U8                      FcPhLowestVersion;          /* 20h */
    U8                      FcPhHighestVersion;         /* 21h */
    U8                      CurrentTargetID;            /* 22h */
    U8                      CurrentBus;                 /* 23h */
} fCONFIG_PAGE_FC_DEVICE_0, MPI_POINTER PTR_CONFIG_PAGE_FC_DEVICE_0,
  FCDevicePage0_t, MPI_POINTER pFCDevicePage0_t;

#define MPI_FC_DEVICE_PAGE0_PAGEVERSION                 (0x02)

#define MPI_FC_DEVICE_PAGE0_FLAGS_TARGETID_BUS_VALID    (0x01)

#define MPI_FC_DEVICE_PAGE0_PROT_IP                     (0x01)
#define MPI_FC_DEVICE_PAGE0_PROT_FCP_TARGET             (0x02)
#define MPI_FC_DEVICE_PAGE0_PROT_FCP_INITIATOR          (0x04)

#define MPI_FC_DEVICE_PAGE0_PGAD_PORT_MASK      (MPI_FC_DEVICE_PGAD_PORT_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_MASK      (MPI_FC_DEVICE_PGAD_FORM_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_NEXT_DID  (MPI_FC_DEVICE_PGAD_FORM_NEXT_DID)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_BUS_TID   (MPI_FC_DEVICE_PGAD_FORM_BUS_TID)
#define MPI_FC_DEVICE_PAGE0_PGAD_DID_MASK       (MPI_FC_DEVICE_PGAD_ND_DID_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_BUS_MASK       (MPI_FC_DEVICE_PGAD_BT_BUS_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_BUS_SHIFT      (MPI_FC_DEVICE_PGAD_BT_BUS_SHIFT)
#define MPI_FC_DEVICE_PAGE0_PGAD_TID_MASK       (MPI_FC_DEVICE_PGAD_BT_TID_MASK)


/****************************************************************************
*   RAID Volume Config Pages
****************************************************************************/

typedef struct _RAID_VOL0_PHYS_DISK
{
    U16                         Reserved;               /* 00h */
    U8                          PhysDiskMap;            /* 02h */
    U8                          PhysDiskNum;            /* 03h */
} RAID_VOL0_PHYS_DISK, MPI_POINTER PTR_RAID_VOL0_PHYS_DISK,
  RaidVol0PhysDisk_t, MPI_POINTER pRaidVol0PhysDisk_t;

#define MPI_RAIDVOL0_PHYSDISK_PRIMARY                   (0x01)
#define MPI_RAIDVOL0_PHYSDISK_SECONDARY                 (0x02)

typedef struct _RAID_VOL0_STATUS
{
    U8                          Flags;                  /* 00h */
    U8                          State;                  /* 01h */
    U16                         Reserved;               /* 02h */
} RAID_VOL0_STATUS, MPI_POINTER PTR_RAID_VOL0_STATUS,
  RaidVol0Status_t, MPI_POINTER pRaidVol0Status_t;

/* RAID Volume Page 0 VolumeStatus defines */

#define MPI_RAIDVOL0_STATUS_FLAG_ENABLED                (0x01)
#define MPI_RAIDVOL0_STATUS_FLAG_QUIESCED               (0x02)
#define MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS     (0x04)

#define MPI_RAIDVOL0_STATUS_STATE_OPTIMAL               (0x00)
#define MPI_RAIDVOL0_STATUS_STATE_DEGRADED              (0x01)
#define MPI_RAIDVOL0_STATUS_STATE_FAILED                (0x02)

typedef struct _RAID_VOL0_SETTINGS
{
    U16                         Settings;       /* 00h */
    U8                          HotSparePool;   /* 01h */ /* MPI_RAID_HOT_SPARE_POOL_ */
    U8                          Reserved;       /* 02h */
} RAID_VOL0_SETTINGS, MPI_POINTER PTR_RAID_VOL0_SETTINGS,
  RaidVol0Settings, MPI_POINTER pRaidVol0Settings;

/* RAID Volume Page 0 VolumeSettings defines */

#define MPI_RAIDVOL0_SETTING_WRITE_CACHING_ENABLE       (0x0001)
#define MPI_RAIDVOL0_SETTING_OFFLINE_ON_SMART           (0x0002)
#define MPI_RAIDVOL0_SETTING_AUTO_CONFIGURE             (0x0004)
#define MPI_RAIDVOL0_SETTING_PRIORITY_RESYNC            (0x0008)
#define MPI_RAIDVOL0_SETTING_USE_PRODUCT_ID_SUFFIX      (0x0010)
#define MPI_RAIDVOL0_SETTING_USE_DEFAULTS               (0x8000)

/* RAID Volume Page 0 HotSparePool defines, also used in RAID Physical Disk */
#define MPI_RAID_HOT_SPARE_POOL_0                       (0x01)
#define MPI_RAID_HOT_SPARE_POOL_1                       (0x02)
#define MPI_RAID_HOT_SPARE_POOL_2                       (0x04)
#define MPI_RAID_HOT_SPARE_POOL_3                       (0x08)
#define MPI_RAID_HOT_SPARE_POOL_4                       (0x10)
#define MPI_RAID_HOT_SPARE_POOL_5                       (0x20)
#define MPI_RAID_HOT_SPARE_POOL_6                       (0x40)
#define MPI_RAID_HOT_SPARE_POOL_7                       (0x80)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX
#define MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_RAID_VOL_0
{
    fCONFIG_PAGE_HEADER      Header;         /* 00h */
    U8                      VolumeID;       /* 04h */
    U8                      VolumeBus;      /* 05h */
    U8                      VolumeIOC;      /* 06h */
    U8                      VolumeType;     /* 07h */ /* MPI_RAID_VOL_TYPE_ */
    RAID_VOL0_STATUS        VolumeStatus;   /* 08h */
    RAID_VOL0_SETTINGS      VolumeSettings; /* 0Ch */
    U32                     MaxLBA;         /* 10h */
    U32                     Reserved1;      /* 14h */
    U32                     StripeSize;     /* 18h */
    U32                     Reserved2;      /* 1Ch */
    U32                     Reserved3;      /* 20h */
    U8                      NumPhysDisks;   /* 24h */
    U8                      Reserved4;      /* 25h */
    U16                     Reserved5;      /* 26h */
    RAID_VOL0_PHYS_DISK     PhysDisk[MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX];/* 28h */
} fCONFIG_PAGE_RAID_VOL_0, MPI_POINTER PTR_CONFIG_PAGE_RAID_VOL_0,
  RaidVolumePage0_t, MPI_POINTER pRaidVolumePage0_t;

#define MPI_RAIDVOLPAGE0_PAGEVERSION                    (0x00)


/****************************************************************************
*   RAID Physical Disk Config Pages
****************************************************************************/

typedef struct _RAID_PHYS_DISK0_ERROR_DATA
{
    U8                      ErrorCdbByte;               /* 00h */
    U8                      ErrorSenseKey;              /* 01h */
    U16                     Reserved;                   /* 02h */
    U16                     ErrorCount;                 /* 04h */
    U8                      ErrorASC;                   /* 06h */
    U8                      ErrorASCQ;                  /* 07h */
    U16                     SmartCount;                 /* 08h */
    U8                      SmartASC;                   /* 0Ah */
    U8                      SmartASCQ;                  /* 0Bh */
} RAID_PHYS_DISK0_ERROR_DATA, MPI_POINTER PTR_RAID_PHYS_DISK0_ERROR_DATA,
  RaidPhysDisk0ErrorData_t, MPI_POINTER pRaidPhysDisk0ErrorData_t;

typedef struct _RAID_PHYS_DISK_INQUIRY_DATA
{
    U8                          VendorID[8];            /* 00h */
    U8                          ProductID[16];          /* 08h */
    U8                          ProductRevLevel[4];     /* 18h */
    U8                          Info[32];               /* 1Ch */
} RAID_PHYS_DISK0_INQUIRY_DATA, MPI_POINTER PTR_RAID_PHYS_DISK0_INQUIRY_DATA,
  RaidPhysDisk0InquiryData, MPI_POINTER pRaidPhysDisk0InquiryData;

typedef struct _RAID_PHYS_DISK0_SETTINGS
{
    U8              SepID;              /* 00h */
    U8              SepBus;             /* 01h */
    U8              HotSparePool;       /* 02h */ /* MPI_RAID_HOT_SPARE_POOL_ */
    U8              PhysDiskSettings;   /* 03h */
} RAID_PHYS_DISK0_SETTINGS, MPI_POINTER PTR_RAID_PHYS_DISK0_SETTINGS,
  RaidPhysDiskSettings_t, MPI_POINTER pRaidPhysDiskSettings_t;

typedef struct _RAID_PHYS_DISK0_STATUS
{
    U8                              Flags;              /* 00h */
    U8                              State;              /* 01h */
    U16                             Reserved;           /* 02h */
} RAID_PHYS_DISK0_STATUS, MPI_POINTER PTR_RAID_PHYS_DISK0_STATUS,
  RaidPhysDiskStatus_t, MPI_POINTER pRaidPhysDiskStatus_t;

/* RAID Volume 2 IM Physical Disk DiskStatus flags */

#define MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC           (0x01)
#define MPI_PHYSDISK0_STATUS_FLAG_QUIESCED              (0x02)

#define MPI_PHYSDISK0_STATUS_ONLINE                     (0x00)
#define MPI_PHYSDISK0_STATUS_MISSING                    (0x01)
#define MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE             (0x02)
#define MPI_PHYSDISK0_STATUS_FAILED                     (0x03)
#define MPI_PHYSDISK0_STATUS_INITIALIZING               (0x04)
#define MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED          (0x05)
#define MPI_PHYSDISK0_STATUS_FAILED_REQUESTED           (0x06)
#define MPI_PHYSDISK0_STATUS_OTHER_OFFLINE              (0xFF)

typedef struct _CONFIG_PAGE_RAID_PHYS_DISK_0
{
    fCONFIG_PAGE_HEADER              Header;             /* 00h */
    U8                              PhysDiskID;         /* 04h */
    U8                              PhysDiskBus;        /* 05h */
    U8                              PhysDiskIOC;        /* 06h */
    U8                              PhysDiskNum;        /* 07h */
    RAID_PHYS_DISK0_SETTINGS        PhysDiskSettings;   /* 08h */
    U32                             Reserved1;          /* 0Ch */
    U32                             Reserved2;          /* 10h */
    U32                             Reserved3;          /* 14h */
    U8                              DiskIdentifier[16]; /* 18h */
    RAID_PHYS_DISK0_INQUIRY_DATA    InquiryData;        /* 28h */
    RAID_PHYS_DISK0_STATUS          PhysDiskStatus;     /* 64h */
    U32                             MaxLBA;             /* 68h */
    RAID_PHYS_DISK0_ERROR_DATA      ErrorData;          /* 6Ch */
} fCONFIG_PAGE_RAID_PHYS_DISK_0, MPI_POINTER PTR_CONFIG_PAGE_RAID_PHYS_DISK_0,
  RaidPhysDiskPage0_t, MPI_POINTER pRaidPhysDiskPage0_t;

#define MPI_RAIDPHYSDISKPAGE0_PAGEVERSION           (0x00)


/****************************************************************************
*   LAN Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_LAN_0
{
    ConfigPageHeader_t      Header;                     /* 00h */
    U16                     TxRxModes;                  /* 04h */
    U16                     Reserved;                   /* 06h */
    U32                     PacketPrePad;               /* 08h */
} fCONFIG_PAGE_LAN_0, MPI_POINTER PTR_CONFIG_PAGE_LAN_0,
  LANPage0_t, MPI_POINTER pLANPage0_t;

#define MPI_LAN_PAGE0_PAGEVERSION                       (0x01)

#define MPI_LAN_PAGE0_RETURN_LOOPBACK                   (0x0000)
#define MPI_LAN_PAGE0_SUPPRESS_LOOPBACK                 (0x0001)
#define MPI_LAN_PAGE0_LOOPBACK_MASK                     (0x0001)

typedef struct _CONFIG_PAGE_LAN_1
{
    ConfigPageHeader_t      Header;                     /* 00h */
    U16                     Reserved;                   /* 04h */
    U8                      CurrentDeviceState;         /* 06h */
    U8                      Reserved1;                  /* 07h */
    U32                     MinPacketSize;              /* 08h */
    U32                     MaxPacketSize;              /* 0Ch */
    U32                     HardwareAddressLow;         /* 10h */
    U32                     HardwareAddressHigh;        /* 14h */
    U32                     MaxWireSpeedLow;            /* 18h */
    U32                     MaxWireSpeedHigh;           /* 1Ch */
    U32                     BucketsRemaining;           /* 20h */
    U32                     MaxReplySize;               /* 24h */
    U32                     NegWireSpeedLow;            /* 28h */
    U32                     NegWireSpeedHigh;           /* 2Ch */
} fCONFIG_PAGE_LAN_1, MPI_POINTER PTR_CONFIG_PAGE_LAN_1,
  LANPage1_t, MPI_POINTER pLANPage1_t;

#define MPI_LAN_PAGE1_PAGEVERSION                       (0x03)

#define MPI_LAN_PAGE1_DEV_STATE_RESET                   (0x00)
#define MPI_LAN_PAGE1_DEV_STATE_OPERATIONAL             (0x01)

#endif

