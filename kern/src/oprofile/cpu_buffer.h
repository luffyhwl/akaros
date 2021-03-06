/**
 * @file cpu_buffer.h
 *
 * @remark Copyright 2002-2009 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <levon@movementarian.org>
 * @author Robert Richter <robert.richter@amd.com>
 */

#ifndef OPROFILE_CPU_BUFFER_H
#define OPROFILE_CPU_BUFFER_H
#include <vfs.h>
#include <kfs.h>
#include <slab.h>
#include <kmalloc.h>
#include <kref.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <error.h>
#include <pmap.h>
#include <smp.h>
#include <oprofile.h>

int alloc_cpu_buffers(void);
void free_cpu_buffers(void);

void start_cpu_work(void);
void end_cpu_work(void);
void flush_cpu_work(void);

/* CPU buffer is composed of samples. 
 * As these are extracted from the buffer, they are encapsulated
 * in entries, which include additional info.
 */
struct op_sample {
	unsigned long eip;
	unsigned long event;
	unsigned long data[0];
};

struct op_entry;

struct oprofile_cpu_buffer {
	spinlock_t lock;
	unsigned long buffer_size;
	struct proc *last_proc;
	int last_is_kernel;
	int tracing;
	unsigned long sample_received;
	unsigned long sample_lost_overflow;
	unsigned long backtrace_aborted;
	unsigned long sample_invalid_eip;
	int cpu;
	struct block *block;
	/* long term plan: when we fill the block,
	 * we write it to fullblock, and pull a
	 * freeblock from the emptyblock queue. 
	 * The thread that pulls fullbocks and
	 * allocates emptyblocks is timer-driven.
	 * Or, barret will make me use his queues,
	 * which is also fine; I just find the queue
	 * functions convenient because they interface to
	 * the dev code so easily.
	 */
	struct queue *fullqueue, *emptyqueue;
};

/* extra data flags */
#define KERNEL_CTX_SWITCH	(1UL << 0)
#define IS_KERNEL		(1UL << 1)
#define TRACE_BEGIN		(1UL << 2)
#define USER_CTX_SWITCH		(1UL << 3)

#endif /* OPROFILE_CPU_BUFFER_H */
