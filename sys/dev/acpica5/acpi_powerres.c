/*-
 * Copyright (c) 2001 Michael Smith
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
 * $FreeBSD: src/sys/dev/acpica/acpi_powerres.c,v 1.22 2004/04/14 17:58:19 njl Exp $
 * $DragonFly: src/sys/dev/acpica5/acpi_powerres.c,v 1.3 2004/06/27 08:52:39 dillon Exp $
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include "acpi.h"
#include <dev/acpica5/acpivar.h>

/*
 * ACPI power resource management.
 *
 * Power resource behaviour is slightly complicated by the fact that
 * a single power resource may provide power for more than one device.
 * Thus, we must track the device(s) being powered by a given power
 * resource, and only deactivate it when there are no powered devices.
 *
 * Note that this only manages resources for known devices.  There is an
 * ugly case where we may turn of power to a device which is in use because
 * we don't know that it depends on a given resource.  We should perhaps
 * try to be smarter about this, but a more complete solution would involve
 * scanning all of the ACPI namespace to find devices we're not currently
 * aware of, and this raises questions about whether they should be left 
 * on, turned off, etc.
 *
 * XXX locking
 */

MALLOC_DEFINE(M_ACPIPWR, "acpipwr", "ACPI power resources");

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_POWERRES
ACPI_MODULE_NAME("POWERRES")

/* Return values from _STA on a power resource */
#define ACPI_PWR_OFF	0
#define ACPI_PWR_ON	1

/* A relationship between a power resource and a consumer. */
struct acpi_powerreference {
    struct acpi_powerconsumer		*ar_consumer;
    struct acpi_powerresource		*ar_resource;
    TAILQ_ENTRY(acpi_powerreference)	ar_rlink; /* link on resource list */
    TAILQ_ENTRY(acpi_powerreference)	ar_clink; /* link on consumer */
};
    
/* A power-managed device. */
struct acpi_powerconsumer {
    /* Device which is powered */
    ACPI_HANDLE				ac_consumer;
    int					ac_state;
    TAILQ_ENTRY(acpi_powerconsumer)	ac_link;
    TAILQ_HEAD(,acpi_powerreference)	ac_references;
};

/* A power resource. */
struct acpi_powerresource {
    TAILQ_ENTRY(acpi_powerresource)	ap_link;
    TAILQ_HEAD(,acpi_powerreference)	ap_references;
    ACPI_HANDLE				ap_resource;
    ACPI_INTEGER			ap_systemlevel;
    ACPI_INTEGER			ap_order;
};

static TAILQ_HEAD(acpi_powerresource_list, acpi_powerresource)
	acpi_powerresources;
static TAILQ_HEAD(acpi_powerconsumer_list, acpi_powerconsumer)
	acpi_powerconsumers;

static ACPI_STATUS	acpi_pwr_register_consumer(ACPI_HANDLE consumer);
#ifdef notyet
static ACPI_STATUS	acpi_pwr_deregister_consumer(ACPI_HANDLE consumer);
#endif /* notyet */
static ACPI_STATUS	acpi_pwr_register_resource(ACPI_HANDLE res);
#ifdef notyet
static ACPI_STATUS	acpi_pwr_deregister_resource(ACPI_HANDLE res);
#endif /* notyet */
static void		acpi_pwr_reference_resource(ACPI_OBJECT *obj,
						    void *arg);
static ACPI_STATUS	acpi_pwr_switch_power(void);
static struct acpi_powerresource
			*acpi_pwr_find_resource(ACPI_HANDLE res);
static struct acpi_powerconsumer
			*acpi_pwr_find_consumer(ACPI_HANDLE consumer);

/* Initialise our lists. */    
static void
acpi_pwr_init(void *junk)
{
    TAILQ_INIT(&acpi_powerresources);
    TAILQ_INIT(&acpi_powerconsumers);
}
SYSINIT(acpi_powerresource, SI_SUB_TUNABLES, SI_ORDER_ANY, acpi_pwr_init, NULL);

/*
 * Register a power resource.
 *
 * It's OK to call this if we already know about the resource.
 */
