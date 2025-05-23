/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/kbd/kbd.c,v 1.17.2.2 2001/07/30 16:46:43 yokota Exp $
 * $DragonFly: src/sys/dev/misc/kbd/kbd.c,v 1.14 2004/12/16 08:30:15 dillon Exp $
 */
/*
 * Generic keyboard driver.
 *
 * Interrupt note: keyboards use clist functions and since usb keyboard
 * interrupts are not protected by spltty(), we must use a critical section
 * to protect against corruption.
 */

#include "opt_kbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/poll.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/thread.h>
#include <sys/thread2.h>

#include <machine/console.h>

#include "kbdreg.h"

#define KBD_INDEX(dev)	minor(dev)

typedef struct genkbd_softc {
	int		gkb_flags;	/* flag/status bits */
#define KB_ASLEEP	(1 << 0)
	struct clist	gkb_q;		/* input queue */
	struct selinfo	gkb_rsel;
} genkbd_softc_t;

static	SLIST_HEAD(, keyboard_driver) keyboard_drivers =
 	SLIST_HEAD_INITIALIZER(keyboard_drivers);

SET_DECLARE(kbddriver_set, const keyboard_driver_t);

/* local arrays */

/*
 * We need at least one entry each in order to initialize a keyboard
 * for the kernel console.  The arrays will be increased dynamically
 * when necessary.
 */

static int		keyboards = 1;
static keyboard_t	*kbd_ini;
static keyboard_t	**keyboard = &kbd_ini;
static keyboard_switch_t *kbdsw_ini;
       keyboard_switch_t **kbdsw = &kbdsw_ini;

#define ARRAY_DELTA	4

static int
kbd_realloc_array(void)
{
	keyboard_t **new_kbd;
	keyboard_switch_t **new_kbdsw;
	int newsize;

	newsize = ((keyboards + ARRAY_DELTA)/ARRAY_DELTA)*ARRAY_DELTA;
	new_kbd = malloc(sizeof(*new_kbd) * newsize, M_DEVBUF,
				M_WAITOK | M_ZERO);
	new_kbdsw = malloc(sizeof(*new_kbdsw) * newsize, M_DEVBUF,
				M_WAITOK | M_ZERO);
	bcopy(keyboard, new_kbd, sizeof(*keyboard)*keyboards);
	bcopy(kbdsw, new_kbdsw, sizeof(*kbdsw)*keyboards);
	crit_enter();
	if (keyboards > 1) {
		free(keyboard, M_DEVBUF);
		free(kbdsw, M_DEVBUF);
	}
	keyboard = new_kbd;
	kbdsw = new_kbdsw;
	keyboards = newsize;
	crit_exit();

	if (bootverbose)
		printf("kbd: new array size %d\n", keyboards);

	return 0;
}

/*
 * Low-level keyboard driver functions.
 *
 * Keyboard subdrivers, such as the AT keyboard driver and the USB keyboard
 * driver, call these functions to initialize the keyboard_t structure
 * and register it to the virtual keyboard driver `kbd'.
 *
 * The reinit call is made when a driver has partially detached a keyboard
 * but does not unregistered it, then wishes to reinitialize it later on.
 * This is how the USB keyboard driver handles the 'default' keyboard,
 * because unregistering the keyboard associated with the console will
 * destroy its console association forever.
 */
void
kbd_reinit_struct(keyboard_t *kbd, int config, int pref)
{
	kbd->kb_flags |= KB_NO_DEVICE;	/* device has not been found */
	kbd->kb_config = config & ~KB_CONF_PROBE_ONLY;
	kbd->kb_led = 0;		/* unknown */
	kbd->kb_data = NULL;
	kbd->kb_keymap = NULL;
	kbd->kb_accentmap = NULL;
	kbd->kb_fkeytab = NULL;
	kbd->kb_fkeytab_size = 0;
	kbd->kb_delay1 = KB_DELAY1;	/* these values are advisory only */
	kbd->kb_delay2 = KB_DELAY2;
	kbd->kb_count = 0;
	kbd->kb_pref = pref;
	bzero(kbd->kb_lastact, sizeof(kbd->kb_lastact));
}

/* initialize the keyboard_t structure */
void
kbd_init_struct(keyboard_t *kbd, char *name, int type, int unit, int config,
		int pref, int port, int port_size)
{
	kbd->kb_flags = 0;
	kbd->kb_name = name;
	kbd->kb_type = type;
	kbd->kb_unit = unit;
	kbd->kb_io_base = port;
	kbd->kb_io_size = port_size;
	kbd_reinit_struct(kbd, config, pref);
}

