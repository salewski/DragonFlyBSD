#!/bin/sh
# Copyright (c) April 1997 Wolfram Schneider <wosch@FreeBSD.org>, Berlin.
#
# portsinfo - Generate list of new ports for last two weeks.
#
# $FreeBSD: src/tools/tools/portsinfo/portsinfo.sh,v 1.5 1999/08/28 00:54:32 peter Exp $
# $DragonFly: src/tools/tools/portsinfo/Attic/portsinfo.sh,v 1.2 2003/06/17 04:29:11 dillon Exp $

PATH=/bin:/usr/bin:/usr/local/bin:$PATH; export PATH

host=http://www.freebsd.org
url=$host/cgi/ports.cgi
time='?type=new&time=2+week+ago&sektion=all'
time2='?type=changed&time=2+week+ago&sektion=all'
info=yes

if [ x"$info" = xyes ]; then

cat <<'EOF'
Introduction
------------

The FreeBSD Ports Collection offers a simple way for users and
administrators to install applications. Each "port" listed here
contains any patches necessary to make the original application source
code compile and run on FreeBSD. Installing an application is as
simple as downloading the port, unpacking it and typing make in the
port directory. The Makefile automatically fetches the application
source code, either from a local disk or via ftp, unpacks it on your
system, applies the patches, and compiles. If all goes well, simply
type make install to install the application.

For more information about using ports, see the ports collection

	http://www.freebsd.org/handbook/ports.html
and
	http://www.freebsd.org/ports/

EOF

    lynx -dump $host/ports/ |
	perl -ne '/^[ ]*There are currently/ && s/^\s+// && print && exit'

cat <<EOF


New ports added last two weeks
------------------------------

EOF


fi


lynx -nolist -dump -reload -nostatus "$url$time" | 
    grep -v "Description :" |
perl -ne 'print if (/^\s*Category/ .. /__________________/)' |
    grep -v ________ | 
perl -ne 'if (/^\s*Category/) { 
		print; for(1..50) {print "="}; print "\n";
          } else { print}'

cat <<EOF


Updated ports last two weeks
-----------------------------------

EOF

lynx -nolist -dump -reload -nostatus "$url$time2" |
    grep -v "Description :" |
perl -ne 's/\[INLINE\]\s*//g; print if (/Category/ .. /XXXXYYYYZZZZ/)' |      
perl -ne 'if (/^\s*Category/) {
                print; for(1..50) {print "="}; print "\n";
          } else { print}'

cat <<EOF

This information was produced by
	$url

EOF

