/*-
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1980, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)msgs.c	8.2 (Berkeley) 4/28/95
 * $FreeBSD: src/usr.bin/msgs/msgs.c,v 1.15.2.2 2003/02/11 21:31:56 mike Exp $
 * $DragonFly: src/usr.bin/msgs/msgs.c,v 1.4 2005/02/02 06:39:09 cpressey Exp $
 */

/*
 * msgs - a user bulletin board program
 *
 * usage:
 *	msgs [fhlopq] [[-]number]	to read messages
 *	msgs -s				to place messages
 *	msgs -c [-days]			to clean up the bulletin board
 *
 * prompt commands are:
 *	y	print message
 *	n	flush message, go to next message
 *	q	flush message, quit
 *	p	print message, turn on 'pipe thru more' mode
 *	P	print message, turn off 'pipe thru more' mode
 *	-	reprint last message
 *	s[-][<num>] [<filename>]	save message
 *	m[-][<num>]	mail with message in temp mbox
 *	x	exit without flushing this message
 *	<num>	print message number <num>
 */

#define V7		/* will look for TERM in the environment */
#define OBJECT		/* will object to messages without Subjects */
/* #define REJECT */	/* will reject messages without Subjects
			   (OBJECT must be defined also) */
/* #define UNBUFFERED *//* use unbuffered output */

#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <setjmp.h>
#include <termcap.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pathnames.h"

#define	CMODE	0644		/* bounds file creation	mode */
#define NO	0
#define YES	1
#define SUPERUSER	0	/* superuser uid */
#define DAEMON		1	/* daemon uid */
#define NLINES	24		/* default number of lines/crt screen */
#define NDAYS	21		/* default keep time for messages */
#define DAYS	*24*60*60	/* seconds/day */
#define MSGSRC	".msgsrc"	/* user's rc file */
#define BOUNDS	"bounds"	/* message bounds file */
#define NEXT	"Next message? [yq]"
#define MORE	"More? [ynq]"
#define NOMORE	"(No more) [q] ?"

typedef	char	bool;

FILE	*msgsrc;
FILE	*newmsg;
const char *sep = "-";
char	inbuf[BUFSIZ];
char	fname[MAXPATHLEN];
char	cmdbuf[MAXPATHLEN + MAXPATHLEN];
char	subj[128];
char	from[128];
char	date[128];
char	*ptr;
char	*in;
bool	local;
bool	ruptible;
bool	totty;
bool	seenfrom;
bool	seensubj;
bool	blankline;
bool	printing = NO;
bool	mailing = NO;
bool	quitit = NO;
bool	sending = NO;
bool	intrpflg = NO;
int	uid;
int	msg;
int	prevmsg;
int	lct;
int	nlines;
int	Lpp = 0;
time_t	t;
time_t	keep;

/* option initialization */
bool	hdrs = NO;
bool	qopt = NO;
bool	hush = NO;
bool	send_msg = NO;
bool	locomode = NO;
bool	use_pager = NO;
bool	clean = NO;
bool	lastcmd = NO;
jmp_buf	tstpbuf;


void ask(const char *);
void gfrsub(FILE *);
int linecnt(FILE *);
int next(char *);
char *nxtfld(unsigned char *);
void onsusp(int);
void onintr(int);
void prmesg(int);
static void usage(void);

