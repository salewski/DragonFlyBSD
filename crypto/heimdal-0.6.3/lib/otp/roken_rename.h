/*
 * Copyright (c) 1998 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* $Id: roken_rename.h,v 1.3 2003/04/16 16:33:57 lha Exp $ */

#ifndef __roken_rename_h__
#define __roken_rename_h__

#ifndef HAVE_SNPRINTF
#define snprintf _otp_snprintf
#endif
#ifndef HAVE_ASPRINTF
#define asprintf _otp_asprintf
#endif
#ifndef HAVE_ASNPRINTF
#define asnprintf _otp_asnprintf
#endif
#ifndef HAVE_VASPRINTF
#define vasprintf _otp_vasprintf
#endif
#ifndef HAVE_VASNPRINTF
#define vasnprintf _otp_vasnprintf
#endif
#ifndef HAVE_VSNPRINTF
#define vsnprintf _otp_vsnprintf
#endif
#ifndef HAVE_STRCASECMP
#define strcasecmp _otp_strcasecmp
#endif
#ifndef HAVE_STRNCASECMP
#define strncasecmp _otp_strncasecmp
#endif
#ifndef HAVE_STRLWR
#define strlwr _otp_strlwr
#endif
#ifndef HAVE_STRLCAT
#define strlcat _otp_strlcat
#endif
#ifndef HAVE_STRLCPY
#define strlcpy _otp_strlcpy
#endif

#endif /* __roken_rename_h__ */
