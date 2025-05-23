/*
 * Data structures and definitions for CAM Control Blocks (CCBs).
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 * $FreeBSD: src/sys/cam/cam_ccb.h,v 1.15.2.3 2003/07/29 04:00:34 njl Exp $
 * $DragonFly: src/sys/bus/cam/cam_ccb.h,v 1.8 2004/09/17 03:39:38 joerg Exp $
 */

#ifndef _CAM_CAM_CCB_H
#define _CAM_CAM_CCB_H 1

#include <sys/queue.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/callout.h>
#include "cam_debug.h"
#include "scsi/scsi_all.h"


/* General allocation length definitions for CCB structures */
#define	IOCDBLEN	CAM_MAX_CDBLEN	/* Space for CDB bytes/pointer */
#define	VUHBALEN	14		/* Vendor Unique HBA length */
#define	SIM_IDLEN	16		/* ASCII string len for SIM ID */
#define	HBA_IDLEN	16		/* ASCII string len for HBA ID */
#define	DEV_IDLEN	16		/* ASCII string len for device names */
#define CCB_PERIPH_PRIV_SIZE 	2	/* size of peripheral private area */
#define CCB_SIM_PRIV_SIZE 	2	/* size of sim private area */

/* Struct definitions for CAM control blocks */

/* Common CCB header */
/* CAM CCB flags */
typedef enum {
	CAM_CDB_POINTER		= 0x00000001,/* The CDB field is a pointer    */
	CAM_QUEUE_ENABLE	= 0x00000002,/* SIM queue actions are enabled */
	CAM_CDB_LINKED		= 0x00000004,/* CCB contains a linked CDB     */
	CAM_NEGOTIATE		= 0x00000008,/*
					      * Perform transport negotiation
					      * with this command.
					      */
	CAM_SCATTER_VALID	= 0x00000010,/* Scatter/gather list is valid  */
	CAM_DIS_AUTOSENSE	= 0x00000020,/* Disable autosense feature     */
	CAM_DIR_RESV		= 0x00000000,/* Data direction (00:reserved)  */
	CAM_DIR_IN		= 0x00000040,/* Data direction (01:DATA IN)   */
	CAM_DIR_OUT		= 0x00000080,/* Data direction (10:DATA OUT)  */
	CAM_DIR_NONE		= 0x000000C0,/* Data direction (11:no data)   */
	CAM_DIR_MASK		= 0x000000C0,/* Data direction Mask	      */
	CAM_SOFT_RST_OP		= 0x00000100,/* Use Soft reset alternative    */
	CAM_ENG_SYNC		= 0x00000200,/* Flush resid bytes on complete */
	CAM_DEV_QFRZDIS		= 0x00000400,/* Disable DEV Q freezing	      */
	CAM_DEV_QFREEZE		= 0x00000800,/* Freeze DEV Q on execution     */
	CAM_HIGH_POWER		= 0x00001000,/* Command takes a lot of power  */
	CAM_SENSE_PTR		= 0x00002000,/* Sense data is a pointer	      */
	CAM_SENSE_PHYS		= 0x00004000,/* Sense pointer is physical addr*/
	CAM_TAG_ACTION_VALID	= 0x00008000,/* Use the tag action in this ccb*/
	CAM_PASS_ERR_RECOVER	= 0x00010000,/* Pass driver does err. recovery*/
	CAM_DIS_DISCONNECT	= 0x00020000,/* Disable disconnect	      */
	CAM_SG_LIST_PHYS	= 0x00040000,/* SG list has physical addrs.   */
	CAM_MSG_BUF_PHYS	= 0x00080000,/* Message buffer ptr is physical*/
	CAM_SNS_BUF_PHYS	= 0x00100000,/* Autosense data ptr is physical*/
	CAM_DATA_PHYS		= 0x00200000,/* SG/Buffer data ptrs are phys. */
	CAM_CDB_PHYS		= 0x00400000,/* CDB poiner is physical	      */
	CAM_ENG_SGLIST		= 0x00800000,/* SG list is for the HBA engine */

/* Phase cognizant mode flags */
	CAM_DIS_AUTOSRP		= 0x01000000,/* Disable autosave/restore ptrs */
	CAM_DIS_AUTODISC	= 0x02000000,/* Disable auto disconnect	      */
	CAM_TGT_CCB_AVAIL	= 0x04000000,/* Target CCB available	      */
	CAM_TGT_PHASE_MODE	= 0x08000000,/* The SIM runs in phase mode    */
	CAM_MSGB_VALID		= 0x10000000,/* Message buffer valid	      */
	CAM_STATUS_VALID	= 0x20000000,/* Status buffer valid	      */
	CAM_DATAB_VALID		= 0x40000000,/* Data buffer valid	      */
	
/* Host target Mode flags */
	CAM_SEND_SENSE		= 0x08000000,/* Send sense data with status   */
	CAM_TERM_IO		= 0x10000000,/* Terminate I/O Message sup.    */
	CAM_DISCONNECT		= 0x20000000,/* Disconnects are mandatory     */
	CAM_SEND_STATUS		= 0x40000000 /* Send status after data phase  */
} ccb_flags;