void
kbd_set_maps(keyboard_t *kbd, keymap_t *keymap, accentmap_t *accmap,
	     fkeytab_t *fkeymap, int fkeymap_size)
{
	kbd->kb_keymap = keymap;
	kbd->kb_accentmap = accmap;
	kbd->kb_fkeytab = fkeymap;
	kbd->kb_fkeytab_size = fkeymap_size;
}

/* declare a new keyboard driver */
int
kbd_add_driver(keyboard_driver_t *driver)
{
	if (SLIST_NEXT(driver, link))
		return EINVAL;
	SLIST_INSERT_HEAD(&keyboard_drivers, driver, link);
	return 0;
}

int
kbd_delete_driver(keyboard_driver_t *driver)
{
	SLIST_REMOVE(&keyboard_drivers, driver, keyboard_driver, link);
	SLIST_NEXT(driver, link) = NULL;
	return 0;
}

/* register a keyboard and associate it with a function table */
int
kbd_register(keyboard_t *kbd)
{
	const keyboard_driver_t **list;
	const keyboard_driver_t *p;
	int index;

	for (index = 0; index < keyboards; ++index) {
		if (keyboard[index] == NULL)
			break;
	}
	if (index >= keyboards) {
		if (kbd_realloc_array())
			return -1;
	}

	kbd->kb_index = index;
	KBD_UNBUSY(kbd);
	KBD_VALID(kbd);
	kbd->kb_active = 0;	/* disabled until someone calls kbd_enable() */
	kbd->kb_token = NULL;
	kbd->kb_callback.kc_func = NULL;
	kbd->kb_callback.kc_arg = NULL;
	callout_init(&kbd->kb_atkbd_timeout_ch);

	SLIST_FOREACH(p, &keyboard_drivers, link) {
		if (strcmp(p->name, kbd->kb_name) == 0) {
			keyboard[index] = kbd;
			kbdsw[index] = p->kbdsw;
			return index;
		}
	}
	SET_FOREACH(list, kbddriver_set) {
		p = *list;
		if (strcmp(p->name, kbd->kb_name) == 0) {
			keyboard[index] = kbd;
			kbdsw[index] = p->kbdsw;
			return index;
		}
	}

	return -1;
}

int
kbd_unregister(keyboard_t *kbd)
{
	int error;

	if ((kbd->kb_index < 0) || (kbd->kb_index >= keyboards))
		return ENOENT;
	if (keyboard[kbd->kb_index] != kbd)
		return ENOENT;

	crit_enter();
	callout_stop(&kbd->kb_atkbd_timeout_ch);
	if (KBD_IS_BUSY(kbd)) {
		error = (*kbd->kb_callback.kc_func)(kbd, KBDIO_UNLOADING,
						    kbd->kb_callback.kc_arg);
		if (error) {
			crit_exit();
			return error;
		}
		if (KBD_IS_BUSY(kbd)) {
			crit_exit();
			return EBUSY;
		}
	}
	KBD_INVALID(kbd);
	keyboard[kbd->kb_index] = NULL;
	kbdsw[kbd->kb_index] = NULL;

	crit_exit();
	return 0;
}

/* find a funciton table by the driver name */
keyboard_switch_t
*kbd_get_switch(char *driver)
{
	const keyboard_driver_t **list;
	const keyboard_driver_t *p;

	SLIST_FOREACH(p, &keyboard_drivers, link) {
		if (strcmp(p->name, driver) == 0)
			return p->kbdsw;
	}
	SET_FOREACH(list, kbddriver_set) {
		p = *list;
		if (strcmp(p->name, driver) == 0)
			return p->kbdsw;
	}

	return NULL;
}

/*
 * Keyboard client functions
 * Keyboard clients, such as the console driver `syscons' and the keyboard
 * cdev driver, use these functions to claim and release a keyboard for
 * exclusive use.
 */

/* find the keyboard specified by a driver name and a unit number */
int
kbd_find_keyboard(char *driver, int unit)
{
	int i;
	int pref;
	int pref_index;

	pref = 0;
	pref_index = -1;

	for (i = 0; i < keyboards; ++i) {
		if (keyboard[i] == NULL)
			continue;
		if (!KBD_IS_VALID(keyboard[i]))
			continue;
		if (strcmp("*", driver) && strcmp(keyboard[i]->kb_name, driver))
			continue;
		if ((unit != -1) && (keyboard[i]->kb_unit != unit))
			continue;
		if (pref <= keyboard[i]->kb_pref) {
			pref = keyboard[i]->kb_pref;
			pref_index = i;
		}
	}
	return (pref_index);
}

