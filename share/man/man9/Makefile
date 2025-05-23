# $FreeBSD: src/share/man/man9/Makefile,v 1.60.2.26 2003/06/13 01:04:17 hmp Exp $
# $DragonFly: src/share/man/man9/Makefile,v 1.15 2004/12/22 07:12:24 hmp Exp $

MAN=	DECLARE_MODULE.9 DELAY.9 DEV_MODULE.9 KASSERT.9 MD5.9 \
	MODULE_DEPEND.9 MODULE_VERSION.9 SPLASSERT.9 SYSCALL_MODULE.9 \
	VFS.9 VFS_FHTOVP.9 VFS_INIT.9 VFS_MOUNT.9 VFS_QUOTACTL.9 \
	VFS_ROOT.9 VFS_START.9 VFS_STATFS.9 VFS_SYNC.9 VFS_UNMOUNT.9 \
	VFS_VGET.9 VFS_VPTOFH.9 VFS_CHECKEXP.9 \
	VOP_ACCESS.9 VOP_ADVLOCK.9 VOP_ATTRIB.9 \
	VOP_BWRITE.9 VOP_CREATE.9 VOP_FSYNC.9 VOP_LEASE.9 \
	VOP_GETPAGES.9 VOP_INACTIVE.9 VOP_IOCTL.9 VOP_LINK.9 \
	VOP_LOCK.9 VOP_LOOKUP.9 VOP_MMAP.9 VOP_OPENCLOSE.9 \
	VOP_PATHCONF.9 VOP_PRINT.9 VOP_RDWR.9 VOP_READDIR.9 \
	VOP_READLINK.9 VOP_REALLOCBLKS.9 VOP_REMOVE.9 VOP_RENAME.9 \
	VOP_STRATEGY.9 \
	accf_data.9 accf_http.9 \
	accept_filter.9 at_exit.9 at_fork.9 bios.9 boot.9 buf.9 bus_dma.9 \
	cd.9 copy.9 \
	crypto.9 devstat.9 \
	devtoname.9 fetch.9 \
	hash.9 \
	ieee80211.9 ieee80211_crypto.9 ieee80211_input.9 ieee80211_ioctl.9 \
	ieee80211_node.9 ieee80211_output.9 ieee80211_proto.9 \
	ieee80211_radiotap.9 \
	ifnet.9 inittodr.9 intro.9 kernacc.9 ktr.9 lock.9 \
	malloc.9 make_dev.9 mbuf.9 microseq.9 mi_switch.9 module.9 namei.9 \
	pci.9 panic.9 physio.9 posix4.9 psignal.9 \
	resettodr.9 \
	rman.9 \
	rtalloc.9 rtentry.9 sleep.9 spl.9 \
	store.9 style.9 suser.9 time.9 timeout.9 uio.9 \
	vget.9 vnode.9 vput.9 vref.9 vrele.9 vslock.9 \
	microtime.9 microuptime.9 tvtohz.9

MAN+=	device.9 device_add_child.9 device_delete_child.9 device_enable.9 \
	device_find_child.9 device_get_children.9 \
	device_get_devclass.9 device_get_driver.9 \
	device_get_ivars.9 device_get_softc.9 device_get_state.9 \
	device_get_unit.9 device_probe_and_attach.9 device_quiet.9 \
	device_set_desc.9 device_set_flags.9 driver.9 \
	devclass.9 devclass_add_driver.9 devclass_find.9 \
	devclass_get_device.9 devclass_get_devices.9 devclass_get_maxunit.9 \
	devclass_get_name.9 devclass_get_softc.9 \
	DEVICE_ATTACH.9 DEVICE_DETACH.9 DEVICE_IDENTIFY.9 DEVICE_PROBE.9 \
	DEVICE_SHUTDOWN.9 \
	DRIVER_MODULE.9 BUS_PRINT_CHILD.9 BUS_SETUP_INTR.9 \
	BUS_READ_IVAR.9 \
	bus_generic_attach.9 bus_generic_detach.9 bus_generic_map_intr.9 \
	bus_generic_print_child.9 bus_generic_read_ivar.9 \
	bus_generic_shutdown.9 \
	bus_alloc_resource.9 bus_release_resource.9 \
	VOP_ACLCHECK.9 VOP_GETACL.9 VOP_GETEXTATTR.9 VOP_SETACL.9 \
	VOP_SETEXTATTR.9 acl.9 extattr.9 \
	sbuf.9 sysctl_add_oid.9 sysctl_ctx_init.9 vfs_unmountall.9 \
	vn_fullpath.9 \
	zone.9

