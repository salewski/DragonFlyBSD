/*
 * Copyright (c) 2004, 2005
 *	Damien Bergamini <damien.bergamini@free.fr>.
 * Copyright (c) 2004, 2005
 *	Andrew Atrens <atrens@nortelnetworks.com>.
 *
 * All rights reserved.
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
 * $DragonFly: src/usr.sbin/iwicontrol/iwicontrol.c,v 1.1 2005/03/06 05:02:03 dillon Exp $
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define SIOCSLOADFW	 _IOW('i', 137, struct ifreq)
#define SIOCSLOADIBSSFW	 _IOW('i', 138, struct ifreq)
#define SIOCSKILLFW	 _IOW('i', 139, struct ifreq)

struct firmware {
	char	*boot;
	int	boot_size;
	char	*ucode;
	int	ucode_size;
	char	*main;
	int	main_size;
};

struct header {
	u_int32_t	version;
	u_int32_t	mode;
} __attribute__((__packed__));

static void usage(void);
static int do_req(const char *, unsigned long, void *);
static void mmap_file(const char *, char **, size_t *);
static void load_firmware(const char *, const char *, const char *);
static void kill_firmware(const char *);
static void get_radio_state(const char *);
static void get_statistics(const char *);
static void dump_debug(const char *);

int
main(int argc, char **argv)
{
	int ch;
	const char *iface = NULL, *mode = "bss", *path = NULL;
	int noflag = 1, kflag = 0, rflag = 0, dflag = 0;

	if (argc > 1 && argv[1][0] != '-') {
		iface = argv[1];
		optind++;
	}

	while ((ch = getopt(argc, argv, "d:i:km:rD")) != -1) {
		if (ch != 'i')
			noflag = 0;

		switch (ch) {
		case 'd':
			path = optarg;
			break;

		case 'i':
			iface = optarg;
			break;

		case 'k':
			kflag = 1;
			break;

		case 'm':
			mode = optarg;
			break;

		case 'r':
			rflag = 1;
			break;

		case 'D':
			dflag = 1;
			break;

		default:
			usage();
		}
	}

	if (iface == NULL)
		usage();

	if (kflag && (path != NULL || rflag))
		usage();

	if (kflag)
		kill_firmware(iface);

	if (path != NULL)
		load_firmware(iface, path, mode);

	if (rflag)
		get_radio_state(iface);

	if (dflag)
		dump_debug(iface);

	if (noflag)
		get_statistics(iface);

	return EX_OK;
}

extern char *__progname;

static void
usage(void)
{
	(void)fprintf(stderr, "usage:  %s iface\n"
	    "\t%s iface -d path [-m bss|ibss]\n"
	    "\t%s iface -k\n"
	    "\t%s iface -r\n", __progname, __progname, __progname,
	    __progname);

	exit(EX_USAGE);
}

static int
do_req(const char *iface, unsigned long req, void *data)
{
	int s;
	struct ifreq ifr;
	int error;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(EX_OSERR, "Can't create socket");

	(void)memset(&ifr, 0, sizeof ifr);
	(void)strncpy(ifr.ifr_name, iface, sizeof ifr.ifr_name);
	ifr.ifr_data = data;
	error = ioctl(s, req, &ifr);

	(void)close(s);

	return error;
}

static void
mmap_file(const char *filename, char **addr, size_t *len)
{
	int fd;
	struct stat st;

	if ((fd = open(filename, O_RDONLY)) == -1)
		err(EX_OSERR, "%s", filename);

	if (fstat(fd, &st) == -1)
		err(EX_OSERR, "Unable to stat %s", filename);

	*len = st.st_size;

	if ((*addr = mmap(NULL, st.st_size, PROT_READ, 0, fd, 0)) == NULL)
		err(EX_OSERR, "Can't map %s into memory", filename);

	*addr += sizeof (struct header);
	*len -= sizeof (struct header);

	(void)close(fd);
}

static void
load_firmware(const char *iface, const char *path, const char *mode)
{
	char filename[FILENAME_MAX];
	struct firmware fw;

	(void)snprintf(filename, sizeof filename, "%s/iwi-boot.fw", path);
	mmap_file(filename, &fw.boot, &fw.boot_size);

	(void)snprintf(filename, sizeof filename, "%s/iwi-ucode-%s.fw", path,
	    mode);
	mmap_file(filename, &fw.ucode, &fw.ucode_size);

	(void)snprintf(filename, sizeof filename, "%s/iwi-%s.fw", path, mode);
	mmap_file(filename, &fw.main, &fw.main_size);

	if (do_req(iface, strstr(mode,"ibss") 
                           ? SIOCSLOADIBSSFW : SIOCSLOADFW, &fw) == -1)
		err(EX_OSERR, "Can't load firmware to driver");
}

static void
kill_firmware(const char *iface)
{
	if (do_req(iface, SIOCSKILLFW, NULL) == -1)
		err(EX_OSERR, "Can't kill firmware");
}

static void
dump_debug(const char *iface)
{
	int dump = 1, len = sizeof dump;
	char oid_name[128];
	snprintf(oid_name, sizeof oid_name, "hw.%s.firmware_logs", iface );
	if (sysctlbyname(oid_name, 0, 0, &dump, len) == -1)
		err(EX_OSERR, "Can't dump firmware logs");
	(void)printf("All firmware logs dumped.\n");
}


static void
get_radio_state(const char *iface)
{
	int radio, len;
	char oid_name[128];

	snprintf(oid_name, sizeof oid_name, "hw.%s.radio", iface );
	len = sizeof radio;
	if (sysctlbyname(oid_name, &radio, &len, NULL, 0) == -1)
		err(EX_OSERR, "Can't get radio transmitter state");

	(void)printf("Radio is %s\n", radio ? "ON" : "OFF");
}

struct statistic {
	int 		index;
	const char	*desc;
};

static const struct statistic tbl[] = {
	{  1, "Current transmission rate" },
	{  2, "Fragmentation threshold" },
	{  3, "RTS threshold" },
	{  4, "Number of frames submitted for transfer" },
	{  5, "Number of frames transmitted" },
	{  6, "Number of unicast frames transmitted" },
	{  7, "Number of unicast 802.11b frames transmitted at 1Mb/s" },
	{  8, "Number of unicast 802.11b frames transmitted at 2Mb/s" },
	{  9, "Number of unicast 802.11b frames transmitted at 5.5Mb/s" },
	{ 10, "Number of unicast 802.11b frames transmitted at 11Mb/s" },

	{ 19, "Number of unicast 802.11g frames transmitted at 1Mb/s" },
	{ 20, "Number of unicast 802.11g frames transmitted at 2Mb/s" },
	{ 21, "Number of unicast 802.11g frames transmitted at 5.5Mb/s" },
	{ 22, "Number of unicast 802.11g frames transmitted at 6Mb/s" },
	{ 23, "Number of unicast 802.11g frames transmitted at 9Mb/s" },
	{ 24, "Number of unicast 802.11g frames transmitted at 11Mb/s" },
	{ 25, "Number of unicast 802.11g frames transmitted at 12Mb/s" },
	{ 26, "Number of unicast 802.11g frames transmitted at 18Mb/s" },
	{ 27, "Number of unicast 802.11g frames transmitted at 24Mb/s" },
	{ 28, "Number of unicast 802.11g frames transmitted at 36Mb/s" },
	{ 29, "Number of unicast 802.11g frames transmitted at 48Mb/s" },
	{ 30, "Number of unicast 802.11g frames transmitted at 54Mb/s" },
	{ 31, "Number of multicast frames transmitted" },
	{ 32, "Number of multicast 802.11b frames transmitted at 1Mb/s" },
	{ 33, "Number of multicast 802.11b frames transmitted at 2Mb/s" },
	{ 34, "Number of multicast 802.11b frames transmitted at 5.5Mb/s" },
	{ 35, "Number of multicast 802.11b frames transmitted at 11Mb/s" },

	{ 44, "Number of multicast 802.11g frames transmitted at 1Mb/s" },
	{ 45, "Number of multicast 802.11g frames transmitted at 2Mb/s" },
	{ 46, "Number of multicast 802.11g frames transmitted at 5.5Mb/s" },
	{ 47, "Number of multicast 802.11g frames transmitted at 6Mb/s" },
	{ 48, "Number of multicast 802.11g frames transmitted at 9Mb/s" },
	{ 49, "Number of multicast 802.11g frames transmitted at 11Mb/s" },
	{ 50, "Number of multicast 802.11g frames transmitted at 12Mb/s" },
	{ 51, "Number of multicast 802.11g frames transmitted at 18Mb/s" },
	{ 52, "Number of multicast 802.11g frames transmitted at 24Mb/s" },
	{ 53, "Number of multicast 802.11g frames transmitted at 36Mb/s" },
	{ 54, "Number of multicast 802.11g frames transmitted at 48Mb/s" },
	{ 55, "Number of multicast 802.11g frames transmitted at 54Mb/s" },
	{ 56, "Number of transmission retries" },
	{ 57, "Number of transmission failures" },
	{ 58, "Number of frames with a bad CRC received" },

	{ 61, "Number of full scans" },
	{ 62, "Number of partial scans" },

	{ 64, "Number of bytes transmitted" },
	{ 65, "Current RSSI" },
	{ 66, "Number of beacons received" },
	{ 67, "Number of beacons missed" },

	{ 0, NULL }
};

static void
get_statistics(const char *iface)
{
	static u_int32_t stats[256];
	char oid_name[128];
	const struct statistic *stt;
	int len;

	snprintf(oid_name, sizeof oid_name, "hw.%s.stats", iface );
	len = sizeof stats;
	if (sysctlbyname(oid_name, stats, &len, NULL, 0) == -1)
		err(EX_OSERR, "Can't retrieve statistics");

	for (stt = tbl; stt->index != 0; stt++)
		(void)printf("%-60s[%u]\n", stt->desc, stats[stt->index]);
}