int
main(int argc, char **argv)
{
	bool newrc, already;
	int rcfirst = 0;		/* first message to print (from .rc) */
	int rcback = 0;			/* amount to back off of rcfirst */
	int firstmsg = 0, nextmsg = 0, lastmsg = 0;
	int blast = 0;
	struct stat buf;		/* stat to check access of bounds */
	FILE *bounds;

#ifdef UNBUFFERED
	setbuf(stdout, NULL);
#endif
	setlocale(LC_ALL, "");

	time(&t);
	setuid(uid = getuid());
	ruptible = (signal(SIGINT, SIG_IGN) == SIG_DFL);
	if (ruptible)
		signal(SIGINT, SIG_DFL);

	argc--, argv++;
	while (argc > 0) {
		if (isdigit(argv[0][0])) {	/* starting message # */
			rcfirst = atoi(argv[0]);
		}
		else if (isdigit(argv[0][1])) {	/* backward offset */
			rcback = atoi( &( argv[0][1] ) );
		}
		else {
			ptr = *argv;
			while (*ptr) switch (*ptr++) {

			case '-':
				break;

			case 'c':
				if (uid != SUPERUSER && uid != DAEMON) {
					fprintf(stderr, "Sorry\n");
					exit(1);
				}
				clean = YES;
				break;

			case 'f':		/* silently */
				hush = YES;
				break;

			case 'h':		/* headers only */
				hdrs = YES;
				break;

			case 'l':		/* local msgs only */
				locomode = YES;
				break;

			case 'o':		/* option to save last message */
				lastcmd = YES;
				break;

			case 'p':		/* pipe thru 'more' during long msgs */
				use_pager = YES;
				break;

			case 'q':		/* query only */
				qopt = YES;
				break;

			case 's':		/* sending TO msgs */
				send_msg = YES;
				break;

			default:
				usage();
			}
		}
		argc--, argv++;
	}

	/*
	 * determine current message bounds
	 */
	snprintf(fname, sizeof(fname), "%s/%s", _PATH_MSGS, BOUNDS);

	/*
	 * Test access rights to the bounds file
	 * This can be a little tricky.  if(send_msg), then
	 * we will create it.  We assume that if(send_msg),	
	 * then you have write permission there.
	 * Else, it better be there, or we bail.
	 */
	if (send_msg != YES) {
		if (stat(fname, &buf) < 0) {
			if (hush != YES) {
				err(errno, "%s", fname);
			} else {
				exit(1);
			}
		}
	}
	bounds = fopen(fname, "r");

	if (bounds != NULL) {
		fscanf(bounds, "%d %d\n", &firstmsg, &lastmsg);
		fclose(bounds);
		blast = lastmsg;	/* save upper bound */
	}

	if (clean)
		keep = t - (rcback? rcback : NDAYS) DAYS;

	if (clean || bounds == NULL) {	/* relocate message bounds */
		struct dirent *dp;
		struct stat stbuf;
		bool seenany = NO;
		DIR	*dirp;

		dirp = opendir(_PATH_MSGS);
		if (dirp == NULL)
			err(errno, "%s", _PATH_MSGS);

		firstmsg = 32767;
		lastmsg = 0;

		for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)){
			register char *cp = dp->d_name;
			register int i = 0;

			if (dp->d_ino == 0)
				continue;
			if (dp->d_namlen == 0)
				continue;

			if (clean)
				snprintf(inbuf, sizeof(inbuf), "%s/%s", _PATH_MSGS, cp);

			while (isdigit(*cp))
				i = i * 10 + *cp++ - '0';
			if (*cp)
				continue;	/* not a message! */

			if (clean) {
				if (stat(inbuf, &stbuf) != 0)
					continue;
				if (stbuf.st_mtime < keep
				    && stbuf.st_mode&S_IWRITE) {
					unlink(inbuf);
					continue;
				}
			}

			if (i > lastmsg)
				lastmsg = i;
			if (i < firstmsg)
				firstmsg = i;
			seenany = YES;
		}
		closedir(dirp);

		if (!seenany) {
			if (blast != 0)	/* never lower the upper bound! */
				lastmsg = blast;
			firstmsg = lastmsg + 1;
		}
		else if (blast > lastmsg)
			lastmsg = blast;

		if (!send_msg) {
			bounds = fopen(fname, "w");
			if (bounds == NULL)
				err(errno, "%s", fname);
			chmod(fname, CMODE);
			fprintf(bounds, "%d %d\n", firstmsg, lastmsg);
			fclose(bounds);
		}
	}

	if (send_msg) {
		/*
		 * Send mode - place msgs in _PATH_MSGS
		 */
		bounds = fopen(fname, "w");
		if (bounds == NULL)
			err(errno, "%s", fname);

		nextmsg = lastmsg + 1;
		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, nextmsg);
		newmsg = fopen(fname, "w");
		if (newmsg == NULL)
			err(errno, "%s", fname);
		chmod(fname, CMODE);

		fprintf(bounds, "%d %d\n", firstmsg, nextmsg);
		fclose(bounds);

		sending = YES;
		if (ruptible)
			signal(SIGINT, onintr);

		if (isatty(fileno(stdin))) {
			ptr = getpwuid(uid)->pw_name;
			printf("Message %d:\nFrom %s %sSubject: ",
				nextmsg, ptr, ctime(&t));
			fflush(stdout);
			fgets(inbuf, sizeof inbuf, stdin);
			putchar('\n');
			fflush(stdout);
			fprintf(newmsg, "From %s %sSubject: %s\n",
				ptr, ctime(&t), inbuf);
			blankline = seensubj = YES;
		}
		else
			blankline = seensubj = NO;
		for (;;) {
			fgets(inbuf, sizeof inbuf, stdin);
			if (feof(stdin) || ferror(stdin))
				break;
			blankline = (blankline || (inbuf[0] == '\n'));
			seensubj = (seensubj || (!blankline && (strncmp(inbuf, "Subj", 4) == 0)));
			fputs(inbuf, newmsg);
		}
