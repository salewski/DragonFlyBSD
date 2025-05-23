/* $FreeBSD: src/sys/dev/isp/isp_tpublic.h,v 1.2.4.3 2001/10/08 05:57:06 mjacob Exp $ */
/* $DragonFly: src/sys/dev/disk/isp/isp_tpublic.h,v 1.2 2003/06/17 04:28:27 dillon Exp $ */
/*
 * Qlogic ISP Host Adapter Public Target Interface Structures && Routines
 *---------------------------------------
 * Copyright (c) 2000 by Matthew Jacob
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
 * Matthew Jacob
 * Feral Software
 * mjacob@feral.com
 */

/*
 * Required software target mode message and event handling structures.
 *
 * The message and event structures are used by the MI layer
 * to propagate messages and events upstream.
 */

#ifndef	IN_MSGLEN
#define	IN_MSGLEN	8
#endif
typedef struct {
	void *		nt_hba;			/* HBA tag */
	u_int64_t	nt_iid;			/* inititator id */
	u_int64_t	nt_tgt;			/* target id */
	u_int64_t	nt_lun;			/* logical unit */
	u_int8_t	nt_bus;			/* bus */
	u_int8_t	nt_tagtype;		/* tag type */
	u_int16_t	nt_tagval;		/* tag value */
	u_int8_t	nt_msg[IN_MSGLEN];	/* message content */
} tmd_msg_t;

typedef struct {
	void *		ev_hba;			/* HBA tag */
	u_int16_t	ev_bus;			/* bus */
	u_int16_t	ev_event;		/* type of async event */
} tmd_event_t;

/*
 * Suggested Software Target Mode Command Handling structure.
 *
 * A note about terminology:
 *
 *   MD stands for "Machine Dependent".
 *
 *    This driver is structured in three layers: Outer MD, core, and inner MD.
 *    The latter also is bus dependent (i.e., is cognizant of PCI bus issues
 *    as well as platform issues).
 *
 *
 *   "Outer Layer" means "Other Module"
 *
 *    Some additional module that actually implements SCSI target command
 *    policy is the recipient of incoming commands and the source of the
 *    disposition for them.
 *
 * The command structure below is one suggested possible MD command structure,
 * but since the handling of thbis is entirely in the MD layer, there is
 * no explicit or implicit requirement that it be used.
 *
 * The cd_private tag should be used by the MD layer to keep a free list
 * of these structures. Code outside of this driver can then use this
 * as an to identify it's own unit structures. That is, when not on the MD
 * layer's freelist, the MD layer should shove into it the identifier
 * that the outer layer has for it- passed in on an initial QIN_HBA_REG
 * call (see below).
 *
 * The cd_hba tag is a tag that uniquely identifies the HBA this target
 * mode command is coming from. The outer layer has to pass this back
 * unchanged to avoid chaos.
 *
 * The cd_iid, cd_tgt, cd_lun and cd_bus tags are used to identify the
 * id of the initiator who sent us a command, the target claim to be, the
 * lun on the target we claim to be, and the bus instance (for multiple
 * bus host adapters) that this applies to (consider it an extra Port
 * parameter). The iid, tgt and lun values are deliberately chosen to be
 * fat so that, for example, World Wide Names can be used instead of
 * the units that the Qlogic firmware uses (in the case where the MD
 * layer maintains a port database, for example).
 *
 * The cd_tagtype field specifies what kind of command tag has been
 * sent with the command. The cd_tagval is the tag's value (low 16
 * bits). It also contains (in the upper 16 bits) any command handle.
 *
 *
 * N.B.: when the MD layer sends this command to outside software
 * the outside software likely *MUST* return the same cd_tagval that
 * was in place because this value is likely what the Qlogic f/w uses
 * to identify a command.
 *
 * The cd_cdb contains storage for the passed in command descriptor block.
 * This is the maximum size we can get out of the Qlogic f/w. There's no
 * passed in length because whoever decodes the command to act upon it
 * will know what the appropriate length is.
 *
 * The tag cd_lflags are the flags set by the MD driver when it gets
 * command incoming or when it needs to inform any outside entities
 * that the last requested action failed.
 *
 * The tag cd_hflags should be set by any outside software to indicate
 * the validity of sense and status fields (defined below) and to indicate
 * the direction data is expected to move. It is an error to have both
 * CDFH_DATA_IN and CDFH_DATA_OUT set.
 *
 * If the CDFH_STSVALID flag is set, the command should be completed (after
 * sending any data and/or status). If CDFH_SNSVALID is set and the MD layer
 * can also handle sending the associated sense data (either back with an
 * FCP RESPONSE IU for Fibre Channel or otherwise automatically handling a
 * REQUEST SENSE from the initator for this target/lun), the MD layer will
 * set the CDFL_SENTSENSE flag on successful transmission of the sense data.
 * It is an error for the CDFH_SNSVALID bit to be set and CDFH_STSVALID not
 * to be set. It is an error for the CDFH_SNSVALID be set and the associated
 * SCSI status (cd_scsi_status) not be set to CHECK CONDITON.
 * 
 * The tag cd_data points to a data segment to either be filled or
 * read from depending on the direction of data movement. The tag
 * is undefined if no data direction is set. The MD layer and outer
 * layers must agree on the meaning of cd_data.
 *
 * The tag cd_totlen is the total data amount expected to be moved
 * over the life of the command. It *may* be set by the MD layer, possibly
 * from the datalen field of an FCP CMND IU unit. If it shows up in the outer
 * layers set to zero and the CDB indicates data should be moved, the outer
 * layer should set it to the amount expected to be moved.
 *
 * The tag cd_resid should be the total residual of data not transferred.
 * The outer layers need to set this at the begining of command processing
 * to equal cd_totlen. As data is successfully moved, this value is decreased.
 * At the end of a command, any nonzero residual indicates the number of bytes
 * requested but not moved. XXXXXXXXXXXXXXXXXXXXXXX TOO VAGUE!!! 
 *
 * The tag cd_xfrlen is the length of the currently active data transfer.
 * This allows several interations between any outside software and the
 * MD layer to move data.
 *
 * The reason that total length and total residual have to be tracked
 * is that fibre channel FCP DATA IU units have to have a relative
 * offset field.
 *
 * N.B.: there is no necessary 1-to-1 correspondence between any one
 * data transfer segment and the number of CTIOs that will be generated
 * satisfy the current data transfer segment. It's not also possible to
 * predict how big a transfer can be before it will be 'too big'. Be
 * reasonable- a 64KB transfer is 'reasonable'. A 1MB transfer may not
 * be. A 32MB transfer is unreasonable. The problem here has to do with
 * how CTIOs can be used to map passed data pointers. In systems which
 * have page based scatter-gather requirements, each PAGESIZEd chunk will
 * consume one data segment descriptor- you get 3 or 4 of them per CTIO.
 * The size of the REQUEST QUEUE you drop a CTIO onto is finite (typically
 * it's 256, but on some systems it's even smaller, and note you have to
 * sure this queue with the initiator side of this driver).
 *
 * The tags cd_sense and cd_scsi_status are pretty obvious.
 *
 * The tag cd_error is to communicate between the MD layer and outer software
 * the current error conditions.
 *
 * The tag cd_reserved pads out the structure to 128 bytes. The first
 * half of the pad area is reserved to the MD layer, and the second half
 * may be used by outer layers, for scratch purposes.
 */