MLINKS+=DRIVER_MODULE.9 MULTI_DRIVER_MODULE.9
MLINKS+=KASSERT.9 KKASSERT.9
MLINKS+=MD5.9 MD5Init.9 MD5.9 MD5Transform.9
MLINKS+=VOP_ATTRIB.9 VOP_GETATTR.9
MLINKS+=VOP_ATTRIB.9 VOP_SETATTR.9
MLINKS+=VOP_CREATE.9 VOP_MKDIR.9
MLINKS+=VOP_CREATE.9 VOP_MKNOD.9
MLINKS+=VOP_CREATE.9 VOP_SYMLINK.9
MLINKS+=VOP_GETPAGES.9 VOP_PUTPAGES.9
MLINKS+=VOP_INACTIVE.9 VOP_RECLAIM.9
MLINKS+=VOP_LOCK.9 VOP_ISLOCKED.9
MLINKS+=VOP_LOCK.9 VOP_UNLOCK.9
MLINKS+=VOP_LOCK.9 vn_lock.9
MLINKS+=VOP_OPENCLOSE.9 VOP_CLOSE.9
MLINKS+=VOP_OPENCLOSE.9 VOP_OPEN.9
MLINKS+=VOP_RDWR.9 VOP_READ.9
MLINKS+=VOP_RDWR.9 VOP_WRITE.9
MLINKS+=VOP_REMOVE.9 VOP_RMDIR.9
MLINKS+=at_exit.9 rm_at_exit.9
MLINKS+=at_fork.9 rm_at_fork.9
MLINKS+=bus_alloc_resource.9 bus_alloc_resource_any.9
MLINKS+=bus_dma.9 busdma.9
MLINKS+=bus_dma.9 bus_dma_tag_create.9
MLINKS+=bus_dma.9 bus_dma_tag_destroy.9
MLINKS+=bus_dma.9 bus_dmamap_create.9
MLINKS+=bus_dma.9 bus_dmamap_destroy.9
MLINKS+=bus_dma.9 bus_dmamap_load.9
MLINKS+=bus_dma.9 bus_dmamap_load_mbuf.9
MLINKS+=bus_dma.9 bus_dmamap_load_uio.9
MLINKS+=bus_dma.9 bus_dmamap_sync.9
MLINKS+=bus_dma.9 bus_dmamap_unload.9
MLINKS+=bus_dma.9 bus_dmamem_alloc.9
MLINKS+=bus_dma.9 bus_dmamem_free.9
MLINKS+=copy.9 copyin.9 copy.9 copyinstr.9 copy.9 copyout.9 copy.9 copystr.9
MLINKS+=devstat.9 devicestat.9 devstat.9 devstat_add_entry.9
MLINKS+=devstat.9 devstat_remove_entry.9 devstat.9 devstat_start_transaction.9
MLINKS+=devstat.9 devstat_end_transaction.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuswintr.9 fetch.9 fusword.9 fetch.9 fuword.9
MLINKS+=hash.9 hashinit.9 hash.9 phashinit.9
MLINKS+=ieee80211.9 ieee80211_attach.9 \
    ieee80211.9 ieee80211_chan2ieee.9 \
	ieee80211.9 ieee80211_chan2mode.9 \
	ieee80211.9 ieee80211_ieee2mhz.9 \
	ieee80211.9 ieee80211_ifattach.9 \
	ieee80211.9 ieee80211_ifdetach.9 \
	ieee80211.9 ieee80211_media2rate.9 \
	ieee80211.9 ieee80211_media_change.9 \
	ieee80211.9 ieee80211_media_init.9 \
	ieee80211.9 ieee80211_media_status.9 \
	ieee80211.9 ieee80211_mhz2ieee.9 \
	ieee80211.9 ieee80211_rate2media.9 \
	ieee80211.9 ieee80211_setmode.9 \
	ieee80211.9 ieee80211_watchdog.9
MLINKS+=ieee80211_crypto.9 ieee80211_crypto_attach.9 \
    ieee80211_crypto.9 ieee80211_crypto_detach.9 \
	ieee80211_crypto.9 ieee80211_wep_crypt.9
