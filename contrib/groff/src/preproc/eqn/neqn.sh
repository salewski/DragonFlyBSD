#!/bin/sh
# Provision of this shell script should not be taken to imply that use of
# GNU eqn with groff -Tascii|-Tlatin1|-Tkoi8-r|-Tutf8|-Tcp1047 is supported.
# $FreeBSD: src/contrib/groff/src/preproc/eqn/neqn.sh,v 1.7.2.2 2001/08/06 17:02:03 ru Exp $
# $DragonFly: src/contrib/groff/src/preproc/eqn/Attic/neqn.sh,v 1.2 2003/06/17 04:24:02 dillon Exp $

# Default device.
case "${LC_ALL-${LC_CTYPE-${LANG}}}" in
  *.UTF-8)
    T=utf8 ;;
  iso_8859_1 | *.ISO*8859-1 | *.ISO*8859-15)
    T=latin1 ;;
  *.IBM-1047)
    T=cp1047 ;;
  *.KOI8-R)
    T=koi8-r ;;
  *)
    T=ascii ;;
esac

: ${GROFF_BIN_PATH=@BINDIR@}
PATH=$GROFF_BIN_PATH:$PATH
export PATH
exec @g@eqn -T${T} ${1+"$@"}

# eof