/* allocate a keyboard */
int
kbd_allocate(char *driver, int unit, void *id, kbd_callback_func_t *func,
	     void *arg)
{
	int index;

	if (func == NULL)
		return -1;

	crit_enter();
	index = kbd_find_keyboard(driver, unit);
	if (index >= 0) {
		if (KBD_IS_BUSY(keyboard[index])) {
			crit_exit();
			return -1;
		}
		keyboard[index]->kb_token = id;
		KBD_BUSY(keyboard[index]);
		keyboard[index]->kb_callback.kc_func = func;
		keyboard[index]->kb_callback.kc_arg = arg;
		(*kbdsw[index]->clear_state)(keyboard[index]);
	}
	crit_exit();
	return index;
}

int
kbd_release(keyboard_t *kbd, void *id)
{
	int error;

	crit_enter();
	if (!KBD_IS_VALID(kbd) || !KBD_IS_BUSY(kbd)) {
		error = EINVAL;
	} else if (kbd->kb_token != id) {
		error = EPERM;
	} else {
		kbd->kb_token = NULL;
		KBD_UNBUSY(kbd);
		kbd->kb_callback.kc_func = NULL;
		kbd->kb_callback.kc_arg = NULL;
		(*kbdsw[kbd->kb_index]->clear_state)(kbd);
		error = 0;
	}
	crit_exit();
	return error;
}

int
kbd_change_callback(keyboard_t *kbd, void *id, kbd_callback_func_t *func,
		    void *arg)
{
	int error;

	crit_enter();
	if (!KBD_IS_VALID(kbd) || !KBD_IS_BUSY(kbd)) {
		error = EINVAL;
	} else if (kbd->kb_token != id) {
		error = EPERM;
	} else if (func == NULL) {
		error = EINVAL;
	} else {
		kbd->kb_callback.kc_func = func;
		kbd->kb_callback.kc_arg = arg;
		error = 0;
	}
	crit_exit();
	return error;
}

/* get a keyboard structure */
keyboard_t
*kbd_get_keyboard(int index)
{
	if ((index < 0) || (index >= keyboards))
		return NULL;
	if (keyboard[index] == NULL)
		return NULL;
	if (!KBD_IS_VALID(keyboard[index]))
		return NULL;
	return keyboard[index];
}

/*
 * The back door for the console driver; configure keyboards
 * This function is for the kernel console to initialize keyboards
 * at very early stage.
 */

int
kbd_configure(int flags)
{
	const keyboard_driver_t **list;
	const keyboard_driver_t *p;

	SLIST_FOREACH(p, &keyboard_drivers, link) {
		if (p->configure != NULL)
			(*p->configure)(flags);
	}
	SET_FOREACH(list, kbddriver_set) {
		p = *list;
		if (p->configure != NULL)
			(*p->configure)(flags);
	}

	return 0;
}

#ifdef KBD_INSTALL_CDEV

/*
 * Virtual keyboard cdev driver functions
 * The virtual keyboard driver dispatches driver functions to
 * appropriate subdrivers.
 */

#define KBD_UNIT(dev)	minor(dev)

static d_open_t		genkbdopen;
static d_close_t	genkbdclose;
static d_read_t		genkbdread;
static d_write_t	genkbdwrite;
static d_ioctl_t	genkbdioctl;
static d_poll_t		genkbdpoll;

#define CDEV_MAJOR	112

static struct cdevsw kbd_cdevsw = {
	/* name */	"kbd",
	/* maj */	CDEV_MAJOR,
	/* flags */	0,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */	genkbdopen,
	/* close */	genkbdclose,
	/* read */	genkbdread,
	/* write */	genkbdwrite,
	/* ioctl */	genkbdioctl,
	/* poll */	genkbdpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};

int
kbd_attach(keyboard_t *kbd)
{
	dev_t dev;

	if (kbd->kb_index >= keyboards)
		return EINVAL;
	if (keyboard[kbd->kb_index] != kbd)
		return EINVAL;

	cdevsw_add(&kbd_cdevsw, -1, kbd->kb_index);
	dev = make_dev(&kbd_cdevsw, kbd->kb_index, UID_ROOT, GID_WHEEL, 0600,
		       "kbd%r", kbd->kb_index);
	if (dev->si_drv1 == NULL)
		dev->si_drv1 = malloc(sizeof(genkbd_softc_t), M_DEVBUF,
				      M_WAITOK);
	bzero(dev->si_drv1, sizeof(genkbd_softc_t));

	printf("kbd%d at %s%d\n", kbd->kb_index, kbd->kb_name, kbd->kb_unit);
	return 0;
}

