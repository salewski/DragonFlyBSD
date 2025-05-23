/* 
 *  gloadavg.c - get load average for Linux
 *  Copyright (C) 1993  Thomas Koenig
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
 * $FreeBSD: src/libexec/atrun/gloadavg.c,v 1.5 1999/08/28 00:09:12 peter Exp $
 * $DragonFly: src/libexec/atrun/gloadavg.c,v 1.3 2004/02/03 23:24:50 rob Exp $
 */

#ifndef __DragonFly__
#define _POSIX_SOURCE 1

/* System Headers */

#include <stdio.h>
#else
#include <stdlib.h>
#endif

/* Local headers */

#include "gloadavg.h"

/* Global functions */

void perr(const char *a);

double
gloadavg(void)
/* return the current load average as a floating point number, or <0 for
 * error
 */
{
    double result;
#ifndef __DragonFly__
    FILE *fp;
    
    if((fp=fopen(PROC_DIR "loadavg","r")) == NULL)
	result = -1.0;
    else
    {
	if(fscanf(fp,"%lf",&result) != 1)
	    result = -1.0;
	fclose(fp);
    }
#else
    if (getloadavg(&result, 1) != 1)
	    perr("error in getloadavg");
#endif
    return result;
}
