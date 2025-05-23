/* 
 *  atrun.c - run jobs queued by at; run with root privileges.
 *  Copyright (C) 1993, 1994 Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/libexec/atrun/atrun.c,v 1.14.2.1 2001/03/05 10:53:23 kris Exp $
 * $DragonFly: src/libexec/atrun/atrun.c,v 1.5 2004/09/20 19:32:19 joerg Exp $
 */

/* System Headers */

#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <paths.h>

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

/* Local headers */

#include "gloadavg.h"
#include "privs.h"

/* Macros */

#ifndef ATJOB_DIR
#define ATJOB_DIR "/usr/spool/atjobs/"
#endif

#ifndef ATSPOOL_DIR
#define ATSPOOL_DIR "/usr/spool/atspool/"
#endif

#ifndef LOADAVG_MX
#define LOADAVG_MX 1.5
#endif

uid_t real_uid, effective_uid;
gid_t real_gid, effective_gid;

/* File scope variables */

static int debug = 0;

void		perr(const char *a);
static void	usage(void);

/* Local functions */
static int
write_string(int fd, const char* a)
{
    return write(fd, a, strlen(a));
}

#undef DEBUG_FORK
#ifdef DEBUG_FORK
static pid_t
myfork(void)
{
	pid_t res;
	res = fork();
	if (res == 0)
	    kill(getpid(),SIGSTOP);
	return res;
}

#define fork myfork
#endif

