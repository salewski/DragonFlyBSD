/*-
 * Copyright (c) 1997 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/module.h,v 1.14.2.3 2002/03/17 11:07:45 alfred Exp $
 * $DragonFly: src/sys/sys/module.h,v 1.9 2005/03/30 04:29:32 swildner Exp $
 */

#ifndef _SYS_MODULE_H_
#define _SYS_MODULE_H_

/*
 * Module metadata types
 */
#define MDT_DEPEND		1	/* argument is a module name */
#define MDT_MODULE		2	/* module declaration */
#define MDT_VERSION		3	/* module version(s) */

#define MDT_STRUCT_VERSION      1	/* version of metadata structure */
#define MDT_SETNAME		"modmetadata_set"

typedef enum modeventtype {
    MOD_LOAD,
    MOD_UNLOAD,
    MOD_SHUTDOWN
} modeventtype_t;

typedef	struct module *module_t;

typedef	int (*modeventhand_t)(module_t, int /*modeventtype_t*/, void *);

/*
 * Struct for registering modules statically via SYSINIT.
 */
typedef struct moduledata {
	const char	*name;	/* module name */
	modeventhand_t	evhand;	/* event handler */
	void		*priv;	/* extra data */
} moduledata_t;

/*
 * A module can use this to report module specific data to
 * the user via kldstat(2).
 */
typedef union modspecific {
    int		intval;
    u_int	uintval;
    long	longval;
    u_long	ulongval;
} modspecific_t;

/*
 * Module dependency declaration
 */
struct mod_depend {
	int	md_ver_minimum;
	int	md_ver_preferred;
	int	md_ver_maximum;
};
  
/*
 * Module version declaration
 */
struct mod_version {  
	int	mv_version;
};
  
struct mod_metadata {   
	int		md_version;     /* structure version MDTV_* */
	int		md_type;        /* type of entry MDT_* */
	void		*md_data;       /* specific data */
	const char	*md_cval;       /* common string label */
};

#ifdef _KERNEL

#define MODULE_METADATA(uniquifier, type, data, cval)			\
	static struct mod_metadata _mod_metadata##uniquifier = {	\
		MDT_STRUCT_VERSION,					\
		type,							\
		data,							\
		cval							\
	};								\
	DATA_SET(modmetadata_set, _mod_metadata##uniquifier)

#define MODULE_DEPEND(module, mdepend, vmin, vpref, vmax)		\
	static struct mod_depend _##module##_depend_on_##mdepend = {	\
		vmin,							\
		vpref,							\
		vmax							\
	};								\
	MODULE_METADATA(_md_##module##_on_##mdepend, MDT_DEPEND,	\
	    &_##module##_depend_on_##mdepend, #mdepend)

#define DECLARE_MODULE(name, data, sub, order) 				\
    MODULE_METADATA(_md_##name, MDT_MODULE, &data, #name);		\
    SYSINIT(name##module, sub, order, module_register_init, &data) 	\
    struct __hack

#define MODULE_VERSION(module, version)					\
	static struct mod_version _##module##_version = {		\
		version							\
	};								\
	MODULE_METADATA(_##module##_version, MDT_VERSION,		\
	    &_##module##_version, #module)

/*
 * This is used to declare a module name that is the same as the KLD
 * name so the kernel can avoid double-loading modules.  It is typically
 * necessary when a module is made up of several bus drivers each with
 * its own separate sysinit.
 */
#define DECLARE_DUMMY_MODULE(name)					\
    MODULE_METADATA(_md_##name, MDT_MODULE, NULL, #name)

void module_register_init(const void *data);
struct linker_file;
int module_register(const struct moduledata *data, struct linker_file *lf);
module_t module_lookupbyname(const char *name);
module_t module_lookupbyid(int modid);
void module_reference(module_t mod);
int module_release(module_t mod);
int module_unload(module_t mod);
int module_getid(module_t mod);
module_t module_getfnext(module_t mod);
void module_setspecific(module_t mod, modspecific_t *datap);

#ifdef MOD_DEBUG

extern int mod_debug;
#define MOD_DEBUG_REFS	1

#define MOD_DPF(cat, args)					\
	do {							\
		if (mod_debug & MOD_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define MOD_DPF(cat, args)

#endif

#endif /* _KERNEL */

#define MAXMODNAME	32

struct module_stat {
    int		version;	/* set to sizeof(struct module_stat) */
    char	name[MAXMODNAME];
    int		refs;
    int		id;
    modspecific_t data;
};

#ifndef _KERNEL

#include <sys/cdefs.h>

__BEGIN_DECLS
int	modnext(int);
int	modfnext(int);
int	modstat(int, struct module_stat *);
int	modfind(const char *);
__END_DECLS

#endif

#endif	/* !_SYS_MODULE_H_ */