int
kbd_detach(keyboard_t *kbd)
{
	dev_t dev;

	if (kbd->kb_index >= keyboards)
		return EINVAL;
	if (keyboard[kbd->kb_index] != kbd)
		return EINVAL;

	/*
	 * Deal with refs properly.  The KBD driver really ought to have
	 * recorded the dev_t separately.
	 */
	if ((dev = make_adhoc_dev(&kbd_cdevsw, kbd->kb_index)) != NODEV) {
		if (dev->si_drv1) {
			free(dev->si_drv1, M_DEVBUF);
			dev->si_drv1 = NULL;
		}
	}
	cdevsw_remove(&kbd_cdevsw, -1, kbd->kb_index);
	return 0;
}

/*
 * Generic keyboard cdev driver functions
 * Keyboard subdrivers may call these functions to implement common
 * driver functions.
 */

#define KB_QSIZE	512
#define KB_BUFSIZE	64

static kbd_callback_func_t genkbd_event;

static int
genkbdopen(dev_t dev, int mode, int flag, d_thread_t *td)
{
	keyboard_t *kbd;
	genkbd_softc_t *sc;
	int i;

	crit_enter();
	sc = dev->si_drv1;
	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((sc == NULL) || (kbd == NULL) || !KBD_IS_VALID(kbd)) {
		crit_exit();
		return ENXIO;
	}
	i = kbd_allocate(kbd->kb_name, kbd->kb_unit, sc,
			 genkbd_event, (void *)sc);
	if (i < 0) {
		crit_exit();
		return EBUSY;
	}
	/* assert(i == kbd->kb_index) */
	/* assert(kbd == kbd_get_keyboard(i)) */

	/*
	 * NOTE: even when we have successfully claimed a keyboard,
	 * the device may still be missing (!KBD_HAS_DEVICE(kbd)).
	 */

#if 0
	bzero(&sc->gkb_q, sizeof(sc->gkb_q));
#endif
	clist_alloc_cblocks(&sc->gkb_q, KB_QSIZE, KB_QSIZE/2); /* XXX */
	sc->gkb_rsel.si_flags = 0;
	sc->gkb_rsel.si_pid = 0;
	crit_exit();

	return 0;
}

static int
genkbdclose(dev_t dev, int mode, int flag, d_thread_t *td)
{
	keyboard_t *kbd;
	genkbd_softc_t *sc;

	/*
	 * NOTE: the device may have already become invalid.
	 * kbd == NULL || !KBD_IS_VALID(kbd)
	 */
	crit_enter();
	sc = dev->si_drv1;
	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((sc == NULL) || (kbd == NULL) || !KBD_IS_VALID(kbd)) {
		/* XXX: we shall be forgiving and don't report error... */
	} else {
		kbd_release(kbd, (void *)sc);
#if 0
		clist_free_cblocks(&sc->gkb_q);
#endif
	}
	crit_exit();
	return 0;
}

static int
genkbdread(dev_t dev, struct uio *uio, int flag)
{
	keyboard_t *kbd;
	genkbd_softc_t *sc;
	u_char buffer[KB_BUFSIZE];
	int len;
	int error;

	/* wait for input */
	crit_enter();
	sc = dev->si_drv1;
	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((sc == NULL) || (kbd == NULL) || !KBD_IS_VALID(kbd)) {
		crit_exit();
		return ENXIO;
	}
	while (sc->gkb_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			crit_exit();
			return EWOULDBLOCK;
		}
		sc->gkb_flags |= KB_ASLEEP;
		error = tsleep((caddr_t)sc, PCATCH, "kbdrea", 0);
		kbd = kbd_get_keyboard(KBD_INDEX(dev));
		if ((kbd == NULL) || !KBD_IS_VALID(kbd)) {
			crit_exit();
			return ENXIO;	/* our keyboard has gone... */
		}
		if (error) {
			sc->gkb_flags &= ~KB_ASLEEP;
			crit_exit();
			return error;
		}
	}
	crit_exit();

	/* copy as much input as possible */
	error = 0;
	while (uio->uio_resid > 0) {
		len = imin(uio->uio_resid, sizeof(buffer));
		len = q_to_b(&sc->gkb_q, buffer, len);
		if (len <= 0)
			break;
		error = uiomove(buffer, len, uio);
		if (error)
			break;
	}

	return error;
}

static int
genkbdwrite(dev_t dev, struct uio *uio, int flag)
{
	keyboard_t *kbd;

	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((kbd == NULL) || !KBD_IS_VALID(kbd))
		return ENXIO;
	return ENODEV;
}

