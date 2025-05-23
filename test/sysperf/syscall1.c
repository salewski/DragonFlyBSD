/*
 * syscall1.c
 *
 * $DragonFly: src/test/sysperf/syscall1.c,v 1.1 2003/08/12 02:29:44 dillon Exp $
 */

#include "blib.h"

int
main(int ac, char **av)
{
    long long count = 0;
    long long max;
    int j;

    printf("timing standard getuid() syscall\n");

    start_timing();
    while (stop_timing(0, NULL) == 0) {
	for (j = 0; j < 100; ++j)
	    getuid();
	count += 100;
    }
    max = count;
    start_timing();
    for (count = 0; count < max; count += 100) {
	for (j = 0; j < 100; ++j)
	    getuid();
    }
    stop_timing(count, "getuid()");
    return(0);
}

