# $FreeBSD: src/kerberos5/lib/libkrb5/Makefile,v 1.17 2004/02/05 18:51:49 ru Exp $
# $DragonFly: src/kerberos5/lib/libkrb5/Makefile,v 1.4 2005/01/16 14:25:46 eirikn Exp $

LIB=	krb5

INCS=	heim_err.h \
	k524_err.h \
	krb5-protos.h \
	krb5-types.h \
	krb5.h \
	krb5_err.h

MAN=	krb5.3 \
	krb5_425_conv_principal.3 \
	krb5_address.3 \
	krb5_aname_to_localname.3 \
	krb5_appdefault.3 \
	krb5_auth_context.3 \
	krb5_build_principal.3 \
	krb5_ccache.3 \
	krb5_config.3 \
	krb5_context.3 \
	krb5_create_checksum.3 \
	krb5_crypto_init.3 \
	krb5_data.3 \
	krb5_encrypt.3 \
	krb5_free_addresses.3 \
	krb5_free_principal.3 \
	krb5_get_all_client_addrs.3 \
	krb5_get_krbhst.3 \
	krb5_init_context.3 \
	krb5_keytab.3 \
	krb5_krbhst_init.3 \
	krb5_kuserok.3 \
	krb5_openlog.3 \
	krb5_parse_name.3 \
	krb5_principal_get_realm.3 \
	krb5_set_default_realm.3 \
	krb5_sname_to_principal.3 \
	krb5_timeofday.3 \
	krb5_unparse_name.3 \
	krb5_verify_user.3 \
	krb5_warn.3
MAN+=	krb5.conf.5
MAN+=	kerberos.8

