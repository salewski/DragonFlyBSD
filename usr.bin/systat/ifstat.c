/*
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTIFSTAT_ERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/systat/ifstat.c,v 1.1 2003/01/04 22:07:24 phk Exp $
 * $DragonFly: src/usr.bin/systat/ifstat.c,v 1.2 2005/01/03 23:47:20 joerg Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <net/if_types.h>	/* For IFT_ETHER */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <float.h>
#include <err.h>
#include <errno.h>

#include "systat.h"
#include "extern.h"
#include "mode.h"
#include "convtbl.h"

                                /* Column numbers */

#define C1	0		/*  0-19 */
#define C2	20		/* 20-39 */ 
#define C3	40		/* 40-59 */
#define C4	60		/* 60-80 */
#define C5	80		/* Used for label positioning. */

static	int col0 = 0;
static	int col1 = C1;
static	int col2 = C2;
static	int col3 = C3;
static	int col4 = C4;
static	int col5 = C5;


SLIST_HEAD(, if_stat)		curlist;	
SLIST_HEAD(, if_stat_disp)	displist;

struct if_stat {
	SLIST_ENTRY(if_stat)	 link;
	char	if_name[IF_NAMESIZE];
	struct	ifmibdata if_mib;
	struct	timeval tv;
	struct	timeval tv_lastchanged;
	u_long	if_in_curtraffic;
	u_long	if_out_curtraffic;
	u_long	if_in_traffic_peak;
	u_long	if_out_traffic_peak;
	u_int	if_row;			/* Index into ifmib sysctl */
	u_int	if_ypos;		/* 0 if not being displayed */
	u_int	display;
};

extern	 u_int curscale;

static	 void  right_align_string(const struct if_stat *);
static	 void  getifmibdata(const int, struct ifmibdata *);
static	 void  sort_interface_list(void);
static	 u_int getifnum(void);

#define IFSTAT_ERR(n, s)	do {					\
	putchar('');							\
	closeifstat(wnd);						\
	err((n), (s));							\
} while (0)

#define STARTING_ROW	(8)
#define ROW_SPACING	(3)

#define TOPLINE 5
#define TOPLABEL \
"      Interface           Traffic               Peak                Total"

#define CLEAR_LINE(y, x)	do {					\
	wmove(wnd, y, x);						\
	wclrtoeol(wnd);							\
} while (0)

#define IN_col2		(ifp->if_in_curtraffic)
#define OUT_col2	(ifp->if_out_curtraffic)
#define IN_col3		(ifp->if_in_traffic_peak)
#define OUT_col3	(ifp->if_out_traffic_peak)
#define IN_col4		(ifp->if_mib.ifmd_data.ifi_ibytes)
#define OUT_col4	(ifp->if_mib.ifmd_data.ifi_obytes)

#define EMPTY_COLUMN 	"                    "
#define CLEAR_COLUMN(y, x)	mvprintw((y), (x), "%20s", EMPTY_COLUMN);

#define DOPUTRATE(c, r, d)	do {					\
	CLEAR_COLUMN(r, c);						\
	mvprintw(r, (c), "%10.3f %s%s  ",				\
		 convert(d##_##c, curscale),				\
		 get_string(d##_##c, curscale),				\
		 "/s");							\
} while (0)

#define DOPUTTOTAL(c, r, d)	do {					\
 	CLEAR_COLUMN((r), (c));						\
 	mvprintw((r), (c), "%12.3f %s  ",				\
 		 convert(d##_##c, SC_AUTO),				\
		 get_string(d##_##c, SC_AUTO));				\
} while (0)

#define PUTRATE(c, r)	do {						\
	DOPUTRATE(c, (r), IN);						\
	DOPUTRATE(c, (r)+1, OUT);					\
} while (0)

#define PUTTOTAL(c, r)	do {						\
	DOPUTTOTAL(c, (r), IN);						\
	DOPUTTOTAL(c, (r)+1, OUT);					\
} while (0)

#define PUTNAME(p) do {							\
	mvprintw(p->if_ypos, 0, "%s", p->if_name);			\
	mvprintw(p->if_ypos, col2-3, "%s", (const char *)"in");		\
	mvprintw(p->if_ypos+1, col2-3, "%s", (const char *)"out");	\
} while (0)