MLINKS+=ieee80211_input.9 ieee80211_decap.9 \
	ieee80211_input.9 ieee80211_recv_mgmt.9
MLINKS+=ieee80211_ioctl.9 ieee80211_cfgget.9 \
	ieee80211_ioctl.9 ieee80211_cfgset.9
MLINKS+=ieee80211_node.9 ieee80211_alloc_node.9 \
    ieee80211_node.9 ieee80211_begin_scan.9 \
	ieee80211_node.9 ieee80211_create_ibss.9 \
	ieee80211_node.9 ieee80211_dup_bss.9 \
	ieee80211_node.9 ieee80211_end_scan.9 \
	ieee80211_node.9 ieee80211_find_node.9 \
	ieee80211_node.9 ieee80211_free_allnodes.9 \
	ieee80211_node.9 ieee80211_free_node.9 \
	ieee80211_node.9 ieee80211_iterate_nodes.9 \
	ieee80211_node.9 ieee80211_lookup_node.9 \
	ieee80211_node.9 ieee80211_next_scan.9 \
	ieee80211_node.9 ieee80211_node_attach.9 \
	ieee80211_node.9 ieee80211_node_detach.9 \
	ieee80211_node.9 ieee80211_node_lateattach.9 \
	ieee80211_node.9 ieee80211_timeout_nodes.9
MLINKS+=ieee80211_output.9 ieee80211_add_rates.9 \
	ieee80211_output.9 ieee80211_add_xrates.9 \
	ieee80211_output.9 ieee80211_encap.9 \
	ieee80211_output.9 ieee80211_send_mgmt.9
MLINKS+=ieee80211_proto.9 ieee80211_dump_pkt.9 \
	ieee80211_proto.9 ieee80211_fix_rate.9 \
	ieee80211_proto.9 ieee80211_print_essid.9 \
	ieee80211_proto.9 ieee80211_proto_attach.9 \
	ieee80211_proto.9 ieee80211_proto_detach.9
MLINKS+=ieee80211_radiotap.9 radiotap.9
MLINKS+=ifnet.9 if_data.9 ifnet.9 ifaddr.9 ifnet.9 ifqueue.9
MLINKS+=kernacc.9 useracc.9
MLINKS+=make_dev.9 destroy_dev.9
MLINKS+=malloc.9 FREE.9 malloc.9 MALLOC.9 malloc.9 free.9
MLINKS+=malloc.9 realloc.9 malloc.9 reallocf.9
MLINKS+=mi_switch.9 cpu_switch.9
MLINKS+=pci.9 pci_read_config.9 pci.9 pci_write_config.9
MLINKS+=pci.9 pci_enable_busmaster.9 pci.9 pci_disable_busmaster.9
MLINKS+=pci.9 pci_enable_io.9 pci.9 pci_disable_io.9
MLINKS+=pci.9 pci_get_powerstate.9 pci.9 pci_set_powerstate.9
MLINKS+=pci.9 pci_find_bsf.9 pci.9 pci_find_device.9
MLINKS+=posix4.9 p1003_1b.9
MLINKS+=psignal.9 gsignal.9 psignal.9 pgsignal.9
MLINKS+=rman.9 rman_activate_resource.9
MLINKS+=rman.9 rman_await_resource.9
MLINKS+=rman.9 rman_deactivate_resource.9
MLINKS+=rman.9 rman_fini.9
MLINKS+=rman.9 rman_fini.9
MLINKS+=rman.9 rman_get_bushandle.9
MLINKS+=rman.9 rman_get_bustag.9
MLINKS+=rman.9 rman_get_end.9
MLINKS+=rman.9 rman_get_flags.9
#MLINKS+=rman.9 rman_get_rid.9
MLINKS+=rman.9 rman_get_size.9
MLINKS+=rman.9 rman_get_start.9
MLINKS+=rman.9 rman_get_virtual.9
MLINKS+=rman.9 rman_init.9
MLINKS+=rman.9 rman_make_alignment_flags.9
MLINKS+=rman.9 rman_manage_region.9
MLINKS+=rman.9 rman_release_resource.9
MLINKS+=rman.9 rman_reserve_resource.9
#MLINKS+=rman.9 rman_reserve_resource_bound.9
MLINKS+=rman.9 rman_set_bushandle.9
MLINKS+=rman.9 rman_set_bustag.9
#MLINKS+=rman.9 rman_set_rid.9
MLINKS+=rman.9 rman_set_virtual.9
MLINKS+=rtalloc.9 rtalloc1.9 rtalloc.9 rtalloc_ign.9
MLINKS+=sleep.9 tsleep.9 sleep.9 wakeup.9 sleep.9 wakeup_one.9
MLINKS+=sleep.9 asleep.9 sleep.9 await.9
MLINKS+=buf.9 bp.9
MLINKS+=spl.9 spl0.9
MLINKS+=spl.9 splbio.9 spl.9 splclock.9 spl.9 splhigh.9 spl.9 splimp.9
MLINKS+=spl.9 splnet.9 spl.9 splsoftclock.9 spl.9 splsofttty.9
MLINKS+=spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splvm.9 spl.9 splx.9
MLINKS+=store.9 subyte.9 store.9 suswintr.9 store.9 susword.9 store.9 suword.9
MLINKS+=suser.9 suser_xxx.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 untimeout.9
MLINKS+=timeout.9 callout_handle_init.9 timeout.9 callout_init.9
MLINKS+=timeout.9 callout_stop.9 timeout.9 callout_reset.9
MLINKS+=uio.9 uiomove.9
MLINKS+=vref.9 VREF.9
MLINKS+=vslock.9 vsunlock.9

