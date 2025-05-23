#
# $FreeBSD: src/usr.bin/vi/Makefile,v 1.27.2.7 2002/08/07 16:32:01 ru Exp $
# $DragonFly: src/usr.bin/vi/Makefile,v 1.2 2003/06/17 04:29:33 dillon Exp $
#
# This has most of the glue needed to compile tknvi and the perl hooks,
# but not all.
#

SRCDIR=		${.CURDIR}/../../contrib/nvi

.if defined(RELEASE_CRUNCH)
# When building `vi' for the fixit floppy, don't include any of the
# API stuff.
APISTUFF=	ex_notcl.c ex_noperl.c

.else

#TCLINTERP=	yes	#we don't have it in the base tree
#TKNVI=		yes	#not ready, needs X11, tk, doesn't quite work yet
#PERLINTERP=	yes	#works if perl has no thread support built in

APISTUFF=	ex_tcl.c ex_perl.c

.if defined(PERLINTERP)
PERL?=		/usr/bin/perl5
CFLAGS+=	-DHAVE_PERL_5_003_01	# If perl >= 5.03.01
.endif

.endif

CFLAGS+=	-DGTAGS

#if using ncurses:
CFLAGS+=	-DSYSV_CURSES

VI=	nvi
EX=	nex
VIEW=	nview

PROG=	nvi

LINKS=	${BINDIR}/${VI} ${BINDIR}/${EX} ${BINDIR}/${VI} ${BINDIR}/${VIEW}
LINKS+= ${BINDIR}/${VI} ${BINDIR}/vi ${BINDIR}/${EX} ${BINDIR}/ex
LINKS+= ${BINDIR}/${VI} ${BINDIR}/view

MAN=	${SRCDIR}/docs/USD.doc/vi.man/vi.1
MLINKS+=vi.1 ex.1 vi.1 view.1
MLINKS+=vi.1 nex.1 vi.1 nview.1 vi.1 nvi.1

CATALOGS= dutch english french german polish ru_SU.KOI8-R spanish swedish \
          uk_UA.KOI8-U
NLLINKS= nl_NL
ENLINKS= en_AU en_CA en_GB en_US
FRLINKS= fr_BE fr_CA fr_CH fr_FR
DELINKS= de_AT de_CH de_DE
ESLINKS= es_ES
SVLINKS= sv_SE
PLLINKS= pl_PL

.PATH:	${SRCDIR}/common
.PATH:	${SRCDIR}/ex
.PATH:	${SRCDIR}/cl
.PATH:	${SRCDIR}/vi

CFLAGS+=-I${.CURDIR} -I${SRCDIR} -I${SRCDIR}/include

.if !defined(TKNVI)
DPADD+=		${LIBNCURSES}
LDADD+=		-lncurses

.else

.PATH:		${SRCDIR}/tk
LDADD+=		-L/usr/local/lib -L/usr/X11R6/lib -ltk41 -lX11
CFLAGS+=	-I/usr/local/include -I/usr/X11R6/include
.endif

.if defined(TCLINTERP)
.PATH:		${SRCDIR}/tcl_api
DPADD+=		${LIBTCL} ${LIBM}
LDADD+=		-ltcl -lm
CFLAGS+=	-DHAVE_TCL_INTERP -I${DESTDIR}/usr/include/tcl
.endif

.if defined(PERLINTERP)
.PATH:		${SRCDIR}/perl_api

# Perl "knows" how to compile its components. Ask it for details...
PERLLIB!=	${PERL} -MConfig -e 'print $$Config{privlib}'
PERLCPPFLAGS!=	cd ${SRCDIR}/build; ${PERL} -MExtUtils::Embed -e 'ccflags;perl_inc'
PERLLIBS!=	cd ${SRCDIR}/build; ${PERL} -MExtUtils::Embed -e 'ldopts'
PERLLDFLAGS!=	cd ${SRCDIR}/build; ${PERL} -MExtUtils::Embed -e 'ccdlflags'

LDADD+=	 ${PERLLDFLAGS} ${PERLLIBS}
CFLAGS+= -DHAVE_PERL_INTERP ${PERLCPPFLAGS}

.endif

CLEANFILES+=${EX}

# Vi curses sources
.if !defined(TKNVI)
SRCS+=	cl_bsd.c cl_funcs.c cl_main.c cl_read.c cl_screen.c cl_term.c
.endif

# Vi Tk sources
.if defined(TKNVI)
SRCS+=	tk_funcs.c tk_main.c tk_read.c tk_screen.c tk_term.c tk_util.c
.endif

# Vi Tcl/Perl interpreter sources
.if defined(TCLINTERP) || defined(PERLINTERP)
SRCS+=	api.c
.endif
.if defined(TCLINTERP)
SRCS+=	tcl.c
.endif
.if defined(PERLINTERP)
# perl.c is generated
SRCS+=	perl.c perlsfio.c
.endif

# General sources.
SRCS+=	cut.c delete.c exf.c key.c line.c log.c main.c mark.c msg.c options.c \
	options_f.c put.c screen.c search.c seq.c recover.c util.c

