/*	$OpenBSD: pflogd.c,v 1.27 2004/02/13 19:01:57 otto Exp $	*/
/*	$DragonFly: src/usr.sbin/pflogd/pflogd.c,v 1.2 2004/12/18 22:48:04 swildner Exp $ */

/*
 * Copyright (c) 2001 Theo de Raadt
 * Copyright (c) 2001 Can Erkin Acar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <machine/inttypes.h>

#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>
#include <pcap-int.h>
#include <pcap.h>

#include "pflogd.h"

pcap_t *hpcap;
static FILE *dpcap;

int Debug = 0;
static int snaplen = DEF_SNAPLEN;
static int cur_snaplen = DEF_SNAPLEN;

volatile sig_atomic_t gotsig_close, gotsig_alrm, gotsig_hup;

const char *filename = PFLOGD_LOG_FILE;
const char *interface = PFLOGD_DEFAULT_IF;
char *filter = NULL;

char errbuf[PCAP_ERRBUF_SIZE];

int log_debug = 0;
unsigned int delay = FLUSH_DELAY;

char *copy_argv(char * const *);
void  dump_packet(u_char *, const struct pcap_pkthdr *, const u_char *);
void  dump_packet_nobuf(u_char *, const struct pcap_pkthdr *, const u_char *);
int   flush_buffer(FILE *);
int   init_pcap(void);
void  purge_buffer(void);
int   reset_dump(void);
int   scan_dump(FILE *, off_t);
int   set_snaplen(int);
void  set_suspended(int);
void  sig_alrm(int);
void  sig_close(int);
void  sig_hup(int);
void  usage(void);

/* buffer must always be greater than snaplen */
static int    bufpkt = 0;	/* number of packets in buffer */
static size_t buflen = 0;	/* allocated size of buffer */
static char  *buffer = NULL;	/* packet buffer */
static char  *bufpos = NULL;	/* position in buffer */
static size_t bufleft = 0;	/* bytes left in buffer */

/* if error, stop logging but count dropped packets */
static int suspended = -1;
static long packets_dropped = 0;

void
set_suspended(int s)
{
	if (suspended == s)
		return;

	suspended = s;
	setproctitle("[%s] -s %d -f %s",
            suspended ? "suspended" : "running", cur_snaplen, filename);
}

char *
copy_argv(char * const *argv)
{
	size_t len = 0, n;
	char *buf;

	if (argv == NULL)
		return (NULL);

	for (n = 0; argv[n]; n++)
		len += strlen(argv[n])+1;
	if (len == 0)
		return (NULL);

	buf = malloc(len);
	if (buf == NULL)
		return (NULL);

	strlcpy(buf, argv[0], len);
	for (n = 1; argv[n]; n++) {
		strlcat(buf, " ", len);
		strlcat(buf, argv[n], len);
	}
	return (buf);
}

void
logmsg(int pri, const char *message, ...)
{
	va_list ap;
	va_start(ap, message);

	if (log_debug) {
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
	} else
		vsyslog(pri, message, ap);
	va_end(ap);
}

void
usage(void)
{
	fprintf(stderr, "usage: pflogd [-Dx] [-d delay] [-f filename] ");
	fprintf(stderr, "[-s snaplen] [expression]\n");
	exit(1);
}

void
sig_close(int sig __unused)
{
	gotsig_close = 1;
}

void
sig_hup(int sig __unused)
{
	gotsig_hup = 1;
}

void
sig_alrm(int sig __unused)
{
	gotsig_alrm = 1;
}

void
set_pcap_filter(void)
{
	struct bpf_program bprog;

	if (pcap_compile(hpcap, &bprog, filter, PCAP_OPT_FIL, 0) < 0)
		logmsg(LOG_WARNING, "%s", pcap_geterr(hpcap));
	else {
		if (pcap_setfilter(hpcap, &bprog) < 0)
			logmsg(LOG_WARNING, "%s", pcap_geterr(hpcap));
		pcap_freecode(&bprog);
	}
}