static ACPI_STATUS
acpi_pwr_register_resource(ACPI_HANDLE res)
{
    ACPI_STATUS			status;
    ACPI_BUFFER			buf;
    ACPI_OBJECT			*obj;
    struct acpi_powerresource	*rp, *srp;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    rp = NULL;
    buf.Pointer = NULL;
    
    /* Look to see if we know about this resource */
    if (acpi_pwr_find_resource(res) != NULL)
	return_ACPI_STATUS (AE_OK);		/* already know about it */

    /* Allocate a new resource */
    rp = malloc(sizeof(*rp), M_ACPIPWR, M_INTWAIT | M_ZERO);
    TAILQ_INIT(&rp->ap_references);
    rp->ap_resource = res;

    /* Get the Power Resource object */
    buf.Length = ACPI_ALLOCATE_BUFFER;
    if (ACPI_FAILURE(status = AcpiEvaluateObject(res, NULL, NULL, &buf))) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "no power resource object\n"));
	goto out;
    }
    obj = buf.Pointer;
    if (obj->Type != ACPI_TYPE_POWER) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "questionable power resource object %s\n",
			 acpi_name(res)));
	status = AE_TYPE;
	goto out;
    }
    rp->ap_systemlevel = obj->PowerResource.SystemLevel;
    rp->ap_order = obj->PowerResource.ResourceOrder;
    
    /* Sort the resource into the list */
    status = AE_OK;
    srp = TAILQ_FIRST(&acpi_powerresources);
    if (srp == NULL || rp->ap_order < srp->ap_order) {
	TAILQ_INSERT_HEAD(&acpi_powerresources, rp, ap_link);
	goto done;
    }
    TAILQ_FOREACH(srp, &acpi_powerresources, ap_link) {
	if (rp->ap_order < srp->ap_order) {
	    TAILQ_INSERT_BEFORE(srp, rp, ap_link);
	    goto done;
	}
    }
    TAILQ_INSERT_TAIL(&acpi_powerresources, rp, ap_link);

 done:
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
		     "registered power resource %s\n", acpi_name(res)));

 out:
    if (buf.Pointer != NULL)
	AcpiOsFree(buf.Pointer);
    if (ACPI_FAILURE(status) && rp != NULL)
	free(rp, M_ACPIPWR);
    return_ACPI_STATUS (status);
}

#ifdef notyet
/*
 * Deregister a power resource.
 */
static ACPI_STATUS
acpi_pwr_deregister_resource(ACPI_HANDLE res)
{
    struct acpi_powerresource	*rp;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    rp = NULL;
    
    /* Find the resource */
    if ((rp = acpi_pwr_find_resource(res)) == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /* Check that there are no consumers referencing this resource */
    if (TAILQ_FIRST(&rp->ap_references) != NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /* Pull it off the list and free it */
    TAILQ_REMOVE(&acpi_powerresources, rp, ap_link);
    free(rp, M_ACPIPWR);

    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "deregistered power resource %s\n",
		     acpi_name(res)));

    return_ACPI_STATUS (AE_OK);
}
#endif /* notyet */

/*
 * Register a power consumer.  
 *
 * It's OK to call this if we already know about the consumer.
 */
static ACPI_STATUS
acpi_pwr_register_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Check to see whether we know about this consumer already */
    if ((pc = acpi_pwr_find_consumer(consumer)) != NULL)
	return_ACPI_STATUS (AE_OK);
    
    /* Allocate a new power consumer */
    pc = malloc(sizeof(*pc), M_ACPIPWR, M_INTWAIT);
    TAILQ_INSERT_HEAD(&acpi_powerconsumers, pc, ac_link);
    TAILQ_INIT(&pc->ac_references);
    pc->ac_consumer = consumer;

    /* XXX we should try to find its current state */
    pc->ac_state = ACPI_STATE_UNKNOWN;

    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "registered power consumer %s\n",
		     acpi_name(consumer)));
    
    return_ACPI_STATUS (AE_OK);
}

#ifdef notyet
/*
 * Deregister a power consumer.
 *
 * This should only be done once the consumer has been powered off.
 * (XXX is this correct?  Check once implemented)
 */
