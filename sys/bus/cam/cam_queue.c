/*
 * CAM request queue management functions.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cam/cam_queue.c,v 1.5 1999/08/28 00:40:41 peter Exp $
 * $DragonFly: src/sys/bus/cam/cam_queue.c,v 1.6 2004/03/15 05:43:52 dillon Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>

#include "cam.h"
#include "cam_ccb.h"
#include "cam_queue.h"
#include "cam_debug.h"

static __inline int
		queue_cmp(cam_pinfo **queue_array, int i, int j);
static __inline void
		swap(cam_pinfo **queue_array, int i, int j);
static void	heap_up(cam_pinfo **queue_array, int new_index);
static void	heap_down(cam_pinfo **queue_array, int index,
			  int last_index);

struct camq *
camq_alloc(int size)
{
	struct camq *camq;

	camq = malloc(sizeof(*camq), M_DEVBUF, M_INTWAIT);
	camq_init(camq, size);
	return (camq);
}
	
int
camq_init(struct camq *camq, int size)
{
	bzero(camq, sizeof(*camq));
	camq->array_size = size;
	if (camq->array_size != 0) {
		camq->queue_array = malloc(size * sizeof(cam_pinfo *), 
					M_DEVBUF, M_INTWAIT);
		/*
		 * Heap algorithms like everything numbered from 1, so
		 * offset our pointer into the heap array by one element.
		 *
		 * XXX this is a really dumb idea.
		 */
		camq->queue_array--;
	}
	return (0);
}

/*
 * Free a camq structure.  This should only be called if a controller
 * driver failes somehow during its attach routine or is unloaded and has
 * obtained a camq structure.  The XPT should ensure that the queue
 * is empty before calling this routine.
 */
void
camq_free(struct camq *queue)
{
	if (queue != NULL) {
		camq_fini(queue);
		free(queue, M_DEVBUF);
	}
}

void
camq_fini(struct camq *queue)
{
	if (queue->queue_array != NULL) {
		/*
		 * Heap algorithms like everything numbered from 1, so
		 * our pointer into the heap array is offset by one element.
		 */
		queue->queue_array++;
		free(queue->queue_array, M_DEVBUF);
	}
}

u_int32_t
camq_resize(struct camq *queue, int new_size)
{
	cam_pinfo **new_array;

#ifdef DIAGNOSTIC
	if (new_size < queue->entries)
		panic("camq_resize: New queue size can't accomodate "
		      "queued entries.");
#endif
	new_array = malloc(new_size * sizeof(cam_pinfo *), M_DEVBUF, M_INTWAIT);

	/*
	 * Heap algorithms like everything numbered from 1, so
	 * remember that our pointer into the heap array is offset
	 * by one element.
	 */
	if (queue->queue_array != NULL) {
		queue->queue_array++;
		bcopy(queue->queue_array, new_array,
		      queue->entries * sizeof(cam_pinfo *));
		free(queue->queue_array, M_DEVBUF);
	}
	queue->queue_array = new_array-1;
	queue->array_size = new_size;
	return (CAM_REQ_CMP);
}

/*
 * camq_insert: Given an array of cam_pinfo* elememnts with
 * the Heap(1, num_elements) property and array_size - num_elements >= 1,
 * output Heap(1, num_elements+1) including new_entry in the array.
 */
void
camq_insert(struct camq *queue, cam_pinfo *new_entry)
{
#ifdef DIAGNOSTIC
	if (queue->entries >= queue->array_size)
		panic("camq_insert: Attempt to insert into a full queue");
#endif
	queue->entries++;
	queue->queue_array[queue->entries] = new_entry;
	new_entry->index = queue->entries;
	if (queue->entries != 0)
		heap_up(queue->queue_array, queue->entries);
}

/*
 * camq_remove:  Given an array of cam_pinfo* elevements with the
 * Heap(1, num_elements) property and an index such that 1 <= index <=
 * num_elements, remove that entry and restore the Heap(1, num_elements-1)
 * property.
 */
cam_pinfo *
camq_remove(struct camq *queue, int index)
{
	cam_pinfo *removed_entry;

	if (index == 0 || index > queue->entries)
		return (NULL);
	removed_entry = queue->queue_array[index];
	if (queue->entries != index) {
		queue->queue_array[index] = queue->queue_array[queue->entries];
		queue->queue_array[index]->index = index;
		heap_down(queue->queue_array, index, queue->entries - 1);
	}
	removed_entry->index = CAM_UNQUEUED_INDEX;
	queue->entries--;
	return (removed_entry);
}

/*
 * camq_change_priority:  Given an array of cam_pinfo* elements with the
 * Heap(1, num_entries) property, an index such that 1 <= index <= num_elements,
 * and an new priority for the element at index, change the priority of
 * element index and restore the Heap(0, num_elements) property.
 */
void
camq_change_priority(struct camq *queue, int index, u_int32_t new_priority)
{
	if (new_priority > queue->queue_array[index]->priority) {
		queue->queue_array[index]->priority = new_priority;
		heap_down(queue->queue_array, index, queue->entries);
	} else {
		/* new_priority <= old_priority */
		queue->queue_array[index]->priority = new_priority;
		heap_up(queue->queue_array, index);
	}
}

struct cam_devq *
cam_devq_alloc(int devices, int openings)
{
	struct cam_devq *devq;

	devq = malloc(sizeof(*devq), M_DEVBUF, M_INTWAIT);
	cam_devq_init(devq, devices, openings);
	return (devq);
}