/* XPT Opcodes for xpt_action */
typedef enum {
/* Function code flags are bits greater than 0xff */
	XPT_FC_QUEUED		= 0x100,
				/* Non-immediate function code */
	XPT_FC_USER_CCB		= 0x200,
	XPT_FC_XPT_ONLY		= 0x400,
				/* Only for the transport layer device */
	XPT_FC_DEV_QUEUED	= 0x800 | XPT_FC_QUEUED,
				/* Passes through the device queues */
/* Common function commands: 0x00->0x0F */
	XPT_NOOP 		= 0x00,
				/* Execute Nothing */
	XPT_SCSI_IO		= 0x01 | XPT_FC_DEV_QUEUED,
				/* Execute the requested I/O operation */
	XPT_GDEV_TYPE		= 0x02,
				/* Get type information for specified device */
	XPT_GDEVLIST		= 0x03,
				/* Get a list of peripheral devices */
	XPT_PATH_INQ		= 0x04,
				/* Path routing inquiry */
	XPT_REL_SIMQ		= 0x05,
				/* Release a frozen SIM queue */
	XPT_SASYNC_CB		= 0x06,
				/* Set Asynchronous Callback Parameters */
	XPT_SDEV_TYPE		= 0x07,
				/* Set device type information */
	XPT_SCAN_BUS		= 0x08 | XPT_FC_QUEUED | XPT_FC_USER_CCB
				       | XPT_FC_XPT_ONLY,
				/* (Re)Scan the SCSI Bus */
	XPT_DEV_MATCH		= 0x09 | XPT_FC_XPT_ONLY,
				/* Get EDT entries matching the given pattern */
	XPT_DEBUG		= 0x0a,
				/* Turn on debugging for a bus, target or lun */
	XPT_PATH_STATS		= 0x0b,
				/* Path statistics (error counts, etc.) */
	XPT_GDEV_STATS		= 0x0c,
				/* Device statistics (error counts, etc.) */
/* SCSI Control Functions: 0x10->0x1F */
	XPT_ABORT		= 0x10,
				/* Abort the specified CCB */
	XPT_RESET_BUS		= 0x11 | XPT_FC_XPT_ONLY,
				/* Reset the specified SCSI bus */
	XPT_RESET_DEV		= 0x12 | XPT_FC_DEV_QUEUED,
				/* Bus Device Reset the specified SCSI device */
	XPT_TERM_IO		= 0x13,
				/* Terminate the I/O process */
	XPT_SCAN_LUN		= 0x14 | XPT_FC_QUEUED | XPT_FC_USER_CCB
				       | XPT_FC_XPT_ONLY,
				/* Scan Logical Unit */
	XPT_GET_TRAN_SETTINGS	= 0x15,
				/*
				 * Get default/user transfer settings
				 * for the target
				 */
	XPT_SET_TRAN_SETTINGS	= 0x16,
				/*
				 * Set transfer rate/width
				 * negotiation settings
				 */
	XPT_CALC_GEOMETRY	= 0x17,
				/*
				 * Calculate the geometry parameters for
				 * a device give the sector size and
				 * volume size.
				 */

/* HBA engine commands 0x20->0x2F */
	XPT_ENG_INQ		= 0x20 | XPT_FC_XPT_ONLY,
				/* HBA engine feature inquiry */
	XPT_ENG_EXEC		= 0x21 | XPT_FC_DEV_QUEUED | XPT_FC_XPT_ONLY,
				/* HBA execute engine request */

/* Target mode commands: 0x30->0x3F */
	XPT_EN_LUN		= 0x30,
				/* Enable LUN as a target */
	XPT_TARGET_IO		= 0x31 | XPT_FC_DEV_QUEUED,
				/* Execute target I/O request */
	XPT_ACCEPT_TARGET_IO	= 0x32 | XPT_FC_QUEUED | XPT_FC_USER_CCB,
				/* Accept Host Target Mode CDB */
	XPT_CONT_TARGET_IO	= 0x33 | XPT_FC_DEV_QUEUED,
				/* Continue Host Target I/O Connection */
	XPT_IMMED_NOTIFY	= 0x34 | XPT_FC_QUEUED | XPT_FC_USER_CCB,
				/* Notify Host Target driver of event */
	XPT_NOTIFY_ACK		= 0x35,
				/* Acknowledgement of event */

/* Vendor Unique codes: 0x80->0x8F */
	XPT_VUNIQUE		= 0x80
} xpt_opcode;