static ACPI_STATUS
acpi_pwr_deregister_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Find the consumer */
    if ((pc = acpi_pwr_find_consumer(consumer)) == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    
    /* Make sure the consumer's not referencing anything right now */
    if (TAILQ_FIRST(&pc->ac_references) != NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /* Pull the consumer off the list and free it */
    TAILQ_REMOVE(&acpi_powerconsumers, pc, ac_link);

    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "deregistered power consumer %s\n",
		     acpi_name(consumer)));

    return_ACPI_STATUS (AE_OK);
}
#endif /* notyet */

/*
 * Set a power consumer to a particular power state.
 */
ACPI_STATUS
acpi_pwr_switch_consumer(ACPI_HANDLE consumer, int state)
{
    struct acpi_powerconsumer	*pc;
    struct acpi_powerreference	*pr;
    ACPI_HANDLE			method_handle, reslist_handle, pr0_handle;
    ACPI_BUFFER			reslist_buffer;
    ACPI_OBJECT			*reslist_object;
    ACPI_STATUS			status;
    char			*method_name, *reslist_name;
    int				res_changed;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* It's never ok to switch a non-existent consumer. */
    if (consumer == NULL)
	return_ACPI_STATUS (AE_NOT_FOUND);

    /* Find the consumer */
    if ((pc = acpi_pwr_find_consumer(consumer)) == NULL) {
	if (ACPI_FAILURE(status = acpi_pwr_register_consumer(consumer)))
	    return_ACPI_STATUS (status);
	if ((pc = acpi_pwr_find_consumer(consumer)) == NULL) {
	    return_ACPI_STATUS (AE_ERROR);	/* something very wrong */
	}
    }

    /* Check for valid transitions */
    if (pc->ac_state == ACPI_STATE_D3 && state != ACPI_STATE_D0)
	return_ACPI_STATUS (AE_BAD_PARAMETER);	/* can only go to D0 from D3 */

    /* Find transition mechanism(s) */
    switch(state) {
    case ACPI_STATE_D0:
	method_name = "_PS0";
	reslist_name = "_PR0";
	break;
    case ACPI_STATE_D1:
	method_name = "_PS1";
	reslist_name = "_PR1";
	break;
    case ACPI_STATE_D2:
	method_name = "_PS2";
	reslist_name = "_PR2";
	break;
    case ACPI_STATE_D3:
	method_name = "_PS3";
	reslist_name = "_PR3";
	break;
    default:
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    }
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "setup to switch %s D%d -> D%d\n",
		     acpi_name(consumer), pc->ac_state, state));

    /*
     * Verify that this state is supported, ie. one of method or
     * reslist must be present.  We need to do this before we go 
     * dereferencing resources (since we might be trying to go to
     * a state we don't support).
     *
     * Note that if any states are supported, the device has to
     * support D0 and D3.  It's never an error to try to go to
     * D0.
     */
    reslist_buffer.Pointer = NULL;
    reslist_object = NULL;
    if (ACPI_FAILURE(AcpiGetHandle(consumer, method_name, &method_handle)))
	method_handle = NULL;
    if (ACPI_FAILURE(AcpiGetHandle(consumer, reslist_name, &reslist_handle)))
	reslist_handle = NULL;
    if (reslist_handle == NULL && method_handle == NULL) {
	if (state == ACPI_STATE_D0) {
	    pc->ac_state = ACPI_STATE_D0;
	    return_ACPI_STATUS (AE_OK);
	}
	if (state != ACPI_STATE_D3)
	    goto bad;

	/* Turn off the resources listed in _PR0 to go to D3. */
	if (ACPI_FAILURE(AcpiGetHandle(consumer, "_PR0", &pr0_handle)))
	    goto bad;
	reslist_buffer.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(pr0_handle, NULL, NULL, &reslist_buffer);
	if (ACPI_FAILURE(status))
	    goto bad;
	reslist_object = (ACPI_OBJECT *)reslist_buffer.Pointer;
	if (reslist_object->Type != ACPI_TYPE_PACKAGE ||
	    reslist_object->Package.Count == 0) {

	    goto bad;
	}
	AcpiOsFree(reslist_buffer.Pointer);
	reslist_buffer.Pointer = NULL;
	reslist_object = NULL;
    }

    /*
     * Check that we can actually fetch the list of power resources
     */
    if (reslist_handle != NULL) {
	reslist_buffer.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiEvaluateObject(reslist_handle, NULL, NULL,
				    &reslist_buffer);
	if (ACPI_FAILURE(status)) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			     "can't evaluate resource list %s\n",
			     acpi_name(reslist_handle)));
	    goto out;
	}
	reslist_object = (ACPI_OBJECT *)reslist_buffer.Pointer;
	if (reslist_object->Type != ACPI_TYPE_PACKAGE) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			     "resource list is not ACPI_TYPE_PACKAGE (%d)\n",
			     reslist_object->Type));
	    status = AE_TYPE;
	    goto out;
	}
    }

    /*
     * Now we are ready to switch, so kill off any current power
     * resource references.
     */
    res_changed = 0;
    while((pr = TAILQ_FIRST(&pc->ac_references)) != NULL) {
	res_changed = 1;
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "removing reference to %s\n",
			 acpi_name(pr->ar_resource->ap_resource)));
	TAILQ_REMOVE(&pr->ar_resource->ap_references, pr, ar_rlink);
	TAILQ_REMOVE(&pc->ac_references, pr, ar_clink);
	free(pr, M_ACPIPWR);
    }

    /*
     * Add new power resource references, if we have any.  Traverse the
     * package that we got from evaluating reslist_handle, and look up each
     * of the resources that are referenced.
     */
    if (reslist_object != NULL) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "referencing %d new resources\n", 
			  reslist_object->Package.Count));
	acpi_ForeachPackageObject(reslist_object, acpi_pwr_reference_resource,
				  pc);
	res_changed = 1;
    }

    /*
     * If we changed anything in the resource list, we need to run a switch
     * pass now.
     */
    if (ACPI_FAILURE(status = acpi_pwr_switch_power())) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "failed to switch resources from %s to D%d\n",
			  acpi_name(consumer), state));

	/* XXX is this appropriate?  Should we return to previous state? */
	goto out;	
    }

    /* Invoke power state switch method (if present) */
    if (method_handle != NULL) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "invoking state transition method %s\n",
			 acpi_name(method_handle)));
	status = AcpiEvaluateObject(method_handle, NULL, NULL, NULL);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "failed to set state - %s\n",
				 AcpiFormatException(status)));
		pc->ac_state = ACPI_STATE_UNKNOWN;

		/* XXX Should we return to previous state? */
		goto out;
	}
    }
	
    /* Transition was successful */
    pc->ac_state = state;
    return_ACPI_STATUS (AE_OK);

 bad:
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
		     "attempt to set unsupported state D%d\n", state));
    status = AE_BAD_PARAMETER;

 out:
    if (reslist_buffer.Pointer != NULL)
	AcpiOsFree(reslist_buffer.Pointer);
    return_ACPI_STATUS (status);
}

