/* $FreeBSD: src/sys/dev/mpt/mpilib/mpi_targ.h,v 1.1.2.2 2002/09/01 23:08:06 mjacob Exp $ */
/* $DragonFly: src/sys/dev/disk/mpt/mpilib/mpi_targ.h,v 1.2 2003/06/17 04:28:28 dillon Exp $ */
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
 *           Name:  MPI_TARG.H
 *          Title:  MPI Target mode messages and structures
 *  Creation Date:  June 22, 2000
 *
 *    MPI Version:  01.02.04
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-22-00  01.00.02  Added _MSG_TARGET_CMD_BUFFER_POST_REPLY structure.
 *                      Corrected DECSRIPTOR typo to DESCRIPTOR.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Modified target mode to use IoIndex instead of
 *                      HostIndex and IocIndex. Added Alias.
 *  01-09-01  01.01.02  Added defines for TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER
 *                      and TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER.
 *  02-20-01  01.01.03  Started using MPI_POINTER.
 *                      Added structures for MPI_TARGET_SCSI_SPI_CMD_BUFFER and
 *                      MPI_TARGET_FCP_CMD_BUFFER.
 *  03-27-01  01.01.04  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Added structure for MPI_TARGET_SCSI_SPI_STATUS_IU.
 *                      Added PriorityReason field to some replies and
 *                      defined more PriorityReason codes.
 *                      Added some defines for to support previous version
 *                      of MPI.
 *  10-04-01  01.02.03  Added PriorityReason to MSG_TARGET_ERROR_REPLY.
 *  11-01-01  01.02.04  Added define for TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TARG_H
#define MPI_TARG_H


/******************************************************************************
*
*        S C S I    T a r g e t    M e s s a g e s
*
*******************************************************************************/

typedef struct _CMD_BUFFER_DESCRIPTOR
{
    U16                     IoIndex;                    /* 00h */
    U16                     Reserved;                   /* 02h */
    union                                               /* 04h */
    {
        U32                 PhysicalAddress32;
        U64                 PhysicalAddress64;
    } _u;
} CMD_BUFFER_DESCRIPTOR, MPI_POINTER PTR_CMD_BUFFER_DESCRIPTOR,
  CmdBufferDescriptor_t, MPI_POINTER pCmdBufferDescriptor_t;


/****************************************************************************/
/* Target Command Buffer Post Request                                       */
/****************************************************************************/

typedef struct _MSG_TARGET_CMD_BUFFER_POST_REQUEST
{
    U8                      BufferPostFlags;            /* 00h */
    U8                      BufferCount;                /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      BufferLength;               /* 04h */
    U8                      Reserved;                   /* 05h */
    U8                      Reserved1;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    CMD_BUFFER_DESCRIPTOR   Buffer[1];                  /* 0Ch */
} MSG_TARGET_CMD_BUFFER_POST_REQUEST, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REQUEST,
  TargetCmdBufferPostRequest_t, MPI_POINTER pTargetCmdBufferPostRequest_t;

#define CMD_BUFFER_POST_FLAGS_PORT_MASK         (0x01)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_MASK    (0x80)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_32      (0)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_64      (1)
#define CMD_BUFFER_POST_FLAGS_64_BIT_ADDR       (0x80)

#define CMD_BUFFER_POST_IO_INDEX_MASK           (0x00003FFF)
#define CMD_BUFFER_POST_IO_INDEX_MASK_0100      (0x000003FF) /* obsolete */


typedef struct _MSG_TARGET_CMD_BUFFER_POST_REPLY
{
    U8                      BufferPostFlags;            /* 00h */
    U8                      BufferCount;                /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      BufferLength;               /* 04h */
    U8                      Reserved;                   /* 05h */
    U8                      Reserved1;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_TARGET_CMD_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REPLY,
  TargetCmdBufferPostReply_t, MPI_POINTER pTargetCmdBufferPostReply_t;

/* the following structure is obsolete as of MPI v1.2 */
typedef struct _MSG_PRIORITY_CMD_RECEIVED_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
} MSG_PRIORITY_CMD_RECEIVED_REPLY, MPI_POINTER PTR_MSG_PRIORITY_CMD_RECEIVED_REPLY,
  PriorityCommandReceivedReply_t, MPI_POINTER pPriorityCommandReceivedReply_t;

#define PRIORITY_REASON_NO_DISCONNECT           (0x00)
#define PRIORITY_REASON_SCSI_TASK_MANAGEMENT    (0x01)
#define PRIORITY_REASON_CMD_PARITY_ERR          (0x02)
#define PRIORITY_REASON_MSG_OUT_PARITY_ERR      (0x03)
#define PRIORITY_REASON_LQ_CRC_ERR              (0x04)
#define PRIORITY_REASON_CMD_CRC_ERR             (0x05)
#define PRIORITY_REASON_PROTOCOL_ERR            (0x06)
#define PRIORITY_REASON_DATA_OUT_PARITY_ERR     (0x07)
#define PRIORITY_REASON_DATA_OUT_CRC_ERR        (0x08)
#define PRIORITY_REASON_UNKNOWN                 (0xFF)