WINDOW *
openifstat(void)
{
	return (subwin(stdscr, LINES-1-5, 0, 5, 0));
}

void
closeifstat(WINDOW *w)
{
	struct if_stat	*node = NULL;

	while (!SLIST_EMPTY(&curlist)) {
		node = SLIST_FIRST(&curlist);
		SLIST_REMOVE_HEAD(&curlist, link);
		free(node);
	}

	if (w != NULL) {
		wclear(w);
		wrefresh(w);
		delwin(w);
	}

	return;
}


void
labelifstat(void)
{

	wmove(wnd, TOPLINE, 0);
	wclrtoeol(wnd);
	mvprintw(TOPLINE, 0, "%s", TOPLABEL);

	return;
}

void
showifstat(void)
{
	struct	if_stat *ifp = NULL;
	SLIST_FOREACH(ifp, &curlist, link) {
		if (ifp->display == 0)
			continue;
		PUTNAME(ifp);
		PUTRATE(col2, ifp->if_ypos);
		PUTRATE(col3, ifp->if_ypos);
		PUTTOTAL(col4, ifp->if_ypos);
	}

	return;
}

int 
initifstat(void)
{
	struct   if_stat *p = NULL;
	u_int	 n = 0, i = 0;

	n = getifnum();
	if (n <= 0)
		return -1;

	SLIST_INIT(&curlist);

	for (i = 0; i < n; i++) {
		p = (struct if_stat *)malloc(sizeof(struct if_stat));
		if (p == NULL)
			IFSTAT_ERR(1, "out of memory");
		memset((void *)p, 0, sizeof(struct if_stat));
		SLIST_INSERT_HEAD(&curlist, p, link);
		p->if_row = i+1;
		getifmibdata(p->if_row, &p->if_mib);
		right_align_string(p);

		/* 
		 * Initially, we only display interfaces that have
		 * received some traffic.
		 */
		if (p->if_mib.ifmd_data.ifi_ibytes != 0)
			p->display = 1;
	}

	sort_interface_list();

	return 1;
}

void
fetchifstat(void)
{
	struct	if_stat *ifp = NULL;
	struct	timeval tv, new_tv, old_tv;
	double	elapsed = 0.0;
	u_int	new_inb, new_outb, old_inb, old_outb = 0;
	u_int	error = 0;
	u_int	we_need_to_sort_interface_list = 0;

	SLIST_FOREACH(ifp, &curlist, link) {
		/* 
		 * Grab a copy of the old input/output values before we
		 * call getifmibdata().
		 */
		old_inb = ifp->if_mib.ifmd_data.ifi_ibytes;		
		old_outb = ifp->if_mib.ifmd_data.ifi_obytes;
		ifp->tv_lastchanged = ifp->if_mib.ifmd_data.ifi_lastchange;

		error = gettimeofday(&new_tv, (struct timezone *)0);
		if (error) 
			IFSTAT_ERR(2, "error getting time of day");
		(void)getifmibdata(ifp->if_row, &ifp->if_mib);


                new_inb = ifp->if_mib.ifmd_data.ifi_ibytes;
                new_outb = ifp->if_mib.ifmd_data.ifi_obytes;

		/* Display interface if it's received some traffic. */
		if (new_inb > 0 && old_inb == 0) {
			ifp->display = 1;
			we_need_to_sort_interface_list++;
		} 

		/*
		 * The rest is pretty trivial.  Calculate the new values
		 * for our current traffic rates, and while we're there,
		 * see if we have new peak rates.
		 */
                old_tv = ifp->tv;
                timersub(&new_tv, &old_tv, &tv);
                elapsed = tv.tv_sec + (tv.tv_usec * 1e-6);

		ifp->if_in_curtraffic = new_inb - old_inb;
		ifp->if_out_curtraffic = new_outb - old_outb;

		/*
		 * Rather than divide by the time specified on the comm-
		 * and line, we divide by ``elapsed'' as this is likely
		 * to be more accurate.
		 */
                ifp->if_in_curtraffic /= elapsed;
                ifp->if_out_curtraffic /= elapsed;

		if (ifp->if_in_curtraffic > ifp->if_in_traffic_peak)
			ifp->if_in_traffic_peak = ifp->if_in_curtraffic;

		if (ifp->if_out_curtraffic > ifp->if_out_traffic_peak)
			ifp->if_out_traffic_peak = ifp->if_out_curtraffic;

		ifp->tv.tv_sec = new_tv.tv_sec;
		ifp->tv.tv_usec = new_tv.tv_usec;

	}

	if (we_need_to_sort_interface_list)
		sort_interface_list();

	return;
}

