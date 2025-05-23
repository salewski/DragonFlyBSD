#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>

#include <err.h>
#include <sysexits.h>
#include "prutil.h"

/*
 * $FreeBSD: src/tools/regression/p1003_1b/prutil.c,v 1.1 2000/02/16 14:28:41 dufault Exp $
 * $DragonFly: src/tools/regression/p1003_1b/prutil.c,v 1.2 2003/06/17 04:29:11 dillon Exp $
 */
void quit(const char *text)
{
	err(errno, text);
}

char *sched_text(int scheduler)
{
	switch(scheduler)
	{
		case SCHED_FIFO:
		return "SCHED_FIFO";

		case SCHED_RR:
		return "SCHED_RR";

		case SCHED_OTHER:
		return "SCHED_OTHER";

		default:
		return "Illegal scheduler value";
	}
}

int sched_is(int line, struct sched_param *p, int shouldbe)
{
	int scheduler;
	struct sched_param param;

	/* What scheduler are we running now?
	 */
	errno = 0;
	scheduler = sched_getscheduler(0);
	if (sched_getparam(0, &param))
		quit("sched_getparam");

	if (p)
		*p = param;

	if (shouldbe != -1 && scheduler != shouldbe)
	{
		fprintf(stderr,
		"At line %d the scheduler should be %s yet it is %s.\n",
		line, sched_text(shouldbe), sched_text(scheduler));

		exit(-1);
	}

	return scheduler;
}
