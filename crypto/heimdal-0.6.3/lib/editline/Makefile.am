# $Id: Makefile.am,v 1.13 2002/08/13 13:48:15 joda Exp $

include $(top_srcdir)/Makefile.am.common

if do_roken_rename
ES = snprintf.c strdup.c
endif

INCLUDES += $(ROKEN_RENAME)

man_MANS = editline.3

lib_LTLIBRARIES = libeditline.la
if el_compat
noinst_LTLIBRARIES = libel_compat.la
else
noinst_LTLIBRARIES =
endif

noinst_PROGRAMS = testit

CHECK_LOCAL =

testit_LDADD = \
	libeditline.la \
	$(LIB_tgetent) \
	$(LIB_roken)

include_HEADERS = editline.h

libeditline_la_SOURCES = \
	complete.c \
	editline.c \
	sysunix.c \
	editline.h \
	roken_rename.h \
	unix.h \
	$(EXTRA_SOURCE)

libeditline_la_LDFLAGS = -static

EXTRA_SOURCE = $(ES) 

libel_compat_la_SOURCES = edit_compat.c

libel_compat_la_LDFLAGS = -static

EXTRA_DIST = $(man_MANS)

snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
strdup.c:
	$(LN_S) $(srcdir)/../roken/strdup.c .