#define XPT_FC_GROUP_MASK		0xF0
#define XPT_FC_GROUP(op) ((op) & XPT_FC_GROUP_MASK)
#define XPT_FC_GROUP_COMMON		0x00
#define XPT_FC_GROUP_SCSI_CONTROL	0x10
#define XPT_FC_GROUP_HBA_ENGINE		0x20
#define XPT_FC_GROUP_TMODE		0x30
#define XPT_FC_GROUP_VENDOR_UNIQUE	0x80

#define XPT_FC_IS_DEV_QUEUED(ccb) 	\
    (((ccb)->ccb_h.func_code & XPT_FC_DEV_QUEUED) == XPT_FC_DEV_QUEUED)
#define XPT_FC_IS_QUEUED(ccb) 	\
    (((ccb)->ccb_h.func_code & XPT_FC_QUEUED) != 0)
typedef union {
	LIST_ENTRY(ccb_hdr) le;
	SLIST_ENTRY(ccb_hdr) sle;
	TAILQ_ENTRY(ccb_hdr) tqe;
	STAILQ_ENTRY(ccb_hdr) stqe;
} camq_entry;

typedef union {
	void		*ptr;
	u_long		field;
	u_int8_t	bytes[sizeof(void *) > sizeof(u_long)
			      ? sizeof(void *) : sizeof(u_long)];
} ccb_priv_entry;

typedef union {
	ccb_priv_entry	entries[CCB_PERIPH_PRIV_SIZE];
	u_int8_t	bytes[CCB_PERIPH_PRIV_SIZE * sizeof(ccb_priv_entry)];
} ccb_ppriv_area;

typedef union {
	ccb_priv_entry	entries[CCB_SIM_PRIV_SIZE];
	u_int8_t	bytes[CCB_SIM_PRIV_SIZE * sizeof(ccb_priv_entry)];
} ccb_spriv_area;

struct ccb_hdr {
	cam_pinfo	pinfo;	/* Info for priority scheduling */
	camq_entry	xpt_links;	/* For chaining in the XPT layer */	
	camq_entry	sim_links;	/* For chaining in the SIM layer */	
	camq_entry	periph_links;/* For chaining in the type driver */
	u_int32_t	retry_count;
	                        /* Callback on completion function */
	void		(*cbfcnp)(struct cam_periph *, union ccb *);
	xpt_opcode	func_code;	/* XPT function code */
	u_int32_t	status;	/* Status returned by CAM subsystem */
				/* Compiled path for this ccb */
	struct		cam_path *path;
	path_id_t	path_id;	/* Path ID for the request */
	target_id_t	target_id;	/* Target device ID */
	lun_id_t	target_lun;	/* Target LUN number */
	u_int32_t	flags;
	ccb_ppriv_area	periph_priv;
	ccb_spriv_area	sim_priv;
	u_int32_t	timeout;	/* Timeout value */
					/* Callout handle used for timeouts */
	struct		callout timeout_ch;
};

/* Get Device Information CCB */
struct ccb_getdev {
	struct	  ccb_hdr ccb_h;
	struct scsi_inquiry_data inq_data;
	u_int8_t  serial_num[252];
	u_int8_t  inq_len;
	u_int8_t  serial_num_len;
};

/* Device Statistics CCB */
struct ccb_getdevstats {
	struct	ccb_hdr	ccb_h;
	int	dev_openings;	/* Space left for more work on device*/	
	int	dev_active;	/* Transactions running on the device */
	int	devq_openings;	/* Space left for more queued work */
	int	devq_queued;	/* Transactions queued to be sent */
	int	held;		/*
				 * CCBs held by peripheral drivers
				 * for this device
				 */
	int	maxtags;	/*
				 * Boundary conditions for number of
				 * tagged operations
				 */
	int	mintags;
	struct	timeval last_reset;  /* Uptime of last bus reset/loop init */
};

typedef enum {
	CAM_GDEVLIST_LAST_DEVICE,
	CAM_GDEVLIST_LIST_CHANGED,
	CAM_GDEVLIST_MORE_DEVS,
	CAM_GDEVLIST_ERROR
} ccb_getdevlist_status_e;

