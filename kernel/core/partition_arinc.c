/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 */

#include <config.h>

#include <core/partition_arinc.h>
#include <core/sched_arinc.h>
#include "thread_internal.h"
#include <common.h>
#include <arch.h>
#include <uaccess.h>


/*
 * Function which is executed in kernel-only partition's context when
 * partition's mode is IDLE.
 * 
 * Note, that we do not enable local preemption here.
 * This has nice effect in case when partition has moved into this mode
 * because of errors: even if some partition's data are corrupted,
 * idle have high chance to work.
 */
static void idle_func(void)
{
    wait_infinitely();
}

void pok_partition_arinc_idle(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	uint32_t fake_sp;
	
	// Unconditionally off preemption.
	part->base_part.preempt_local_disabled = 1;
	
	pok_context_restart(
		&part->base_part.initial_sp,
		&idle_func,
		&fake_sp);
}

/* Helpers */

/*
 * Reset thread object as it is not used.
 */
static void thread_reset(pok_thread_t* t)
{
    t->name[0] = '\0';
    // Everything else will be set at thread creation time.
}


/* 
 * Initialize thread object. 
 * 
 * Executed once during partition initialization.
 */
static void thread_init(pok_thread_t* t)
{
    pok_dstack_alloc(&t->initial_sp, KERNEL_STACK_SIZE_DEFAULT);
}

// This name is not accessible for user space
static char main_thread_name[MAX_NAME_LENGTH] = "main";

/* Start function for partition. */
static void partition_arinc_start(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

	if(part->base_part.restarted_externally)
	{
		part->base_part.restarted_externally = FALSE;
		current_partition_arinc->mode = POK_PARTITION_MODE_INIT_COLD;
	}

	for(int i = 0; i < part->nports_queuing; i++)
	{
		pok_port_queuing_init(&part->ports_queuing[i]);
	}

	for(int i = 0; i < part->nports_sampling; i++)
	{
		pok_port_sampling_init(&part->ports_sampling[i]);
	}

	INIT_LIST_HEAD(&part->eligible_threads);
	delayed_event_queue_init(&part->queue_deadline);
	delayed_event_queue_init(&part->queue_delayed);
    
	for(int i = 0; i < part->nthreads; i++)
	{
		thread_reset(&part->threads[i]);
	}
	
	part->nthreads_used = 0;
	part->user_stack_state = 0;
	
	part->intra_memory_size_used = 0;
	part->nbuffers_used = 0;
	part->nblackboards_used = 0;
	part->nsemaphores_used = 0;
	part->nevents_used = 0;

	part->thread_current = NULL;
#ifdef POK_NEEDS_ERROR_HANDLING
	part->thread_error = NULL;
	INIT_LIST_HEAD(&part->error_list);
#endif

    pok_thread_t* thread_main = &part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID];

	thread_main->entry = (void* __user)part->main_entry;
	thread_main->base_priority = 0;
	thread_main->period = POK_TIME_INFINITY;
	thread_main->time_capacity = POK_TIME_INFINITY;
	thread_main->deadline = DEADLINE_SOFT;
	strncpy(thread_main->name, main_thread_name, MAX_NAME_LENGTH);
	thread_main->user_stack_size = part->main_user_stack_size;

    thread_create(thread_main);

	part->nthreads_used = POK_PARTITION_ARINC_MAIN_THREAD_ID + 1;

	sched_arinc_start();

	/* Current context is lost and may be reused for "do_nothing" thread
	 * or for IDLE partition's mode.
	 */
}

void pok_partition_arinc_reset(pok_partition_mode_t mode)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	
	part->base_part.preempt_local_disabled = 1;
	
	assert(mode == POK_PARTITION_MODE_INIT_WARM
		|| mode == POK_PARTITION_MODE_INIT_COLD);
	
	assert(mode == POK_PARTITION_MODE_INIT_COLD ||
		part->mode != POK_PARTITION_MODE_INIT_COLD);

	part->mode = mode;

	pok_partition_restart();
}

