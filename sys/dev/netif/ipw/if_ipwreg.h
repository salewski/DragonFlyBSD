/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_ipwreg.h,v 1.2.2.1 2005/01/13 20:01:04 damien Exp $
 * $DragonFly: src/sys/dev/netif/ipw/Attic/if_ipwreg.h,v 1.1 2005/03/06 18:25:12 dillon Exp $
 */

#define IPW_NTBD	128
#define IPW_TBD_SZ	(IPW_NTBD * sizeof (struct ipw_bd))
#define IPW_NDATA	(IPW_NTBD / 2)
#define IPW_NRBD	128
#define IPW_RBD_SZ	(IPW_NRBD * sizeof (struct ipw_bd))
#define IPW_STATUS_SZ	(IPW_NRBD * sizeof (struct ipw_status))

#define IPW_CSR_INTR		0x0008
#define IPW_CSR_INTR_MASK	0x000c
#define IPW_CSR_INDIRECT_ADDR	0x0010
#define IPW_CSR_INDIRECT_DATA	0x0014
#define IPW_CSR_AUTOINC_ADDR	0x0018
#define IPW_CSR_AUTOINC_DATA	0x001c
#define IPW_CSR_RST		0x0020
#define IPW_CSR_CTL		0x0024
#define IPW_CSR_IO		0x0030
#define IPW_CSR_TX_BD_BASE	0x0200
#define IPW_CSR_TX_BD_SIZE	0x0204
#define IPW_CSR_RX_BD_BASE	0x0240
#define IPW_CSR_RX_STATUS_BASE	0x0244
#define IPW_CSR_RX_BD_SIZE	0x0248
#define IPW_CSR_TX_READ_INDEX	0x0280
#define IPW_CSR_RX_READ_INDEX	0x02a0
#define IPW_CSR_TABLE1_BASE	0x0380
#define IPW_CSR_TABLE2_BASE	0x0384
#define IPW_CSR_TX_WRITE_INDEX	0x0f80
#define IPW_CSR_RX_WRITE_INDEX	0x0fa0

/* possible flags for register IPW_CSR_INTR */
#define IPW_INTR_TX_TRANSFER	0x00000001
#define IPW_INTR_RX_TRANSFER	0x00000002
#define IPW_INTR_STATUS_CHANGE	0x00000010
#define IPW_INTR_COMMAND_DONE	0x00010000
#define IPW_INTR_FW_INIT_DONE	0x01000000
#define IPW_INTR_FATAL_ERROR	0x40000000
#define IPW_INTR_PARITY_ERROR	0x80000000

#define IPW_INTR_MASK							\
	(IPW_INTR_TX_TRANSFER | IPW_INTR_RX_TRANSFER |			\
	 IPW_INTR_STATUS_CHANGE | IPW_INTR_COMMAND_DONE |		\
	 IPW_INTR_FW_INIT_DONE | IPW_INTR_FATAL_ERROR |			\
	 IPW_INTR_PARITY_ERROR)

/* possible flags for register IPW_CSR_RST */
#define IPW_RST_PRINCETON_RESET	0x00000001
#define IPW_RST_SW_RESET	0x00000080
#define IPW_RST_MASTER_DISABLED	0x00000100
#define IPW_RST_STOP_MASTER	0x00000200

/* possible flags for register IPW_CSR_CTL */
#define IPW_CTL_CLOCK_READY	0x00000001
#define IPW_CTL_ALLOW_STANDBY	0x00000002
#define IPW_CTL_INIT		0x00000004

/* possible flags for register IPW_CSR_IO */
#define IPW_IO_GPIO1_ENABLE	0x00000008
#define IPW_IO_GPIO1_MASK	0x0000000c
#define IPW_IO_GPIO3_MASK	0x000000c0
#define IPW_IO_LED_OFF		0x00002000
#define IPW_IO_RADIO_DISABLED	0x00010000

#define IPW_STATE_ASSOCIATED		0x0004
#define IPW_STATE_ASSOCIATION_LOST	0x0008
#define IPW_STATE_SCAN_COMPLETE		0x0020
#define IPW_STATE_RADIO_DISABLED	0x0100
#define IPW_STATE_DISABLED		0x0200
#define IPW_STATE_SCANNING		0x0800

/* table1 offsets */
#define IPW_INFO_LOCK			480
#define IPW_INFO_APS_CNT		604
#define IPW_INFO_APS_BASE		608
#define IPW_INFO_CARD_DISABLED		628
#define IPW_INFO_CURRENT_CHANNEL	756
#define IPW_INFO_CURRENT_TX_RATE	768