static int
genkbdioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, d_thread_t *td)
{
	keyboard_t *kbd;
	int error;

	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((kbd == NULL) || !KBD_IS_VALID(kbd))
		return ENXIO;
	error = (*kbdsw[kbd->kb_index]->ioctl)(kbd, cmd, arg);
	if (error == ENOIOCTL)
		error = ENODEV;
	return error;
}

static int
genkbdpoll(dev_t dev, int events, d_thread_t *td)
{
	keyboard_t *kbd;
	genkbd_softc_t *sc;
	int revents;

	revents = 0;
	crit_enter();
	sc = dev->si_drv1;
	kbd = kbd_get_keyboard(KBD_INDEX(dev));
	if ((sc == NULL) || (kbd == NULL) || !KBD_IS_VALID(kbd)) {
		revents =  POLLHUP;	/* the keyboard has gone */
	} else if (events & (POLLIN | POLLRDNORM)) {
		if (sc->gkb_q.c_cc > 0)
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->gkb_rsel);
	}
	crit_exit();
	return revents;
}

static int
genkbd_event(keyboard_t *kbd, int event, void *arg)
{
	genkbd_softc_t *sc;
	size_t len;
	u_char *cp;
	int mode;
	int c;

	/* assert(KBD_IS_VALID(kbd)) */
	sc = (genkbd_softc_t *)arg;

	switch (event) {
	case KBDIO_KEYINPUT:
		break;
	case KBDIO_UNLOADING:
		/* the keyboard is going... */
		kbd_release(kbd, (void *)sc);
		if (sc->gkb_flags & KB_ASLEEP) {
			sc->gkb_flags &= ~KB_ASLEEP;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->gkb_rsel);
		return 0;
	default:
		return EINVAL;
	}

	/* obtain the current key input mode */
	if ((*kbdsw[kbd->kb_index]->ioctl)(kbd, KDGKBMODE, (caddr_t)&mode))
		mode = K_XLATE;

	/* read all pending input */
	while ((*kbdsw[kbd->kb_index]->check_char)(kbd)) {
		c = (*kbdsw[kbd->kb_index]->read_char)(kbd, FALSE);
		if (c == NOKEY)
			continue;
		if (c == ERRKEY)	/* XXX: ring bell? */
			continue;
		if (!KBD_IS_BUSY(kbd))
			/* the device is not open, discard the input */
			continue;

		/* store the byte as is for K_RAW and K_CODE modes */
		if (mode != K_XLATE) {
			putc(KEYCHAR(c), &sc->gkb_q);
			continue;
		}

		/* K_XLATE */
		if (c & RELKEY)	/* key release is ignored */
			continue;

		/* process special keys; most of them are just ignored... */
		if (c & SPCLKEY) {
			switch (KEYCHAR(c)) {
			default:
				/* ignore them... */
				continue;
			case BTAB:	/* a backtab: ESC [ Z */
				putc(0x1b, &sc->gkb_q);
				putc('[', &sc->gkb_q);
				putc('Z', &sc->gkb_q);
				continue;
			}
		}

		/* normal chars, normal chars with the META, function keys */
		switch (KEYFLAGS(c)) {
		case 0:			/* a normal char */
			putc(KEYCHAR(c), &sc->gkb_q);
			break;
		case MKEY:		/* the META flag: prepend ESC */
			putc(0x1b, &sc->gkb_q);
			putc(KEYCHAR(c), &sc->gkb_q);
			break;
		case FKEY | SPCLKEY:	/* a function key, return string */
			cp = (*kbdsw[kbd->kb_index]->get_fkeystr)(kbd,
							KEYCHAR(c), &len);
			if (cp != NULL) {
				while (len-- >  0)
					putc(*cp++, &sc->gkb_q);
			}
			break;
		}
	}

	/* wake up sleeping/polling processes */
	if (sc->gkb_q.c_cc > 0) {
		if (sc->gkb_flags & KB_ASLEEP) {
			sc->gkb_flags &= ~KB_ASLEEP;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->gkb_rsel);
	}

	return 0;
}

#endif /* KBD_INSTALL_CDEV */

/*
 * Generic low-level keyboard functions
 * The low-level functions in the keyboard subdriver may use these
 * functions.
 */

