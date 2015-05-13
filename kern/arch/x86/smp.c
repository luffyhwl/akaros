/*
 * Copyright (c) 2009 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 */

#include <arch/arch.h>
#include <bitmask.h>
#include <smp.h>

#include <atomic.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pmap.h>
#include <env.h>
#include <trap.h>

/*************************** IPI Wrapper Stuff ********************************/
// checklists to protect the global interrupt_handlers for 0xf0, f1, f2, f3, f4
// need to be global, since there is no function that will always exist for them
handler_wrapper_t handler_wrappers[NUM_HANDLER_WRAPPERS];

static int smp_call_function(uint8_t type, uint32_t dest, isr_t handler,
                             void *data, handler_wrapper_t **wait_wrapper)
{
	int8_t state = 0;
	uint32_t wrapper_num;
	handler_wrapper_t* wrapper;
	extern atomic_t outstanding_calls;

	// prevents us from ever having more than NUM_HANDLER_WRAPPERS callers in
	// the process of competing for vectors.  not decremented until both after
	// the while(1) loop and after it's been waited on.
	atomic_inc(&outstanding_calls);
	if (atomic_read(&outstanding_calls) > NUM_HANDLER_WRAPPERS) {
		atomic_dec(&outstanding_calls);
		return -EBUSY;
	}

	// assumes our cores are numbered in order
	if ((type == 4) && (dest >= num_cpus))
		panic("Destination CPU %d does not exist!", dest);

	// build the mask based on the type and destination
	INIT_CHECKLIST_MASK(cpu_mask, MAX_NUM_CPUS);
	// set checklist mask's size dynamically to the num cpus actually present
	cpu_mask.size = num_cpus;
	switch (type) {
		case 1: // self
			SET_BITMASK_BIT(cpu_mask.bits, core_id());
			break;
		case 2: // all
			FILL_BITMASK(cpu_mask.bits, num_cpus);
			break;
		case 3: // all but self
			FILL_BITMASK(cpu_mask.bits, num_cpus);
			CLR_BITMASK_BIT(cpu_mask.bits, core_id());
			break;
		case 4: // physical mode
			// note this only supports sending to one specific physical id
			// (only sets one bit, so if multiple cores have the same phys id
			// the first one through will set this).
			SET_BITMASK_BIT(cpu_mask.bits, dest);
			break;
		case 5: // logical mode
			// TODO
			warn("Logical mode bitmask handler protection not implemented!");
			break;
		default:
			panic("Invalid type for cross-core function call!");
	}

	// Find an available vector/wrapper.  Starts with this core's id (mod the
	// number of wrappers).  Walk through on conflict.
	// Commit returns an error if it wanted to give up for some reason,
	// like taking too long to acquire the lock or clear the mask, at which
	// point, we try the next one.
	// When we are done, wrapper points to the one we finally got.
	// this wrapper_num trick doesn't work as well if you send a bunch in a row
	// and wait, since you always check your main one (which is currently busy).
	wrapper_num = core_id() % NUM_HANDLER_WRAPPERS;
	while(1) {
		wrapper = &handler_wrappers[wrapper_num];
		if (!commit_checklist_wait(wrapper->cpu_list, &cpu_mask))
			break;
		wrapper_num = (wrapper_num + 1) % NUM_HANDLER_WRAPPERS;
		/*
		uint32_t count = 0;
		// instead of deadlock, smp_call can fail with this.  makes it harder
		// to use (have to check your return value).  consider putting a delay
		// here too (like if wrapper_num == initial_wrapper_num)
		if (count++ > NUM_HANDLER_WRAPPERS * 1000) // note 1000 isn't enough...
			return -EBUSY;
		*/
	}

	// Wanting to wait is expressed by having a non-NULL handler_wrapper_t**
	// passed in.  Pass out our reference to wrapper, to wait later.
	// If we don't want to wait, release the checklist (though it is still not
	// clear, so it can't be used til everyone checks in).
	if (wait_wrapper)
		*wait_wrapper = wrapper;
	else {
		release_checklist(wrapper->cpu_list);
		atomic_dec(&outstanding_calls);
	}

	/* TODO: once we can unregister, we can reregister.  This here assumes that
	 * there is only one IRQ registered, and its the one for SMP call function.
	 * We're waiting on RCU to do a nice unregister. */
	extern struct irq_handler *irq_handlers[];
	if (!irq_handlers[wrapper->vector]) {
		register_irq(wrapper->vector, handler, data, MKBUS(BusIPI, 0, 0, 0));
	} else {
		/* we're replacing the old one.  hope it was ours, and the IRQ is firing
		 * concurrently (if it is, there's an smp_call bug)! */
		irq_handlers[wrapper->vector]->isr = handler;
		irq_handlers[wrapper->vector]->data = data;
	}

	// WRITE MEMORY BARRIER HERE
	enable_irqsave(&state);
	// Send the proper type of IPI.  I made up these numbers.
	switch (type) {
		case 1:
			send_self_ipi(wrapper->vector);
			break;
		case 2:
			send_broadcast_ipi(wrapper->vector);
			break;
		case 3:
			send_all_others_ipi(wrapper->vector);
			break;
		case 4: // physical mode
			send_ipi(dest, wrapper->vector);
			break;
		case 5: // logical mode
			send_group_ipi(dest, wrapper->vector);
			break;
		default:
			panic("Invalid type for cross-core function call!");
	}
	// wait long enough to receive our own broadcast (PROBABLY WORKS) TODO
	lapic_wait_to_send();
	disable_irqsave(&state);
	return 0;
}

// Wrapper functions.  Add more as they are needed.
int smp_call_function_self(isr_t handler, void *data,
                           handler_wrapper_t **wait_wrapper)
{
	return smp_call_function(1, 0, handler, data, wait_wrapper);
}

int smp_call_function_all(isr_t handler, void *data,
                          handler_wrapper_t **wait_wrapper)
{
	return smp_call_function(2, 0, handler, data, wait_wrapper);
}

int smp_call_function_single(uint32_t dest, isr_t handler, void *data,
                             handler_wrapper_t **wait_wrapper)
{
	return smp_call_function(4, dest, handler, data, wait_wrapper);
}

// If you want to wait, pass the address of a pointer up above, then call
// this to do the actual waiting.  Be somewhat careful about uninitialized 
// or old wrapper pointers.
int smp_call_wait(handler_wrapper_t* wrapper)
{
	if (wrapper) {
		waiton_checklist(wrapper->cpu_list);
		return 0;
	} else {
		warn("Attempting to wait on null wrapper!  Check your return values!");
		return -EFAIL;
	}
}

