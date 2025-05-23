# $FreeBSD: src/secure/lib/libcrypto/Makefile,v 1.15.2.14 2003/02/14 22:38:14 nectar Exp $
# $DragonFly: src/secure/lib/libcrypto/Makefile,v 1.4 2004/03/23 07:47:14 dillon Exp $

LIB=		crypto
SHLIB_MAJOR=	3

NOLINT=		true

.if exists(Makefile.man)
.include "Makefile.man"
.endif
.if defined(NOTYET)
MAN+=	config.5 des_modes.7
.endif

.include "Makefile.inc"

# base sources
SRCS+=	cpt_err.c cryptlib.c cversion.c ebcdic.c ex_data.c mem.c mem_clr.c \
	mem_dbg.c o_time.c tmdiff.c uid.c

# aes
SRCS+=	aes_cbc.c aes_cfb.c aes_core.c aes_ctr.c aes_ecb.c aes_misc.c aes_ofb.c

# asn1
SRCS+=	a_bitstr.c a_bool.c a_bytes.c a_d2i_fp.c a_digest.c \
	a_dup.c a_enum.c a_gentm.c a_hdr.c a_i2d_fp.c a_int.c \
	a_mbstr.c a_meth.c a_object.c a_octet.c a_print.c \
	a_set.c a_sign.c a_strex.c a_strnid.c a_time.c a_type.c \
	a_utctm.c a_utf8.c a_verify.c asn1_err.c asn1_lib.c \
	asn1_par.c asn_moid.c asn_pack.c d2i_pr.c d2i_pu.c \
	evp_asn1.c f_enum.c f_int.c f_string.c i2d_pr.c i2d_pu.c \
	n_pkey.c nsseq.c p5_pbe.c p5_pbev2.c p8_pkey.c t_bitst.c \
	t_crl.c t_pkey.c t_req.c t_spki.c t_x509.c t_x509a.c \
	tasn_dec.c tasn_enc.c tasn_fre.c tasn_new.c tasn_typ.c \
	tasn_utl.c x_algor.c x_attrib.c x_bignum.c x_crl.c \
	x_exten.c x_info.c x_long.c x_name.c x_pkey.c x_pubkey.c \
	x_req.c x_sig.c x_spki.c x_val.c x_x509.c x_x509a.c

# bf
SRCS+=	bf_cfb64.c bf_ecb.c bf_enc.c bf_ofb64.c bf_skey.c

# bio
SRCS+=	b_dump.c b_print.c b_sock.c bf_buff.c bf_lbuf.c bf_nbio.c \
	bf_null.c bio_cb.c bio_err.c bio_lib.c bss_acpt.c bss_bio.c \
	bss_conn.c bss_fd.c bss_file.c bss_log.c bss_mem.c \
	bss_null.c bss_sock.c

# bn
SRCS+=	bn_add.c bn_asm.c bn_blind.c bn_ctx.c bn_div.c bn_err.c bn_exp.c \
	bn_exp2.c bn_gcd.c bn_kron.c bn_lib.c bn_mod.c bn_mont.c \
	bn_mpi.c bn_mul.c bn_prime.c bn_print.c bn_rand.c bn_recp.c \
	bn_shift.c bn_sqr.c bn_sqrt.c bn_word.c

# buffer
SRCS+=	buf_err.c buffer.c

# cast
SRCS+=	c_cfb64.c c_ecb.c c_enc.c c_ofb64.c c_skey.c

# comp
SRCS+=	c_rle.c c_zlib.c comp_err.c comp_lib.c

# conf
SRCS+=	conf_api.c conf_def.c conf_err.c conf_lib.c conf_mall.c conf_mod.c conf_sap.c

# des
SRCS+=	cbc3_enc.c cbc_cksm.c cbc_enc.c cfb64ede.c cfb64enc.c cfb_enc.c \
	des_enc.c des_old.c des_old2.c ecb3_enc.c ecb_enc.c ede_cbcm_enc.c \
	enc_read.c enc_writ.c fcrypt.c fcrypt_b.c ofb64ede.c ofb64enc.c \
	ofb_enc.c pcbc_enc.c qud_cksm.c rand_key.c read2pwd.c \
	rpc_enc.c set_key.c str2key.c xcbc_enc.c

# dh
SRCS+=	dh_asn1.c dh_check.c dh_err.c dh_gen.c dh_key.c dh_lib.c