int
init_pcap(void)
{
	hpcap = pcap_open_live(interface, snaplen, 1, PCAP_TO_MS, errbuf);
	if (hpcap == NULL) {
		logmsg(LOG_ERR, "Failed to initialize: %s", errbuf);
		return (-1);
	}

	if (pcap_datalink(hpcap) != DLT_PFLOG) {
		logmsg(LOG_ERR, "Invalid datalink type");
		pcap_close(hpcap);
		hpcap = NULL;
		return (-1);
	}

	set_pcap_filter();

	cur_snaplen = snaplen = pcap_snapshot(hpcap);

	/* From contrib/pf/pflogd.c 1.5 FreeBSD: BPF locking is not
	 * (yet) supported.
	 */
	#ifndef __DragonFly__
	/* lock */
	if (ioctl(pcap_fileno(hpcap), BIOCLOCK) < 0) {
		logmsg(LOG_ERR, "BIOCLOCK: %s", strerror(errno));
		return (-1);
	}
	#endif

	return (0);
}

int
set_snaplen(int snap)
{
	if (priv_set_snaplen(snap))
		return (1);

	if (cur_snaplen > snap)
		purge_buffer();

	cur_snaplen = snap;

	return (0);
}

int
reset_dump(void)
{
	struct pcap_file_header hdr;
	struct stat st;
	int fd;
	FILE *fp;

	if (hpcap == NULL)
		return (-1);

	if (dpcap) {
		flush_buffer(dpcap);
		fclose(dpcap);
		dpcap = NULL;
	}

	/*
	 * Basically reimplement pcap_dump_open() because it truncates
	 * files and duplicates headers and such.
	 */
	fd = priv_open_log();
	if (fd < 0)
		return (1);

	fp = fdopen(fd, "a+");

	if (fp == NULL) {
		logmsg(LOG_ERR, "Error: %s: %s", filename, strerror(errno));
		return (1);
	}
	if (fstat(fileno(fp), &st) == -1) {
		logmsg(LOG_ERR, "Error: %s: %s", filename, strerror(errno));
		return (1);
	}

	/* set FILE unbuffered, we do our own buffering */
	if (setvbuf(fp, NULL, _IONBF, 0)) {
		logmsg(LOG_ERR, "Failed to set output buffers");
		return (1);
	}

#define TCPDUMP_MAGIC 0xa1b2c3d4

	if (st.st_size == 0) {
		if (snaplen != cur_snaplen) {
			logmsg(LOG_NOTICE, "Using snaplen %d", snaplen);
			if (set_snaplen(snaplen)) {
				logmsg(LOG_WARNING,
				    "Failed, using old settings");
			}
		}
		hdr.magic = TCPDUMP_MAGIC;
		hdr.version_major = PCAP_VERSION_MAJOR;
		hdr.version_minor = PCAP_VERSION_MINOR;
		hdr.thiszone = hpcap->tzoff;
		hdr.snaplen = hpcap->snapshot;
		hdr.sigfigs = 0;
		hdr.linktype = hpcap->linktype;

		if (fwrite((char *)&hdr, sizeof(hdr), 1, fp) != 1) {
			fclose(fp);
			return (1);
		}
	} else if (scan_dump(fp, st.st_size)) {
		/* XXX move file and continue? */
		fclose(fp);
		return (1);
	}

	dpcap = fp;

	set_suspended(0);
	flush_buffer(fp);

	return (0);
}

