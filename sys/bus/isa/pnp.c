/*
 * Copyright (c) 1996, Sujal M. Patel
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
 *	$FreeBSD: src/sys/isa/pnp.c,v 1.5.2.1 2002/10/14 09:31:09 nyan Exp $
 *	$DragonFly: src/sys/bus/isa/pnp.c,v 1.5 2004/04/07 05:54:32 dillon Exp $
 *      from: pnp.c,v 1.11 1999/05/06 22:11:19 peter Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include "isavar.h"
#include "pnpreg.h"
#include "pnpvar.h"
#include <machine/clock.h>

typedef struct _pnp_id {
	u_int32_t vendor_id;
	u_int32_t serial;
	u_char checksum;
} pnp_id;

struct pnp_set_config_arg {
	int	csn;		/* Card number to configure */
	int	ldn;		/* Logical device on card */
};

struct pnp_quirk {
	u_int32_t vendor_id;	/* Vendor of the card */
	u_int32_t logical_id;	/* ID of the device with quirk */
	int	type;
	int	arg1;
	int	arg2;
};

#define PNP_QUIRK_WRITE_REG	1 /* Need to write a pnp register  */
#define PNP_QUIRK_EXTRA_IO	2 /* Has extra io ports */

struct pnp_quirk pnp_quirks[] = {
        /*
         * The Gravis UltraSound needs register 0xf2 to be set to 0xff
         * to enable power.
         * XXX need to know the logical device id.
         */
        { 0x0100561e /* GRV0001 */,     0,
          PNP_QUIRK_WRITE_REG,  0xf2,    0xff },
        /*
         * An emu8000 does not give us other than the first
         * port.
         */
        { 0x0100561e /* GRV0001 */,     0,
          PNP_QUIRK_WRITE_REG,  0xf2,    0xff },
        /*
         * An emu8000 does not give us other than the first
         * port.
         */
        { 0x26008c0e /* SB16 */,        0x21008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0x42008c0e /* SB32(CTL0042) */,       0x21008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0x44008c0e /* SB32(CTL0044) */,       0x21008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0x49008c0e /* SB32(CTL0049) */,       0x21008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0xf1008c0e /* SB32(CTL00f1) */,       0x21008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0xc1008c0e /* SB64(CTL00c1) */,       0x22008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0xc5008c0e /* SB64(CTL00c5) */,       0x22008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },
        { 0xe4008c0e /* SB64(CTL00e4) */,       0x22008c0e,
          PNP_QUIRK_EXTRA_IO,   0x400,   0x800 },

        { 0 }
};

#ifdef PC98
/* Some NEC PnP cards have 9 bytes serial code. */
static pnp_id necids[] = {
	{0x4180a3b8, 0xffffffff, 0x00},	/* PC-9801CB-B04 (NEC8041) */
	{0x5181a3b8, 0xffffffff, 0x46},	/* PC-9821CB2-B04(NEC8151) */
	{0x5182a3b8, 0xffffffff, 0xb8},	/* PC-9801-XX    (NEC8251) */
	{0x9181a3b8, 0xffffffff, 0x00},	/* PC-9801-120   (NEC8191) */
	{0, 0, 0}
};
#endif

#if 0
/*
 * these entries are initialized using the autoconfig menu
 * The struct is invalid (and must be initialized) if the first
 * CSN is zero. The init code fills invalid entries with CSN 255
 * which is not a supported value.
 */

struct pnp_cinfo pnp_ldn_overrides[MAX_PNP_LDN] = {
    { 0 }
};
#endif

/* The READ_DATA port that we are using currently */
static int pnp_rd_port;

static void   pnp_send_initiation_key(void);
static int    pnp_get_serial(pnp_id *p);
static int    pnp_isolation_protocol(device_t parent);

char *
pnp_eisaformat(u_int32_t id)
{
	u_int8_t *data = (u_int8_t *) &id;
	static char idbuf[8];
	const char  hextoascii[] = "0123456789abcdef";

	idbuf[0] = '@' + ((data[0] & 0x7c) >> 2);
	idbuf[1] = '@' + (((data[0] & 0x3) << 3) + ((data[1] & 0xe0) >> 5));
	idbuf[2] = '@' + (data[1] & 0x1f);
	idbuf[3] = hextoascii[(data[2] >> 4)];
	idbuf[4] = hextoascii[(data[2] & 0xf)];
	idbuf[5] = hextoascii[(data[3] >> 4)];
	idbuf[6] = hextoascii[(data[3] & 0xf)];
	idbuf[7] = 0;
	return(idbuf);
}