/* 
 * We want to right justify our interface names against the first column
 * (first sixteen or so characters), so we need to do some alignment.
 */
static void
right_align_string(const struct if_stat *ifp)
{
	int	 str_len = 0, pad_len = 0;
	char	*newstr = NULL, *ptr = NULL;

	if (ifp == NULL || ifp->if_mib.ifmd_name == NULL)
		return;
	else {
		/* string length + '\0' */
		str_len = strlen(ifp->if_mib.ifmd_name)+1;
		pad_len = IF_NAMESIZE-(str_len);

		newstr = (char *)ifp->if_name;
		ptr = newstr + pad_len;
		(void)memset((void *)newstr, (int)' ', IF_NAMESIZE);
		(void)strncpy(ptr, (const char *)&ifp->if_mib.ifmd_name,
			      str_len);
	}

	return;
}

/*
 * This function iterates through our list of interfaces, identifying
 * those that are to be displayed (ifp->display = 1).  For each interf-
 * rface that we're displaying, we generate an appropriate position for
 * it on the screen (ifp->if_ypos).
 *
 * This function is called any time a change is made to an interface's
 * ``display'' state.
 */
void
sort_interface_list(void)
{
	struct	if_stat	*ifp = NULL;
	u_int	y = 0;	

	y = STARTING_ROW;
	SLIST_FOREACH(ifp, &curlist, link) {
		if (ifp->display) {
			ifp->if_ypos = y;
			y += ROW_SPACING;
		}
	}
}

static
unsigned int
getifnum(void)
{
	int	error   = 0;
	u_int	data    = 0;
	size_t	datalen = 0;
	static	int name[] = { CTL_NET,
			       PF_LINK,
			       NETLINK_GENERIC,
			       IFMIB_SYSTEM,
			       IFMIB_IFCOUNT };

	datalen = sizeof(data);
	error = sysctl(name, 
		       5,
		       (void *)&data,
		       (size_t *)&datalen,
		       (void *)NULL,
		       (size_t)0);
	if (error)
		IFSTAT_ERR(1, "sysctl error");
	return data;
}

static void 
getifmibdata(int row, struct ifmibdata *data)
{
	int	error   = 0;
	size_t	datalen = 0;
	static	int name[] = { CTL_NET,
			       PF_LINK,
			       NETLINK_GENERIC,
			       IFMIB_IFDATA,
			       0,
			       IFDATA_GENERAL };
	datalen = sizeof(*data);
	name[4] = row;

	errno = 0;
	error = sysctl(name,
		       6,
		       (void *)data,
		       (size_t *)&datalen,
		       (void *)NULL,
		       (size_t)0);
	if (error != 0 && errno != ENOENT)
		IFSTAT_ERR(2, "sysctl error getting interface data");
}

int
cmdifstat(char *cmd, char *args)
{
	int	retval = 0;

	retval = ifcmd(cmd, args);
	/* ifcmd() returns 1 on success */
	if (retval == 1) {
		showifstat();
		refresh();
	}

	return retval;
}
