/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/i386/include/smp.h,v 1.50.2.5 2001/02/13 22:32:45 tegge Exp $
 * $DragonFly: src/sys/i386/include/Attic/smp.h,v 1.11 2004/03/01 06:33:16 dillon Exp $
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && !APIC_IO */

#if defined(SMP) || defined(APIC_IO)

#ifndef LOCORE

/*
 * For sending values to POST displays.
 * XXX FIXME: where does this really belong, isa.h/isa.c perhaps?
 */
extern int current_postcode;  /** XXX currently in mp_machdep.c */
#define POSTCODE(X)	current_postcode = (X), \
			outb(0x80, current_postcode)
#define POSTCODE_LO(X)	current_postcode &= 0xf0, \
			current_postcode |= ((X) & 0x0f), \
			outb(0x80, current_postcode)
#define POSTCODE_HI(X)	current_postcode &= 0x0f, \
			current_postcode |= (((X) << 4) & 0xf0), \
			outb(0x80, current_postcode)


#include "apicreg.h"

/* global data in mpboot.s */
extern int			bootMP_size;

/* functions in mpboot.s */
void	bootMP			(void);

/* global data in apic_vector.s */
extern volatile u_int		stopped_cpus;
extern volatile u_int		started_cpus;

extern volatile u_int		checkstate_probed_cpus;
extern void (*cpustop_restartfunc) (void);

/* functions in apic_ipl.s */
void	apic_eoi		(void);
u_int	io_apic_read		(int, int);
void	io_apic_write		(int, int, u_int);

/* global data in mp_machdep.c */
extern int			bsp_apic_ready;
extern int			mp_naps;
extern int			mp_nbusses;
extern int			mp_napics;
extern int			mp_picmode;
extern int			boot_cpu_id;
extern vm_offset_t		cpu_apic_address;
extern vm_offset_t		io_apic_address[];
extern u_int32_t		cpu_apic_versions[];
extern u_int32_t		*io_apic_versions;
extern int			cpu_num_to_apic_id[];
extern int			io_num_to_apic_id[];
extern int			apic_id_to_logical[];
#define APIC_INTMAPSIZE 24
struct apic_intmapinfo {
  	int ioapic;
	int int_pin;
	volatile void *apic_address;
	int redirindex;
};
extern struct apic_intmapinfo	int_to_apicintpin[];
extern struct pcb		stoppcbs[];

/* functions in mp_machdep.c */
u_int	mp_bootaddress		(u_int);
int	mp_probe		(void);
void	mp_start		(void);
void	mp_announce		(void);
u_int	isa_apic_mask		(u_int);
int	isa_apic_irq		(int);
int	pci_apic_irq		(int, int, int);
int	apic_irq		(int, int);
int	next_apic_irq		(int);
int	undirect_isa_irq	(int);
int	undirect_pci_irq	(int);
int	apic_bus_type		(int);
int	apic_src_bus_id		(int, int);
int	apic_src_bus_irq	(int, int);
int	apic_int_type		(int, int);
int	apic_trigger		(int, int);
int	apic_polarity		(int, int);
void	assign_apic_irq		(int apic, int intpin, int irq);
void	revoke_apic_irq		(int irq);
void	bsp_apic_configure	(void);
void	init_secondary		(void);
int	stop_cpus		(u_int);
void	ap_init			(void);
int	restart_cpus		(u_int);
void	forward_signal		(struct proc *);
void	forward_roundrobin	(void);
#ifdef	APIC_INTR_REORDER
void	set_lapic_isrloc	(int, int);
#endif /* APIC_INTR_REORDER */
void	smp_rendezvous_action	(void);
void	smp_rendezvous		(void (*)(void *), 
				     void (*)(void *),
				     void (*)(void *),
				     void *arg);

/* global data in mpapic.c */
extern volatile lapic_t		lapic;
extern volatile ioapic_t	**ioapic;

/* functions in mpapic.c */
void	apic_dump		(char*);
void	apic_initialize		(void);
void	imen_dump		(void);
int	apic_ipi		(int, int, int);
void	selected_apic_ipi	(u_int, int, int);
void	single_apic_ipi(int cpu, int vector, int delivery_mode);
int	single_apic_ipi_passive(int cpu, int vector, int delivery_mode);
int	io_apic_setup		(int);
void	io_apic_setup_intpin	(int, int);
void	io_apic_set_id		(int, int);
int	io_apic_get_id		(int);
int	ext_int_setup		(int, int);

#if defined(READY)
void	clr_io_apic_mask24	(int, u_int32_t);
void	set_io_apic_mask24	(int, u_int32_t);
#endif /* READY */

void	set_apic_timer		(int);
int	read_apic_timer		(void);
void	u_sleep			(int);
void	cpu_send_ipiq		(int);
int	cpu_send_ipiq_passive	(int);

/* global data in init_smp.c */
extern cpumask_t		smp_active_mask;

#endif /* !LOCORE */
#else	/* !SMP && !APIC_IO */

#define	smp_active_mask	1	/* smp_active_mask always 1 on UP machines */

#endif

#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