static int pok_sched_arinc_get_number_of_threads(pok_partition_t* part)
{
	pok_partition_arinc_t* part_arinc = container_of(part,
		typeof(*part_arinc), base_part);

	return part_arinc->nthreads + 1; //IDLE thread
}

static int pok_sched_arinc_get_current_thread_index(pok_partition_t* part)
{
	pok_partition_arinc_t* part_arinc = container_of(part,
		typeof(*part_arinc), base_part);

	return part_arinc->thread_current
		? part_arinc->thread_current - part_arinc->threads
		: part_arinc->nthreads; //IDLE thread
}

static int pok_sched_arinc_get_thread_at_index(pok_partition_t* part,
	int index, void** private)
{
    pok_partition_arinc_t* part_arinc = container_of(part,
		typeof(*part_arinc), base_part);
	if(index >part_arinc->nthreads) return 1;

	if(index == part_arinc->nthreads)
	{
		*private = NULL;
	}
	else
	{
		*private = part_arinc->threads + index;
	}

	return 0;
}

static void pok_sched_arinc_get_thread_info(pok_partition_t* part, int index, void* private,
	print_cb_t print_cb, void* cb_data)
{
// Write given string (null-terminated)
#define WRITE_STR(s) print_cb(s, strlen(s), cb_data)
	pok_thread_t* t = private;
	pok_partition_arinc_t* part_arinc = container_of(part, typeof(*part_arinc), base_part);
	if(!t)
	{
		WRITE_STR("IDLE");
	}
	else if(index == POK_PARTITION_ARINC_MAIN_THREAD_ID)
	{
		// Main thread. Currently do not bother with its state
		WRITE_STR(t->name);
	}
	else if(index < part_arinc->nthreads_used)
	{
		WRITE_STR(t->name);
		// Write state of the thread
		WRITE_STR(" ");
		switch(t->state)
		{
		case POK_STATE_STOPPED:
			WRITE_STR("Stopped");
		break;
		case POK_STATE_WAITING:
			WRITE_STR("Waiting"); // Waiting for anything except resume.
		break;
		case POK_STATE_RUNNABLE:
			if(t->suspended)
				WRITE_STR("Suspended");
			else if(part_arinc->thread_current == t)
				WRITE_STR("Running");
			else
				WRITE_STR("Ready");
		break;
		default:
			unreachable();
		}
	}
	else
	{
		WRITE_STR("[Not created]");
	}
#undef WRITE_STR
}

static struct regs* pok_sched_arinc_get_thread_registers(pok_partition_t* part,
	int index, void* private)
{
	pok_thread_t* t = private;
	pok_partition_arinc_t* part_arinc = container_of(part, typeof(*part_arinc), base_part);

	if(!t)
	{
		// Idle thread
		return NULL;
	}
	else if(index < part_arinc->nthreads_used && t->entry_sp_user)
	{
		return (struct regs*) t->entry_sp_user;
	}
	else
	{
		// Not created or user space hasn't been called yet.
		return NULL;
	}
}


static const struct pok_partition_sched_operations arinc_sched_ops = {
	.on_event = &pok_sched_arinc_on_event,
	.get_number_of_threads = &pok_sched_arinc_get_number_of_threads,
	.get_current_thread_index = &pok_sched_arinc_get_current_thread_index,
	.get_thread_at_index = &pok_sched_arinc_get_thread_at_index,
	.get_thread_info = &pok_sched_arinc_get_thread_info,
	.get_thread_registers = &pok_sched_arinc_get_thread_registers,
};

static const struct pok_partition_operations arinc_ops = {
	.start = &partition_arinc_start,
	.process_partition_error = &pok_partition_arinc_process_error,
};

