#!/bin/sh
# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#
# $FreeBSD: src/share/examples/BSD_daemon/poster.sh,v 1.1.2.1 2001/03/04 09:19:24 phk Exp $
# $DragonFly: src/share/examples/BSD_daemon/Attic/poster.sh,v 1.2 2003/06/17 04:36:57 dillon Exp $
#

echo '%!'
echo '/beastie {'
cat beastie.eps
echo '} def'
cat FreeBSD.pfa

echo '

/mm {25.4 div 72 mul} def
/FreeBSD findfont 120 scalefont setfont
/center 210 mm 2 div def
/top 297 mm def
/cshow { dup stringwidth pop 2 div neg 0 rmoveto show } def

% "FreeBSD" across the top.
% 691 is "top - height of caps - (left - X("F"))"
center 691 moveto
(FreeBSD) cshow

% Put beastie in the center
/sc 1.25 def
center 125 moveto 
384 sc mul 2 div neg 0 rmoveto
gsave currentpoint translate sc sc scale beastie grestore

% A box for the bottom text
gsave
10 10 moveto
210 mm 20 sub 0 rlineto
0 70 rlineto
210 mm 20 sub neg 0 rlineto
closepath
.7 .7 .9 setrgbcolor
fill
grestore

% Bottom text
center 70 moveto 
/NRBWelshGillianBold findfont 50 scalefont setfont

center 30 moveto 
(http://www.FreeBSD.org) cshow

% Do not forget Kirks copyright string.
10 10 moveto 
/Times-Roman findfont 8 scalefont setfont
(BSD Daemon ) show
/Symbol findfont 8 scalefont setfont
(\343 ) show
/Times-Roman findfont 8 scalefont setfont
(Copyright 1988 by Marshall Kirk McKusick.  All Rights Reserved.) show

showpage
'
