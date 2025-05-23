#!/bin/sh
# Emulate nroff with groff.
# $FreeBSD: src/contrib/groff/src/roff/nroff/nroff.sh,v 1.11.2.3 2003/02/14 15:22:08 ru Exp $
# $DragonFly: src/contrib/groff/src/roff/nroff/Attic/nroff.sh,v 1.2 2003/06/17 04:24:02 dillon Exp $

prog="$0"
# Default device.
# First try the "locale charmap" command, because it's most reliable.
# On systems where it doesn't exist, look at the environment variables.
case "`#locale charmap 2>/dev/null`" in
  UTF-8)
    T=-Tutf8 ;;
  ISO-8859-1)
    T=-Tlatin1 ;;
  IBM-1047)
    T=-Tcp1047 ;;
  *)
    case "${LC_ALL-${LC_CTYPE-${LANG}}}" in
      *.UTF-8)
        T=-Tutf8 ;;
      iso_8859_1 | *.ISO*8859-1 | *.ISO*8859-15)
        T=-Tlatin1 ;;
      *.IBM-1047)
        T=-Tcp1047 ;;
      *.KOI8-R)
        T=-Tkoi8-r ;;
      *)
        case "$LESSCHARSET" in
          utf-8)
            T=-Tutf8 ;;
          latin1)
            T=-Tlatin1 ;;
          cp1047)
            T=-Tcp1047 ;;
          *)
            T=-Tascii ;;
          esac ;;
     esac ;;
esac
opts=

# `for i; do' doesn't work with some versions of sh

for i
  do
  case $1 in
    -c)
      opts="$opts -P-c" ;;
    -h)
      opts="$opts -P-h" ;;
    -[eq] | -s*)
      # ignore these options
      ;;
    -[mrnoT])
      echo "$prog: option $1 requires an argument" >&2
      exit 1 ;;
    -[iptSUC] | -[mrno]*)
      opts="$opts $1" ;;
    -Tascii | -Tlatin1 | -Tkoi8-r | -Tutf8 | -Tcp1047)
      T=$1 ;;
    -T*)
      # ignore other devices
      ;;
    -u*)
      # Solaris 2.2 `man' uses -u0; ignore it,
      # since `less' and `more' can use the emboldening info.
      ;;
    -v | --version)
      echo "GNU nroff (groff) version @VERSION@"
      exit 0 ;;
    --help)
      echo "usage: nroff [-cChipt] [-mNAME] [-nNUM] [-oLIST] [-rCN] [-Tname] [FILE...]"
      exit 0 ;;
    --)
      shift
      break ;;
    -)
      break ;;
    -*)
      echo "$prog: invalid option $1" >&2
      exit 1 ;;
    *)
      break ;;
  esac
  shift
done

# This shell script is intended for use with man, so warnings are
# probably not wanted.  Also load nroff-style character definitions.

: ${GROFF_BIN_PATH=@BINDIR@}
export GROFF_BIN_PATH
PATH=$GROFF_BIN_PATH:$PATH groff -mtty-char $T $opts ${1+"$@"}

# eof