int
scan_dump(FILE *fp, off_t size)
{
	struct pcap_file_header hdr;
	struct pcap_pkthdr ph;
	off_t pos;

	/*
	 * Must read the file, compare the header against our new
	 * options (in particular, snaplen) and adjust our options so
	 * that we generate a correct file. Furthermore, check the file
	 * for consistency so that we can append safely.
	 *
	 * XXX this may take a long time for large logs.
	 */
	fseek(fp, 0L, SEEK_SET);

	if (fread((char *)&hdr, sizeof(hdr), 1, fp) != 1) {
		logmsg(LOG_ERR, "Short file header");
		return (1);
	}

	if (hdr.magic != TCPDUMP_MAGIC ||
	    hdr.version_major != PCAP_VERSION_MAJOR ||
	    hdr.version_minor != PCAP_VERSION_MINOR ||
	    (int)hdr.linktype != hpcap->linktype ||
	    hdr.snaplen > PFLOGD_MAXSNAPLEN) {
		logmsg(LOG_ERR, "Invalid/incompatible log file, move it away");
		return (1);
	}

	pos = sizeof(hdr);

	while (!feof(fp)) {
		off_t len = fread((char *)&ph, 1, sizeof(ph), fp);
		if (len == 0)
			break;

		if (len != sizeof(ph))
			goto error;
		if (ph.caplen > hdr.snaplen || ph.caplen > PFLOGD_MAXSNAPLEN)
			goto error;
		pos += sizeof(ph) + ph.caplen;
		if (pos > size)
			goto error;
		fseek(fp, ph.caplen, SEEK_CUR);
	}

	if (pos != size)
		goto error;

	if ((int)hdr.snaplen != cur_snaplen) {
		logmsg(LOG_WARNING,
		       "Existing file has different snaplen %u, using it",
		       hdr.snaplen);
		if (set_snaplen(hdr.snaplen)) {
			logmsg(LOG_WARNING,
			       "Failed, using old settings, offset %llu",
			       (unsigned long long) size);
		}
	}

	return (0);

 error:
	logmsg(LOG_ERR, "Corrupted log file.");
	return (1);
}

/* dump a packet directly to the stream, which is unbuffered */
void
dump_packet_nobuf(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	FILE *f = (FILE *)user;

	if (suspended) {
		packets_dropped++;
		return;
	}

	if (fwrite(h, sizeof(*h), 1, f) != 1) {
		/* try to undo header to prevent corruption */
		off_t pos = ftello(f);
		if (pos < sizeof(*h) ||
		    ftruncate(fileno(f), pos - sizeof(*h))) {
			logmsg(LOG_ERR, "Write failed, corrupted logfile!");
			set_suspended(1);
			gotsig_close = 1;
			return;
		}
		goto error;
	}

	if (fwrite(sp, h->caplen, 1, f) != 1)
		goto error;

	return;

error:
	set_suspended(1);
	packets_dropped ++;
	logmsg(LOG_ERR, "Logging suspended: fwrite: %s", strerror(errno));
}

int
flush_buffer(FILE *f)
{
	off_t offset;
	int len = bufpos - buffer;

	if (len <= 0)
		return (0);

	offset = ftello(f);
	if (offset == (off_t)-1) {
		set_suspended(1);
		logmsg(LOG_ERR, "Logging suspended: ftello: %s",
		    strerror(errno));
		return (1);
	}

	if (fwrite(buffer, len, 1, f) != 1) {
		set_suspended(1);
		logmsg(LOG_ERR, "Logging suspended: fwrite: %s",
		    strerror(errno));
		ftruncate(fileno(f), offset);
		return (1);
	}

	set_suspended(0);
	bufpos = buffer;
	bufleft = buflen;
	bufpkt = 0;

	return (0);
}

void
purge_buffer(void)
{
	packets_dropped += bufpkt;

	set_suspended(0);
	bufpos = buffer;
	bufleft = buflen;
	bufpkt = 0;
}

/* append packet to the buffer, flushing if necessary */
void
dump_packet(u_char *user, const struct pcap_pkthdr *h, const u_char *sp)
{
	FILE *f = (FILE *)user;
	size_t len = sizeof(*h) + h->caplen;

	if (len < sizeof(*h) || h->caplen > (size_t)cur_snaplen) {
		logmsg(LOG_NOTICE, "invalid size %u (%u/%u), packet dropped",
		       len, cur_snaplen, snaplen);
		packets_dropped++;
		return;
	}

	if (len <= bufleft)
		goto append;

	if (suspended) {
		packets_dropped++;
		return;
	}

	if (flush_buffer(f)) {
		packets_dropped++;
		return;
	}

	if (len > bufleft) {
		dump_packet_nobuf(user, h, sp);
		return;
	}

 append:	
	memcpy(bufpos, h, sizeof(*h));
	memcpy(bufpos + sizeof(*h), sp, h->caplen);

	bufpos += len;
	bufleft -= len;
	bufpkt++;

	return;
}

