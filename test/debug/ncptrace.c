/*
 * NCPTRACE.C
 *
 * cc -I/usr/src/sys ncptrace.c -o /usr/local/bin/ncptrace -lkvm
 *
 * ncptrace
 * ncptrace [path]
 *
 * Trace and dump the kernel namecache hierarchy.  If a path is specified
 * the trace begins there, otherwise the trace begins at the root.
 *
 *
 * Copyright (c) 2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $DragonFly: src/test/debug/ncptrace.c,v 1.6 2005/03/24 20:15:11 dillon Exp $
 */

#define _KERNEL_STRUCTURES
#include <sys/param.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/namecache.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/swap_pager.h>
#include <vm/vnode_pager.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <getopt.h>

struct nlist Nl[] = {
    { "_rootncp" },
    { NULL }
};

void kkread(kvm_t *kd, u_long addr, void *buf, size_t nbytes);
void dumpncp(kvm_t *kd, int tab, struct namecache *ncptr, const char *path);

main(int ac, char **av)
{
    struct namecache *ncptr;
    kvm_t *kd;
    const char *corefile = NULL;
    const char *sysfile = NULL;
    int ch;
    int i;

    while ((ch = getopt(ac, av, "M:N:")) != -1) {
	switch(ch) {
	case 'M':
	    corefile = optarg;
	    break;
	case 'N':
	    sysfile = optarg;
	    break;
	default:
	    fprintf(stderr, "%s [-M core] [-N system]\n", av[0]);
	    exit(1);
	}
    }
    ac -= optind;
    av += optind;

    if ((kd = kvm_open(sysfile, corefile, NULL, O_RDONLY, "kvm:")) == NULL) {
	perror("kvm_open");
	exit(1);
    }
    if (kvm_nlist(kd, Nl) != 0) {
	perror("kvm_nlist");
	exit(1);
    }
    kkread(kd, Nl[0].n_value, &ncptr, sizeof(ncptr));
    if (ac == 0) {
	dumpncp(kd, 0, ncptr, NULL);
    } else {
	for (i = 0; i < ac; ++i) {
	    if (av[i][0] != '/')
		fprintf(stderr, "%s: path must start at the root\n", av[i]);
	    dumpncp(kd, 0, ncptr, av[i]);
	}
    }
}

void
dumpncp(kvm_t *kd, int tab, struct namecache *ncptr, const char *path)
{
    struct namecache ncp;
    struct namecache *ncscan;
    const char *ptr;
    int haschildren;
    char name[256];

    kkread(kd, (u_long)ncptr, &ncp, sizeof(ncp));
    if (ncp.nc_nlen < sizeof(name)) {
	kkread(kd, (u_long)ncp.nc_name, name, ncp.nc_nlen);
	name[ncp.nc_nlen] = 0;
    } else {
	name[0] = 0;
    }
    if (tab == 0) {
	strcpy(name, "ROOT");
	if (path)
	    ++path;
    } else if (ncp.nc_flag & NCF_MOUNTPT) {
	strcpy(name, "MOUNTGLUE");
    } else if (name[0] == 0) {
	strcpy(name, "?");
	if (path)
	    return;
    } else if (path) {
	if ((ptr = strchr(path, '/')) == NULL)
	    ptr = path + strlen(path);
	if (strlen(name) != ptr - path ||
	    bcmp(name, path, ptr - path) != 0
	) {
	    return;
	}
	path = ptr;
	if (*path == '/')
	    ++path;
	if (*path == 0)
	    path = NULL;
    }

    if (ncp.nc_list.tqh_first)
	haschildren = 1;
    else
	haschildren = 0;

    if (path)
	printf("ELM ");
    else
	printf("%*.*s%s ", tab, tab, "", name);
    printf("[ncp=%p par=%p %04x vp=%p", 
	    ncptr, ncp.nc_parent, ncp.nc_flag, ncp.nc_vp);
    if (ncp.nc_timeout)
	printf(" timo=%d", ncp.nc_timeout);
    if (ncp.nc_refs)
	printf(" refs=%d", ncp.nc_refs);
    if ((ncp.nc_flag & NCF_UNRESOLVED) == 0 && ncp.nc_error)
	printf(" error=%d", ncp.nc_error);
    if (ncp.nc_exlocks)
	printf(" LOCKED(%d,td=%p)", ncp.nc_exlocks, ncp.nc_locktd);
    printf("]");

    if (path) {
	printf(" %s\n", name);
    } else {
	printf("%s\n", haschildren ? " {" : "");
    }
    for (ncscan = ncp.nc_list.tqh_first; ncscan; ncscan = ncp.nc_entry.tqe_next) {
	kkread(kd, (u_long)ncscan, &ncp, sizeof(ncp));
	dumpncp(kd, (path ? (tab ? tab : 4) : tab + 4), ncscan, path);
    }
    if (haschildren && path == NULL)
	printf("%*.*s}\n", tab, tab, "");
}

void
kkread(kvm_t *kd, u_long addr, void *buf, size_t nbytes)
{
    if (kvm_read(kd, addr, buf, nbytes) != nbytes) {
        perror("kvm_read");
        exit(1);
    }
}

