/* Copyright (c) 2013 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Plan9 style Rendezvous (http://plan9.bell-labs.com/sys/doc/sleep.html)
 *
 * We implement it with CVs, and it can handle multiple sleepers/wakers. */

#include <rendez.h>
#include <kthread.h>
#include <alarm.h>
#include <assert.h>
#include <smp.h>

void rendez_init(struct rendez *rv)
{
	cv_init_irqsave(&rv->cv);
}

void rendez_sleep(struct rendez *rv, int (*cond)(void*), void *arg)
{
	int8_t irq_state = 0;
	cv_lock_irqsave(&rv->cv, &irq_state);
	/* Mesa-style semantics, which is definitely what you want.  See the
	 * discussion at the end of the URL above. */
	while (!cond(arg)) {
		cv_wait(&rv->cv);
		cpu_relax();
	}
	cv_unlock_irqsave(&rv->cv, &irq_state);
}

/* Force a wakeup of all waiters on the rv, including non-timeout users.  For
 * those, they will just wake up, see the condition is still false (probably)
 * and go back to sleep. */
static void rendez_alarm_handler(struct alarm_waiter *awaiter)
{
	struct rendez *rv = (struct rendez*)awaiter->data;
	rendez_wakeup(rv);
}

/* Like sleep, but it will timeout in 'msec' milliseconds. */
void rendez_sleep_timeout(struct rendez *rv, int (*cond)(void*), void *arg,
                          unsigned int msec)
{
	int8_t irq_state = 0;
	struct alarm_waiter awaiter;
	struct timer_chain *pcpui_tchain = &per_cpu_info[core_id()].tchain;

	assert((int)msec > 0);
	/* The handler will call rendez_wake, but won't mess with the condition
	 * state.  It's enough to break us out of cv_wait() to see .has_fired. */
	init_awaiter(&awaiter, rendez_alarm_handler);
	awaiter.data = rv;
	set_awaiter_rel(&awaiter, msec);
	/* Set our alarm on this cpu's tchain.  Note that when we sleep in cv_wait,
	 * we could be migrated, and later on we could be unsetting the alarm
	 * remotely. */
	set_alarm(pcpui_tchain, &awaiter);
	cv_lock_irqsave(&rv->cv, &irq_state);
	/* We could wake early for a few reasons.  Legit wakeups after a changed
	 * condition (and we should exit), other alarms with different timeouts (and
	 * we should go back to sleep), etc.  Note it is possible for our alarm to
	 * fire immediately upon setting it: before we even cv_lock. */
	while (!cond(arg) && !awaiter.has_fired) {
		cv_wait(&rv->cv);
		cpu_relax();
	}
	cv_unlock_irqsave(&rv->cv, &irq_state);
	/* Turn off our alarm.  If it already fired, this is a no-op.  Note this
	 * could be cross-core. */
	unset_alarm(pcpui_tchain, &awaiter);
}

/* plan9 rendez returned a pointer to the proc woken up.  we return "true" if we
 * woke someone up. */
bool rendez_wakeup(struct rendez *rv)
{
	int8_t irq_state = 0;
	bool ret;
	/* The plan9 style "one sleeper, one waker" could get by with a signal here.
	 * But we want to make sure all potential waiters are woken up. */
	cv_lock_irqsave(&rv->cv, &irq_state);
	ret = rv->cv.nr_waiters ? TRUE : FALSE;
	__cv_broadcast(&rv->cv);
	cv_unlock_irqsave(&rv->cv, &irq_state);
	return ret;
}