typedef struct _MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
} MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  TargetCmdBufferPostErrorReply_t, MPI_POINTER pTargetCmdBufferPostErrorReply_t;


typedef struct _MPI_TARGET_FCP_CMD_BUFFER
{
    U8      FcpLun[8];                                  /* 00h */
    U8      FcpCntl[4];                                 /* 08h */
    U8      FcpCdb[16];                                 /* 0Ch */
    U32     FcpDl;                                      /* 1Ch */
} MPI_TARGET_FCP_CMD_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_CMD_BUFFER,
  MpiTargetFcpCmdBuffer, MPI_POINTER pMpiTargetFcpCmdBuffer;


typedef struct _MPI_TARGET_SCSI_SPI_CMD_BUFFER
{
    /* SPI L_Q information unit */
    U8      L_QType;                                    /* 00h */
    U8      Reserved;                                   /* 01h */
    U16     Tag;                                        /* 02h */
    U8      LogicalUnitNumber[8];                       /* 04h */
    U32     DataLength;                                 /* 0Ch */
    /* SPI command information unit */
    U8      ReservedFirstByteOfCommandIU;               /* 10h */
    U8      TaskAttribute;                              /* 11h */
    U8      TaskManagementFlags;                        /* 12h */
    U8      AdditionalCDBLength;                        /* 13h */
    U8      CDB[16];                                    /* 14h */
} MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MpiTargetScsiSpiCmdBuffer, MPI_POINTER pMpiTargetScsiSpiCmdBuffer;


/****************************************************************************/
/* Target Assist Request                                                    */
/****************************************************************************/

typedef struct _MSG_TARGET_ASSIST_REQUEST
{
    U8                      StatusCode;                 /* 00h */
    U8                      TargetAssistFlags;          /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     QueueTag;                   /* 04h */
    U8                      Reserved;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U8                      LUN[8];                     /* 10h */
    U32                     RelativeOffset;             /* 18h */
    U32                     DataLength;                 /* 1Ch */
    SGE_IO_UNION            SGL[1];                     /* 20h */
} MSG_TARGET_ASSIST_REQUEST, MPI_POINTER PTR_MSG_TARGET_ASSIST_REQUEST,
  TargetAssistRequest_t, MPI_POINTER pTargetAssistRequest_t;

#define TARGET_ASSIST_FLAGS_DATA_DIRECTION          (0x01)
#define TARGET_ASSIST_FLAGS_AUTO_STATUS             (0x02)
#define TARGET_ASSIST_FLAGS_HIGH_PRIORITY           (0x04)
#define TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER       (0x80)


typedef struct _MSG_TARGET_ERROR_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
    U32                     TransferCount;              /* 18h */
} MSG_TARGET_ERROR_REPLY, MPI_POINTER PTR_MSG_TARGET_ERROR_REPLY,
  TargetErrorReply_t, MPI_POINTER pTargetErrorReply_t;


/****************************************************************************/
/* Target Status Send Request                                               */
/****************************************************************************/

typedef struct _MSG_TARGET_STATUS_SEND_REQUEST
{
    U8                      StatusCode;                 /* 00h */
    U8                      StatusFlags;                /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     QueueTag;                   /* 04h */
    U8                      Reserved;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U8                      LUN[8];                     /* 10h */
    SGE_SIMPLE_UNION        StatusDataSGE;              /* 18h */
} MSG_TARGET_STATUS_SEND_REQUEST, MPI_POINTER PTR_MSG_TARGET_STATUS_SEND_REQUEST,
  TargetStatusSendRequest_t, MPI_POINTER pTargetStatusSendRequest_t;

#define TARGET_STATUS_SEND_FLAGS_AUTO_GOOD_STATUS   (0x01)
#define TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY      (0x04)
#define TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER  (0x80)

typedef struct _MPI_TARGET_FCP_RSP_BUFFER
{
    U8      Reserved0[8];                               /* 00h */
    U8      FcpStatus;                                  /* 08h */
    U8      FcpFlags;                                   /* 09h */
    U8      Reserved1[2];                               /* 0Ah */
    U32     FcpResid;                                   /* 0Ch */
    U32     FcpSenseLength;                             /* 10h */
    U32     FcpResponseLength;                          /* 14h */
    U8      FcpResponseData[8];                         /* 18h */
    U8      FcpSenseData[32]; /* Pad to 64 bytes */     /* 20h */
} MPI_TARGET_FCP_RSP_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_RSP_BUFFER,
  MpiTargetFcpRspBuffer, MPI_POINTER pMpiTargetFcpRspBuffer;

typedef struct _MPI_TARGET_SCSI_SPI_STATUS_IU
{
    U8      Reserved0;                                  /* 00h */
    U8      Reserved1;                                  /* 01h */
    U8      Valid;                                      /* 02h */
    U8      Status;                                     /* 03h */
    U32     SenseDataListLength;                        /* 04h */
    U32     PktFailuresListLength;                      /* 08h */
    U8      SenseData[52]; /* Pad the IU to 64 bytes */ /* 0Ch */
} MPI_TARGET_SCSI_SPI_STATUS_IU, MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_STATUS_IU,
  TargetScsiSpiStatusIU_t, MPI_POINTER pTargetScsiSpiStatusIU_t;

