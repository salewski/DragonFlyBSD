# $Id: Makefile.am,v 1.53.4.2 2003/10/14 16:13:14 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += -I../asn1 -I$(srcdir)/../asn1 $(INCLUDE_des)

BUILT_SOURCES = asn1_Key.c asn1_Event.c asn1_HDBFlags.c asn1_hdb_entry.c \
	asn1_Salt.c hdb_err.c hdb_err.h asn1_GENERATION.c

foo = asn1_Key.x asn1_GENERATION.x asn1_Event.x asn1_HDBFlags.x asn1_hdb_entry.x asn1_Salt.x

CLEANFILES = $(BUILT_SOURCES) $(foo) hdb_asn1.h asn1_files

noinst_PROGRAMS = convert_db
LDADD = libhdb.la \
	$(LIB_openldap) \
	../krb5/libkrb5.la \
	../asn1/libasn1.la \
	$(LIB_des) \
	$(LIB_roken)

lib_LTLIBRARIES = libhdb.la
libhdb_la_LDFLAGS = -version-info 7:7:0

libhdb_la_SOURCES =				\
	common.c				\
	db.c					\
	db3.c					\
	hdb-ldap.c				\
	hdb.c					\
	keytab.c				\
	mkey.c					\
	ndbm.c					\
	print.c					\
	$(BUILT_SOURCES)

INCLUDES += $(INCLUDE_openldap)

include_HEADERS = hdb.h hdb_err.h hdb_asn1.h hdb-protos.h hdb-private.h

libhdb_la_LIBADD = ../krb5/libkrb5.la ../asn1/libasn1.la ../roken/libroken.la $(LIB_openldap) $(DBLIB) $(LIB_NDBM)

$(libhdb_la_OBJECTS): $(srcdir)/hdb-protos.h $(srcdir)/hdb-private.h

$(srcdir)/hdb-protos.h:
	cd $(srcdir); perl ../../cf/make-proto.pl -q -P comment -o hdb-protos.h $(libhdb_la_SOURCES) || rm -f hdb-protos.h

$(srcdir)/hdb-private.h:
	cd $(srcdir); perl ../../cf/make-proto.pl -q -P comment -p hdb-private.h $(libhdb_la_SOURCES) || rm -f hdb-private.h

$(foo) hdb_asn1.h: asn1_files

asn1_files: ../asn1/asn1_compile$(EXEEXT) $(srcdir)/hdb.asn1
	../asn1/asn1_compile$(EXEEXT) $(srcdir)/hdb.asn1 hdb_asn1

$(libhdb_la_OBJECTS): hdb_asn1.h hdb_err.h

$(convert_db_OBJECTS): hdb_asn1.h hdb_err.h

# to help stupid solaris make

hdb_err.h: hdb_err.et
