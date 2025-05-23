/*
 * Copyright (c) 1995 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * Snoop stuff.
 *
 * $FreeBSD: src/usr.sbin/watch/watch.c,v 1.18.2.3 2002/08/17 00:59:03 mikeh Exp $
 * $DragonFly: src/usr.sbin/watch/watch.c,v 1.4 2004/12/18 22:48:14 swildner Exp $
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/snoop.h>
#include <sys/stat.h>
#include <sys/linker.h>
#include <sys/module.h>

#include <err.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termcap.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MSG_INIT	"Snoop started."
#define MSG_OFLOW	"Snoop stopped due to overflow. Reconnecting."
#define MSG_CLOSED	"Snoop stopped due to tty close. Reconnecting."
#define MSG_CHANGE	"Snoop device change by user request."
#define MSG_NOWRITE	"Snoop device change due to write failure."


#define DEV_NAME_LEN	1024	/* for /dev/ttyXX++ */
#define MIN_SIZE	256

#define CHR_SWITCH	24	/* Ctrl+X	 */
#define CHR_CLEAR	23	/* Ctrl+V	 */

static void	clear(void);
static void	timestamp (const char *);
static void	set_tty(void);
static void	unset_tty(void);
static void	fatal(int, const char *);
static int	open_snp(void);
static void	cleanup(int);
static void	usage(void);
static void	setup_scr(void);
static void	attach_snp(void);
static void	detach_snp(void);
static void	set_dev(const char *);
static void	ask_dev(char *, const char *);

int             opt_reconn_close = 0;
int             opt_reconn_oflow = 0;
int             opt_interactive = 1;
int             opt_timestamp = 0;
int		opt_write = 0;
int		opt_no_switch = 0;
const char	*opt_snpdev;

char            dev_name[DEV_NAME_LEN];
int             snp_io;
dev_t		snp_tty;
int             std_in = 0, std_out = 1;


int             clear_ok = 0;
struct termios  otty;
char            tbuf[1024], gbuf[1024];


static void
clear()
{
	if (clear_ok)
		tputs(gbuf, 1, putchar);
	fflush(stdout);
}

static void
timestamp(buf)
	const char     *buf;
{
	time_t          t;
	char            btmp[1024];
	clear();
	printf("\n---------------------------------------------\n");
	t = time(NULL);
	strftime(btmp, 1024, "Time: %d %b %H:%M", localtime(&t));
	printf("%s\n", btmp);
	printf("%s\n", buf);
	printf("---------------------------------------------\n");
	fflush(stdout);
}

static void
set_tty()
{
	struct termios  ntty;

	tcgetattr (std_in, &otty);
	ntty = otty;
	ntty.c_lflag &= ~ICANON;    /* disable canonical operation  */
	ntty.c_lflag &= ~ECHO;
#ifdef FLUSHO
	ntty.c_lflag &= ~FLUSHO;
#endif
#ifdef PENDIN
	ntty.c_lflag &= ~PENDIN;
#endif
#ifdef IEXTEN
	ntty.c_lflag &= ~IEXTEN;
#endif
	ntty.c_cc[VMIN] = 1;        /* minimum of one character */
	ntty.c_cc[VTIME] = 0;       /* timeout value        */

	ntty.c_cc[VINTR] = 07;   /* ^G */
	ntty.c_cc[VQUIT] = 07;   /* ^G */
	tcsetattr (std_in, TCSANOW, &ntty);
}

static void
unset_tty()
{
	tcsetattr (std_in, TCSANOW, &otty);
}


static void
fatal(error, buf)
	int		      error;
	const char           *buf;
{
	unset_tty();
	if (buf)
		errx(error, "fatal: %s", buf);
	else
		exit(error);
}

static int
open_snp()
{
	char            snp[] = {_PATH_DEV "snpX"};
	char            c;
	int             f, mode;

	if (opt_write)
		mode = O_RDWR;
	else
		mode = O_RDONLY;

	if (opt_snpdev == NULL)
		for (c = '0'; c <= '9'; c++) {
			snp[8] = c;
			if ((f = open(snp, mode)) < 0)
				continue;
			return f;
		}
	else
		if ((f = open(opt_snpdev, mode)) != -1)
			return (f);
	fatal(EX_OSFILE, "cannot open snoop device");
	return (0);
}


static void
cleanup(signo)
	int		signo __unused;
{
	if (opt_timestamp)
		timestamp("Logging Exited.");
	close(snp_io);
	unset_tty();
	exit(EX_OK);
}


static void
usage()
{
	fprintf(stderr, "usage: watch [-ciotnW] [tty name]\n");
	exit(EX_USAGE);
}

static void
setup_scr()
{
	char           *cbuf = gbuf, *term;
	if (!opt_interactive)
		return;
	if ((term = getenv("TERM")))
		if (tgetent(tbuf, term) == 1)
			if (tgetstr("cl", &cbuf))
				clear_ok = 1;
	set_tty();
	clear();
}

static void
detach_snp()
{
	dev_t		dev;

	dev = NODEV;
	ioctl(snp_io, SNPSTTY, &dev);
}

static void
attach_snp()
{
	if (ioctl(snp_io, SNPSTTY, &snp_tty) != 0)
		fatal(EX_UNAVAILABLE, "cannot attach to tty");
	if (opt_timestamp)
		timestamp("Logging Started.");
}


