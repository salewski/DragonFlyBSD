#
# Copyright (c) 2000-2001, Boris Popov
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#    This product includes software developed by Boris Popov.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD: src/sys/libkern/iconv_converter_if.m,v 1.1.2.1 2001/05/21 08:28:07 bp Exp $
# $DragonFly: src/sys/libiconv/iconv_converter_if.m,v 1.3 2004/03/18 18:27:47 dillon Exp $
#

#include <sys/iconv.h>

INTERFACE iconv_converter;

STATICMETHOD int open {
	struct iconv_converter_class *dcp;
	struct iconv_cspair *cspto;
	struct iconv_cspair *cspfrom;
	void **hpp;
};

METHOD int close {
	void *handle;
};

METHOD int conv {
	void *handle;
	const char **inbuf;
        size_t *inbytesleft;
	char **outbuf;
	size_t *outbytesleft;
};

STATICMETHOD int init {
	struct iconv_converter_class *dcp;
} DEFAULT iconv_converter_initstub;

STATICMETHOD void done {
	struct iconv_converter_class *dcp;
} DEFAULT iconv_converter_donestub;

STATICMETHOD const char * name {
	struct iconv_converter_class *dcp;
};