#ifdef OBJECT
		if (!seensubj) {
			printf("NOTICE: Messages should have a Subject field!\n");
#ifdef REJECT
			unlink(fname);
#endif
			exit(1);
		}
#endif
		exit(ferror(stdin));
	}
	if (clean)
		exit(0);

	/*
	 * prepare to display messages
	 */
	totty = (isatty(fileno(stdout)) != 0);
	use_pager = use_pager && totty;

	snprintf(fname, sizeof(fname), "%s/%s", getenv("HOME"), MSGSRC);
	msgsrc = fopen(fname, "r");
	if (msgsrc) {
		newrc = NO;
		fscanf(msgsrc, "%d\n", &nextmsg);
		fclose(msgsrc);
		if (nextmsg > lastmsg+1) {
			printf("Warning: bounds have been reset (%d, %d)\n",
				firstmsg, lastmsg);
			truncate(fname, (off_t)0);
			newrc = YES;
		}
		else if (!rcfirst)
			rcfirst = nextmsg - rcback;
	}
	else
		newrc = YES;
	msgsrc = fopen(fname, "r+");
	if (msgsrc == NULL)
		msgsrc = fopen(fname, "w");
	if (msgsrc == NULL)
		err(errno, "%s", fname);
	if (rcfirst) {
		if (rcfirst > lastmsg+1) {
			printf("Warning: the last message is number %d.\n",
				lastmsg);
			rcfirst = nextmsg;
		}
		if (rcfirst > firstmsg)
			firstmsg = rcfirst;	/* don't set below first msg */
	}
	if (newrc) {
		nextmsg = firstmsg;
		fseek(msgsrc, 0L, 0);
		fprintf(msgsrc, "%d\n", nextmsg);
		fflush(msgsrc);
	}

#ifdef V7
	if (totty) {
		struct winsize win;
		if (ioctl(fileno(stdout), TIOCGWINSZ, &win) != -1)
			Lpp = win.ws_row;
		if (Lpp <= 0) {
			if (tgetent(inbuf, getenv("TERM")) <= 0
			    || (Lpp = tgetnum("li")) <= 0) {
				Lpp = NLINES;
			}
		}
	}
#endif
	Lpp -= 6;	/* for headers, etc. */

	already = NO;
	prevmsg = firstmsg;
	printing = YES;
	if (ruptible)
		signal(SIGINT, onintr);

	/*
	 * Main program loop
	 */
	for (msg = firstmsg; msg <= lastmsg; msg++) {

		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, msg);
		newmsg = fopen(fname, "r");
		if (newmsg == NULL)
			continue;

		gfrsub(newmsg);		/* get From and Subject fields */
		if (locomode && !local) {
			fclose(newmsg);
			continue;
		}

		if (qopt) {	/* This has to be located here */
			printf("There are new messages.\n");
			exit(0);
		}

		if (already && !hdrs)
			putchar('\n');

		/*
		 * Print header
		 */
		if (totty)
			signal(SIGTSTP, onsusp);
		(void) setjmp(tstpbuf);
		already = YES;
		nlines = 2;
		if (seenfrom) {
			printf("Message %d:\nFrom %s %s", msg, from, date);
			nlines++;
		}
		if (seensubj) {
			printf("Subject: %s", subj);
			nlines++;
		}
		else {
			if (seenfrom) {
				putchar('\n');
				nlines++;
			}
			while (nlines < 6
			    && fgets(inbuf, sizeof inbuf, newmsg)
			    && inbuf[0] != '\n') {
				fputs(inbuf, stdout);
				nlines++;
			}
		}

		lct = linecnt(newmsg);
		if (lct)
			printf("(%d%sline%s) ", lct, seensubj? " " : " more ",
			    (lct == 1) ? "" : "s");

		if (hdrs) {
			printf("\n-----\n");
			fclose(newmsg);
			continue;
		}

		/*
		 * Ask user for command
		 */
		if (totty)
			ask(lct? MORE : (msg==lastmsg? NOMORE : NEXT));
		else
			inbuf[0] = 'y';
		if (totty)
			signal(SIGTSTP, SIG_DFL);
