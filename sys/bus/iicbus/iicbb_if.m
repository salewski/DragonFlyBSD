#
# Copyright (c) 1998 Nicolas Souchu
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
# $FreeBSD: src/sys/dev/iicbus/iicbb_if.m,v 1.3 1999/08/28 00:41:58 peter Exp $
# $DragonFly: src/sys/bus/iicbus/iicbb_if.m,v 1.3 2003/11/17 00:54:39 asmodai Exp $
#

#include <sys/bus.h>

INTERFACE iicbb;

#
# iicbus callback
#
METHOD int callback {
	device_t dev;
	int index;
	caddr_t data;
};

#
# Set I2C bus lines
#
METHOD void setlines {
	device_t dev;
	int ctrl;
	int data;
};

#
# Get I2C bus lines
#
#
METHOD int getdataline {
	device_t dev;
};

#
# Reset interface
#
METHOD int reset {
	device_t dev;
	u_char speed;
	u_char addr;
	u_char *oldaddr;
};
