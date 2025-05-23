#!/bin/sh
#
# Copyright (c) 2000  The FreeBSD Project
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
# $FreeBSD: src/etc/rc.d/atm3.sh,v 1.10 2002/06/13 22:14:36 gordon Exp $
# $DragonFly: src/etc/rc.d/atm3.sh,v 1.3 2004/01/26 17:21:15 rob Exp $
#

# Start ATM daemons
# XXX - This script uses global variables set by scripts atm1 and atm2.
#	Ideally this shouldn't be the case.
#

# PROVIDE: atm3
# REQUIRE: atm2
# BEFORE: DAEMON
# KEYWORD: DragonFly

. /etc/rc.subr
dummy_rc_command "$1"

atm3_start()
{
	echo -n 'Starting ATM daemons:'

	# Start SCSP daemon (if needed)
	case ${atm_scspd} in
	1)
		echo -n ' scspd'
		scspd
		;;
	esac

	# Start ATMARP daemon (if needed)
	if [ -n "${atm_atmarpd}" ]; then
		echo -n ' atmarpd'
		atmarpd ${atm_atmarpd}
	fi
	echo '.'
}

load_rc_config "XXX"

case ${atm_enable} in
[Yy][Ee][Ss])
	atm3_start
	;;
esac