MLINKS=	krb5_425_conv_principal.3 krb5_425_conv_principal_ext.3 \
	krb5_425_conv_principal.3 krb5_524_conv_principal.3 \
	krb5_address.3 krb5_addr2sockaddr.3 \
	krb5_address.3 krb5_address_compare.3 \
	krb5_address.3 krb5_address_order.3 \
	krb5_address.3 krb5_address_search.3 \
	krb5_address.3 krb5_addresses.3 \
	krb5_address.3 krb5_anyaddr.3 \
	krb5_address.3 krb5_append_addresses.3 \
	krb5_address.3 krb5_copy_address.3 \
	krb5_address.3 krb5_copy_addresses.3 \
	krb5_address.3 krb5_free_address.3 \
	krb5_address.3 krb5_free_addresses.3 \
	krb5_address.3 krb5_h_addr2addr.3 \
	krb5_address.3 krb5_h_addr2sockaddr.3 \
	krb5_address.3 krb5_make_addrport.3 \
	krb5_address.3 krb5_max_sockaddr_size.3 \
	krb5_address.3 krb5_parse_address.3 \
	krb5_address.3 krb5_print_address.3 \
	krb5_address.3 krb5_sockaddr2address.3 \
	krb5_address.3 krb5_sockaddr2port.3 \
	krb5_address.3 krb5_sockaddr_uninteresting.3 \
	krb5_appdefault.3 krb5_appdefault_boolean.3 \
	krb5_appdefault.3 krb5_appdefault_string.3 \
	krb5_appdefault.3 krb5_appdefault_time.3 \
	krb5_auth_context.3 krb5_auth_con_free.3 \
	krb5_auth_context.3 krb5_auth_con_genaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_getaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_getflags.3 \
	krb5_auth_context.3 krb5_auth_con_getkey.3 \
	krb5_auth_context.3 krb5_auth_con_getlocalsubkey.3 \
	krb5_auth_context.3 krb5_auth_con_getrcache.3 \
	krb5_auth_context.3 krb5_auth_con_getremotesubkey.3 \
	krb5_auth_context.3 krb5_auth_con_getuserkey.3 \
	krb5_auth_context.3 krb5_auth_con_init.3 \
	krb5_auth_context.3 krb5_auth_con_initivector.3 \
	krb5_auth_context.3 krb5_auth_con_setaddrs.3 \
	krb5_auth_context.3 krb5_auth_con_setaddrs_from_fd.3 \
	krb5_auth_context.3 krb5_auth_con_setflags.3 \
	krb5_auth_context.3 krb5_auth_con_setivector.3 \
	krb5_auth_context.3 krb5_auth_con_setkey.3 \
	krb5_auth_context.3 krb5_auth_con_setlocalsubkey.3 \
	krb5_auth_context.3 krb5_auth_con_setrcache.3 \
	krb5_auth_context.3 krb5_auth_con_setremotesubkey.3 \
	krb5_auth_context.3 krb5_auth_con_setuserkey.3 \
	krb5_auth_context.3 krb5_auth_getauthenticator.3 \
	krb5_auth_context.3 krb5_auth_getcksumtype.3 \
	krb5_auth_context.3 krb5_auth_getkeytype.3 \
	krb5_auth_context.3 krb5_auth_getlocalseqnumber.3 \
	krb5_auth_context.3 krb5_auth_getremoteseqnumber.3 \
	krb5_auth_context.3 krb5_auth_setcksumtype.3 \
	krb5_auth_context.3 krb5_auth_setkeytype.3 \
	krb5_auth_context.3 krb5_auth_setlocalseqnumber.3 \
	krb5_auth_context.3 krb5_auth_setremoteseqnumber.3 \
	krb5_build_principal.3 krb5_build_principal_ext.3 \
	krb5_build_principal.3 krb5_build_principal_va.3 \
	krb5_build_principal.3 krb5_build_principal_va_ext.3 \
	krb5_build_principal.3 krb5_make_principal.3 \
	krb5_ccache.3 krb5_cc_close.3 \
	krb5_ccache.3 krb5_cc_copy_cache.3 \
	krb5_ccache.3 krb5_cc_cursor.3 \
	krb5_ccache.3 krb5_cc_default.3 \
	krb5_ccache.3 krb5_cc_default_name.3 \
	krb5_ccache.3 krb5_cc_destroy.3 \
	krb5_ccache.3 krb5_cc_end_seq_get.3 \
	krb5_ccache.3 krb5_cc_gen_new.3 \
	krb5_ccache.3 krb5_cc_get_name.3 \
	krb5_ccache.3 krb5_cc_get_ops.3 \
	krb5_ccache.3 krb5_cc_get_principal.3 \
	krb5_ccache.3 krb5_cc_get_type.3 \
	krb5_ccache.3 krb5_cc_get_version.3 \
	krb5_ccache.3 krb5_cc_initialize.3 \
	krb5_ccache.3 krb5_cc_next_cred.3 \
	krb5_ccache.3 krb5_cc_ops.3 \
	krb5_ccache.3 krb5_cc_register.3 \
	krb5_ccache.3 krb5_cc_remove_cred.3 \
	krb5_ccache.3 krb5_cc_resolve.3 \
	krb5_ccache.3 krb5_cc_retrieve_cred.3 \
	krb5_ccache.3 krb5_cc_set_default_name.3 \
	krb5_ccache.3 krb5_cc_set_flags.3 \
	krb5_ccache.3 krb5_cc_store_cred.3 \
	krb5_ccache.3 krb5_fcc_ops.3 \
	krb5_ccache.3 krb5_mcc_ops.3 \
	krb5_config.3 krb5_config_get_bool_default.3 \
	krb5_config.3 krb5_config_get_int_default.3 \
	krb5_config.3 krb5_config_get_string_default.3 \
	krb5_config.3 krb5_config_get_time_default.3 \
	krb5_create_checksum.3 krb5_checksum_is_collision_proof.3 \
	krb5_create_checksum.3 krb5_checksum_is_keyed.3 \
	krb5_create_checksum.3 krb5_checksumsize.3 \
	krb5_create_checksum.3 krb5_verify_checksum.3 \
	krb5_crypto_init.3 krb5_crypto_destroy.3 \
	krb5_data.3 krb5_copy_data.3 \
	krb5_data.3 krb5_data_alloc.3 \
	krb5_data.3 krb5_data_copy.3 \
	krb5_data.3 krb5_data_free.3 \
	krb5_data.3 krb5_data_realloc.3 \
	krb5_data.3 krb5_data_zero.3 \
	krb5_data.3 krb5_free_data.3 \
	krb5_data.3 krb5_free_data_contents.3 \
	krb5_encrypt.3 krb5_decrypt.3 \
	krb5_encrypt.3 krb5_decrypt_EncryptedData.3 \
	krb5_encrypt.3 krb5_encrypt_EncryptedData.3 \
	krb5_get_all_client_addrs.3 krb5_get_all_server_addrs.3 \
	krb5_get_krbhst.3 krb5_free_krbhst.3 \
	krb5_get_krbhst.3 krb5_get_krb524hst.3 \
	krb5_get_krbhst.3 krb5_get_krb_admin_hst.3 \
	krb5_get_krbhst.3 krb5_get_krb_changepw_hst.3 \
	krb5_init_context.3 krb5_free_context.3 \
	krb5_keytab.3 krb5_keytab_entry.3 \
	krb5_keytab.3 krb5_kt_add_entry.3 \
	krb5_keytab.3 krb5_kt_close.3 \
	krb5_keytab.3 krb5_kt_compare.3 \
	krb5_keytab.3 krb5_kt_copy_entry_contents.3 \
	krb5_keytab.3 krb5_kt_cursor.3 \
	krb5_keytab.3 krb5_kt_default.3 \
	krb5_keytab.3 krb5_kt_default_name.3 \
	krb5_keytab.3 krb5_kt_end_seq_get.3 \
	krb5_keytab.3 krb5_kt_free_entry.3 \
	krb5_keytab.3 krb5_kt_get_entry.3 \
	krb5_keytab.3 krb5_kt_get_name.3 \
	krb5_keytab.3 krb5_kt_get_type.3 \
	krb5_keytab.3 krb5_kt_next_entry.3 \
	krb5_keytab.3 krb5_kt_ops.3 \
	krb5_keytab.3 krb5_kt_read_service_key.3 \
	krb5_keytab.3 krb5_kt_register.3 \
	krb5_keytab.3 krb5_kt_remove_entry.3 \
	krb5_keytab.3 krb5_kt_resolve.3 \
	krb5_keytab.3 krb5_kt_start_seq_get.3 \
	krb5_krbhst_init.3 krb5_krbhst_format_string.3 \
	krb5_krbhst_init.3 krb5_krbhst_free.3 \
	krb5_krbhst_init.3 krb5_krbhst_get_addrinfo.3 \
	krb5_krbhst_init.3 krb5_krbhst_next.3 \
	krb5_krbhst_init.3 krb5_krbhst_next_as_string.3 \
	krb5_krbhst_init.3 krb5_krbhst_reset.3 \
	krb5_openlog.3 krb5_addlog_dest.3 \
	krb5_openlog.3 krb5_addlog_func.3 \
	krb5_openlog.3 krb5_closelog.3 \
	krb5_openlog.3 krb5_initlog.3 \
	krb5_openlog.3 krb5_log.3 \
	krb5_openlog.3 krb5_log_msg.3 \
	krb5_openlog.3 krb5_vlog.3 \
	krb5_openlog.3 krb5_vlog_msg.3 \
	krb5_principal_get_realm.3 krb5_principal_get_comp_string.3 \
	krb5_set_default_realm.3 krb5_free_host_realm.3 \
	krb5_set_default_realm.3 krb5_get_default_realm.3 \
	krb5_set_default_realm.3 krb5_get_default_realms.3 \
	krb5_set_default_realm.3 krb5_get_host_realm.3 \
	krb5_sname_to_principal.3 krb5_sock_to_principal.3 \
	krb5_timeofday.3 krb5_us_timeofday.3 \
	krb5_verify_user.3 krb5_verify_opt_init.3 \
	krb5_verify_user.3 krb5_verify_opt_set_flags.3 \
	krb5_verify_user.3 krb5_verify_opt_set_keytab.3 \
	krb5_verify_user.3 krb5_verify_opt_set_secure.3 \
	krb5_verify_user.3 krb5_verify_opt_set_service.3 \
	krb5_verify_user.3 krb5_verify_user_lrealm.3 \
	krb5_verify_user.3 krb5_verify_user_opt.3 \
	krb5_warn.3 krb5_err.3 \
	krb5_warn.3 krb5_errx.3 \
	krb5_warn.3 krb5_set_warn_dest.3 \
	krb5_warn.3 krb5_verr.3 \
	krb5_warn.3 krb5_verrx.3 \
	krb5_warn.3 krb5_vwarn.3 \
	krb5_warn.3 krb5_vwarnx.3 \
	krb5_warn.3 krb5_warnx.3

