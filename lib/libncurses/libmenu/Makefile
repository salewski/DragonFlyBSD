# $DragonFly: src/lib/libncurses/libmenu/Makefile,v 1.3 2005/03/14 15:15:13 joerg Exp $

LIB=    menu

.include "../Makefile.inc"

NCURSESMENUDIR= ${NCURSESDIR}/menu ${.OBJDIR}

.PATH: ${NCURSESMENUDIR}

CFLAGS+= 	-I${NCURSESDIR}/menu

TERMINFO_CAPS=  ${NCURSESDIR}/include/Caps

ncurses_def.h:
	${NCURSESDIR}/include/MKncurses_def.sh ${NCURSESDIR}/include/ncurses_defs > ${.TARGET}
curses.h:
	cat ${.CURDIR}/../include/curses.head >$@
	${NCURSESDIR}/include/MKkey_defs.sh $(TERMINFO_CAPS) >>$@
	cat ${NCURSESDIR}/include/curses.tail >>$@

CLEANFILES=     curses.h ncurses_def.h

SRCS=   \
	ncurses_def.h \
	curses.h

SRCS+=	\
	m_attribs.c \
	m_cursor.c \
	m_driver.c \
	m_format.c \
	m_global.c \
	m_hook.c \
	m_item_cur.c \
	m_item_nam.c \
	m_item_new.c \
	m_item_opt.c \
	m_item_top.c \
	m_item_use.c \
	m_item_val.c \
	m_item_vis.c \
	m_items.c \
	m_new.c \
	m_opts.c \
	m_pad.c \
	m_pattern.c \
	m_post.c \
	m_req_name.c \
	m_scale.c \
	m_spacing.c \
	m_sub.c \
	m_userptr.c \
	m_win.c

INCS=	menu.h eti.h

.include <bsd.lib.mk>