struct ccb_getdevlist {
	struct ccb_hdr		ccb_h;
	char 			periph_name[DEV_IDLEN];
	u_int32_t		unit_number;
	unsigned int		generation;
	u_int32_t		index;
	ccb_getdevlist_status_e	status;
};

typedef enum {
	PERIPH_MATCH_NONE	= 0x000,
	PERIPH_MATCH_PATH	= 0x001,
	PERIPH_MATCH_TARGET	= 0x002,
	PERIPH_MATCH_LUN	= 0x004,
	PERIPH_MATCH_NAME	= 0x008,
	PERIPH_MATCH_UNIT	= 0x010,
	PERIPH_MATCH_ANY	= 0x01f
} periph_pattern_flags;

struct periph_match_pattern {
	char			periph_name[DEV_IDLEN];
	u_int32_t		unit_number;
	path_id_t		path_id;
	target_id_t		target_id;
	lun_id_t		target_lun;
	periph_pattern_flags	flags;
};

typedef enum {
	DEV_MATCH_NONE		= 0x000,
	DEV_MATCH_PATH		= 0x001,
	DEV_MATCH_TARGET	= 0x002,
	DEV_MATCH_LUN		= 0x004,
	DEV_MATCH_INQUIRY	= 0x008,
	DEV_MATCH_ANY		= 0x00f
} dev_pattern_flags;

struct device_match_pattern {
	path_id_t				path_id;
	target_id_t				target_id;
	lun_id_t				target_lun;
	struct scsi_static_inquiry_pattern	inq_pat;
	dev_pattern_flags			flags;
};

typedef enum {
	BUS_MATCH_NONE		= 0x000,
	BUS_MATCH_PATH		= 0x001,
	BUS_MATCH_NAME		= 0x002,
	BUS_MATCH_UNIT		= 0x004,
	BUS_MATCH_BUS_ID	= 0x008,
	BUS_MATCH_ANY		= 0x00f
} bus_pattern_flags;

struct bus_match_pattern {
	path_id_t		path_id;
	char			dev_name[DEV_IDLEN];
	u_int32_t		unit_number;
	u_int32_t		bus_id;
	bus_pattern_flags	flags;
};

union match_pattern {
	struct periph_match_pattern	periph_pattern;
	struct device_match_pattern	device_pattern;
	struct bus_match_pattern	bus_pattern;
};

typedef enum {
	DEV_MATCH_PERIPH,
	DEV_MATCH_DEVICE,
	DEV_MATCH_BUS
} dev_match_type;

struct dev_match_pattern {
	dev_match_type		type;
	union match_pattern	pattern;
};

struct periph_match_result {
	char			periph_name[DEV_IDLEN];
	u_int32_t		unit_number;
	path_id_t		path_id;
	target_id_t		target_id;
	lun_id_t		target_lun;
};

typedef enum {
	DEV_RESULT_NOFLAG		= 0x00,
	DEV_RESULT_UNCONFIGURED		= 0x01
} dev_result_flags;

struct device_match_result {
	path_id_t			path_id;
	target_id_t			target_id;
	lun_id_t			target_lun;
	struct scsi_inquiry_data	inq_data;
	dev_result_flags		flags;
};

struct bus_match_result {
	path_id_t	path_id;
	char		dev_name[DEV_IDLEN];
	u_int32_t	unit_number;
	u_int32_t	bus_id;
};

union match_result {
	struct periph_match_result	periph_result;
	struct device_match_result	device_result;
	struct bus_match_result		bus_result;
};

struct dev_match_result {
	dev_match_type		type;
	union match_result	result;
};

typedef enum {
	CAM_DEV_MATCH_LAST,
	CAM_DEV_MATCH_MORE,
	CAM_DEV_MATCH_LIST_CHANGED,
	CAM_DEV_MATCH_SIZE_ERROR,
	CAM_DEV_MATCH_ERROR
} ccb_dev_match_status;

typedef enum {
	CAM_DEV_POS_NONE	= 0x000,
	CAM_DEV_POS_BUS		= 0x001,
	CAM_DEV_POS_TARGET	= 0x002,
	CAM_DEV_POS_DEVICE	= 0x004,
	CAM_DEV_POS_PERIPH	= 0x008,
	CAM_DEV_POS_PDPTR	= 0x010,
	CAM_DEV_POS_TYPEMASK	= 0xf00,
	CAM_DEV_POS_EDT		= 0x100,
	CAM_DEV_POS_PDRV	= 0x200
} dev_pos_type;

struct ccb_dm_cookie {
	void 	*bus;
	void	*target;	
	void	*device;
	void	*periph;
	void	*pdrv;
};