/* table2 offsets */
#define IPW_INFO_CURRENT_SSID	48
#define IPW_INFO_CURRENT_BSSID	112

/* supported rates */
#define IPW_RATE_DS1	1
#define IPW_RATE_DS2	2
#define IPW_RATE_DS5	4
#define IPW_RATE_DS11	8

/* firmware binary image header */
struct ipw_firmware_hdr {
	u_int32_t	version;
	u_int32_t	main_size;	/* firmware size */
	u_int32_t	ucode_size;	/* microcode size */
} __packed;

/* buffer descriptor */
struct ipw_bd {
	u_int32_t	physaddr;
	u_int32_t	len;
	u_int8_t	flags;
#define IPW_BD_FLAG_TX_FRAME_802_3		0x00
#define IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT	0x01
#define IPW_BD_FLAG_TX_FRAME_COMMAND		0x02
#define IPW_BD_FLAG_TX_FRAME_802_11		0x04
#define IPW_BD_FLAG_TX_LAST_FRAGMENT		0x08
	u_int8_t	nfrag;	/* number of fragments */
	u_int8_t	reserved[6];
} __packed;

/* status */
struct ipw_status {
	u_int32_t	len;
	u_int16_t	code;
#define IPW_STATUS_CODE_COMMAND		0
#define IPW_STATUS_CODE_NEWSTATE	1
#define IPW_STATUS_CODE_DATA_802_11	2
#define IPW_STATUS_CODE_DATA_802_3	3
#define IPW_STATUS_CODE_NOTIFICATION	4
	u_int8_t	flags;
#define IPW_STATUS_FLAG_DECRYPTED	0x01
#define IPW_STATUS_FLAG_WEP_ENCRYPTED	0x02
	u_int8_t	rssi;	/* received signal strength indicator */
} __packed;

/* data header */
struct ipw_hdr {
	u_int32_t	type;
#define IPW_HDR_TYPE_SEND	33
	u_int32_t	subtype;
	u_int8_t	encrypted;
	u_int8_t	encrypt;
	u_int8_t	keyidx;
	u_int8_t	keysz;
	u_int8_t	key[IEEE80211_KEYBUF_SIZE];
	u_int8_t	reserved[10];
	u_int8_t	src_addr[IEEE80211_ADDR_LEN];
	u_int8_t	dst_addr[IEEE80211_ADDR_LEN];
	u_int16_t	fragmentsz;
} __packed;

/* command */
struct ipw_cmd {
	u_int32_t	type;
#define IPW_CMD_ENABLE				2
#define IPW_CMD_SET_CONFIGURATION		6
#define IPW_CMD_SET_ESSID			8
#define IPW_CMD_SET_MANDATORY_BSSID		9
#define IPW_CMD_SET_MAC_ADDRESS			11
#define IPW_CMD_SET_MODE			12
#define IPW_CMD_SET_CHANNEL			14
#define IPW_CMD_SET_RTS_THRESHOLD		15
#define IPW_CMD_SET_FRAG_THRESHOLD		16
#define IPW_CMD_SET_POWER_MODE			17
#define IPW_CMD_SET_TX_RATES			18
#define IPW_CMD_SET_BASIC_TX_RATES		19
#define IPW_CMD_SET_WEP_KEY			20
#define IPW_CMD_SET_WEP_KEY_INDEX		25
#define IPW_CMD_SET_WEP_FLAGS			26
#define IPW_CMD_ADD_MULTICAST			27
#define IPW_CMD_SET_BEACON_INTERVAL		29
#define IPW_CMD_SET_TX_POWER_INDEX		36
#define IPW_CMD_BROADCAST_SCAN			43
#define IPW_CMD_DISABLE				44
#define IPW_CMD_SET_DESIRED_BSSID		45
#define IPW_CMD_SET_SCAN_OPTIONS		46
#define IPW_CMD_PREPARE_POWER_DOWN		58
#define IPW_CMD_DISABLE_PHY			61
#define IPW_CMD_SET_SECURITY_INFORMATION	67
	u_int32_t	subtype;
	u_int32_t	seq;
	u_int32_t	len;
	u_int8_t	data[400];
	u_int32_t	status;
	u_int8_t	reserved[68];
} __packed;

/* possible values for command IPW_CMD_SET_POWER_MODE */
#define IPW_POWER_MODE_CAM	0
#define IPW_POWER_AUTOMATIC	6

/* possible values for command IPW_CMD_SET_MODE */
#define IPW_MODE_BSS		0
#define IPW_MODE_IBSS		1
#define IPW_MODE_MONITOR	2