/*
 * Called to create a reference between a power consumer and a power resource
 * identified in the object.
 */
static void
acpi_pwr_reference_resource(ACPI_OBJECT *obj, void *arg)
{
    struct acpi_powerconsumer	*pc = (struct acpi_powerconsumer *)arg;
    struct acpi_powerreference	*pr;
    struct acpi_powerresource	*rp;
    ACPI_HANDLE			res;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    res = acpi_GetReference(NULL, obj);
    if (res == NULL) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "can't create a power reference for object type %d\n",
			 obj->Type));
	return_VOID;
    }

    /* Create/look up the resource */
    if (ACPI_FAILURE(status = acpi_pwr_register_resource(res))) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			 "couldn't register power resource %s - %s\n",
			 obj->String.Pointer, AcpiFormatException(status)));
	return_VOID;
    }
    if ((rp = acpi_pwr_find_resource(res)) == NULL) {
	ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "power resource list corrupted\n"));
	return_VOID;
    }
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "found power resource %s\n",
		     acpi_name(rp->ap_resource)));

    /* Create a reference between the consumer and resource */
    pr = malloc(sizeof(*pr), M_ACPIPWR, M_INTWAIT | M_ZERO);
    pr->ar_consumer = pc;
    pr->ar_resource = rp;
    TAILQ_INSERT_TAIL(&pc->ac_references, pr, ar_clink);
    TAILQ_INSERT_TAIL(&rp->ap_references, pr, ar_rlink);
    
    return_VOID;
}


