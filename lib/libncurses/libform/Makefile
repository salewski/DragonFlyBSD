# $DragonFly: src/lib/libncurses/libform/Makefile,v 1.3 2005/03/14 15:15:13 joerg Exp $

LIB=    form

.include "../Makefile.inc"

NCURSESFORMDIR= ${NCURSESDIR}/form ${.OBJDIR}

.PATH: ${NCURSESFORMDIR}

CFLAGS+= 	-I${NCURSESDIR}/menu

TERMINFO_CAPS=  ${NCURSESDIR}/include/Caps

ncurses_def.h:
	${NCURSESDIR}/include/MKncurses_def.sh ${NCURSESDIR}/include/ncurses_defs > ${.TARGET}
curses.h:
	cat ${.CURDIR}/../include/curses.head >$@
	${NCURSESDIR}/include/MKkey_defs.sh $(TERMINFO_CAPS) >>$@
	cat ${NCURSESDIR}/include/curses.tail >>$@

CLEANFILES=	curses.h ncurses_def.h

SRCS=	\
	ncurses_def.h \
	curses.h

SRCS+=	\
	fld_arg.c \
	fld_attr.c \
	fld_current.c \
	fld_def.c \
	fld_dup.c \
	fld_ftchoice.c \
	fld_ftlink.c \
	fld_info.c \
	fld_just.c \
	fld_link.c \
	fld_max.c \
	fld_move.c \
	fld_newftyp.c \
	fld_opts.c \
	fld_pad.c \
	fld_page.c \
	fld_stat.c \
	fld_type.c \
	fld_user.c \
	frm_cursor.c \
	frm_data.c \
	frm_def.c \
	frm_driver.c \
	frm_hook.c \
	frm_opts.c \
	frm_page.c \
	frm_post.c \
	frm_req_name.c \
	frm_scale.c \
	frm_sub.c \
	frm_user.c \
	frm_win.c \
	fty_alnum.c \
	fty_alpha.c \
	fty_enum.c \
	fty_int.c \
	fty_ipv4.c \
	fty_num.c \
	fty_regex.c

INCS=	form.h

.include <bsd.lib.mk>