static void
set_dev(name)
	const char     *name;
{
	char            buf[DEV_NAME_LEN];
	struct stat	sb;

	if (strlen(name) > 5 && !strncmp(name, _PATH_DEV, sizeof _PATH_DEV - 1)) {
		snprintf(buf, sizeof buf, "%s", name);
	}
	else {
		if (strlen(name) == 2)
			sprintf(buf, "%s%s", _PATH_TTY, name);
		else
			sprintf(buf, "%s%s", _PATH_DEV, name);
	}

	if (*name == '\0' || stat(buf, &sb) < 0)
		fatal(EX_DATAERR, "bad device name");

	if ((sb.st_mode & S_IFMT) != S_IFCHR)
		fatal(EX_DATAERR, "must be a character device");

	snp_tty = sb.st_rdev;
	attach_snp();
}

void
ask_dev(dbuf, msg)
        char	       *dbuf;
        const char     *msg;
{
	char            buf[DEV_NAME_LEN];
	int             len;

	clear();
	unset_tty();

	if (msg)
		printf("%s\n", msg);
	if (dbuf)
		printf("Enter device name [%s]:", dbuf);
	else
		printf("Enter device name:");

	if (fgets(buf, DEV_NAME_LEN - 1, stdin)) {
		len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		if (buf[0] != '\0' && buf[0] != ' ')
			strcpy(dbuf, buf);
	}
	set_tty();
}

#define READB_LEN	5

int
main(ac, av)
	int             ac;
	char          **av;
{
	int             res, idata, rv;
	size_t		nread, b_size = MIN_SIZE;
	char            ch, *buf, chb[READB_LEN];
	fd_set          fd_s;

	setlocale(LC_TIME, "");

	if (isatty(std_out))
		opt_interactive = 1;
	else
		opt_interactive = 0;


	while ((ch = getopt(ac, av, "Wciotnf:")) != -1)
		switch (ch) {
		case 'W':
			opt_write = 1;
			break;
		case 'c':
			opt_reconn_close = 1;
			break;
		case 'i':
			opt_interactive = 1;
			break;
		case 'o':
			opt_reconn_oflow = 1;
			break;
		case 't':
			opt_timestamp = 1;
			break;
		case 'n':
			opt_no_switch = 1;
			break;
		case 'f':
			opt_snpdev = optarg;
			break;
		case '?':
		default:
			usage();
		}

	if (modfind("snp") == -1)
		if (kldload("snp") == -1 || modfind("snp") == -1)
			warn("snp module not available");

	signal(SIGINT, cleanup);

	setup_scr();
	snp_io = open_snp();

	if (*(av += optind) == NULL) {
		if (opt_interactive && !opt_no_switch)
			ask_dev(dev_name, MSG_INIT);
		else
			fatal(EX_DATAERR, "no device name given");
	} else
		strncpy(dev_name, *av, DEV_NAME_LEN);

	set_dev(dev_name);

	if (!(buf = (char *) malloc(b_size)))
		fatal(EX_UNAVAILABLE, "malloc failed");

	FD_ZERO(&fd_s);

	while (1) {
		if (opt_interactive)
			FD_SET(std_in, &fd_s);
		FD_SET(snp_io, &fd_s);
		res = select(snp_io + 1, &fd_s, NULL, NULL, NULL);
		if (opt_interactive && FD_ISSET(std_in, &fd_s)) {

			if ((res = ioctl(std_in, FIONREAD, &nread)) != 0)
				fatal(EX_OSERR, "ioctl(FIONREAD)");
			if (nread > READB_LEN)
				nread = READB_LEN;
			rv = read(std_in, chb, nread);
			if (rv == -1 || (unsigned)rv != nread)
				fatal(EX_IOERR, "read (stdin) failed");

			switch (chb[0]) {
			case CHR_CLEAR:
				clear();
				break;
			case CHR_SWITCH:
				if (!opt_no_switch) {
					detach_snp();
					ask_dev(dev_name, MSG_CHANGE);
					set_dev(dev_name);
					break;
				}
			default:
				if (opt_write) {
					rv = write(snp_io, chb, nread);
					if (rv == -1 || (unsigned)rv != nread) {
						detach_snp();
						if (opt_no_switch)
							fatal(EX_IOERR, "write failed");
						ask_dev(dev_name, MSG_NOWRITE);
						set_dev(dev_name);
					}
				}

			}
		}
		if (!FD_ISSET(snp_io, &fd_s))
			continue;

		if ((res = ioctl(snp_io, FIONREAD, &idata)) != 0)
			fatal(EX_OSERR, "ioctl(FIONREAD)");

		switch (idata) {
		case SNP_OFLOW:
			if (opt_reconn_oflow)
				attach_snp();
			else if (opt_interactive && !opt_no_switch) {
				ask_dev(dev_name, MSG_OFLOW);
				set_dev(dev_name);
			} else
				cleanup(-1);
			break;
		case SNP_DETACH:
		case SNP_TTYCLOSE:
			if (opt_reconn_close)
				attach_snp();
			else if (opt_interactive && !opt_no_switch) {
				ask_dev(dev_name, MSG_CLOSED);
				set_dev(dev_name);
			} else
				cleanup(-1);
			break;
		default:
			nread = (unsigned)idata;
			if (nread < (b_size / 2) && (b_size / 2) > MIN_SIZE) {
				free(buf);
				if (!(buf = (char *) malloc(b_size / 2)))
					fatal(EX_UNAVAILABLE, "malloc failed");
				b_size = b_size / 2;
			}
			if (nread > b_size) {
				b_size = (nread % 2) ? (nread + 1) : (nread);
				free(buf);
				if (!(buf = (char *) malloc(b_size)))
					fatal(EX_UNAVAILABLE, "malloc failed");
			}
			rv = read(snp_io, buf, nread);
			if (rv == -1 || (unsigned)rv != nread)
				fatal(EX_IOERR, "read failed");
			rv = write(std_out, buf, nread);
			if (rv == -1 || (unsigned)rv != nread)
				fatal(EX_IOERR, "write failed");
		}
	}			/* While */
	return(0);
}