int
genkbd_commonioctl(keyboard_t *kbd, u_long cmd, caddr_t arg)
{
	keyarg_t *keyp;
	fkeyarg_t *fkeyp;
	int i;

	crit_enter();
	switch (cmd) {

	case KDGKBINFO:		/* get keyboard information */
		((keyboard_info_t *)arg)->kb_index = kbd->kb_index;
		i = imin(strlen(kbd->kb_name) + 1,
			 sizeof(((keyboard_info_t *)arg)->kb_name));
		bcopy(kbd->kb_name, ((keyboard_info_t *)arg)->kb_name, i);
		((keyboard_info_t *)arg)->kb_unit = kbd->kb_unit;
		((keyboard_info_t *)arg)->kb_type = kbd->kb_type;
		((keyboard_info_t *)arg)->kb_config = kbd->kb_config;
		((keyboard_info_t *)arg)->kb_flags = kbd->kb_flags;
		break;

	case KDGKBTYPE:		/* get keyboard type */
		*(int *)arg = kbd->kb_type;
		break;

	case KDGETREPEAT:	/* get keyboard repeat rate */
		((int *)arg)[0] = kbd->kb_delay1;
		((int *)arg)[1] = kbd->kb_delay2; 
		break;

	case GIO_KEYMAP:	/* get keyboard translation table */
		bcopy(kbd->kb_keymap, arg, sizeof(*kbd->kb_keymap));
		break;
	case PIO_KEYMAP:	/* set keyboard translation table */
#ifndef KBD_DISABLE_KEYMAP_LOAD
		bzero(kbd->kb_accentmap, sizeof(*kbd->kb_accentmap));
		bcopy(arg, kbd->kb_keymap, sizeof(*kbd->kb_keymap));
		break;
#else
		crit_exit();
		return ENODEV;
#endif

	case GIO_KEYMAPENT:	/* get keyboard translation table entry */
		keyp = (keyarg_t *)arg;
		if (keyp->keynum >= sizeof(kbd->kb_keymap->key)
					/sizeof(kbd->kb_keymap->key[0])) {
			crit_exit();
			return EINVAL;
		}
		bcopy(&kbd->kb_keymap->key[keyp->keynum], &keyp->key,
		      sizeof(keyp->key));
		break;
	case PIO_KEYMAPENT:	/* set keyboard translation table entry */
#ifndef KBD_DISABLE_KEYMAP_LOAD
		keyp = (keyarg_t *)arg;
		if (keyp->keynum >= sizeof(kbd->kb_keymap->key)
					/sizeof(kbd->kb_keymap->key[0])) {
			crit_exit();
			return EINVAL;
		}
		bcopy(&keyp->key, &kbd->kb_keymap->key[keyp->keynum],
		      sizeof(keyp->key));
		break;
#else
		crit_exit();
		return ENODEV;
#endif

	case GIO_DEADKEYMAP:	/* get accent key translation table */
		bcopy(kbd->kb_accentmap, arg, sizeof(*kbd->kb_accentmap));
		break;
	case PIO_DEADKEYMAP:	/* set accent key translation table */
#ifndef KBD_DISABLE_KEYMAP_LOAD
		bcopy(arg, kbd->kb_accentmap, sizeof(*kbd->kb_accentmap));
		break;
#else
		crit_exit();
		return ENODEV;
#endif

	case GETFKEY:		/* get functionkey string */
		fkeyp = (fkeyarg_t *)arg;
		if (fkeyp->keynum >= kbd->kb_fkeytab_size) {
			crit_exit();
			return EINVAL;
		}
		bcopy(kbd->kb_fkeytab[fkeyp->keynum].str, fkeyp->keydef,
		      kbd->kb_fkeytab[fkeyp->keynum].len);
		fkeyp->flen = kbd->kb_fkeytab[fkeyp->keynum].len;
		break;
	case SETFKEY:		/* set functionkey string */
#ifndef KBD_DISABLE_KEYMAP_LOAD
		fkeyp = (fkeyarg_t *)arg;
		if (fkeyp->keynum >= kbd->kb_fkeytab_size) {
			crit_exit();
			return EINVAL;
		}
		kbd->kb_fkeytab[fkeyp->keynum].len = imin(fkeyp->flen, MAXFK);
		bcopy(fkeyp->keydef, kbd->kb_fkeytab[fkeyp->keynum].str,
		      kbd->kb_fkeytab[fkeyp->keynum].len);
		break;
#else
		crit_exit();
		return ENODEV;
#endif

	default:
		crit_exit();
		return ENOIOCTL;
	}

	crit_exit();
	return 0;
}

/* get a pointer to the string associated with the given function key */
u_char
*genkbd_get_fkeystr(keyboard_t *kbd, int fkey, size_t *len)
{
	if (kbd == NULL)
		return NULL;
	fkey -= F_FN;
	if (fkey > kbd->kb_fkeytab_size)
		return NULL;
	*len = kbd->kb_fkeytab[fkey].len;
	return kbd->kb_fkeytab[fkey].str;
}