#ifndef	_LP64
#if	defined(__alpha__) || defined(__sparcv9cpu) || defined(__sparc_v9__) ||\
    defined(__ia64__)
#define	_LP64
#endif
#endif

#ifndef	_TMD_PAD_LEN
#ifdef	_LP64
#define	_TMD_PAD_LEN	12
#else
#define	_TMD_PAD_LEN	24
#endif
#endif
#ifndef	ATIO_CDBLEN
#define	ATIO_CDBLEN	26
#endif
#ifndef	QLTM_SENSELEN
#define	QLTM_SENSELEN	18
#endif
typedef struct tmd_cmd {
	void *			cd_private;	/* layer private data */
	void *			cd_hba;		/* HBA tag */
	void *			cd_data;	/* 'pointer' to data */
	u_int64_t		cd_iid;		/* initiator ID */
	u_int64_t		cd_tgt;		/* target id */
	u_int64_t		cd_lun;		/* logical unit */
	u_int8_t		cd_bus;		/* bus */
	u_int8_t		cd_tagtype;	/* tag type */
	u_int32_t		cd_tagval;	/* tag value */
	u_int8_t		cd_cdb[ATIO_CDBLEN];	/* Command */
	u_int8_t		cd_lflags;	/* flags lower level sets */
	u_int8_t		cd_hflags;	/* flags higher level sets */
	u_int32_t		cd_totlen;	/* total data requirement */
	u_int32_t		cd_resid;	/* total data residual */
	u_int32_t		cd_xfrlen;	/* current data requirement */
	int32_t			cd_error;	/* current error */
	u_int8_t		cd_sense[QLTM_SENSELEN];
	u_int16_t		cd_scsi_status;	/* closing SCSI status */
	u_int8_t		cd_reserved[_TMD_PAD_LEN];
} tmd_cmd_t;

#define	CDFL_SNSVALID	0x01		/* sense data (from f/w) valid */
#define	CDFL_NODISC	0x02		/* disconnects disabled */
#define	CDFL_SENTSENSE	0x04		/* last action sent sense data */
#define	CDFL_SENTSTATUS	0x08		/* last action sent status */
#define	CDFL_ERROR	0x10		/* last action ended in error */
#define	CDFL_BUSY	0x40		/* this command is not on a free list */
#define	CDFL_PRIVATE_0	0x80		/* private layer flags */

