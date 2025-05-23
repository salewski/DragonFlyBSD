/*	$OpenBSD: privsep.c,v 1.8 2004/03/14 19:17:05 otto Exp $	*/
/*	$DragonFly: src/usr.sbin/pflogd/privsep.c,v 1.1 2004/09/21 21:25:28 joerg Exp $ */

/*
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/bpf.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <pcap-int.h>
#include <syslog.h>
#include <unistd.h>
#include "pflogd.h"

enum cmd_types {
	PRIV_SET_SNAPLEN,	/* set the snaplength */
	PRIV_OPEN_LOG		/* open logfile for appending */
};

static int priv_fd = -1;
static volatile pid_t child_pid = -1;

volatile sig_atomic_t gotsig_chld = 0;

static void sig_pass_to_chld(int);
static void sig_chld(int);
static int  may_read(int, void *, size_t);
static void must_read(int, void *, size_t);
static void must_write(int, void *, size_t);
static int  set_snaplen(int snap);

/* bpf filter expression common to parent and child */
extern char *filter;
extern char *errbuf;
extern char *filename;
extern pcap_t *hpcap;

/* based on syslogd privsep */
int
priv_init(void)
{
	int i, fd, socks[2], cmd;
	int snaplen, ret;
	struct passwd *pw;

	for (i = 1; i < NSIG; i++)
		signal(i, SIG_DFL);

	/* Create sockets */
	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, socks) == -1)
		err(1, "socketpair() failed");

	pw = getpwnam("_pflogd");
	if (pw == NULL)
		errx(1, "unknown user _pflogd");
	endpwent();

	child_pid = fork();
	if (child_pid < 0)
		err(1, "fork() failed");

	if (!child_pid) {
		gid_t gidset[1];

		/* Child - drop privileges and return */
		if (chroot(pw->pw_dir) != 0)
			err(1, "unable to chroot");
		if (chdir("/") != 0)
			err(1, "unable to chdir");

		gidset[0] = pw->pw_gid;
		if (setgroups(1, gidset) == -1)
			err(1, "setgroups() failed");
		if (setegid(pw->pw_gid) == -1)
			err(1, "setegid() failed");
		if (setgid(pw->pw_gid) == -1)
			err(1, "setgid() failed");
		if (seteuid(pw->pw_uid) == -1)
			err(1, "seteuid() failed");
		if (setuid(pw->pw_uid) == -1)
			err(1, "setuid() failed");
		close(socks[0]);
		priv_fd = socks[1];
		return 0;
	}

	/* Father */
	/* Pass ALRM/TERM/HUP through to child, and accept CHLD */
	signal(SIGALRM, sig_pass_to_chld);
	signal(SIGTERM, sig_pass_to_chld);
	signal(SIGHUP,  sig_pass_to_chld);
	signal(SIGCHLD, sig_chld);

	setproctitle("[priv]");
	close(socks[1]);

	while (!gotsig_chld) {
		if (may_read(socks[0], &cmd, sizeof(int)))
			break;
		switch (cmd) {
		case PRIV_SET_SNAPLEN:
			logmsg(LOG_DEBUG,
			    "[priv]: msg PRIV_SET_SNAPLENGTH received");
			must_read(socks[0], &snaplen, sizeof(int));

			ret = set_snaplen(snaplen);
			if (ret) {
				logmsg(LOG_NOTICE,
				   "[priv]: set_snaplen failed for snaplen %d",
				   snaplen);
			}

			must_write(socks[0], &ret, sizeof(int));
			break;

		case PRIV_OPEN_LOG:
			logmsg(LOG_DEBUG,
			    "[priv]: msg PRIV_OPEN_LOG received");
			/* create or append logs but do not follow symlinks */
			fd = open(filename,
			    O_RDWR|O_CREAT|O_APPEND|O_NONBLOCK|O_NOFOLLOW,
			    0600);
			if (fd < 0)
				logmsg(LOG_NOTICE,
				    "[priv]: failed to open %s: %s",
				    filename, strerror(errno));
			send_fd(socks[0], fd);
			close(fd);
			break;

		default:
			logmsg(LOG_ERR, "[priv]: unknown command %d", cmd);
			_exit(1);
			/* NOTREACHED */
		}
	}

	_exit(1);
}

/* this is called from parent */
static int
set_snaplen(int snap)
{
	if (hpcap == NULL)
		return (1);

	hpcap->snapshot = snap;
	set_pcap_filter();

	return 0;
}


/*
 * send the snaplength to privileged process
 */
int
priv_set_snaplen(int snaplen)
{
	int cmd, ret;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion", __func__);

	cmd = PRIV_SET_SNAPLEN;

	must_write(priv_fd, &cmd, sizeof(int));
	must_write(priv_fd, &snaplen, sizeof(int));

	must_read(priv_fd, &ret, sizeof(int));

	/* also set hpcap->snapshot in child */
	if (ret == 0)
		hpcap->snapshot = snaplen;

	return (ret);
}

/* Open log-file */
int
priv_open_log(void)
{
	int cmd, fd;

	if (priv_fd < 0)
		errx(1, "%s: called from privileged portion\n", __func__);

	cmd = PRIV_OPEN_LOG;
	must_write(priv_fd, &cmd, sizeof(int));
	fd = receive_fd(priv_fd);

	return (fd);
}

/* If priv parent gets a TERM or HUP, pass it through to child instead */
static void
sig_pass_to_chld(int sig)
{
	int oerrno = errno;

	if (child_pid != -1)
		kill(child_pid, sig);
	errno = oerrno;
}

/* if parent gets a SIGCHLD, it will exit */
static void
sig_chld(int sig __unused)
{
	gotsig_chld = 1;
}

/* Read all data or return 1 for error.  */
static int
may_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res;
	size_t pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			return (1);
		default:
			pos += res;
		}
	}
	return (0);
}

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
static void
must_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res;
	size_t pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
static void
must_write(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res;
	size_t pos = 0;

	while (n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
		case 0:
			_exit(0);
		default:
			pos += res;
		}
	}
}