void pok_partition_arinc_init(pok_partition_arinc_t* part)
{
	size_t size = part->size;

	pok_dstack_alloc(&part->base_part.initial_sp, DEFAULT_STACK_SIZE);

	uintptr_t base_addr = (uintptr_t) pok_bsp_alloc_partition(part->size);
	uintptr_t base_vaddr = pok_space_base_vaddr(base_addr);
	
    /* 
	 * Memory.
	 */
	part->base_addr   = base_addr;
	part->base_vaddr  = base_vaddr;

	pok_create_space (part->base_part.space_id, base_addr, size);

	// TODO: this should be performed on restart too.
	pok_arch_load_partition(part,
		part->partition_id, /* elf_id*/
		part->base_part.space_id,
		&part->main_entry);

	for(int i = 0; i < part->nthreads; i++)
	{
		thread_init(&part->threads[i]);
	}
	
	part->base_part.part_ops = &arinc_ops;
	part->base_part.part_sched_ops = &arinc_sched_ops;
	
	part->intra_memory = part->intra_memory_size
		? pok_bsp_mem_alloc(part->intra_memory_size)
		: NULL;
}

void* partition_arinc_im_get(size_t size, size_t alignment)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	
	size_t start = ALIGN_SIZE(part->intra_memory_size_used, alignment);
	
	// TODO: revisit boundary check
	if(start <= part->intra_memory_size
		&&	start + size <= part->intra_memory_size) {
		
		part->intra_memory_size_used = start + size;
		return part->intra_memory + start;
	}
	
	return NULL;
}


/* 
 * Return current state of partition's intra memory.
 * 
 * Value return may be used in partition_arinc_im_rollback().
 */
void* partition_arinc_im_current(void)
{
	return (void*)current_partition_arinc->intra_memory_size_used;
}

/*
 * Revert all intra memory usage requests since given state.
 * 
 * `state` should be obtains with partition_arinc_im_current().
 */
void partition_arinc_im_rollback(void* prev_state)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	size_t size_used = (size_t)prev_state;
	
	assert(size_used <= part->intra_memory_size);
	
	part->intra_memory_size_used = size_used;
}


/*
 * Transition from INIT_* mode to NORMAL.
 * 
 * Executed with local preemption disabled.
 */
static void partition_set_mode_normal(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	
	// Cached value of current time.
	pok_time_t current_time = POK_GETTICK();
	/*
	 * Cached value of first release point.
	 * 
	 * NOTE: Initially it is not cached. Such implementation allows to
	 * not calculate release point for partition which have no periodic
	 * processes.
	 */
	pok_time_t periodic_release_point = POK_TIME_INFINITY;

	part->mode = POK_PARTITION_MODE_NORMAL;
	part->lock_level = 0;
	pok_sched_local_invalidate();

	for(int i = POK_PARTITION_ARINC_MAIN_THREAD_ID + 1; i < part->nthreads_used; i++)
	{
		pok_thread_t* t = &part->threads[i];
		pok_time_t thread_start_time;
		
		if(t->state == POK_STATE_STOPPED) continue;
		
		/*
		 * The only thing thread can wait in `INIT_*` mode is
		 * NORMAL mode switch.
		 */
		
		if(pok_time_is_infinity(t->period))
		{
			// Aperiodic process.
			thread_start_time = current_time + t->delayed_time;
		}
		else
		{
			// Periodic process
			if(pok_time_is_infinity(periodic_release_point))
				periodic_release_point = get_next_periodic_processing_start();
			thread_start_time = periodic_release_point + t->delayed_time;
		}
		
		if(thread_start_time <= current_time)
			thread_wake_up(t);
		else
			thread_delay_event(t, thread_start_time, &thread_wake_up);
		
		if(!pok_time_is_infinity(t->time_capacity))
		{
			thread_set_deadline(t, thread_start_time + t->time_capacity);
		}
	}
}


// Executed with local preemption disabled.
static pok_ret_t partition_set_mode_internal (const pok_partition_mode_t mode)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	
	switch(mode)
	{
	case POK_PARTITION_MODE_INIT_WARM:
		if(part->mode == POK_PARTITION_MODE_INIT_COLD)
			return POK_ERRNO_PARTITION_MODE;
	// Walkthrough
	case POK_PARTITION_MODE_INIT_COLD:
		part->base_part.start_condition = POK_START_CONDITION_PARTITION_RESTART;
		pok_partition_arinc_reset(mode); // Never return.
	break;
	case POK_PARTITION_MODE_IDLE:
		pok_partition_arinc_idle(); // Never return.
	break;
	case POK_PARTITION_MODE_NORMAL:
		if(part->mode == POK_PARTITION_MODE_NORMAL)
			return POK_ERRNO_UNAVAILABLE; //TODO: revise error code
		partition_set_mode_normal();
	break;
	default:
		return POK_ERRNO_EINVAL;
	}
	
	return POK_ERRNO_OK;
}