/* diagnostic dump */
static char
*get_kbd_type_name(int type)
{
	static struct {
		int type;
		char *name;
	} name_table[] = {
		{ KB_84,	"AT 84" },
		{ KB_101,	"AT 101/102" },
		{ KB_OTHER,	"generic" },
	};
	int i;

	for (i = 0; i < sizeof(name_table)/sizeof(name_table[0]); ++i) {
		if (type == name_table[i].type)
			return name_table[i].name;
	}
	return "unknown";
}

void
genkbd_diag(keyboard_t *kbd, int level)
{
	if (level > 0) {
		printf("kbd%d: %s%d, %s (%d), config:0x%x, flags:0x%x", 
		       kbd->kb_index, kbd->kb_name, kbd->kb_unit,
		       get_kbd_type_name(kbd->kb_type), kbd->kb_type,
		       kbd->kb_config, kbd->kb_flags);
		if (kbd->kb_io_base > 0)
			printf(", port:0x%x-0x%x", kbd->kb_io_base, 
			       kbd->kb_io_base + kbd->kb_io_size - 1);
		printf("\n");
	}
}

#define set_lockkey_state(k, s, l)				\
	if (!((s) & l ## DOWN)) {				\
		int i;						\
		(s) |= l ## DOWN;				\
		(s) ^= l ## ED;					\
		i = (s) & LOCK_MASK;				\
		(*kbdsw[(k)->kb_index]->ioctl)((k), KDSETLED, (caddr_t)&i); \
	}

static u_int
save_accent_key(keyboard_t *kbd, u_int key, int *accents)
{
	int i;

	/* make an index into the accent map */
	i = key - F_ACC + 1;
	if ((i > kbd->kb_accentmap->n_accs)
	    || (kbd->kb_accentmap->acc[i - 1].accchar == 0)) {
		/* the index is out of range or pointing to an empty entry */
		*accents = 0;
		return ERRKEY;
	}

	/* 
	 * If the same accent key has been hit twice, produce the accent char
	 * itself.
	 */
	if (i == *accents) {
		key = kbd->kb_accentmap->acc[i - 1].accchar;
		*accents = 0;
		return key;
	}

	/* remember the index and wait for the next key  */
	*accents = i; 
	return NOKEY;
}

static u_int
make_accent_char(keyboard_t *kbd, u_int ch, int *accents)
{
	struct acc_t *acc;
	int i;

	acc = &kbd->kb_accentmap->acc[*accents - 1];
	*accents = 0;

	/* 
	 * If the accent key is followed by the space key,
	 * produce the accent char itself.
	 */
	if (ch == ' ')
		return acc->accchar;

	/* scan the accent map */
	for (i = 0; i < NUM_ACCENTCHARS; ++i) {
		if (acc->map[i][0] == 0)	/* end of table */
			break;
		if (acc->map[i][0] == ch)
			return acc->map[i][1];
	}
	/* this char cannot be accented... */
	return ERRKEY;
}

int
genkbd_keyaction(keyboard_t *kbd, int keycode, int up, int *shiftstate,
		 int *accents)
{
	struct keyent_t *key;
	int state = *shiftstate;
	int action;
	int f;
	int i;

	i = keycode;
	f = state & (AGRS | ALKED);
	if ((f == AGRS1) || (f == AGRS2) || (f == ALKED))
		i += ALTGR_OFFSET;
	key = &kbd->kb_keymap->key[i];
	i = ((state & SHIFTS) ? 1 : 0)
	    | ((state & CTLS) ? 2 : 0)
	    | ((state & ALTS) ? 4 : 0);
	if (((key->flgs & FLAG_LOCK_C) && (state & CLKED))
		|| ((key->flgs & FLAG_LOCK_N) && (state & NLKED)) )
		i ^= 1;

	if (up) {	/* break: key released */
		action = kbd->kb_lastact[keycode];
		kbd->kb_lastact[keycode] = NOP;
		switch (action) {
		case LSHA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = LSH;
			/* FALL THROUGH */
		case LSH:
			state &= ~SHIFTS1;
			break;
		case RSHA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = RSH;
			/* FALL THROUGH */
		case RSH:
			state &= ~SHIFTS2;
			break;
		case LCTRA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = LCTR;
			/* FALL THROUGH */
		case LCTR:
			state &= ~CTLS1;
			break;
		case RCTRA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = RCTR;
			/* FALL THROUGH */
		case RCTR:
			state &= ~CTLS2;
			break;
		case LALTA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = LALT;
			/* FALL THROUGH */
		case LALT:
			state &= ~ALTS1;
			break;
		case RALTA:
			if (state & SHIFTAON) {
				set_lockkey_state(kbd, state, ALK);
				state &= ~ALKDOWN;
			}
			action = RALT;
			/* FALL THROUGH */
		case RALT:
			state &= ~ALTS2;
			break;
		case ASH:
			state &= ~AGRS1;
			break;
		case META:
			state &= ~METAS1;
			break;
		case NLK:
			state &= ~NLKDOWN;
			break;
		case CLK:
#ifndef PC98
			state &= ~CLKDOWN;
#else
			state &= ~CLKED;
			i = state & LOCK_MASK;
			(*kbdsw[kbd->kb_index]->ioctl)(kbd, KDSETLED,
						       (caddr_t)&i);
#endif
			break;
		case SLK:
			state &= ~SLKDOWN;
			break;
		case ALK:
			state &= ~ALKDOWN;
			break;
		case NOP:
			/* release events of regular keys are not reported */
			*shiftstate &= ~SHIFTAON;
			return NOKEY;
		}
		*shiftstate = state & ~SHIFTAON;
		return (SPCLKEY | RELKEY | action);
	} else {	/* make: key pressed */
		action = key->map[i];
		state &= ~SHIFTAON;
		if (key->spcl & (0x80 >> i)) {
			/* special keys */
			if (kbd->kb_lastact[keycode] == NOP)
				kbd->kb_lastact[keycode] = action;
			if (kbd->kb_lastact[keycode] != action)
				action = NOP;
			switch (action) {
			/* LOCKING KEYS */
			case NLK:
				set_lockkey_state(kbd, state, NLK);
				break;
			case CLK:
#ifndef PC98
				set_lockkey_state(kbd, state, CLK);
#else
				state |= CLKED;
				i = state & LOCK_MASK;
				(*kbdsw[kbd->kb_index]->ioctl)(kbd, KDSETLED,
							       (caddr_t)&i);
#endif
				break;
			case SLK:
				set_lockkey_state(kbd, state, SLK);
				break;
			case ALK:
				set_lockkey_state(kbd, state, ALK);
				break;
			/* NON-LOCKING KEYS */
			case SPSC: case RBT:  case SUSP: case STBY:
			case DBG:  case NEXT: case PREV: case PNC:
			case HALT: case PDWN:
				*accents = 0;
				break;
			case BTAB:
				*accents = 0;
				action |= BKEY;
				break;
			case LSHA:
				state |= SHIFTAON;
				action = LSH;
				/* FALL THROUGH */
			case LSH:
				state |= SHIFTS1;
				break;
			case RSHA:
				state |= SHIFTAON;
				action = RSH;
				/* FALL THROUGH */
			case RSH:
				state |= SHIFTS2;
				break;
			case LCTRA:
				state |= SHIFTAON;
				action = LCTR;
				/* FALL THROUGH */
			case LCTR:
				state |= CTLS1;
				break;
			case RCTRA:
				state |= SHIFTAON;
				action = RCTR;
				/* FALL THROUGH */
			case RCTR:
				state |= CTLS2;
				break;
			case LALTA:
				state |= SHIFTAON;
				action = LALT;
				/* FALL THROUGH */
			case LALT:
				state |= ALTS1;
				break;
			case RALTA:
				state |= SHIFTAON;
				action = RALT;
				/* FALL THROUGH */
			case RALT:
				state |= ALTS2;
				break;
			case ASH:
				state |= AGRS1;
				break;
			case META:
				state |= METAS1;
				break;
			case NOP:
				*shiftstate = state;
				return NOKEY;
			default:
				/* is this an accent (dead) key? */
				*shiftstate = state;
				if (action >= F_ACC && action <= L_ACC) {
					action = save_accent_key(kbd, action,
								 accents);
					switch (action) {
					case NOKEY:
					case ERRKEY:
						return action;
					default:
						if (state & METAS)
							return (action | MKEY);
						else
							return action;
					}
					/* NOT REACHED */
				}
				/* other special keys */
				if (*accents > 0) {
					*accents = 0;
					return ERRKEY;
				}
				if (action >= F_FN && action <= L_FN)
					action |= FKEY;
				/* XXX: return fkey string for the FKEY? */
				return (SPCLKEY | action);
			}
			*shiftstate = state;
			return (SPCLKEY | action);
		} else {
			/* regular keys */
			kbd->kb_lastact[keycode] = NOP;
			*shiftstate = state;
			if (*accents > 0) {
				/* make an accented char */
				action = make_accent_char(kbd, action, accents);
				if (action == ERRKEY)
					return action;
			}
			if (state & METAS)
				action |= MKEY;
			return action;
		}
	}
	/* NOT REACHED */
}