/****************************************************************************/
/* Target Mode Abort Request                                                */
/****************************************************************************/

typedef struct _MSG_TARGET_MODE_ABORT_REQUEST
{
    U8                      AbortType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U32                     MsgContextToAbort;          /* 10h */
} MSG_TARGET_MODE_ABORT, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT,
  TargetModeAbort_t, MPI_POINTER pTargetModeAbort_t;

#define TARGET_MODE_ABORT_TYPE_ALL_CMD_BUFFERS      (0x00)
#define TARGET_MODE_ABORT_TYPE_ALL_IO               (0x01)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO             (0x02)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO_REQUEST     (0x03)

/* Target Mode Abort Reply */

typedef struct _MSG_TARGET_MODE_ABORT_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved3;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     AbortCount;                 /* 14h */
} MSG_TARGET_MODE_ABORT_REPLY, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT_REPLY,
  TargetModeAbortReply_t, MPI_POINTER pTargetModeAbortReply_t;


/****************************************************************************/
/* Target Mode Context Reply                                                */
/****************************************************************************/

#define TARGET_MODE_REPLY_IO_INDEX_MASK         (0x00003FFF)
#define TARGET_MODE_REPLY_IO_INDEX_SHIFT        (0)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_MASK  (0x03FFC000)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT (14)
#define TARGET_MODE_REPLY_ALIAS_MASK            (0x0C000000)
#define TARGET_MODE_REPLY_ALIAS_SHIFT           (26)
#define TARGET_MODE_REPLY_PORT_MASK             (0x10000000)
#define TARGET_MODE_REPLY_PORT_SHIFT            (28)


#define GET_IO_INDEX(x)     (((x) & TARGET_MODE_REPLY_IO_INDEX_MASK)           \
                                    >> TARGET_MODE_REPLY_IO_INDEX_SHIFT)

#define SET_IO_INDEX(t, i)                                                     \
            ((t) = ((t) & ~TARGET_MODE_REPLY_IO_INDEX_MASK) |                  \
                              (((i) << TARGET_MODE_REPLY_IO_INDEX_SHIFT) &     \
                                             TARGET_MODE_REPLY_IO_INDEX_MASK))

#define GET_INITIATOR_INDEX(x) (((x) & TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) \
                                   >> TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT)

#define SET_INITIATOR_INDEX(t, ii)                                             \
        ((t) = ((t) & ~TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) |               \
                        (((ii) << TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT) &   \
                                      TARGET_MODE_REPLY_INITIATOR_INDEX_MASK))

#define GET_ALIAS(x) (((x) & TARGET_MODE_REPLY_ALIAS_MASK)                     \
                                               >> TARGET_MODE_REPLY_ALIAS_SHIFT)

#define SET_ALIAS(t, a)  ((t) = ((t) & ~TARGET_MODE_REPLY_ALIAS_MASK) |        \
                                    (((a) << TARGET_MODE_REPLY_ALIAS_SHIFT) &  \
                                                 TARGET_MODE_REPLY_ALIAS_MASK))

#define GET_PORT(x) (((x) & TARGET_MODE_REPLY_PORT_MASK)                       \
                                               >> TARGET_MODE_REPLY_PORT_SHIFT)

#define SET_PORT(t, p)  ((t) = ((t) & ~TARGET_MODE_REPLY_PORT_MASK) |          \
                                    (((p) << TARGET_MODE_REPLY_PORT_SHIFT) &   \
                                                  TARGET_MODE_REPLY_PORT_MASK))

/* the following obsolete values are for MPI v1.0 support */
#define TARGET_MODE_REPLY_0100_MASK_HOST_INDEX       (0x000003FF)
#define TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX      (0)
#define TARGET_MODE_REPLY_0100_MASK_IOC_INDEX        (0x001FF800)
#define TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX       (11)
#define TARGET_MODE_REPLY_0100_PORT_MASK             (0x00400000)
#define TARGET_MODE_REPLY_0100_PORT_SHIFT            (22)
#define TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX  (0x1F800000)
#define TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX (23)

#define GET_HOST_INDEX_0100(x) (((x) & TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX)

#define SET_HOST_INDEX_0100(t, hi)                                             \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) |           \
                         (((hi) << TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX) &  \
                                      TARGET_MODE_REPLY_0100_MASK_HOST_INDEX))

#define GET_IOC_INDEX_0100(x)   (((x) & TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX)

#define SET_IOC_INDEX_0100(t, ii)                                              \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) |            \
                        (((ii) << TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX) &    \
                                     TARGET_MODE_REPLY_0100_MASK_IOC_INDEX))

#define GET_INITIATOR_INDEX_0100(x)                                            \
            (((x) & TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX)               \
                              >> TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX)

#define SET_INITIATOR_INDEX_0100(t, ii)                                        \
        ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX) |          \
                   (((ii) << TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX) &   \
                                TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX))


#endif