pok_ret_t pok_partition_set_mode_current (const pok_partition_mode_t mode)
{
	pok_ret_t res;
	
	pok_preemption_local_disable();
	res = partition_set_mode_internal(mode);
	pok_preemption_local_enable();
	
	return res;
}

/**
 * Get partition information. Used for ARINC GET_PARTITION_STATUS function.
 */
pok_ret_t pok_current_partition_get_id(pok_partition_id_t *id)
{
    *id = current_partition_arinc->partition_id;
    return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_get_period (pok_time_t* __user period)
{
    if(!check_user_write(period)) return POK_ERRNO_EFAULT;
    
    __put_user(period, current_partition_arinc->period);
    return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_get_duration (pok_time_t* __user duration)
{
	if(!check_user_write(duration)) return POK_ERRNO_EFAULT;
    
    __put_user(duration, current_partition_arinc->duration);

    return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_get_operating_mode (pok_partition_mode_t* __user op_mode)
{
    if(!check_user_write(op_mode)) return POK_ERRNO_EFAULT;
    
    __put_user(op_mode, current_partition_arinc->mode);

    return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_get_lock_level (int32_t* __user lock_level)
{
	if(!check_user_write(lock_level)) return POK_ERRNO_EFAULT;

    __put_user(lock_level, current_partition_arinc->lock_level);
    
    return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_get_start_condition (pok_start_condition_t *start_condition)
{
    if(!check_user_write(start_condition)) return POK_ERRNO_EFAULT;
    
    __put_user(start_condition,
		current_partition_arinc->base_part.start_condition);

    return POK_ERRNO_OK;
}

/* 
 * Whether lock level cannot be changed now. 
 * 
 * NOTE: Doesn't require disabled local preemption.
 */
static pok_bool_t is_lock_level_blocked(void)
{
	pok_partition_arinc_t* part = current_partition_arinc;
	return part->mode != POK_PARTITION_MODE_NORMAL ||
      part->thread_current == part->thread_error;
}

pok_ret_t pok_current_partition_inc_lock_level(int32_t * __user lock_level)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    if(is_lock_level_blocked())
		return POK_ERRNO_PARTITION_MODE;
    if(part->lock_level == MAX_LOCK_LEVEL)
		return POK_ERRNO_EINVAL;
	if(!check_user_write(lock_level))
		return POK_ERRNO_EFAULT;

    pok_preemption_local_disable();
	part->lock_level++;
	part->thread_locked = part->thread_current;
	// Note: this doesn't invalidate any scheduling event.
	pok_preemption_local_enable();

	__put_user(lock_level, current_partition_arinc->lock_level);

	return POK_ERRNO_OK;
}

pok_ret_t pok_current_partition_dec_lock_level(int32_t * __user lock_level)
{
    if(is_lock_level_blocked())
		return POK_ERRNO_PARTITION_MODE;
    if(current_partition_arinc->lock_level == 0)
		return POK_ERRNO_EINVAL;
	if(!check_user_write(lock_level))
		return POK_ERRNO_EFAULT;

    pok_preemption_local_disable();
    if(--current_partition_arinc->lock_level == 0)
    {
		if(current_thread->eligible_elem.prev != &current_partition_arinc->eligible_threads)
		{
			// We are not the first thread in eligible queue
			pok_sched_local_invalidate();
		}
	}
	pok_preemption_local_enable();
	
	__put_user(lock_level, current_partition_arinc->lock_level);
	
	return POK_ERRNO_OK;
}


void pok_partition_arinc_init_all(void)
{
	for(int i = 0; i < pok_partitions_arinc_n; i++)
	{
		pok_partition_arinc_init(&pok_partitions_arinc[i]);

#ifdef POK_NEEDS_INSTRUMENTATION
		pok_instrumentation_partition_archi (i);
#endif
	}
}