struct ccb_dev_position {
	u_int			generations[4];
#define	CAM_BUS_GENERATION	0x00
#define CAM_TARGET_GENERATION	0x01
#define CAM_DEV_GENERATION	0x02
#define CAM_PERIPH_GENERATION	0x03
	dev_pos_type		position_type;
	struct ccb_dm_cookie	cookie;
};

struct ccb_dev_match {
	struct ccb_hdr			ccb_h;
	ccb_dev_match_status		status;
	u_int32_t			num_patterns;
	u_int32_t			pattern_buf_len;
	struct dev_match_pattern	*patterns;
	u_int32_t			num_matches;
	u_int32_t			match_buf_len;
	struct dev_match_result		*matches;
	struct ccb_dev_position		pos;
};

/*
 * Definitions for the path inquiry CCB fields.
 */
#define CAM_VERSION	0x14	/* Hex value for current version */

typedef enum {
	PI_MDP_ABLE	= 0x80,	/* Supports MDP message */
	PI_WIDE_32	= 0x40,	/* Supports 32 bit wide SCSI */
	PI_WIDE_16	= 0x20, /* Supports 16 bit wide SCSI */
	PI_SDTR_ABLE	= 0x10,	/* Supports SDTR message */
	PI_LINKED_CDB	= 0x08, /* Supports linked CDBs */
	PI_TAG_ABLE	= 0x02,	/* Supports tag queue messages */
	PI_SOFT_RST	= 0x01	/* Supports soft reset alternative */
} pi_inqflag;

typedef enum {
	PIT_PROCESSOR	= 0x80,	/* Target mode processor mode */
	PIT_PHASE	= 0x40,	/* Target mode phase cog. mode */
	PIT_DISCONNECT	= 0x20,	/* Disconnects supported in target mode */
	PIT_TERM_IO	= 0x10,	/* Terminate I/O message supported in TM */
	PIT_GRP_6	= 0x08,	/* Group 6 commands supported */
	PIT_GRP_7	= 0x04	/* Group 7 commands supported */
} pi_tmflag;

typedef enum {
	PIM_SCANHILO	= 0x80,	/* Bus scans from high ID to low ID */
	PIM_NOREMOVE	= 0x40,	/* Removeable devices not included in scan */
	PIM_NOINITIATOR	= 0x20,	/* Initiator role not supported. */
	PIM_NOBUSRESET  = 0x10, /* User has disabled initial BUS RESET */
	PIM_NO_6_BYTE   = 0x08  /* Do not send 6-byte commands */
} pi_miscflag;

/* Path Inquiry CCB */
struct ccb_pathinq {
	struct 	    ccb_hdr ccb_h;
	u_int8_t    version_num;	/* Version number for the SIM/HBA */
	u_int8_t    hba_inquiry;	/* Mimic of INQ byte 7 for the HBA */
	u_int8_t    target_sprt;	/* Flags for target mode support */
	u_int8_t    hba_misc;		/* Misc HBA features */
	u_int16_t   hba_eng_cnt;	/* HBA engine count */
					/* Vendor Unique capabilities */
	u_int8_t    vuhba_flags[VUHBALEN];
	u_int32_t   max_target;		/* Maximum supported Target */
	u_int32_t   max_lun;		/* Maximum supported Lun */
	u_int32_t   async_flags;	/* Installed Async handlers */
	path_id_t   hpath_id;		/* Highest Path ID in the subsystem */
	target_id_t initiator_id;	/* ID of the HBA on the SCSI bus */
	char	    sim_vid[SIM_IDLEN];	/* Vendor ID of the SIM */
	char	    hba_vid[HBA_IDLEN];	/* Vendor ID of the HBA */
	char 	    dev_name[DEV_IDLEN];/* Device name for SIM */
	u_int32_t   unit_number;	/* Unit number for SIM */
	u_int32_t   bus_id;		/* Bus ID for SIM */
	u_int32_t   base_transfer_speed;/* Base bus speed in KB/sec */
};

/* Path Statistics CCB */
struct ccb_pathstats {
	struct	ccb_hdr	ccb_h;
	struct	timeval last_reset;	/* Uptime of last bus reset/loop init */
};

typedef union {
	u_int8_t *sense_ptr;		/*
					 * Pointer to storage
					 * for sense information
					 */
	                                /* Storage Area for sense information */
	struct	 scsi_sense_data sense_buf;
} sense_t;

typedef union {
	u_int8_t  *cdb_ptr;		/* Pointer to the CDB bytes to send */
					/* Area for the CDB send */
	u_int8_t  cdb_bytes[IOCDBLEN];
} cdb_t;