static void
run_file(const char *filename, uid_t uid, gid_t gid)
{
/* Run a file by by spawning off a process which redirects I/O,
 * spawns a subshell, then waits for it to complete and sends
 * mail to the user.
 */
    pid_t pid;
    int fd_out, fd_in;
    int queue;
    char mailbuf[LOGNAMESIZE + 1], fmt[49];
    char *mailname = NULL;
    FILE *stream;
    int send_mail = 0;
    struct stat buf, lbuf;
    off_t size;
    struct passwd *pentry;
    int fflags;
    uid_t nuid;
    gid_t ngid;


    PRIV_START

    if (chmod(filename, S_IRUSR) != 0)
    {
	perr("cannot change file permissions");
    }

    PRIV_END

    pid = fork();
    if (pid == -1)
	perr("cannot fork");
    
    else if (pid != 0)
	return;

    /* Let's see who we mail to.  Hopefully, we can read it from
     * the command file; if not, send it to the owner, or, failing that,
     * to root.
     */

    pentry = getpwuid(uid);
    if (pentry == NULL)
    {
	syslog(LOG_ERR,"Userid %lu not found - aborting job %s",
	       (unsigned long) uid, filename);
        exit(EXIT_FAILURE);
    }
    PRIV_START

    stream=fopen(filename, "r");

    PRIV_END

#ifdef __DragonFly__
    if (pentry->pw_expire && time(NULL) >= pentry->pw_expire)
    {
	syslog(LOG_ERR, "Userid %lu is expired - aborting job %s",
		(unsigned long) uid, filename);
	exit(EXIT_FAILURE);
    }
#endif

    if (stream == NULL)
	perr("cannot open input file");

    if ((fd_in = dup(fileno(stream))) <0)
	perr("error duplicating input file descriptor");

    if (fstat(fd_in, &buf) == -1)
	perr("error in fstat of input file descriptor");

    if (lstat(filename, &lbuf) == -1)
	perr("error in fstat of input file");

    if (S_ISLNK(lbuf.st_mode)) {
	syslog(LOG_ERR,"Symbolic link encountered in job %s - aborting",
		filename);
	exit(EXIT_FAILURE);
    }
    if ((lbuf.st_dev != buf.st_dev) || (lbuf.st_ino != buf.st_ino) ||
        (lbuf.st_uid != buf.st_uid) || (lbuf.st_gid != buf.st_gid) ||
        (lbuf.st_size!=buf.st_size)) {
	syslog(LOG_ERR,"Somebody changed files from under us for job %s - "
	"aborting",filename);
	exit(EXIT_FAILURE);
    }
    if (buf.st_nlink > 1) {
	syslog(LOG_ERR,"Someboy is trying to run a linked script for job %s",
		filename);
	exit(EXIT_FAILURE);
    }
    if ((fflags = fcntl(fd_in, F_GETFD)) <0)
	perr("error in fcntl");

    fcntl(fd_in, F_SETFD, fflags & ~FD_CLOEXEC);

    snprintf(fmt, sizeof(fmt),
	"#!/bin/sh\n# atrun uid=%%ld gid=%%ld\n# mail %%%ds %%d",
                          LOGNAMESIZE);
    if (fscanf(stream, fmt, &nuid, &ngid, mailbuf, &send_mail) != 4) {
	syslog(LOG_ERR,"File %s is in wrong format - aborting", filename);
	exit(EXIT_FAILURE);
    }
    if (mailbuf[0] == '-') {
	syslog(LOG_ERR,"illegal mail name %s in %s",mailbuf,filename);
	exit(EXIT_FAILURE);
    }
    mailname = mailbuf;
    if (nuid != uid) {
	syslog(LOG_ERR,"Job %s - userid %u does not match file uid %lu",
		filename, nuid, (unsigned long)uid);
	exit(EXIT_FAILURE);
    }
    if (ngid != gid) {
	syslog(LOG_ERR,"Job %s - groupid %u does not match file gid %lu",
		filename, ngid, (unsigned long)gid);
	exit(EXIT_FAILURE);
    }
    fclose(stream);
    if (chdir(ATSPOOL_DIR) < 0)
	perr("cannot chdir to " ATSPOOL_DIR);
    
    /* Create a file to hold the output of the job we are about to run.
     * Write the mail header.
     */    
    if((fd_out=open(filename,
		O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0)
	perr("cannot create output file");

    write_string(fd_out, "Subject: Output from your job ");
    write_string(fd_out, filename);
    write_string(fd_out, "\n\n");
    fstat(fd_out, &buf);
    size = buf.st_size;

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
 
    pid = fork();
    if (pid < 0)
	perr("error in fork");

    else if (pid == 0)
    {
	char *nul = NULL;
	char **nenvp = &nul;

	/* Set up things for the child; we want standard input from the input file,
	 * and standard output and error sent to our output file.
	 */

	if (lseek(fd_in, (off_t) 0, SEEK_SET) < 0)
	    perr("error in lseek");

	if (dup(fd_in) != STDIN_FILENO)
	    perr("error in I/O redirection");

	if (dup(fd_out) != STDOUT_FILENO)
	    perr("error in I/O redirection");

	if (dup(fd_out) != STDERR_FILENO)
	    perr("error in I/O redirection");

	close(fd_in);
	close(fd_out);
	if (chdir(ATJOB_DIR) < 0)
	    perr("cannot chdir to " ATJOB_DIR);

	queue = *filename;

	PRIV_START

        nice(tolower(queue) - 'a');
	
	if (initgroups(pentry->pw_name,pentry->pw_gid))
	    perr("cannot delete saved userids");

	if (setgid(gid) < 0 || setegid(pentry->pw_gid) < 0)
	    perr("cannot change group");

	if (setlogin(pentry->pw_name))
	    perr("cannot set login name");

	if (setuid(uid) < 0 || seteuid(uid) < 0)
	    perr("cannot set user id");

	if (chdir(pentry->pw_dir))
		chdir("/");

	if(execle("/bin/sh","sh",(char *) NULL, nenvp) != 0)
	    perr("exec failed for /bin/sh");

	PRIV_END
    }
    /* We're the parent.  Let's wait.
     */
    close(fd_in);
    close(fd_out);
    waitpid(pid, (int *) NULL, 0);

    /* Send mail.  Unlink the output file first, so it is deleted after
     * the run.
     */
    stat(filename, &buf);
    if (open(filename, O_RDONLY) != STDIN_FILENO)
        perr("open of jobfile failed");

    unlink(filename);
    if ((buf.st_size != size) || send_mail)
    {    
	PRIV_START

	if (initgroups(pentry->pw_name,pentry->pw_gid))
	    perr("cannot delete saved userids");

	if (setgid(gid) < 0 || setegid(pentry->pw_gid) < 0)
	    perr("cannot change group");

	if (setlogin(pentry->pw_name))
	    perr("cannot set login name");

	if (setuid(uid) < 0 || seteuid(uid) < 0)
	    perr("cannot set user id");

	if (chdir(pentry->pw_dir))
		chdir("/");

	execl(_PATH_SENDMAIL, "sendmail", "-F", "Atrun Service",
			"-odi", "-oem",
			mailname, (char *) NULL);
	    perr("exec failed for mail command");

	PRIV_END
    }
    exit(EXIT_SUCCESS);
}

/* Global functions */

/* Needed in gloadavg.c */
void
perr(const char *a)
{
    if (debug)
    {
	warn("%s", a);
    }
    else
	syslog(LOG_ERR, "%s: %m", a);

    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
/* Browse through  ATJOB_DIR, checking all the jobfiles wether they should
 * be executed and or deleted. The queue is coded into the first byte of
 * the job filename, the date (in minutes since Eon) as a hex number in the
 * following eight bytes, followed by a dot and a serial number.  A file
 * which has not been executed yet is denoted by its execute - bit set.
 * For those files which are to be executed, run_file() is called, which forks
 * off a child which takes care of I/O redirection, forks off another child
 * for execution and yet another one, optionally, for sending mail.
 * Files which already have run are removed during the next invocation.
 */
    DIR *spool;
    struct dirent *dirent;
    struct stat buf;
    unsigned long ctm;
    unsigned long jobno;
    char queue;
    time_t now, run_time;
    char batch_name[] = "Z2345678901234";
    uid_t batch_uid;
    gid_t batch_gid;
    int c;
    int run_batch;
    double load_avg = LOADAVG_MX;

/* We don't need root privileges all the time; running under uid and gid daemon
 * is fine.
 */

    RELINQUISH_PRIVS_ROOT(DAEMON_UID, DAEMON_GID)

    openlog("atrun", LOG_PID, LOG_CRON);

    opterr = 0;
    while((c=getopt(argc, argv, "dl:"))!= -1)
    {
	switch (c)
	{
	case 'l': 
	    if (sscanf(optarg, "%lf", &load_avg) != 1)
		perr("garbled option -l");
	    if (load_avg <= 0.)
		load_avg = LOADAVG_MX;
	    break;

	case 'd':
	    debug ++;
	    break;

	case '?':
	default:
	    usage();
	}
    }

    if (chdir(ATJOB_DIR) != 0)
	perr("cannot change to " ATJOB_DIR);

    /* Main loop. Open spool directory for reading and look over all the
     * files in there. If the filename indicates that the job should be run
     * and the x bit is set, fork off a child which sets its user and group
     * id to that of the files and exec a /bin/sh which executes the shell
     * script. Unlink older files if they should no longer be run.  For
     * deletion, their r bit has to be turned on.
     *
     * Also, pick the oldest batch job to run, at most one per invocation of
     * atrun.
     */
    if ((spool = opendir(".")) == NULL)
	perr("cannot read " ATJOB_DIR);

    now = time(NULL);
    run_batch = 0;
    batch_uid = (uid_t) -1;
    batch_gid = (gid_t) -1;

    while ((dirent = readdir(spool)) != NULL) {
	if (stat(dirent->d_name,&buf) != 0)
	    perr("cannot stat in " ATJOB_DIR);

	/* We don't want directories
	 */
	if (!S_ISREG(buf.st_mode)) 
	    continue;

	if (sscanf(dirent->d_name,"%c%5lx%8lx",&queue,&jobno,&ctm) != 3)
	    continue;

	run_time = (time_t) ctm*60;

	if ((S_IXUSR & buf.st_mode) && (run_time <=now)) {
	    if (isupper(queue) && (strcmp(batch_name,dirent->d_name) > 0)) {
		run_batch = 1;
		strncpy(batch_name, dirent->d_name, sizeof(batch_name));
		batch_uid = buf.st_uid;
		batch_gid = buf.st_gid;
	    }
	
	/* The file is executable and old enough
	 */
	    if (islower(queue))
		run_file(dirent->d_name, buf.st_uid, buf.st_gid);
	}
	/*  Delete older files
	 */
	if ((run_time < now) && !(S_IXUSR & buf.st_mode) && (S_IRUSR & buf.st_mode))
	    unlink(dirent->d_name);
    }
    /* run the single batch file, if any
    */
    if (run_batch && (gloadavg() < load_avg))
	run_file(batch_name, batch_uid, batch_gid);

    closelog();
    exit(EXIT_SUCCESS);
}

static void
usage()
{
    if (debug)
	fprintf(stderr, "usage: atrun [-l load_avg] [-d]\n");
    else
	syslog(LOG_ERR, "usage: atrun [-l load_avg] [-d]"); 

    exit(EXIT_FAILURE);
}
