/* gsc.h - programming interface to the scanner device driver `gsc'
 *
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Gunther Schadow.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/include/gsc.h,v 1.2.10.1 2000/08/03 01:01:19 peter Exp $
 * $DragonFly: src/sys/i386/include/Attic/gsc.h,v 1.2 2003/06/17 04:28:35 dillon Exp $
 */

#ifndef	_MACHINE_GSC_H_
#define	_MACHINE_GSC_H_

#include <sys/ioccom.h>

#define GSC_GRES	_IOR('S', 1, int)	/* get resolution / dpi */
#define GSC_SRES	_IOW('S', 2, int)	/* set resolution / dpi */
#define GSC_GWIDTH	_IOR('S', 3, int)	/* get width / pixels */
#define	GSC_SWIDTH	_IOW('S', 4, int)	/* set width / pixels */
#define GSC_GHEIGHT	_IOR('S', 5, int)	/* get height / pixels */
#define GSC_SHEIGHT	_IOW('S', 6, int)	/* set height / pixels */

#define GSC_GBLEN	_IOR('S', 7, int)	/* get buffer length / lines */
#define GSC_SBLEN	_IOW('S', 8, int)	/* set buffer length / lines */
#define GSC_GBTIME	_IOR('S', 9, int)	/* get buffer timeout / s */
#define GSC_SBTIME	_IOW('S', 10, int)	/* set buffer timeout / s */

#define GSC_SRESSW	_IO('S', 11)	        /* set resolution by switch */

#endif /* !_MACHINE_GSC_H_ */