# Ex source.
SRCS+=	ex.c ex_abbrev.c ex_append.c ex_args.c ex_argv.c ex_at.c ex_bang.c \
	ex_cd.c ex_cmd.c ex_cscope.c ex_delete.c ex_display.c \
	ex_edit.c ex_equal.c ex_file.c ex_filter.c ex_global.c \
	ex_init.c ex_join.c ex_map.c ex_mark.c ex_mkexrc.c ex_move.c \
	ex_open.c ex_preserve.c ex_print.c ex_put.c ex_quit.c \
	ex_read.c ex_screen.c ex_script.c ex_set.c ex_shell.c \
	ex_shift.c ex_source.c ex_stop.c ex_subst.c ex_tag.c \
	ex_txt.c ex_undo.c ex_usage.c ex_util.c ex_version.c ex_visual.c \
	ex_write.c ex_yank.c ex_z.c ${APISTUFF}

# Vi source.
SRCS+=	getc.c v_at.c v_ch.c v_cmd.c v_delete.c v_ex.c v_increment.c v_init.c \
	v_itxt.c v_left.c v_mark.c v_match.c v_paragraph.c v_put.c v_redraw.c \
	v_replace.c v_right.c v_screen.c v_scroll.c v_search.c v_section.c \
	v_sentence.c v_status.c v_txt.c v_ulcase.c v_undo.c \
	v_util.c v_word.c v_xchar.c v_yank.c v_z.c v_zexit.c vi.c

# Vi screen source.
SRCS+=	vs_line.c vs_msg.c vs_refresh.c vs_relative.c vs_smap.c vs_split.c

# Generate perl.c
.if defined(PERLINTERP)
perl.c:	perl.xs typemap
	echo "#define _PATH_PERLSCRIPTS \"/usr/share/vi/perl\"" > ${.TARGET}
	$(PERL) $(PERLLIB)/ExtUtils/xsubpp -typemap \
	    $(PERLLIB)/ExtUtils/typemap $(SRCDIR)/perl_api/perl.xs >> ${.TARGET}
	($(PERL) -ne 'print "sub $$1 {\$$curscr->$$1(\@_)}\n" \
	    if /newXS\("VI::([^":]*)"/;' $@ ; echo "1;") > VI.pm

CLEANFILES+=	VI.pm perl.c
.endif

# unifdef has some *weird* exit codes, sigh!  RTFM unifdef(1)...
ex_notcl.c: ex_tcl.c
	-unifdef -UHAVE_TCL_INTERP ${SRCDIR}/ex/ex_tcl.c > ${.TARGET}

ex_noperl.c: ex_perl.c
	-unifdef -UHAVE_PERL_INTERP ${SRCDIR}/ex/ex_perl.c > ${.TARGET}

CLEANFILES+=	ex_notcl.c ex_noperl.c

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${CATALOGS:S;^;${SRCDIR}/catalog/;} \
		${DESTDIR}/usr/share/vi/catalog
	for l in ${NLLINKS}; do \
		ln -fs dutch ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs dutch ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs dutch ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs dutch ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
	done
	for l in ${ENLINKS}; do \
		ln -fs english ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs english ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs english ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs english ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
		ln -fs english ${DESTDIR}/usr/share/vi/catalog/$$l.US-ASCII; \
	done
	ln -fs english ${DESTDIR}/usr/share/vi/catalog/en_US.US-ASCII
	ln -fs english ${DESTDIR}/usr/share/vi/catalog/US-ASCII
	ln -fs english ${DESTDIR}/usr/share/vi/catalog/ASCII
	ln -fs english ${DESTDIR}/usr/share/vi/catalog/POSIX
	ln -fs english ${DESTDIR}/usr/share/vi/catalog/C
	for l in ${FRLINKS}; do \
		ln -fs french ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs french ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs french ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs french ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
	done
	for l in ${DELINKS}; do \
		ln -fs german ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs german ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs german ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs german ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
	done
	for l in ${ESLINKS}; do \
		ln -fs spanish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs spanish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs spanish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs spanish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
	done
	for l in ${SVLINKS}; do \
		ln -fs swedish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-1; \
		ln -fs swedish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-1; \
		ln -fs swedish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-15; \
		ln -fs swedish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-15; \
	done
	for l in ${PLLINKS}; do \
		ln -fs polish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO8859-2; \
		ln -fs polish ${DESTDIR}/usr/share/vi/catalog/$$l.ISO_8859-2; \
	done
	ln -fs ru_SU.KOI8-R ${DESTDIR}/usr/share/vi/catalog/ru_RU.KOI8-R
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SRCDIR}/tcl_scripts/*.tcl \
		${DESTDIR}/usr/share/vi/tcl
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SRCDIR}/perl_scripts/*.pl \
		${DESTDIR}/usr/share/vi/perl
.if defined(PERLINTERP)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 VI.pm \
		${DESTDIR}/usr/share/vi/perl
.endif

.include <bsd.prog.mk>