int
cam_devq_init(struct cam_devq *devq, int devices, int openings)
{
	bzero(devq, sizeof(*devq));
	camq_init(&devq->alloc_queue, devices);
	camq_init(&devq->send_queue, devices);
	devq->alloc_openings = openings;
	devq->alloc_active = 0;
	devq->send_openings = openings;
	devq->send_active = 0;	
	devq->refcount = 1;
	return (0);	
}

void
cam_devq_reference(struct cam_devq *devq)
{
	++devq->refcount;
}

void
cam_devq_release(struct cam_devq *devq)
{
	if (--devq->refcount == 0) {
		if (devq->alloc_active || devq->send_active)
			printf("cam_devq_release: WARNING active allocations %d active send %d!\n", devq->alloc_active, devq->send_active);
		camq_fini(&devq->alloc_queue);
		camq_fini(&devq->send_queue);
		free(devq, M_DEVBUF);
	}
}

u_int32_t
cam_devq_resize(struct cam_devq *camq, int devices)
{
	u_int32_t retval;

	retval = camq_resize(&camq->alloc_queue, devices);

	if (retval == CAM_REQ_CMP)
		retval = camq_resize(&camq->send_queue, devices);

	return (retval);
}

struct cam_ccbq *
cam_ccbq_alloc(int openings)
{
	struct cam_ccbq *ccbq;

	ccbq = malloc(sizeof(*ccbq), M_DEVBUF, M_INTWAIT);
	cam_ccbq_init(ccbq, openings);
	return (ccbq);
}

void
cam_ccbq_free(struct cam_ccbq *ccbq)
{
	if (ccbq) {
		camq_fini(&ccbq->queue);
		free(ccbq, M_DEVBUF);
	}
}

u_int32_t
cam_ccbq_resize(struct cam_ccbq *ccbq, int new_size)
{
	int delta;
	int space_left;

	delta = new_size - (ccbq->dev_active + ccbq->dev_openings);
	space_left = new_size
	    - ccbq->queue.entries
	    - ccbq->held
	    - ccbq->dev_active;

	/*
	 * Only attempt to change the underlying queue size if we are
	 * shrinking it and there is space for all outstanding entries
	 * in the new array or we have been requested to grow the array.
	 * We don't fail in the case where we can't reduce the array size,
	 * but clients that care that the queue be "garbage collected"
	 * should detect this condition and call us again with the
	 * same size once the outstanding entries have been processed.
	 */
	if (space_left < 0
	 || camq_resize(&ccbq->queue, new_size) == CAM_REQ_CMP) {
		ccbq->devq_openings += delta;
		ccbq->dev_openings += delta;
		return (CAM_REQ_CMP);
	} else {
		return (CAM_RESRC_UNAVAIL);
	}
}

int
cam_ccbq_init(struct cam_ccbq *ccbq, int openings)
{
	bzero(ccbq, sizeof(*ccbq));
	camq_init(&ccbq->queue, openings);
	ccbq->devq_openings = openings;
	ccbq->dev_openings = openings;	
	TAILQ_INIT(&ccbq->active_ccbs);
	return (0);
}

/*
 * Heap routines for manipulating CAM queues.
 */
/*
 * queue_cmp: Given an array of cam_pinfo* elements and indexes i
 * and j, return less than 0, 0, or greater than 0 if i is less than,
 * equal too, or greater than j respectively.
 */
static __inline int
queue_cmp(cam_pinfo **queue_array, int i, int j)
{
	if (queue_array[i]->priority == queue_array[j]->priority)
		return (  queue_array[i]->generation
			- queue_array[j]->generation );
	else
		return (  queue_array[i]->priority
			- queue_array[j]->priority );
}

/*
 * swap: Given an array of cam_pinfo* elements and indexes i and j,
 * exchange elements i and j.
 */
static __inline void
swap(cam_pinfo **queue_array, int i, int j)
{
	cam_pinfo *temp_qentry;

	temp_qentry = queue_array[j];
	queue_array[j] = queue_array[i];
	queue_array[i] = temp_qentry;
	queue_array[j]->index = j;
	queue_array[i]->index = i;
}

/*
 * heap_up:  Given an array of cam_pinfo* elements with the
 * Heap(1, new_index-1) property and a new element in location
 * new_index, output Heap(1, new_index).
 */
static void
heap_up(cam_pinfo **queue_array, int new_index)
{
	int child;
	int parent;

	child = new_index;

	while (child != 1) {

		parent = child >> 1;
		if (queue_cmp(queue_array, parent, child) <= 0)
			break;
		swap(queue_array, parent, child);
		child = parent;
	}
}

/*
 * heap_down:  Given an array of cam_pinfo* elements with the
 * Heap(index + 1, num_entries) property with index containing
 * an unsorted entry, output Heap(index, num_entries).
 */
static void
heap_down(cam_pinfo **queue_array, int index, int num_entries)
{
	int child;
	int parent;
	
	parent = index;
	child = parent << 1;
	for (; child <= num_entries; child = parent << 1) {

		if (child < num_entries) {
			/* child+1 is the right child of parent */
			if (queue_cmp(queue_array, child + 1, child) < 0)
				child++;
		}
		/* child is now the least child of parent */
		if (queue_cmp(queue_array, parent, child) <= 0)
			break;
		swap(queue_array, child, parent);
		parent = child;
	}
}
