/*-
 * Copyright (c) 1998 Nicolas Souchu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/include/smb.h,v 1.3 1999/08/28 00:44:24 peter Exp $
 * $DragonFly: src/sys/i386/include/Attic/smb.h,v 1.3 2003/06/28 04:16:03 dillon Exp $
 *
 */
#ifndef _MACHINE_SMB_H_
#define _MACHINE_SMB_H_

#include <sys/ioccom.h>

struct smbcmd {
	char cmd;
	int count;
	u_char slave;
	union {
		char byte;
		short word;

		char *byte_ptr;
		short *word_ptr;

		struct {
			short sdata;
			short *rdata;
		} process;
	} data;
};

#define SMB_QUICK_WRITE	_IOW('i', 1, struct smbcmd)
#define SMB_QUICK_READ	_IOW('i', 2, struct smbcmd)
#define SMB_SENDB	_IOW('i', 3, struct smbcmd)
#define SMB_RECVB	_IOW('i', 4, struct smbcmd)
#define SMB_WRITEB	_IOW('i', 5, struct smbcmd)
#define SMB_WRITEW	_IOW('i', 6, struct smbcmd)
#define SMB_READB	_IOW('i', 7, struct smbcmd)
#define SMB_READW	_IOW('i', 8, struct smbcmd)
#define SMB_PCALL	_IOW('i', 9, struct smbcmd)
#define SMB_BWRITE	_IOW('i', 10, struct smbcmd)
#define SMB_BREAD	_IOW('i', 11, struct smbcmd)

#endif
