#!/bin/sh -e
#
# Copyright (c) 2002 Ruslan Ermilov, The FreeBSD Project
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
# $FreeBSD: src/tools/make_libdeps.sh,v 1.2.2.1 2002/07/23 12:12:30 ru Exp $
# $DragonFly: src/tools/make_libdeps.sh,v 1.3 2003/08/05 07:45:43 asmodai Exp $

export PATH=/usr/bin

FS=': '				# internal field separator
LIBDEPENDS=./_libdeps		# intermediate output file
USRSRC=${1:-/usr/src}		# source root
LIBS="
	lib
	gnu/lib
	gnu/usr.bin/perl/libperl
	kerberos5/lib
	secure/lib
	usr.bin/lex/lib
	usr.sbin/pcvt/keycap
"				# where to scan for libraries

# This sed(1) filter is used to convert -lfoo to path/to/libfoo.
#
SED_FILTER="
sed -E
    -e's; ;! ;g'
    -e's;$;!;'
    -e's;-lm!;lib/msun;g'
    -e's;-l(asn1|gssapi|krb5|roken)!;kerberos5/lib/lib\1;g'
    -e's;-l(crypto|ssh)!;secure/lib/lib\1;g'
    -e's;-l([^!]+)!;lib/lib\1;g'
"

# Generate interdependencies between libraries.
#
genlibdepends()
{
	(
		cd ${USRSRC}
		find ${LIBS} -mindepth 1 -name Makefile |
		xargs grep -l 'bsd\.lib\.mk' |
		while read makefile; do
			libdir=$(dirname ${makefile})
			deps=$(
				cd ${libdir}
				make -V LDADD
			)
			if [ "${deps}" ]; then
				echo ${libdir}"${FS}"$(
					echo ${deps} |
					eval ${SED_FILTER}
				)
			fi
		done
	)
}

main()
{
	if [ ! -f ${LIBDEPENDS} ]; then
		genlibdepends >${LIBDEPENDS}
	fi

	prebuild_libs=$(
		awk -F"${FS}" '{ print $2 }' ${LIBDEPENDS} |rs 0 1 |sort -u
	)
	echo "Libraries with dependents:"
	echo
	echo ${prebuild_libs} |
	rs 0 1
	echo

	echo "List of interdependencies:"
	echo
	for lib in ${prebuild_libs}; do
		grep "^${lib}${FS}" ${LIBDEPENDS} || true
	done |
	awk -F"${FS}" '{
		if ($2 in dependents)
			dependents[$2]=dependents[$2]" "$1
		else
			dependents[$2]=$1
	}
	END {
		for (lib in dependents)
			print dependents[lib]": " lib
	}' |
	sort

	exit 0
}

main
