# $FreeBSD: src/share/syscons/keymaps/Makefile,v 1.44.2.11 2003/05/18 08:57:28 murray Exp $
# $DragonFly: src/share/syscons/keymaps/Makefile,v 1.4 2005/03/28 23:06:47 swildner Exp $

KEYMAPS=  INDEX.keymaps
KEYMAPS+= be.iso.kbd be.iso.acc.kbd
KEYMAPS+= bg.bds.ctrlcaps.kbd bg.phonetic.ctrlcaps.kbd
KEYMAPS+= br275.iso.kbd br275.iso.acc.kbd br275.cp850.kbd
KEYMAPS+= ce.iso2.kbd
KEYMAPS+= cs.latin2.qwertz.kbd
KEYMAPS+= cz.iso2.kbd
KEYMAPS+= danish.iso.kbd danish.cp865.kbd
KEYMAPS+= el.iso07.kbd
KEYMAPS+= estonian.iso.kbd estonian.iso15.kbd estonian.cp850.kbd
KEYMAPS+= finnish.iso.kbd finnish.cp850.kbd
KEYMAPS+= fr.iso.kbd fr.iso.acc.kbd fr.dvorak.kbd
KEYMAPS+= fr_CA.iso.acc.kbd
KEYMAPS+= german.iso.kbd german.cp850.kbd
KEYMAPS+= gr.elot.acc.kbd gr.us101.acc.kbd
KEYMAPS+= hr.iso.kbd
KEYMAPS+= hu.iso2.101keys.kbd hu.iso2.102keys.kbd
KEYMAPS+= icelandic.iso.kbd icelandic.iso.acc.kbd
KEYMAPS+= it.iso.kbd
KEYMAPS+= iw.iso8.kbd
KEYMAPS+= jp.106.kbd jp.106x.kbd
KEYMAPS+= lat-amer.kbd
KEYMAPS+= lt.iso4.kbd
KEYMAPS+= norwegian.iso.kbd norwegian.dvorak.kbd
KEYMAPS+= pl_PL.ISO8859-2.kbd 
KEYMAPS+= pt.iso.kbd pt.iso.acc.kbd
KEYMAPS+= ru.koi8-r.kbd ru.koi8-r.shift.kbd ru.koi8-r.win.kbd ru.cp866.kbd ru.iso5.kbd
KEYMAPS+= si.iso.kbd
KEYMAPS+= sk.iso2.kbd
KEYMAPS+= spanish.iso.kbd spanish.iso.acc.kbd spanish.iso15.acc.kbd
KEYMAPS+= swedish.iso.kbd swedish.cp850.kbd
KEYMAPS+= swissfrench.iso.kbd swissfrench.iso.acc.kbd swissfrench.cp850.kbd
KEYMAPS+= swissgerman.iso.kbd swissgerman.iso.acc.kbd swissgerman.cp850.kbd
KEYMAPS+= tr.iso9.q.kbd
KEYMAPS+= ua.koi8-u.kbd ua.koi8-u.shift.alt.kbd ua.iso5.kbd
KEYMAPS+= uk.iso.kbd uk.iso-ctrl.kbd uk.cp850.kbd uk.cp850-ctrl.kbd
KEYMAPS+= us.iso.kbd us.dvorak.kbd us.dvorakx.kbd us.emacs.kbd us.pc-ctrl.kbd
KEYMAPS+= us.unix.kbd

MAPSDIR = ${SHAREDIR}/syscons/keymaps
NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${KEYMAPS} \
	    ${DESTDIR}${MAPSDIR}

.include <bsd.prog.mk>