#define	CDFH_SNSVALID	0x01		/* sense data valid */
#define	CDFH_STSVALID	0x02		/* status valid */
#define	CDFH_NODATA	0x00		/* no data transfer expected */
#define	CDFH_DATA_IN	0x04		/* target (us) -> initiator (them) */
#define	CDFH_DATA_OUT	0x08		/* initiator (them) -> target (us) */
#define	CDFH_DATA_MASK	0x0C		/* mask to cover data direction */
#define	CDFH_PRIVATE_0	0x80		/* private layer flags */

/*
 * Action codes set by the Qlogic MD target driver for
 * the external layer to figure out what to do with.
 */
typedef enum {
	QOUT_HBA_REG=0,	/* the argument is a pointer to a hba_register_t */
	QOUT_TMD_START,	/* the argument is a pointer to a tmd_cmd_t */
	QOUT_TMD_DONE,	/* the argument is a pointer to a tmd_cmd_t */
	QOUT_TEVENT,	/* the argument is a pointer to a tmd_event_t */
	QOUT_TMSG,	/* the argument is a pointer to a tmd_msg_t */
	QOUT_HBA_UNREG	/* the argument is a pointer to a hba_register_t */
} tact_e;

/*
 * Action codes set by the external layer for the
 * MD Qlogic driver to figure out what to do with.
 */
typedef enum {
	QIN_HBA_REG=6,	/* the argument is a pointer to a hba_register_t */
	QIN_ENABLE,	/* the argument is a pointer to a tmd_cmd_t */
	QIN_DISABLE,	/* the argument is a pointer to a tmd_cmd_t */
	QIN_TMD_CONT,	/* the argument is a pointer to a tmd_cmd_t */
	QIN_TMD_FIN,	/* the argument is a pointer to a done tmd_cmd_t */
	QIN_HBA_UNREG	/* the argument is a pointer to a hba_register_t */
} qact_e;

/*
 * A word about the START/CONT/DONE/FIN dance:
 *
 *	When the HBA is enabled for receiving commands, one may	show up
 *	without notice. When that happens, the Qlogic target mode driver
 *	gets a tmd_cmd_t, fills it with the info that just arrived, and
 *	calls the outer layer with a QOUT_TMD_START code and pointer to
 *	the tmd_cmd_t.
 *
 *	The outer layer decodes the command, fetches data, prepares stuff,
 *	whatever, and starts by passing back the pointer with a QIN_TMD_CONT
 *	code which causes the Qlogic target mode driver to generate CTIOs to
 *	satisfy whatever action needs to be taken. When those CTIOs complete,
 *	the Qlogic target driver sends the pointer to the cmd_tmd_t back with
 *	a QOUT_TMD_DONE code. This repeats for as long as necessary.
 *
 *	The outer layer signals it wants to end the command by settings within
 *	the tmd_cmd_t itself. When the final QIN_TMD_CONT is reported completed,
 *	the outer layer frees the tmd_cmd_t by sending the pointer to it
 *	back with a QIN_TMD_FIN code.
 *
 *	The graph looks like:
 *
 *	QOUT_TMD_START -> [ QIN_TMD_CONT -> QOUT_TMD_DONE ] * -> QIN_TMD_FIN.
 *
 */

/*
 * A word about ENABLE/DISABLE: the argument is a pointer to an tmd_cmd_t
 * with cd_hba, cd_bus, cd_tgt and cd_lun filled out. If an error occurs
 * in either enabling or disabling the described lun, cd_lflags is set
 * with CDFL_ERROR.
 *
 * Logical unit zero must be the first enabled and the last disabled.
 */

/*
 * Target handler functions.
 * The MD target handler function (the outer layer calls this)
 * should be be prototyped like:
 *
 *	void target_action(qact_e, void *arg)
 *
 * The outer layer target handler function (the MD layer calls this)
 * should be be prototyped like:
 *
 *	void system_action(tact_e, void *arg)
 */

/*
 * This structure is used to register to other software modules the
 * binding of an HBA identifier, driver name and instance and the
 * lun width capapbilities of this target driver. It's up to each
 * platform to figure out how it wants to do this, but a typical
 * sequence would be for the MD layer to find some external module's
 * entry point and start by sending a QOUT_HBA_REG with info filled
 * in, and the external module to call back with a QIN_HBA_REG that
 * passes back the corresponding information.
 */
typedef struct {
	void *	r_identity;
	char	r_name[8];
	int	r_inst;
	int	r_lunwidth;
	int	r_buswidth;
	void   (*r_action)(int, void *);
} hba_register_t;
