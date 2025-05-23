#
# SSLeay/fips/aes/Makefile
#

DIR=	aes
TOP=	../..
CC=	cc
INCLUDES=
CFLAG=-g
INSTALL_PREFIX=
OPENSSLDIR=     /usr/local/ssl
INSTALLTOP=/usr/local/ssl
MAKEDEPPROG=	makedepend
MAKEDEPEND=	$(TOP)/util/domd $(TOP) -MD $(MAKEDEPPROG)
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=fips_aesavs.c
TESTDATA=fips_aes_data
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=fips_aes_core.c fips_aes_selftest.c
LIBOBJ=fips_aes_core.o fips_aes_selftest.o

SRC= $(LIBSRC)

EXHEADER=
HEADER=	$(EXHEADER) fips_aes_locl.h

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd $(TOP); $(MAKE) DIRS=fips FDIRS=$(DIR) sub_all)

all:	check lib

check:
	TOP=`pwd`/$(TOP) ../fips_check_sha1 fingerprint.sha1 $(SRC) $(HEADER)

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@sleep 2; touch lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/test $(TESTDATA)
	@$(PERL) $(TOP)/util/mklink.pl $(TOP)/apps $(APPS)

install:
	@if test -n "$(EXHEADER)"; then \
	  for i in $(EXHEADER) ; \
	  do  \
	    (cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	    chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	  done; \
	fi

tags:
	ctags $(SRC)

tests:

top_fips_aesavs:
	(cd $(TOP); $(MAKE) DIRS=fips FDIRS=$(DIR) TARGET=fips_aesavs sub_target)

fips_aesavs: fips_aesavs.o $(TOP)/libcrypto.a
	$(CC) $(CFLAGS) -o fips_aesavs fips_aesavs.o $(PEX_LIBS) $(TOP)/libcrypto.a $(EX_LIBS)
	TOP=$(TOP) $(TOP)/fips/openssl_fips_fingerprint $(TOP)/libcrypto.a fips_aesavs

fips_test: top top_fips_aesavs
	find ../testvectors/aes/req -name '*.req' > testlist
	-rm -rf ../testvectors/aes/rsp
	mkdir ../testvectors/aes/rsp
	./fips_aesavs -d testlist

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) \
		$(SRC) $(TEST)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff
# DO NOT DELETE THIS LINE -- make depend depends on it.

fips_aes_core.o: ../../include/openssl/aes.h ../../include/openssl/e_os2.h
fips_aes_core.o: ../../include/openssl/fips.h
fips_aes_core.o: ../../include/openssl/opensslconf.h fips_aes_core.c
fips_aes_core.o: fips_aes_locl.h
fips_aes_selftest.o: ../../include/openssl/aes.h ../../include/openssl/bio.h
fips_aes_selftest.o: ../../include/openssl/crypto.h
fips_aes_selftest.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_aes_selftest.o: ../../include/openssl/fips.h ../../include/openssl/lhash.h
fips_aes_selftest.o: ../../include/openssl/opensslconf.h
fips_aes_selftest.o: ../../include/openssl/opensslv.h
fips_aes_selftest.o: ../../include/openssl/safestack.h
fips_aes_selftest.o: ../../include/openssl/stack.h
fips_aes_selftest.o: ../../include/openssl/symhacks.h fips_aes_selftest.c
fips_aesavs.o: ../../e_os.h ../../include/openssl/aes.h
fips_aesavs.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
fips_aesavs.o: ../../include/openssl/blowfish.h ../../include/openssl/bn.h
fips_aesavs.o: ../../include/openssl/cast.h ../../include/openssl/crypto.h
fips_aesavs.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
fips_aesavs.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
fips_aesavs.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
fips_aesavs.o: ../../include/openssl/evp.h ../../include/openssl/fips.h
fips_aesavs.o: ../../include/openssl/idea.h ../../include/openssl/lhash.h
fips_aesavs.o: ../../include/openssl/md2.h ../../include/openssl/md4.h
fips_aesavs.o: ../../include/openssl/md5.h ../../include/openssl/mdc2.h
fips_aesavs.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
fips_aesavs.o: ../../include/openssl/opensslconf.h
fips_aesavs.o: ../../include/openssl/opensslv.h
fips_aesavs.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rc2.h
fips_aesavs.o: ../../include/openssl/rc4.h ../../include/openssl/rc5.h
fips_aesavs.o: ../../include/openssl/ripemd.h ../../include/openssl/rsa.h
fips_aesavs.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
fips_aesavs.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
fips_aesavs.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
fips_aesavs.o: fips_aesavs.c