# dsa
SRCS+=	dsa_asn1.c dsa_err.c dsa_gen.c dsa_key.c dsa_lib.c dsa_ossl.c dsa_sign.c dsa_vrf.c

# dso
SRCS+=	dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c dso_openssl.c

# ec
SRCS+=	ec_cvt.c ec_err.c ec_lib.c ec_mult.c ecp_mont.c ecp_nist.c \
	ecp_recp.c ecp_smpl.c

# engine
SRCS+=	eng_all.c eng_cnf.c eng_ctrl.c eng_dyn.c eng_err.c eng_fat.c \
	eng_init.c eng_lib.c eng_list.c eng_openssl.c eng_pkey.c \
	eng_table.c hw_4758_cca.c hw_4758_cca_err.c hw_aep.c hw_aep_err.c \
	hw_atalla.c hw_atalla_err.c hw_cryptodev.c hw_cswift.c \
	hw_cswift_err.c hw_ncipher.c hw_ncipher_err.c hw_nuron.c \
	hw_nuron_err.c hw_sureware.c hw_sureware_err.c hw_ubsec.c \
	hw_ubsec_err.c tb_cipher.c tb_dh.c tb_digest.c tb_dsa.c tb_rand.c \
	tb_rsa.c

# err
SRCS+=	err.c err_all.c err_prn.c

# evp
SRCS+=	bio_b64.c bio_enc.c bio_md.c bio_ok.c c_all.c c_allc.c c_alld.c \
	digest.c e_aes.c e_bf.c e_cast.c e_des.c e_des3.c e_idea.c \
	e_null.c e_rc2.c e_rc4.c e_rc5.c e_xcbc_d.c encode.c evp_acnf.c \
	evp_enc.c evp_err.c evp_key.c evp_lib.c evp_pbe.c evp_pkey.c \
	m_dss.c m_dss1.c m_md2.c m_md4.c m_md5.c m_mdc2.c m_null.c \
	m_ripemd.c m_sha.c m_sha1.c names.c openbsd_hw.c p5_crpt.c \
	p5_crpt2.c p_dec.c p_enc.c p_lib.c p_open.c p_seal.c p_sign.c \
	p_verify.c

# hmac
SRCS+=	hmac.c

# idea
.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
SRCS+=	i_cbc.c i_cfb64.c i_ecb.c i_ofb64.c i_skey.c
.endif

# krb5
#SRCS+=	krb5_asn.c

# lhash
SRCS+=	lh_stats.c lhash.c

# md2
SRCS+=	md2_dgst.c md2_one.c

# md4
SRCS+=	md4_dgst.c md4_one.c

# md5
SRCS+=	md5_dgst.c md5_one.c

# mdc2
SRCS+=	mdc2_one.c mdc2dgst.c

# objects
SRCS+=	o_names.c obj_dat.c obj_err.c obj_lib.c

# ocsp
SRCS+=	ocsp_asn.c ocsp_cl.c ocsp_err.c ocsp_ext.c ocsp_ht.c \
	ocsp_lib.c ocsp_prn.c ocsp_srv.c ocsp_vfy.c

# pem
SRCS+=	pem_all.c pem_err.c pem_info.c pem_lib.c pem_oth.c pem_pk8.c \
	pem_pkey.c pem_seal.c pem_sign.c pem_x509.c pem_xaux.c

# pkcs12
SRCS+=	p12_add.c p12_asn.c p12_attr.c p12_crpt.c p12_crt.c \
	p12_decr.c p12_init.c p12_key.c p12_kiss.c p12_mutl.c \
	p12_npas.c p12_p8d.c p12_p8e.c p12_utl.c pk12err.c

# pkcs7
SRCS+=	example.c pk7_asn1.c pk7_attr.c pk7_dgst.c pk7_doit.c \
	pk7_lib.c pk7_mime.c pk7_smime.c pkcs7err.c

# rand
SRCS+=	md_rand.c rand_egd.c rand_err.c rand_lib.c rand_unix.c randfile.c

# rc2
SRCS+=	rc2_cbc.c rc2_ecb.c rc2_skey.c rc2cfb64.c rc2ofb64.c

# rc4
SRCS+=	rc4_enc.c rc4_skey.c

# rc5
SRCS+=	rc5_ecb.c rc5_enc.c rc5_skey.c rc5cfb64.c rc5ofb64.c

# ripemd
SRCS+=	rmd_dgst.c rmd_one.c