SRCS=	acl.c \
	add_et_list.c \
	addr_families.c \
	aname_to_localname.c \
	appdefault.c \
	asn1_glue.c \
	auth_context.c \
	build_ap_req.c \
	build_auth.c \
	cache.c \
	changepw.c \
	codec.c \
	config_file.c \
	config_file_netinfo.c \
	constants.c \
	context.c \
	convert_creds.c \
	copy_host_realm.c \
	crc.c \
	creds.c \
	crypto.c \
	data.c \
	eai_to_heim_errno.c \
	error_string.c \
	expand_hostname.c \
	fcache.c \
	free.c \
	free_host_realm.c \
	generate_seq_number.c \
	generate_subkey.c \
	get_addrs.c \
	get_cred.c \
	get_default_principal.c \
	get_default_realm.c \
	get_for_creds.c \
	get_host_realm.c \
	get_in_tkt.c \
	get_in_tkt_pw.c \
	get_in_tkt_with_keytab.c \
	get_in_tkt_with_skey.c \
	get_port.c \
	heim_err.c \
	heim_err.h \
	init_creds.c \
	init_creds_pw.c \
	k524_err.c \
	k524_err.h \
	keyblock.c \
	keytab.c \
	keytab_any.c \
	keytab_file.c \
	keytab_keyfile.c \
	keytab_krb4.c \
	keytab_memory.c \
	krb5_err.c \
	krb5_err.h \
	krbhst.c \
	kuserok.c \
	log.c \
	mcache.c \
	misc.c \
	mk_error.c \
	mk_priv.c \
	mk_rep.c \
	mk_req.c \
	mk_req_ext.c \
	mk_safe.c \
	n-fold.c \
	net_read.c \
	net_write.c \
	padata.c \
	principal.c \
	prog_setup.c \
	prompter_posix.c \
	rd_cred.c \
	rd_error.c \
	rd_priv.c \
	rd_rep.c \
	rd_req.c \
	rd_safe.c \
	read_message.c \
	recvauth.c \
	replay.c \
	send_to_kdc.c \
	sendauth.c \
	set_default_realm.c \
	sock_principal.c \
	store.c \
	store_emem.c \
	store_fd.c \
	store_mem.c \
	ticket.c \
	time.c \
	transited.c \
	verify_init.c \
	verify_user.c \
	version.c \
	warn.c \
	write_message.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/krb5 ${.CURDIR}/../../include
