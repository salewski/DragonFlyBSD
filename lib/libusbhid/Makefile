#	$NetBSD: Makefile,v 1.5 1999/07/23 09:44:38 mrg Exp $
#	$FreeBSD: src/lib/libusbhid/Makefile,v 1.6 2002/03/28 18:44:24 joe Exp $
#	$DragonFly: src/lib/libusbhid/Makefile,v 1.3 2004/01/31 06:56:39 dillon Exp $

LIB=	usbhid
MAN=	usbhid.3

SHLIB_MAJOR= 0
SHLIB_MINOR= 0

MLINKS=	usbhid.3 libusbhid.3 usbhid.3 hid_get_report_desc.3 \
	usbhid.3 hid_dispose_report_desc.3 \
	usbhid.3 hid_start_parse.3 usbhid.3 hid_end_parse.3 \
	usbhid.3 hid_get_item.3 usbhid.3 hid_report_size.3 \
	usbhid.3 hid_locate.3 \
	usbhid.3 hid_usage_page.3 usbhid.3 hid_usage_in_page.3 \
	usbhid.3 hid_init.3 \
	usbhid.3 hid_get_data.3 usbhid.3 hid_set_data.3

SRCS=	descr.c parse.c usage.c data.c

INCS=	libusbhid.h

.include <bsd.lib.mk>