MLINKS+=device_add_child.9 device_add_child_ordered.9
MLINKS+=device_enable.9 device_disable.9
MLINKS+=device_enable.9 device_is_enabled.9
MLINKS+=device_get_state.9 device_busy.9
MLINKS+=device_get_state.9 device_unbusy.9
MLINKS+=device_get_state.9 device_is_alive.9
MLINKS+=device_quiet.9 device_verbose.9
MLINKS+=device_quiet.9 device_is_quiet.9
MLINKS+=device_set_desc.9 device_get_desc.9
MLINKS+=device_set_desc.9 device_set_desc_copy.9
MLINKS+=device_set_flags.9 device_get_flags.9
MLINKS+=devclass_add_driver.9 devclass_delete_driver.9
MLINKS+=devclass_add_driver.9 devclass_find_driver.9
MLINKS+=BUS_READ_IVAR.9 BUS_WRITE_IVAR.9
MLINKS+=bus_generic_read_ivar.9 bus_generic_write_ivar.9

MLINKS+=lock.9 lockcount.9
MLINKS+=lock.9 lockcountnb.9
MLINKS+=lock.9 lockinit.9
MLINKS+=lock.9 lockmgr.9
MLINKS+=lock.9 lockmgr_printinfo.9
MLINKS+=lock.9 lockstatus.9

MLINKS+=microtime.9 getmicrotime.9 microtime.9 nanotime.9
MLINKS+=microtime.9 getnanotime.9
MLINKS+=microuptime.9 getmicrouptime.9 microuptime.9 nanouptime.9
MLINKS+=microuptime.9 getnanouptime.9

MLINKS+=sysctl_add_oid.9 sysctl_remove_oid.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_OID.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_NODE.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_STRING.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_INT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_UINT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_LONG.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_ULONG.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_OPAQUE.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_STRUCT.9
MLINKS+=sysctl_add_oid.9 SYSCTL_ADD_PROC.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_free.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_add.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_del.9
MLINKS+=sysctl_ctx_init.9 sysctl_ctx_entry_find.9

MLINKS+=sbuf.9 sbuf_new.9
MLINKS+=sbuf.9 sbuf_clear.9
MLINKS+=sbuf.9 sbuf_setpos.9
MLINKS+=sbuf.9 sbuf_cat.9
MLINKS+=sbuf.9 sbuf_cpy.9
MLINKS+=sbuf.9 sbuf_printf.9
MLINKS+=sbuf.9 sbuf_vprintf.9
MLINKS+=sbuf.9 sbuf_putc.9
MLINKS+=sbuf.9 sbuf_trim.9
MLINKS+=sbuf.9 sbuf_overflowed.9
MLINKS+=sbuf.9 sbuf_finish.9
MLINKS+=sbuf.9 sbuf_data.9
MLINKS+=sbuf.9 sbuf_len.9
MLINKS+=sbuf.9 sbuf_delete.9

MLINKS+=zone.9 zalloc.9
MLINKS+=zone.9 zfree.9
MLINKS+=zone.9 zinit.9
MLINKS+=zone.9 zinitna.9
MLINKS+=zone.9 zbootinit.9

.include <bsd.prog.mk>