/*
 * SCSI I/O Request CCB used for the XPT_SCSI_IO and XPT_CONT_TARGET_IO
 * function codes.
 */
struct ccb_scsiio {
	struct	   ccb_hdr ccb_h;
	union	   ccb *next_ccb;	/* Ptr for next CCB for action */
	u_int8_t   *req_map;		/* Ptr to mapping info */
	u_int8_t   *data_ptr;		/* Ptr to the data buf/SG list */
	u_int32_t  dxfer_len;		/* Data transfer length */
					/* Autosense storage */	
	struct     scsi_sense_data sense_data;
	u_int8_t   sense_len;		/* Number of bytes to autosense */
	u_int8_t   cdb_len;		/* Number of bytes for the CDB */
	u_int16_t  sglist_cnt;		/* Number of SG list entries */
	u_int8_t   scsi_status;		/* Returned SCSI status */
	u_int8_t   sense_resid;		/* Autosense resid length: 2's comp */
	u_int32_t  resid;		/* Transfer residual length: 2's comp */
	cdb_t	   cdb_io;		/* Union for CDB bytes/pointer */
	u_int8_t   *msg_ptr;		/* Pointer to the message buffer */
	u_int16_t  msg_len;		/* Number of bytes for the Message */
	u_int8_t   tag_action;		/* What to do for tag queueing */
	/*
	 * The tag action should be either the define below (to send a
	 * non-tagged transaction) or one of the defined scsi tag messages
	 * from scsi_message.h.
	 */
#define		CAM_TAG_ACTION_NONE	0x00
	u_int	   tag_id;		/* tag id from initator (target mode) */
	u_int	   init_id;		/* initiator id of who selected */
};

struct ccb_accept_tio {
	struct	   ccb_hdr ccb_h;
	cdb_t	   cdb_io;		/* Union for CDB bytes/pointer */
	u_int8_t   cdb_len;		/* Number of bytes for the CDB */
	u_int8_t   tag_action;		/* What to do for tag queueing */
	u_int8_t   sense_len;		/* Number of bytes of Sense Data */
	u_int      tag_id;		/* tag id from initator (target mode) */
	u_int      init_id;		/* initiator id of who selected */
	struct     scsi_sense_data sense_data;
};

/* Release SIM Queue */
struct ccb_relsim {
	struct ccb_hdr ccb_h;
	u_int32_t      release_flags;
#define RELSIM_ADJUST_OPENINGS		0x01
#define RELSIM_RELEASE_AFTER_TIMEOUT	0x02
#define RELSIM_RELEASE_AFTER_CMDCMPLT	0x04
#define RELSIM_RELEASE_AFTER_QEMPTY	0x08
	u_int32_t      openings;
	u_int32_t      release_timeout;
	u_int32_t      qfrozen_cnt;
};

/*
 * Definitions for the asynchronous callback CCB fields.
 */
typedef enum {
	AC_GETDEV_CHANGED	= 0x800,/* Getdev info might have changed */
	AC_INQ_CHANGED		= 0x400,/* Inquiry info might have changed */
	AC_TRANSFER_NEG		= 0x200,/* New transfer settings in effect */
	AC_LOST_DEVICE		= 0x100,/* A device went away */
	AC_FOUND_DEVICE		= 0x080,/* A new device was found */
	AC_PATH_DEREGISTERED	= 0x040,/* A path has de-registered */
	AC_PATH_REGISTERED	= 0x020,/* A new path has been registered */
	AC_SENT_BDR		= 0x010,/* A BDR message was sent to target */
	AC_SCSI_AEN		= 0x008,/* A SCSI AEN has been received */
	AC_UNSOL_RESEL		= 0x002,/* Unsolicited reselection occurred */
	AC_BUS_RESET		= 0x001	/* A SCSI bus reset occurred */
} ac_code;

typedef void ac_callback_t (void *softc, u_int32_t code,
			    struct cam_path *path, void *args);

/* Set Asynchronous Callback CCB */
struct ccb_setasync {
	struct ccb_hdr	 ccb_h;
	u_int32_t	 event_enable;	/* Async Event enables */	
	ac_callback_t	*callback;
	void		*callback_arg;
};

/* Set Device Type CCB */
struct ccb_setdev {
	struct	   ccb_hdr ccb_h;
	u_int8_t   dev_type;	/* Value for dev type field in EDT */
};

/* SCSI Control Functions */

/* Abort XPT request CCB */
struct ccb_abort {
	struct 	ccb_hdr ccb_h;
	union	ccb *abort_ccb;	/* Pointer to CCB to abort */
};

/* Reset SCSI Bus CCB */
struct ccb_resetbus {
	struct	ccb_hdr ccb_h;
};