static void
pnp_write(int d, u_char r)
{
	outb (_PNP_ADDRESS, d);
	outb (_PNP_WRITE_DATA, r);
}

#if 0

static u_char
pnp_read(int d)
{
	outb (_PNP_ADDRESS, d);
	return (inb(3 | (pnp_rd_port <<2)));
}

#endif

/*
 * Send Initiation LFSR as described in "Plug and Play ISA Specification",
 * Intel May 94.
 */
static void
pnp_send_initiation_key()
{
	int cur, i;

	/* Reset the LSFR */
	outb(_PNP_ADDRESS, 0);
	outb(_PNP_ADDRESS, 0); /* yes, we do need it twice! */

	cur = 0x6a;
	outb(_PNP_ADDRESS, cur);

	for (i = 1; i < 32; i++) {
		cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
		outb(_PNP_ADDRESS, cur);
	}
}


/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
static int
pnp_get_serial(pnp_id *p)
{
	int i, bit, valid = 0, sum = 0x6a;
	u_char *data = (u_char *)p;

	bzero(data, sizeof(char) * 9);
	outb(_PNP_ADDRESS, PNP_SERIAL_ISOLATION);
	for (i = 0; i < 72; i++) {
		bit = inb((pnp_rd_port << 2) | 0x3) == 0x55;
		DELAY(250);	/* Delay 250 usec */

		/* Can't Short Circuit the next evaluation, so 'and' is last */
		bit = (inb((pnp_rd_port << 2) | 0x3) == 0xaa) && bit;
		DELAY(250);	/* Delay 250 usec */

		valid = valid || bit;

		if (i < 64)
			sum = (sum >> 1) |
				(((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);

		data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
	}

	valid = valid && (data[8] == sum);

	return valid;
}

/*
 * Fill's the buffer with resource info from the device.
 * Returns the number of characters read.
 */
static int
pnp_get_resource_info(u_char *buffer, int len)
{
	int i, j, count;
	u_char temp;

	count = 0;
	for (i = 0; i < len; i++) {
		outb(_PNP_ADDRESS, PNP_STATUS);
		for (j = 0; j < 100; j++) {
			if ((inb((pnp_rd_port << 2) | 0x3)) & 0x1)
				break;
			DELAY(1);
		}
		if (j == 100) {
			printf("PnP device failed to report resource data\n");
			return count;
		}
		outb(_PNP_ADDRESS, PNP_RESOURCE_DATA);
		temp = inb((pnp_rd_port << 2) | 0x3);
		if (buffer != NULL)
			buffer[i] = temp;
		count++;
	}
	return count;
}

#if 0
/*
 * write_pnp_parms initializes a logical device with the parms
 * in d, and then activates the board if the last parameter is 1.
 */

static int
write_pnp_parms(struct pnp_cinfo *d, pnp_id *p, int ldn)
{
    int i, empty = -1 ;

    pnp_write (SET_LDN, ldn );
    i = pnp_read(SET_LDN) ;
    if (i != ldn) {
	printf("Warning: LDN %d does not exist\n", ldn);
    }
    for (i = 0; i < 8; i++) {
	pnp_write(IO_CONFIG_BASE + i * 2, d->ic_port[i] >> 8 );
	pnp_write(IO_CONFIG_BASE + i * 2 + 1, d->ic_port[i] & 0xff );
    }
    for (i = 0; i < 4; i++) {
	pnp_write(MEM_CONFIG + i*8, (d->ic_mem[i].base >> 16) & 0xff );
	pnp_write(MEM_CONFIG + i*8+1, (d->ic_mem[i].base >> 8) & 0xff );
	pnp_write(MEM_CONFIG + i*8+2, d->ic_mem[i].control & 0xff );
	pnp_write(MEM_CONFIG + i*8+3, (d->ic_mem[i].range >> 16) & 0xff );
	pnp_write(MEM_CONFIG + i*8+4, (d->ic_mem[i].range >> 8) & 0xff );
    }
    for (i = 0; i < 2; i++) {
	pnp_write(IRQ_CONFIG + i*2    , d->irq[i] );
	pnp_write(IRQ_CONFIG + i*2 + 1, d->irq_type[i] );
	pnp_write(DRQ_CONFIG + i, d->drq[i] );
    }
    /*
     * store parameters read into the current kernel
     * so manual editing next time is easier
     */
    for (i = 0 ; i < MAX_PNP_LDN; i++) {
	if (pnp_ldn_overrides[i].csn == d->csn &&
		pnp_ldn_overrides[i].ldn == ldn) {
	    d->flags = pnp_ldn_overrides[i].flags ;
	    pnp_ldn_overrides[i] = *d ;
	    break ;
	} else if (pnp_ldn_overrides[i].csn < 1 ||
		pnp_ldn_overrides[i].csn == 255)
	    empty = i ;
    }
    if (i== MAX_PNP_LDN && empty != -1)
	pnp_ldn_overrides[empty] = *d;

    /*
     * Here should really perform the range check, and
     * return a failure if not successful.
     */
    pnp_write (IO_RANGE_CHECK, 0);
    DELAY(1000); /* XXX is it really necessary ? */
    pnp_write (ACTIVATE, d->enable ? 1 : 0);
    DELAY(1000); /* XXX is it really necessary ? */
    return 1 ;
}
#endif

/*
 * This function is called after the bus has assigned resource
 * locations for a logical device.
 */
static void
pnp_set_config(void *arg, struct isa_config *config, int enable)
{
	int csn = ((struct pnp_set_config_arg *) arg)->csn;
	int ldn = ((struct pnp_set_config_arg *) arg)->ldn;
	int i;

	/*
	 * First put all cards into Sleep state with the initiation
	 * key, then put our card into Config state.
	 */
	pnp_send_initiation_key();
	pnp_write(PNP_WAKE, csn);

	/*
	 * Select our logical device so that we can program it.
	 */
	pnp_write(PNP_SET_LDN, ldn);

	/*
	 * Now program the resources.
	 */
	for (i = 0; i < config->ic_nmem; i++) {
		u_int32_t start = config->ic_mem[i].ir_start;
		u_int32_t size =  config->ic_mem[i].ir_size;
		if (start & 0xff)
			panic("pnp_set_config: bogus memory assignment");
		pnp_write(PNP_MEM_BASE_HIGH(i), (start >> 16) & 0xff);
		pnp_write(PNP_MEM_BASE_LOW(i), (start >> 8) & 0xff);
		pnp_write(PNP_MEM_RANGE_HIGH(i), (size >> 16) & 0xff);
		pnp_write(PNP_MEM_RANGE_LOW(i), (size >> 8) & 0xff);
	}
	for (; i < ISA_NMEM; i++) {
		pnp_write(PNP_MEM_BASE_HIGH(i), 0);
		pnp_write(PNP_MEM_BASE_LOW(i), 0);
		pnp_write(PNP_MEM_RANGE_HIGH(i), 0);
		pnp_write(PNP_MEM_RANGE_LOW(i), 0);
	}

	for (i = 0; i < config->ic_nport; i++) {
		u_int32_t start = config->ic_port[i].ir_start;
		pnp_write(PNP_IO_BASE_HIGH(i), (start >> 8) & 0xff);
		pnp_write(PNP_IO_BASE_LOW(i), (start >> 0) & 0xff);
	}
	for (; i < ISA_NPORT; i++) {
		pnp_write(PNP_IO_BASE_HIGH(i), 0);
		pnp_write(PNP_IO_BASE_LOW(i), 0);
	}

	for (i = 0; i < config->ic_nirq; i++) {
		int irq = ffs(config->ic_irqmask[i]) - 1;
		pnp_write(PNP_IRQ_LEVEL(i), irq);
		pnp_write(PNP_IRQ_TYPE(i), 2); /* XXX */
	}
	for (; i < ISA_NIRQ; i++) {
		/*
		 * IRQ 0 is not a valid interrupt selection and
		 * represents no interrupt selection.
		 */
		pnp_write(PNP_IRQ_LEVEL(i), 0);
	}		

	for (i = 0; i < config->ic_ndrq; i++) {
		int drq = ffs(config->ic_drqmask[i]) - 1;
		pnp_write(PNP_DMA_CHANNEL(i), drq);
	}
	for (; i < ISA_NDRQ; i++) {
		/*
		 * DMA channel 4, the cascade channel is used to
		 * indicate no DMA channel is active.
		 */
		pnp_write(PNP_DMA_CHANNEL(i), 4);
	}		

	pnp_write(PNP_ACTIVATE, enable ? 1 : 0);

	/*
	 * Wake everyone up again, we are finished.
	 */
	pnp_write(PNP_CONFIG_CONTROL, PNP_CONFIG_CONTROL_WAIT_FOR_KEY);
}

/*
 * Process quirks for a logical device.. The card must be in Config state.
 */
void
pnp_check_quirks(u_int32_t vendor_id, u_int32_t logical_id,
    int ldn, struct isa_config *config)
{
	struct pnp_quirk *qp;

	for (qp = &pnp_quirks[0]; qp->vendor_id; qp++) {
		if (qp->vendor_id == vendor_id
		    && (qp->logical_id == 0
			|| qp->logical_id == logical_id)) {
			switch (qp->type) {
			case PNP_QUIRK_WRITE_REG:
				pnp_write(PNP_SET_LDN, ldn);
				pnp_write(qp->arg1, qp->arg2);
				break;
			case PNP_QUIRK_EXTRA_IO:
				if (config == NULL)
					break;
                                if (qp->arg1 != 0) {
                                        config->ic_nport++;
                                        config->ic_port[config->ic_nport - 1] = config->ic_port[0];
                                        config->ic_port[config->ic_nport - 1].ir_start += qp->arg1;
                                        config->ic_port[config->ic_nport - 1].ir_end += qp->arg1;
                                }
                                if (qp->arg2 != 0) {
                                        config->ic_nport++;
                                        config->ic_port[config->ic_nport - 1] = config->ic_port[0];
                                        config->ic_port[config->ic_nport - 1].ir_start += qp->arg2;
                                        config->ic_port[config->ic_nport - 1].ir_end += qp->arg2;
                                }
                                break;

			}
		}
	}
}

/*
 * Scan Resource Data for Logical Devices.
 *
 * This function exits as soon as it gets an error reading *ANY*
 * Resource Data or it reaches the end of Resource Data.  In the first
 * case the return value will be TRUE, FALSE otherwise.
 */
static int
pnp_create_devices(device_t parent, pnp_id *p, int csn,
		   u_char *resources, int len)
{
	u_char tag, *resp, *resinfo, *startres = 0;
	int large_len, scanning = len, retval = FALSE;
	u_int32_t logical_id;
	u_int32_t compat_id;
	device_t dev = 0;
	int ldn = 0;
	struct pnp_set_config_arg *csnldn;
	char buf[100];
	char *desc = 0;

	resp = resources;
	while (scanning > 0) {
		tag = *resp++;
		scanning--;
		if (PNP_RES_TYPE(tag) != 0) {
			/* Large resource */
			if (scanning < 2) {
				scanning = 0;
				continue;
			}
			large_len = resp[0] + (resp[1] << 8);
			resp += 2;

			if (scanning < large_len) {
				scanning = 0;
				continue;
			}
			resinfo = resp;
			resp += large_len;
			scanning -= large_len;

			if (PNP_LRES_NUM(tag) == PNP_TAG_ID_ANSI) {
				if (large_len > sizeof(buf) - 1)
					large_len = sizeof(buf) - 1;
				bcopy(resinfo, buf, large_len);

				/*
				 * Trim trailing spaces.
				 */
				while (buf[large_len-1] == ' ')
					large_len--;
				buf[large_len] = '\0';
				desc = buf;
				if (dev)
					device_set_desc_copy(dev, desc);
				continue;
			}

			continue;
		}
		
		/* Small resource */
		if (scanning < PNP_SRES_LEN(tag)) {
			scanning = 0;
			continue;
		}
		resinfo = resp;
		resp += PNP_SRES_LEN(tag);
		scanning -= PNP_SRES_LEN(tag);;
			
		switch (PNP_SRES_NUM(tag)) {
		case PNP_TAG_LOGICAL_DEVICE:
			/*
			 * Parse the resources for the previous
			 * logical device (if any).
			 */
			if (startres) {
				pnp_parse_resources(dev, startres,
					    resinfo - startres - 1, ldn);
				dev = 0;
				startres = 0;
			}

			/* 
			 * A new logical device. Scan for end of
			 * resources.
			 */
			bcopy(resinfo, &logical_id, 4);
			pnp_check_quirks(p->vendor_id, logical_id, ldn, NULL);
			compat_id = 0;
			dev = BUS_ADD_CHILD(parent, ISA_ORDER_PNP, NULL, -1);
			if (desc)
				device_set_desc_copy(dev, desc);
			isa_set_vendorid(dev, p->vendor_id);
			isa_set_serial(dev, p->serial);
			isa_set_logicalid(dev, logical_id);
			csnldn = malloc(sizeof *csnldn, M_DEVBUF, M_WAITOK);
			csnldn->csn = csn;
			csnldn->ldn = ldn;
			ISA_SET_CONFIG_CALLBACK(parent, dev,
						pnp_set_config, csnldn);
			ldn++;
			startres = resp;
			break;
		    
		case PNP_TAG_END:
			if (!startres) {
				device_printf(parent,
					      "malformed resources\n");
				scanning = 0;
				break;
			}
			pnp_parse_resources(dev, startres,
					    resinfo - startres - 1, ldn);
			dev = 0;
			startres = 0;
			scanning = 0;
			break;

		default:
			/* Skip this resource */
			break;
		}
	}

	return retval;
}

/*
 * Read 'amount' bytes of resources from the card, allocating memory
 * as needed. If a buffer is already available, it should be passed in
 * '*resourcesp' and its length in '*spacep'. The number of resource
 * bytes already in the buffer should be passed in '*lenp'. The memory
 * allocated will be returned in '*resourcesp' with its size and the
 * number of bytes of resources in '*spacep' and '*lenp' respectively.
 */
static int
pnp_read_bytes(int amount, u_char **resourcesp, int *spacep, int *lenp)
{
	u_char *resources = *resourcesp;
	u_char *newres;
	int space = *spacep;
	int len = *lenp;

	if (space == 0) {
		space = 1024;
		resources = malloc(space, M_TEMP, M_WAITOK);
	}
	
	if (len + amount > space) {
		int extra = 1024;
		while (len + amount > space + extra)
			extra += 1024;
		newres = malloc(space + extra, M_TEMP, M_WAITOK);
		bcopy(resources, newres, len);
		free(resources, M_TEMP);
		resources = newres;
		space += extra;
	}

	if (pnp_get_resource_info(resources + len, amount) != amount)
		return EINVAL;
	len += amount;

	*resourcesp = resources;
	*spacep = space;
	*lenp = len;

	return 0;
}

/*
 * Read all resources from the card, allocating memory as needed. If a
 * buffer is already available, it should be passed in '*resourcesp'
 * and its length in '*spacep'. The memory allocated will be returned
 * in '*resourcesp' with its size and the number of bytes of resources
 * in '*spacep' and '*lenp' respectively.
 */
static int
pnp_read_resources(u_char **resourcesp, int *spacep, int *lenp)
{
	u_char *resources = *resourcesp;
	int space = *spacep;
	int len = 0;
	int error, done;
	u_char tag;

	error = 0;
	done = 0;
	while (!done) {
		error = pnp_read_bytes(1, &resources, &space, &len);
		if (error)
			goto out;
		tag = resources[len-1];
		if (PNP_RES_TYPE(tag) == 0) {
			/*
			 * Small resource, read contents.
			 */
			error = pnp_read_bytes(PNP_SRES_LEN(tag),
					       &resources, &space, &len);
			if (error)
				goto out;
			if (PNP_SRES_NUM(tag) == PNP_TAG_END)
				done = 1;
		} else {
			/*
			 * Large resource, read length and contents.
			 */
			error = pnp_read_bytes(2, &resources, &space, &len);
			if (error)
				goto out;
			error = pnp_read_bytes(resources[len-2]
					       + (resources[len-1] << 8),
					       &resources, &space, &len);
			if (error)
				goto out;
		}
	}

 out:
	*resourcesp = resources;
	*spacep = space;
	*lenp = len;
	return error;
}

/*
 * Run the isolation protocol. Use pnp_rd_port as the READ_DATA port
 * value (caller should try multiple READ_DATA locations before giving
 * up). Upon exiting, all cards are aware that they should use
 * pnp_rd_port as the READ_DATA port.
 *
 * In the first pass, a csn is assigned to each board and pnp_id's
 * are saved to an array, pnp_devices. In the second pass, each
 * card is woken up and the device configuration is called.
 */
static int
pnp_isolation_protocol(device_t parent)
{
	int csn;
	pnp_id id;
	int found = 0, len;
	u_char *resources = 0;
	int space = 0;
	int error;
#ifdef PC98
	int n, necpnp;
	u_char buffer[10];
#endif

	/*
	 * Put all cards into the Sleep state so that we can clear
	 * their CSNs.
	 */
	pnp_send_initiation_key();

	/*
	 * Clear the CSN for all cards.
	 */
	pnp_write(PNP_CONFIG_CONTROL, PNP_CONFIG_CONTROL_RESET_CSN);

	/*
	 * Move all cards to the Isolation state.
	 */
	pnp_write(PNP_WAKE, 0);

	/*
	 * Tell them where the read point is going to be this time.
	 */
	pnp_write(PNP_SET_RD_DATA, pnp_rd_port);

	for (csn = 1; csn < PNP_MAX_CARDS; csn++) {
		/*
		 * Start the serial isolation protocol.
		 */
		outb(_PNP_ADDRESS, PNP_SERIAL_ISOLATION);
		DELAY(1000);	/* Delay 1 msec */

		if (pnp_get_serial(&id)) {
			/*
			 * We have read the id from a card
			 * successfully. The card which won the
			 * isolation protocol will be in Isolation
			 * mode and all others will be in Sleep.
			 * Program the CSN of the isolated card
			 * (taking it to Config state) and read its
			 * resources, creating devices as we find
			 * logical devices on the card.
			 */
			pnp_write(PNP_SET_CSN, csn);
#ifdef PC98
			if (bootverbose)
				printf("PnP Vendor ID = %x\n", id.vendor_id);
			/* Check for NEC PnP (9 bytes serial). */
			for (n = necpnp = 0; necids[n].vendor_id; n++) {
				if (id.vendor_id == necids[n].vendor_id) {
					necpnp = 1;
					break;
				}
			}
			if (necpnp) {
				if (bootverbose)
					printf("It seems to NEC-PnP card (%s).\n",
					       pnp_eisaformat(id.vendor_id));
				/*  Read dummy 9 bytes serial area. */
				pnp_get_resource_info(buffer, 9);
			} else {
				if (bootverbose)
					printf("It seems to Normal-ISA-PnP card (%s).\n",
					       pnp_eisaformat(id.vendor_id));
			}
			if (bootverbose)
				printf("Reading PnP configuration for %s.\n",
				       pnp_eisaformat(id.vendor_id));
#endif
			error = pnp_read_resources(&resources,
						   &space,
						   &len);
			if (error)
				break;
			pnp_create_devices(parent, &id, csn,
					   resources, len);
			found++;
		} else
			break;

		/*
		 * Put this card back to the Sleep state and
		 * simultaneously move all cards which don't have a
		 * CSN yet to Isolation state.
		 */
		pnp_write(PNP_WAKE, 0);
	}

	/*
	 * Unless we have chosen the wrong read port, all cards will
	 * be in Sleep state. Put them back into WaitForKey for
	 * now. Their resources will be programmed later.
	 */
	pnp_write(PNP_CONFIG_CONTROL, PNP_CONFIG_CONTROL_WAIT_FOR_KEY);

	/*
	 * Cleanup.
	 */
	if (resources)
		free(resources, M_TEMP);

	return found;
}


/*
 * pnp_identify()
 *
 * autoconfiguration of pnp devices. This routine just runs the
 * isolation protocol over several ports, until one is successful.
 *
 * may be called more than once ?
 *
 */

static void
pnp_identify(driver_t *driver, device_t parent)
{
	int num_pnp_devs;

#if 0
	if (pnp_ldn_overrides[0].csn == 0) {
		if (bootverbose)
			printf("Initializing PnP override table\n");
		bzero (pnp_ldn_overrides, sizeof(pnp_ldn_overrides));
		pnp_ldn_overrides[0].csn = 255 ;
	}
#endif

	/* Try various READ_DATA ports from 0x203-0x3ff */
	for (pnp_rd_port = 0x80; (pnp_rd_port < 0xff); pnp_rd_port += 0x10) {
		if (bootverbose)
			printf("Trying Read_Port at %x\n", (pnp_rd_port << 2) | 0x3);

		num_pnp_devs = pnp_isolation_protocol(parent);
		if (num_pnp_devs)
			break;
	}
}

static device_method_t pnp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	pnp_identify),

	{ 0, 0 }
};

static driver_t pnp_driver = {
	"pnp",
	pnp_methods,
	1,			/* no softc */
};

static devclass_t pnp_devclass;

DRIVER_MODULE(pnp, isa, pnp_driver, pnp_devclass, 0, 0);