# rsa
SRCS+=	rsa_asn1.c rsa_chk.c rsa_eay.c rsa_err.c rsa_gen.c rsa_lib.c \
	rsa_none.c rsa_null.c rsa_oaep.c rsa_pk1.c rsa_saos.c \
	rsa_sign.c rsa_ssl.c

# sha
SRCS+=	sha1_one.c sha1dgst.c sha_dgst.c sha_one.c

# stack
SRCS+=	stack.c

# threads
SRCS+=	th-lock.c

# txt_db
SRCS+=	txt_db.c

# ui
SRCS+=	ui_compat.c ui_err.c ui_lib.c ui_openssl.c ui_util.c

# x509
SRCS+=	by_dir.c by_file.c x509_att.c x509_cmp.c x509_d2.c \
	x509_def.c x509_err.c x509_ext.c x509_lu.c x509_obj.c \
	x509_r2x.c x509_req.c x509_set.c x509_trs.c x509_txt.c \
	x509_v3.c x509_vfy.c x509cset.c x509name.c x509rset.c \
	x509spki.c x509type.c x_all.c

# x509v3
SRCS+=	v3_akey.c v3_akeya.c v3_alt.c v3_bcons.c v3_bitst.c \
	v3_conf.c v3_cpols.c v3_crld.c v3_enum.c v3_extku.c \
	v3_genn.c v3_ia5.c v3_info.c v3_int.c v3_lib.c v3_ocsp.c \
	v3_pku.c v3_prn.c v3_purp.c v3_skey.c v3_sxnet.c v3_utl.c v3err.c

INCS=		${HDRS} openssl/evp.h openssl/opensslconf.h
INCSDIR=	${INCLUDEDIR}/openssl

.include <bsd.lib.mk>

.if defined(MAKE_IDEA) && ${MAKE_IDEA} == YES
_ideapath=	${LCRYPTO_SRC}/crypto/idea
.endif

.PATH: \
	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/aes \
	${LCRYPTO_SRC}/crypto/asn1 \
	${LCRYPTO_SRC}/crypto/bf \
	${LCRYPTO_SRC}/crypto/bio \
	${LCRYPTO_SRC}/crypto/bn \
	${LCRYPTO_SRC}/crypto/buffer \
	${LCRYPTO_SRC}/crypto/cast \
	${LCRYPTO_SRC}/crypto/comp \
	${LCRYPTO_SRC}/crypto/conf \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/dh \
	${LCRYPTO_SRC}/crypto/dsa \
	${LCRYPTO_SRC}/crypto/dso \
	${LCRYPTO_SRC}/crypto/ec \
	${LCRYPTO_SRC}/crypto/engine \
	${LCRYPTO_SRC}/crypto/err \
	${LCRYPTO_SRC}/crypto/evp \
	${LCRYPTO_SRC}/crypto/hmac \
	${_ideapath} \
	${LCRYPTO_SRC}/crypto/krb5 \
	${LCRYPTO_SRC}/crypto/lhash \
	${LCRYPTO_SRC}/crypto/md2 \
	${LCRYPTO_SRC}/crypto/md4 \
	${LCRYPTO_SRC}/crypto/md5 \
	${LCRYPTO_SRC}/crypto/mdc2 \
	${LCRYPTO_SRC}/crypto/objects \
	${LCRYPTO_SRC}/crypto/ocsp \
	${LCRYPTO_SRC}/crypto/pem \
	${LCRYPTO_SRC}/crypto/pkcs12 \
	${LCRYPTO_SRC}/crypto/pkcs7 \
	${LCRYPTO_SRC}/crypto/rand \
	${LCRYPTO_SRC}/crypto/rc2 \
	${LCRYPTO_SRC}/crypto/rc4 \
	${LCRYPTO_SRC}/crypto/rc5 \
	${LCRYPTO_SRC}/crypto/ripemd \
	${LCRYPTO_SRC}/crypto/rsa \
	${LCRYPTO_SRC}/crypto/sha \
	${LCRYPTO_SRC}/crypto/stack \
	${LCRYPTO_SRC}/crypto/threads \
	${LCRYPTO_SRC}/crypto/txt_db \
	${LCRYPTO_SRC}/crypto/ui \
	${LCRYPTO_SRC}/crypto/x509 \
	${LCRYPTO_SRC}/crypto/x509v3 \
	${LCRYPTO_SRC} \
	${.CURDIR}/man