/* Reset SCSI Device CCB */
struct ccb_resetdev {
	struct	ccb_hdr ccb_h;
};

/* Terminate I/O Process Request CCB */
struct ccb_termio {
	struct	ccb_hdr ccb_h;
	union	ccb *termio_ccb;	/* Pointer to CCB to terminate */
};

/* Get/Set transfer rate/width/disconnection/tag queueing settings */
struct ccb_trans_settings {
	struct	ccb_hdr ccb_h;
	u_int	valid;	/* Which fields to honor */
#define	CCB_TRANS_SYNC_RATE_VALID	0x01
#define	CCB_TRANS_SYNC_OFFSET_VALID	0x02
#define	CCB_TRANS_BUS_WIDTH_VALID	0x04
#define	CCB_TRANS_DISC_VALID		0x08
#define	CCB_TRANS_TQ_VALID		0x10
	u_int	flags;
#define	CCB_TRANS_CURRENT_SETTINGS	0x01
#define	CCB_TRANS_USER_SETTINGS		0x02
#define	CCB_TRANS_DISC_ENB		0x04
#define	CCB_TRANS_TAG_ENB		0x08
	u_int	sync_period;
	u_int	sync_offset;
	u_int	bus_width;
};

/*
 * Calculate the geometry parameters for a device
 * give the block size and volume size in blocks.
 */
struct ccb_calc_geometry {
	struct	  ccb_hdr ccb_h;
	u_int32_t block_size;
	u_int32_t volume_size;
	u_int16_t cylinders;		
	u_int8_t  heads;
	u_int8_t  secs_per_track;
};

/*
 * Rescan the given bus, or bus/target/lun
 */
struct ccb_rescan {
	struct	ccb_hdr ccb_h;
	cam_flags	flags;
};

/*
 * Turn on debugging for the given bus, bus/target, or bus/target/lun.
 */
struct ccb_debug {
	struct	ccb_hdr ccb_h;
	cam_debug_flags flags;
};

/* Target mode structures. */

struct ccb_en_lun {
	struct	  ccb_hdr ccb_h;
	u_int16_t grp6_len;		/* Group 6 VU CDB length */
	u_int16_t grp7_len;		/* Group 7 VU CDB length */
	u_int8_t  enable;
};

struct ccb_immed_notify {
	struct	  ccb_hdr ccb_h;
	struct    scsi_sense_data sense_data;
	u_int8_t  sense_len;		/* Number of bytes in sense buffer */
	u_int8_t  initiator_id;		/* Id of initiator that selected */
	u_int8_t  message_args[7];	/* Message Arguments */
};

struct ccb_notify_ack {
	struct	  ccb_hdr ccb_h;
	u_int16_t seq_id;		/* Sequence identifier */
	u_int8_t  event;		/* Event flags */
};

/* HBA engine structures. */

typedef enum {
	EIT_BUFFER,	/* Engine type: buffer memory */
	EIT_LOSSLESS,	/* Engine type: lossless compression */
	EIT_LOSSY,	/* Engine type: lossy compression */
	EIT_ENCRYPT	/* Engine type: encryption */
} ei_type;

typedef enum {
	EAD_VUNIQUE,	/* Engine algorithm ID: vendor unique */
	EAD_LZ1V1,	/* Engine algorithm ID: LZ1 var.1 */
	EAD_LZ2V1,	/* Engine algorithm ID: LZ2 var.1 */
	EAD_LZ2V2	/* Engine algorithm ID: LZ2 var.2 */
} ei_algo;

struct ccb_eng_inq {
	struct	  ccb_hdr ccb_h;
	u_int16_t eng_num;	/* The engine number for this inquiry */
	ei_type   eng_type;	/* Returned engine type */
	ei_algo   eng_algo;	/* Returned engine algorithm type */
	u_int32_t eng_memeory;	/* Returned engine memory size */
};

struct ccb_eng_exec {	/* This structure must match SCSIIO size */
	struct	  ccb_hdr ccb_h;
	u_int8_t  *pdrv_ptr;	/* Ptr used by the peripheral driver */
	u_int8_t  *req_map;	/* Ptr for mapping info on the req. */
	u_int8_t  *data_ptr;	/* Pointer to the data buf/SG list */
	u_int32_t dxfer_len;	/* Data transfer length */
	u_int8_t  *engdata_ptr;	/* Pointer to the engine buffer data */
	u_int16_t sglist_cnt;	/* Num of scatter gather list entries */
	u_int32_t dmax_len;	/* Destination data maximum length */
	u_int32_t dest_len;	/* Destination data length */
	int32_t	  src_resid;	/* Source residual length: 2's comp */
	u_int32_t timeout;	/* Timeout value */
	u_int16_t eng_num;	/* Engine number for this request */
	u_int16_t vu_flags;	/* Vendor Unique flags */
};