/*
 * Switch power resources to conform to the desired state.
 *
 * Consumers may have modified the power resource list in an arbitrary
 * fashion; we sweep it in sequence order.
 */
static ACPI_STATUS
acpi_pwr_switch_power(void)
{
    struct acpi_powerresource	*rp;
    ACPI_STATUS			status;
    int				cur;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * Sweep the list forwards turning things on.
     */
    TAILQ_FOREACH(rp, &acpi_powerresources, ap_link) {
	if (TAILQ_FIRST(&rp->ap_references) == NULL) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			     "%s has no references, not turning on\n",
			     acpi_name(rp->ap_resource)));
	    continue;
	}

	/* We could cache this if we trusted it not to change under us */
	status = acpi_GetInteger(rp->ap_resource, "_STA", &cur);
	if (ACPI_FAILURE(status)) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "can't get status of %s - %d\n",
			      acpi_name(rp->ap_resource), status));

	    /* XXX is this correct?  Always switch if in doubt? */
	    continue;
	}

	/*
	 * Switch if required.  Note that we ignore the result of the switch
	 * effort; we don't know what to do if it fails, so checking wouldn't
	 * help much.
	 */
	if (cur != ACPI_PWR_ON) {
	    status = AcpiEvaluateObject(rp->ap_resource, "_ON", NULL, NULL);
	    if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
				 "failed to switch %s on - %s\n", 
				 acpi_name(rp->ap_resource),
				 AcpiFormatException(status)));
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "switched %s on\n",
				 acpi_name(rp->ap_resource)));
	    }
	} else {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "%s is already on\n",
			     acpi_name(rp->ap_resource)));
	}
    }
    
    /* Sweep the list backwards turning things off. */
    TAILQ_FOREACH_REVERSE(rp, &acpi_powerresources, acpi_powerresource_list,
	ap_link) {

	if (TAILQ_FIRST(&rp->ap_references) != NULL) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
			     "%s has references, not turning off\n",
			     acpi_name(rp->ap_resource)));
	    continue;
	}

	/* We could cache this if we trusted it not to change under us */
	status = acpi_GetInteger(rp->ap_resource, "_STA", &cur);
	if (ACPI_FAILURE(status)) {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "can't get status of %s - %d\n",
			      acpi_name(rp->ap_resource), status));
	    /* XXX is this correct?  Always switch if in doubt? */
	    continue;
	}

	/*
	 * Switch if required.  Note that we ignore the result of the switch
	 * effort; we don't know what to do if it fails, so checking wouldn't
	 * help much.
	 */
	if (cur != ACPI_PWR_OFF) {
	    status = AcpiEvaluateObject(rp->ap_resource, "_OFF", NULL, NULL);
	    if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS,
				 "failed to switch %s off - %s\n", 
				 acpi_name(rp->ap_resource),
				 AcpiFormatException(status)));
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "switched %s off\n",
				 acpi_name(rp->ap_resource)));
	    }
	} else {
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "%s is already off\n",
			     acpi_name(rp->ap_resource)));
	}
    }

    return_ACPI_STATUS (AE_OK);
}

/*
 * Find a power resource's control structure.
 */
static struct acpi_powerresource *
acpi_pwr_find_resource(ACPI_HANDLE res)
{
    struct acpi_powerresource	*rp;
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    TAILQ_FOREACH(rp, &acpi_powerresources, ap_link) {
	if (rp->ap_resource == res)
	    break;
    }

    return_PTR (rp);
}

/*
 * Find a power consumer's control structure.
 */
static struct acpi_powerconsumer *
acpi_pwr_find_consumer(ACPI_HANDLE consumer)
{
    struct acpi_powerconsumer	*pc;
    
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    TAILQ_FOREACH(pc, &acpi_powerconsumers, ac_link) {
	if (pc->ac_consumer == consumer)
	    break;
    }

    return_PTR (pc);
}