cmnd:
		in = inbuf;
		switch (*in) {
			case 'x':
			case 'X':
				exit(0);

			case 'q':
			case 'Q':
				quitit = YES;
				printf("--Postponed--\n");
				exit(0);
				/* intentional fall-thru */
			case 'n':
			case 'N':
				if (msg >= nextmsg) sep = "Flushed";
				prevmsg = msg;
				break;

			case 'p':
			case 'P':
				use_pager = (*in++ == 'p');
				/* intentional fallthru */
			case '\n':
			case 'y':
			default:
				if (*in == '-') {
					msg = prevmsg-1;
					sep = "replay";
					break;
				}
				if (isdigit(*in)) {
					msg = next(in);
					sep = in;
					break;
				}

				prmesg(nlines + lct + (seensubj? 1 : 0));
				prevmsg = msg;

		}

		printf("--%s--\n", sep);
		sep = "-";
		if (msg >= nextmsg) {
			nextmsg = msg + 1;
			fseek(msgsrc, 0L, 0);
			fprintf(msgsrc, "%d\n", nextmsg);
			fflush(msgsrc);
		}
		if (newmsg)
			fclose(newmsg);
		if (quitit)
			break;
	}

	/*
	 * Make sure .rc file gets updated
	 */
	if (--msg >= nextmsg) {
		nextmsg = msg + 1;
		fseek(msgsrc, 0L, 0);
		fprintf(msgsrc, "%d\n", nextmsg);
		fflush(msgsrc);
	}
	if (already && !quitit && lastcmd && totty) {
		/*
		 * save or reply to last message?
		 */
		msg = prevmsg;
		ask(NOMORE);
		if (inbuf[0] == '-' || isdigit(inbuf[0]))
			goto cmnd;
	}
	if (!(already || hush || qopt))
		printf("No new messages.\n");
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: msgs [fhlopq] [[-]number]\n");
	exit(1);
}