/* possible flags for command IPW_CMD_SET_WEP_FLAGS */
#define IPW_WEPON	0x8

/* structure for command IPW_CMD_SET_WEP_KEY */
struct ipw_wep_key {
	u_int8_t	idx;
	u_int8_t	len;
	u_int8_t	key[13];
} __packed;

/* structure for command IPW_CMD_SET_SECURITY_INFORMATION */
struct ipw_security {
	u_int32_t	ciphers;
#define IPW_CIPHER_NONE		0x00000001
#define IPW_CIPHER_WEP40	0x00000002
#define IPW_CIPHER_WEP104	0x00000020
	u_int16_t	reserved1;
	u_int8_t	authmode;
#define IPW_AUTH_OPEN	0
#define IPW_AUTH_SHARED	1
	u_int16_t	reserved2;
} __packed;

/* structure for command IPW_CMD_SET_SCAN_OPTIONS */
struct ipw_scan_options {
	u_int32_t	flags;
#define IPW_SCAN_DO_NOT_ASSOCIATE	0x00000001
#define IPW_SCAN_PASSIVE		0x00000008
	u_int32_t	channels;
} __packed;

/* structure for command IPW_CMD_SET_CONFIGURATION */
struct ipw_configuration {
	u_int32_t	flags;
#define IPW_CFG_PROMISCUOUS	0x00000004
#define IPW_CFG_PREAMBLE_AUTO	0x00000010
#define IPW_CFG_IBSS_AUTO_START	0x00000020
#define IPW_CFG_802_1x_ENABLE	0x00004000
#define IPW_CFG_BSS_MASK	0x00008000
#define IPW_CFG_IBSS_MASK	0x00010000
	u_int32_t	bss_chan;
	u_int32_t	ibss_chan;
} __packed;

/* element in AP table */
struct ipw_node {
	u_int32_t	reserved1[2];
	u_int8_t	bssid[IEEE80211_ADDR_LEN];
	u_int8_t	chan;
	u_int8_t	rates;
	u_int16_t	reserved2;
	u_int16_t	capinfo;
	u_int16_t	reserved3;
	u_int16_t	intval;
	u_int8_t	reserved4[28];
	u_int8_t	essid[IEEE80211_NWID_LEN];
	u_int16_t	reserved5;
	u_int8_t	esslen;
	u_int8_t	reserved6[7];
	u_int8_t	rssi;
} __packed;

/* EEPROM = Electrically Erasable Programmable Read-Only Memory */

#define IPW_MEM_EEPROM_CTL	0x00300040

#define IPW_EEPROM_RADIO        0x11
#define IPW_EEPROM_MAC		0x21
#define IPW_EEPROM_CHANNEL_LIST	0x37

#define IPW_EEPROM_DELAY	1	/* minimum hold time (microsecond) */

#define IPW_EEPROM_C	(1 << 0)	/* Serial Clock */
#define IPW_EEPROM_S	(1 << 1)	/* Chip Select */
#define IPW_EEPROM_D	(1 << 2)	/* Serial data input */
#define IPW_EEPROM_Q	(1 << 4)	/* Serial data output */

#define IPW_EEPROM_SHIFT_D	2
#define IPW_EEPROM_SHIFT_Q	4

/*
 * control and status registers access macros
 */
#define CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define CSR_WRITE_MULTI_1(sc, reg, buf, len)				\
	bus_space_write_multi_1((sc)->sc_st, (sc)->sc_sh, (reg), 	\
	    (buf), (len))

/*
 * indirect memory space access macros
 */
#define MEM_WRITE_1(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IPW_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_1((sc), IPW_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_2(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IPW_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_2((sc), IPW_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_4(sc, addr, val) do {					\
	CSR_WRITE_4((sc), IPW_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_4((sc), IPW_CSR_INDIRECT_DATA, (val));		\
} while (/* CONSTCOND */0)

#define MEM_WRITE_MULTI_1(sc, addr, buf, len) do {			\
	CSR_WRITE_4((sc), IPW_CSR_INDIRECT_ADDR, (addr));		\
	CSR_WRITE_MULTI_1((sc), IPW_CSR_INDIRECT_DATA, (buf), (len));	\
} while (/* CONSTCOND */0)

/*
 * EEPROM access macro
 */
#define IPW_EEPROM_CTL(sc, val) do {					\
	MEM_WRITE_4((sc), IPW_MEM_EEPROM_CTL, (val));			\
	DELAY(IPW_EEPROM_DELAY);					\
} while (0)