/*
 * Definitions for the timeout field in the SCSI I/O CCB.
 */
#define	CAM_TIME_DEFAULT	0x00000000	/* Use SIM default value */
#define	CAM_TIME_INFINITY	0xFFFFFFFF	/* Infinite timeout */

#define	CAM_SUCCESS	0	/* For signaling general success */
#define	CAM_FAILURE	1	/* For signaling general failure */

#define CAM_FALSE	0
#define CAM_TRUE	1

#define XPT_CCB_INVALID	-1	/* for signaling a bad CCB to free */

/*
 * Union of all CCB types for kernel space allocation.  This union should
 * never be used for manipulating CCBs - its only use is for the allocation
 * and deallocation of raw CCB space and is the return type of xpt_ccb_alloc
 * and the argument to xpt_ccb_free.
 */
union ccb {
	struct	ccb_hdr			ccb_h;	/* For convenience */
	struct	ccb_scsiio		csio;
	struct	ccb_getdev		cgd;
	struct	ccb_getdevlist		cgdl;
	struct	ccb_pathinq		cpi;
	struct	ccb_relsim		crs;
	struct	ccb_setasync		csa;
	struct	ccb_setdev		csd;
	struct	ccb_pathstats		cpis;
	struct	ccb_getdevstats		cgds;
	struct	ccb_dev_match		cdm;
	struct	ccb_trans_settings	cts;
	struct	ccb_calc_geometry	ccg;	
	struct	ccb_abort		cab;
	struct	ccb_resetbus		crb;
	struct	ccb_resetdev		crd;
	struct	ccb_termio		tio;
	struct	ccb_accept_tio		atio;
	struct	ccb_scsiio		ctio;
	struct	ccb_en_lun		cel;
	struct	ccb_immed_notify	cin;
	struct	ccb_notify_ack		cna;
	struct	ccb_eng_inq		cei;
	struct	ccb_eng_exec		cee;
	struct 	ccb_rescan		crcn;
	struct  ccb_debug		cdbg;
};

__BEGIN_DECLS
static __inline void
cam_fill_csio(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int32_t flags, u_int8_t tag_action,
	      u_int8_t *data_ptr, u_int32_t dxfer_len,
	      u_int8_t sense_len, u_int8_t cdb_len,
	      u_int32_t timeout);

static __inline void
cam_fill_ctio(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int32_t flags, u_int tag_action, u_int tag_id,
	      u_int init_id, u_int scsi_status, u_int8_t *data_ptr,
	      u_int32_t dxfer_len, u_int32_t timeout);
	
static __inline void
cam_fill_csio(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int32_t flags, u_int8_t tag_action,
	      u_int8_t *data_ptr, u_int32_t dxfer_len,
	      u_int8_t sense_len, u_int8_t cdb_len,
	      u_int32_t timeout)
{
	csio->ccb_h.func_code = XPT_SCSI_IO;
	csio->ccb_h.flags = flags;
	csio->ccb_h.retry_count = retries;	
	csio->ccb_h.cbfcnp = cbfcnp;
	csio->ccb_h.timeout = timeout;
	csio->data_ptr = data_ptr;
	csio->dxfer_len = dxfer_len;
	csio->sense_len = sense_len;
	csio->cdb_len = cdb_len;
	csio->tag_action = tag_action;
}

static __inline void
cam_fill_ctio(struct ccb_scsiio *csio, u_int32_t retries,
	      void (*cbfcnp)(struct cam_periph *, union ccb *),
	      u_int32_t flags, u_int tag_action, u_int tag_id,
	      u_int init_id, u_int scsi_status, u_int8_t *data_ptr,
	      u_int32_t dxfer_len, u_int32_t timeout)
{
	csio->ccb_h.func_code = XPT_CONT_TARGET_IO;
	csio->ccb_h.flags = flags;
	csio->ccb_h.retry_count = retries;	
	csio->ccb_h.cbfcnp = cbfcnp;
	csio->ccb_h.timeout = timeout;
	csio->data_ptr = data_ptr;
	csio->dxfer_len = dxfer_len;
	csio->scsi_status = scsi_status;
	csio->tag_action = tag_action;
	csio->tag_id = tag_id;
	csio->init_id = init_id;
}

void cam_calc_geometry(struct ccb_calc_geometry *ccg, int extended);

__END_DECLS

#endif /* _CAM_CAM_CCB_H */