void
prmesg(int length)
{
	FILE *outf;
	char *env_pager;

	if (use_pager && length > Lpp) {
		signal(SIGPIPE, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		if ((env_pager = getenv("PAGER")) == NULL) {
			snprintf(cmdbuf, sizeof(cmdbuf), _PATH_PAGER, Lpp);
		} else {
			snprintf(cmdbuf, sizeof(cmdbuf), "%s", env_pager);
		}
		outf = popen(cmdbuf, "w");
		if (!outf)
			outf = stdout;
		else
			setbuf(outf, (char *)NULL);
	}
	else
		outf = stdout;

	if (seensubj)
		putc('\n', outf);

	while (fgets(inbuf, sizeof inbuf, newmsg)) {
		fputs(inbuf, outf);
		if (ferror(outf)) {
			clearerr(outf);
			break;
		}
	}

	if (outf != stdout) {
		pclose(outf);
		signal(SIGPIPE, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
	}
	else {
		fflush(stdout);
	}

	/* force wait on output */
	tcdrain(fileno(stdout));
}

void
onintr(int unused __unused)
{
	signal(SIGINT, onintr);
	if (mailing)
		unlink(fname);
	if (sending) {
		unlink(fname);
		puts("--Killed--");
		exit(1);
	}
	if (printing) {
		putchar('\n');
		if (hdrs)
			exit(0);
		sep = "Interrupt";
		if (newmsg)
			fseek(newmsg, 0L, 2);
		intrpflg = YES;
	}
}

/*
 * We have just gotten a susp.  Suspend and prepare to resume.
 */
void
onsusp(int unused __unused)
{
	signal(SIGTSTP, SIG_DFL);
	sigsetmask(0);
	kill(0, SIGTSTP);
	signal(SIGTSTP, onsusp);
	if (!mailing)
		longjmp(tstpbuf, 0);
}

int
linecnt(FILE *f)
{
	off_t oldpos = ftell(f);
	int l = 0;
	char lbuf[BUFSIZ];

	while (fgets(lbuf, sizeof lbuf, f))
		l++;
	clearerr(f);
	fseek(f, oldpos, 0);
	return (l);
}

int
next(char *buf)
{
	int i;
	sscanf(buf, "%d", &i);
	sprintf(buf, "Goto %d", i);
	return(--i);
}

void
ask(const char *prompt)
{
	char	inch;
	int	n, cmsg, fd;
	off_t	oldpos;
	FILE	*cpfrom, *cpto;

	printf("%s ", prompt);
	fflush(stdout);
	intrpflg = NO;
	(void) fgets(inbuf, sizeof inbuf, stdin);
	if ((n = strlen(inbuf)) > 0 && inbuf[n - 1] == '\n')
		inbuf[n - 1] = '\0';
	if (intrpflg)
		inbuf[0] = 'x';

	/*
	 * Handle 'mail' and 'save' here.
	 */
	if ((inch = inbuf[0]) == 's' || inch == 'm') {
		if (inbuf[1] == '-')
			cmsg = prevmsg;
		else if (isdigit(inbuf[1]))
			cmsg = atoi(&inbuf[1]);
		else
			cmsg = msg;
		snprintf(fname, sizeof(fname), "%s/%d", _PATH_MSGS, cmsg);

		oldpos = ftell(newmsg);

		cpfrom = fopen(fname, "r");
		if (!cpfrom) {
			printf("Message %d not found\n", cmsg);
			ask (prompt);
			return;
		}

		if (inch == 's') {
			in = nxtfld(inbuf);
			if (*in) {
				for (n=0; in[n] > ' '; n++) { /* sizeof fname? */
					fname[n] = in[n];
				}
				fname[n] = NULL;
			}
			else
				strcpy(fname, "Messages");
			fd = open(fname, O_RDWR|O_EXCL|O_CREAT|O_APPEND);
		}
		else {
			strcpy(fname, _PATH_TMP);
			fd = mkstemp(fname);
			if (fd != -1) {
				snprintf(cmdbuf, sizeof(cmdbuf), _PATH_MAIL,
				    fname);
				mailing = YES;
			}
		}
		if (fd == -1 || (cpto = fdopen(fd, "a")) == NULL) {
			if (fd != -1)
				close(fd);
			warn("%s", fname);
			mailing = NO;
			fseek(newmsg, oldpos, 0);
			ask(prompt);
			return;
		}

		while ((n = fread(inbuf, 1, sizeof inbuf, cpfrom)))
			fwrite(inbuf, 1, n, cpto);

		fclose(cpfrom);
		fclose(cpto);
		fseek(newmsg, oldpos, 0);	/* reposition current message */
		if (inch == 's')
			printf("Message %d saved in \"%s\"\n", cmsg, fname);
		else {
			system(cmdbuf);
			unlink(fname);
			mailing = NO;
		}
		ask(prompt);
	}
}

void
gfrsub(FILE *infile)
{
	off_t frompos;
	int count;

	seensubj = seenfrom = NO;
	local = YES;
	subj[0] = from[0] = date[0] = NULL;

	/*
	 * Is this a normal message?
	 */
	if (fgets(inbuf, sizeof inbuf, infile)) {
		if (strncmp(inbuf, "From", 4)==0) {
			/*
			 * expected form starts with From
			 */
			seenfrom = YES;
			frompos = ftell(infile);
			ptr = from;
			in = nxtfld(inbuf);
			if (*in) {
				count = sizeof(from) - 1;
				while (*in && *in > ' ' && count-- > 0) {
					if (*in == ':' || *in == '@' ||
					    *in == '!')
						local = NO;
					*ptr++ = *in++;
				}
			}
			*ptr = NULL;
			if (*(in = nxtfld(in)))
				strncpy(date, in, sizeof date);
			else {
				date[0] = '\n';
				date[1] = NULL;
			}
		}
		else {
			/*
			 * not the expected form
			 */
			fseek(infile, 0L, 0);
			return;
		}
	}
	else
		/*
		 * empty file ?
		 */
		return;

	/*
	 * look for Subject line until EOF or a blank line
	 */
	while (fgets(inbuf, sizeof inbuf, infile)
	    && !(blankline = (inbuf[0] == '\n'))) {
		/*
		 * extract Subject line
		 */
		if (!seensubj && strncmp(inbuf, "Subj", 4)==0) {
			seensubj = YES;
			frompos = ftell(infile);
			strncpy(subj, nxtfld(inbuf), sizeof subj);
		}
	}
	if (!blankline)
		/*
		 * ran into EOF
		 */
		fseek(infile, frompos, 0);

	if (!seensubj)
		/*
		 * for possible use with Mail
		 */
		strncpy(subj, "(No Subject)\n", sizeof subj);
}

char *
nxtfld(unsigned char *s)
{
	if (*s) while (*s && !isspace(*s)) s++;     /* skip over this field */
	if (*s) while (*s && isspace(*s)) s++;    /* find start of next field */
	return (s);
}