int
main(int argc, char **argv)
{
	struct pcap_stat pstat;
	int ch, np, Xflag = 0;
	pcap_handler phandler = dump_packet;

	/* Neither FreeBSD nor DFly have this; Max seems to think this may
	 * be a paranoid check. Comment it out:
	closefrom(STDERR_FILENO + 1);
	 */

	while ((ch = getopt(argc, argv, "Dxd:s:f:")) != -1) {
		switch (ch) {
		case 'D':
			Debug = 1;
			break;
		case 'd':
			delay = atoi(optarg);
			if (delay < 5 || delay > 60*60)
				usage();
			break;
		case 'f':
			filename = optarg;
			break;
		case 's':
			snaplen = atoi(optarg);
			if (snaplen <= 0)
				snaplen = DEF_SNAPLEN;
			if (snaplen > PFLOGD_MAXSNAPLEN)
				snaplen = PFLOGD_MAXSNAPLEN;
			break;
		case 'x':
			Xflag++;
			break;
		default:
			usage();
		}

	}

	log_debug = Debug;
	argc -= optind;
	argv += optind;

	if (!Debug) {
		openlog("pflogd", LOG_PID | LOG_CONS, LOG_DAEMON);
		if (daemon(0, 0)) {
			logmsg(LOG_WARNING, "Failed to become daemon: %s",
			    strerror(errno));
		}
		pidfile(NULL);
	}

	umask(S_IRWXG | S_IRWXO);

	/* filter will be used by the privileged process */
	if (argc) {
		filter = copy_argv(argv);
		if (filter == NULL)
			logmsg(LOG_NOTICE, "Failed to form filter expression");
	}

	/* initialize pcap before dropping privileges */
	if (init_pcap()) {
		logmsg(LOG_ERR, "Exiting, init failure");
		exit(1);
	}

	/* Privilege separation begins here */
	if (priv_init()) {
		logmsg(LOG_ERR, "unable to privsep");
		exit(1);
	}

	setproctitle("[initializing]");
	/* Process is now unprivileged and inside a chroot */
	signal(SIGTERM, sig_close);
	signal(SIGINT, sig_close);
	signal(SIGQUIT, sig_close);
	signal(SIGALRM, sig_alrm);
	signal(SIGHUP, sig_hup);
	alarm(delay);

	buffer = malloc(PFLOGD_BUFSIZE);

	if (buffer == NULL) {
		logmsg(LOG_WARNING, "Failed to allocate output buffer");
		phandler = dump_packet_nobuf;
	} else {
		bufleft = buflen = PFLOGD_BUFSIZE;
		bufpos = buffer;
		bufpkt = 0;
	}

	if (reset_dump()) {
		if (Xflag)
			return (1);

		logmsg(LOG_ERR, "Logging suspended: open error");
		set_suspended(1);
	} else if (Xflag)
		return (0);

	while (1) {
		np = pcap_dispatch(hpcap, PCAP_NUM_PKTS,
		    dump_packet, (u_char *)dpcap);
		if (np < 0)
			logmsg(LOG_NOTICE, "%s", pcap_geterr(hpcap));

		if (gotsig_close)
			break;
		if (gotsig_hup) {
			if (reset_dump()) {
				logmsg(LOG_ERR,
				    "Logging suspended: open error");
				set_suspended(1);
			}
			gotsig_hup = 0;
		}

		if (gotsig_alrm) {
			if (dpcap)
				flush_buffer(dpcap);
			gotsig_alrm = 0;
			alarm(delay);
		}
	}

	logmsg(LOG_NOTICE, "Exiting");
	if (dpcap) {
		flush_buffer(dpcap);
		fclose(dpcap);
	}
	purge_buffer();

	if (pcap_stats(hpcap, &pstat) < 0)
		logmsg(LOG_WARNING, "Reading stats: %s", pcap_geterr(hpcap));
	else
		logmsg(LOG_NOTICE,
		    "%u packets received, %u/%u dropped (kernel/pflogd)",
		    pstat.ps_recv, pstat.ps_drop, packets_dropped);

	pcap_close(hpcap);
	if (!Debug)
		closelog();
	return (0);
}
