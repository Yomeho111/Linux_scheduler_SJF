#include <linux/cpumask.h>

void init_oven_rq(struct oven_rq *oven_rq)
{
	INIT_LIST_HEAD(&oven_rq->oven_rq_list);
	oven_rq->oven_nr_running = 0;
	oven_rq->total_weight = 0;
}


// For updating info about the currently running task
// For example, for command "top"
// From Kyle, do this!
// Copied from update_curr_rt
static void update_curr_oven(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	u64 delta_exec;
	u64 now;

	if (curr->sched_class != &oven_sched_class)
		return;

	now = rq_clock_task(rq);
	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	schedstat_set(curr->stats.exec_max,
		      max(curr->stats.exec_max, delta_exec));

	trace_sched_stat_runtime(curr, delta_exec, 0);

	update_current_exec_runtime(curr, now, delta_exec);
}

static inline void
update_stats_wait_start_oven(struct rq *rq, struct task_struct *p)
{
	struct sched_statistics *stats;

	if (!schedstat_enabled())
		return;

	stats = &p->stats;

	__update_stats_wait_start(rq, p, stats);
}

static inline void
update_stats_wait_end_oven(struct rq *rq, struct task_struct *p)
{
	struct sched_statistics *stats;

	if (!schedstat_enabled())
		return;

	stats = &p->stats;

	__update_stats_wait_end(rq, p, stats);
}

/*
 * Adding/removing a task to/from a priority array:
 * traverse to it's place and put it to the tail
 * (after all tasks with the same weight if there are any)
 */
static void
enqueue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_oven_entity *oven_se = &p->oven;
	struct sched_oven_entity *entry;
	int added = 0;

	check_schedstat_required();
	update_stats_wait_start_oven(rq, p);
	//printk(KERN_INFO "enqueue_task_oven(), pid = %d, cpu = %d", task_pid_nr(p), rq->cpu);
	list_for_each_entry(entry, &rq->oven.oven_rq_list, run_list) {
		if (oven_se->weight < entry->weight) {
			list_add_tail(&oven_se->run_list, &entry->run_list);
			added = 1;
			break;
		}
	}

	if (!added) {
		list_add_tail(&oven_se->run_list, &rq->oven.oven_rq_list);
	}

	oven_se->on_rq = 1;
	++rq->oven.oven_nr_running;
	rq->oven.total_weight += oven_se->weight;
	add_nr_running(rq, 1);
}

static void
dequeue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
	// dequeue are called when calling schedule(), so we already have the rq lock
	// the linked list shouldn't be empty, since we're dequeue the current task
	// should we for each entry of the rq?
	struct sched_oven_entity *oven_se = &p->oven;
	update_curr_oven(rq);

	if (oven_se->on_rq) {
		list_del(&oven_se->run_list);
		oven_se->on_rq = 0;
		--rq->oven.oven_nr_running;
		rq->oven.total_weight -= oven_se->weight;
		sub_nr_running(rq, 1);
	}
}


static void requeue_task_oven(struct rq *rq, struct task_struct *p)
{
	/*put the task to the tail of the rq*/
	struct sched_oven_entity *oven_se = &p->oven;
	struct sched_oven_entity *entry;
	int added = 0;
	//printk(KERN_INFO "requeue_task_oven(), pid = %d, , cpu = %d", task_pid_nr(p), rq->cpu);

	list_del(&oven_se->run_list);

	list_for_each_entry(entry, &rq->oven.oven_rq_list, run_list) {
		if (oven_se->weight < entry->weight) {
			list_add_tail(&oven_se->run_list, &entry->run_list);
			added = 1;
			break;
		}
	}

	if (!added) {
		list_add_tail(&oven_se->run_list, &rq->oven.oven_rq_list);
	}
}


// do we need it?
static void yield_task_oven(struct rq *rq)
{
	// Should yield the current task and call schedule()
	// requeue and call schedule()
	// For rt:
	// requeue_task_rt(rq, rq->curr, 0);
	// return for now?
	//printk(KERN_INFO "yield_task_oven(), curr->pid = %d, cpu = %d", task_pid_nr(rq->curr), rq->cpu);
	requeue_task_oven(rq, rq->curr);
}

// Should we preempt the curr running task?
static void check_preempt_curr_oven(struct rq *rq, struct task_struct *p, int flags)
{
	//printk(KERN_INFO "check_preempt_curr_oven(), pid = %d, curr->pid = %d, cpu = %d", task_pid_nr(p), task_pid_nr(rq->curr), rq->cpu);
	if (p->oven.weight < rq->curr->oven.weight) {
		resched_curr(rq);
	}
}

// When scheduling parameters change
// if running, put_prev_task class
// a good place to requeue (especially for RR (weight = 10000), because that won't get to prio_changed)
// requeue should be: traverse to it's place and put it to the tail (after all tasks with the same weight if there are any)
static void put_prev_task_oven(struct rq *rq, struct task_struct *p)
{
	// do nothing before stop running, only update the statistics
	// printk(KERN_INFO "put_prev_task_oven()");
	// printk(KERN_INFO "put_prev_task_oven(), pid = %d, cpu = %d", task_pid_nr(p), rq->cpu);
	if (p->oven.on_rq)
		update_stats_wait_start_oven(rq, p);
	update_curr_oven(rq);
}



static struct task_struct *pick_task_oven(struct rq *rq)
{
	// logically should be the same with pick_next_task_idle()
	// return the task_struct that has the node rq head is pointing to
	struct sched_oven_entity *oven_se;
	struct task_struct *next_task;
	if (list_empty(&rq->oven.oven_rq_list))
		return NULL;
	oven_se = list_first_entry(&rq->oven.oven_rq_list, struct sched_oven_entity, run_list);
	next_task = container_of(oven_se, struct task_struct, oven);
	// printk(KERN_INFO "pick_task_oven(), curr->pid = %d, cpu = %d, next->pid = %d", task_pid_nr(rq->curr), rq->cpu, task_pid_nr(next_task));
	return next_task;
}

// When scheduling parameters change
// if running, set_next_task new class
// can be left empty
static void set_next_task_oven(struct rq *rq, struct task_struct *p, bool first)
{
	// printk(KERN_INFO "Weight of task %d is %d, inside set_next_task_oven()", task_pid_nr(next), next->oven.weight);
	p->se.exec_start = rq_clock_task(rq);

	if (p->oven.on_rq)
		update_stats_wait_end_oven(rq, p);
}

// Should this function be static or not?
// pick_next_task_rt static, pick_next_task_idle not
struct task_struct *pick_next_task_oven(struct rq *rq)
{
	struct task_struct *p = pick_task_oven(rq);
	// printk(KERN_INFO "pick_next_task_oven(), see pick_task_oven()");
	if (p)
		set_next_task_oven(rq, p, true);
	return p;
}

#ifdef CONFIG_SMP

static int
select_task_rq_oven(struct task_struct *p, int cpu, int flags)
{
	struct rq *rq;
	struct rq *curr_rq;
	int curr_cpu;
	//printk(KERN_INFO "select_task_rq_oven(), pid = %d, old cpu = %d", task_pid_nr(p), cpu);

	if (!(flags & (WF_TTWU | WF_FORK)))
		goto out;

	rq = cpu_rq(cpu);

	// rq and cpu should be the one with the lowest total weight
	rcu_read_lock();
	for_each_online_cpu(curr_cpu) {
		curr_rq = cpu_rq(curr_cpu);
		if (curr_rq->oven.total_weight < rq->oven.total_weight) {
			rq = curr_rq;
			cpu = curr_cpu;
		}
	}
	rcu_read_unlock();
out:
	return cpu;
}


static struct task_struct *pick_oven_pushable_task(struct rq *rq, int cpu)
{
	struct task_struct *p;
	struct sched_oven_entity *entry;

	list_for_each_entry(entry, &rq->oven.oven_rq_list, run_list) {
		p = container_of(entry, struct task_struct, oven);
		if (!task_current(rq, p) && !is_migration_disabled(p) && !task_on_cpu(rq, p) &&
	    cpumask_test_cpu(cpu, &p->cpus_mask))
			return p;
	}

	return NULL;
}

static void pull_oven_task(struct rq *this_rq)
{
	int this_cpu = this_rq->cpu, cpu;
	bool resched = false;
	struct task_struct *p = NULL;
	struct rq *src_rq;

	for_each_online_cpu(cpu) {
		if (this_cpu == cpu)
			continue;
		src_rq = cpu_rq(cpu);
		if (src_rq->oven.oven_nr_running < 2)
			continue;
		double_lock_balance(this_rq, src_rq);

		p = pick_oven_pushable_task(src_rq, this_cpu);

		if (p) {
			// printk(KERN_INFO "pull_oven_task() for balance, pid = %d, old cpu = %d", task_pid_nr(p), cpu);
			deactivate_task(src_rq, p, 0);
			set_task_cpu(p, this_cpu);
			activate_task(this_rq, p, 0);
			resched = true;
		}
		double_unlock_balance(this_rq, src_rq);

		if (resched)
			break;
	}


	if (resched)
		resched_curr(this_rq);
}

static inline bool sched_oven_runnable(struct rq *rq)
{
	return rq->oven.oven_nr_running > 0;
}


static int
balance_oven(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	// printk(KERN_INFO "balance_oven(), prev->pid = %d, cpu = %d", task_pid_nr(prev), rq->cpu);
	if (rq->nr_running)
		return 1;

	rq_unpin_lock(rq, rf);
	pull_oven_task(rq);
	rq_repin_lock(rq, rf);
	return sched_oven_runnable(rq);
}

#endif

static void task_tick_oven(struct rq *rq, struct task_struct *curr, int queued)
{
	// printk(KERN_INFO "task_tick_oven()");
	struct sched_oven_entity *oven_se = &curr->oven;
	update_curr_oven(rq);

	if (oven_se->weight < MAX_OVEN_WEIGHT)
		return;

	requeue_task_oven(rq, curr);
	resched_curr(rq);
	return;
}

static void
prio_changed_oven(struct rq *rq, struct task_struct *p, int oldprio)
{
	// additionally, check next thing on the list have a smaller weight + switched_to
	// if smaller, we need to requeue p and resched curr
	// if larger, just similar to switched_to
	struct sched_oven_entity *oven_se = &p->oven;

	if (!task_on_rq_queued(p))
		return;
	//printk(KERN_INFO "prio_changed_oven(), pid = %d, cpu = %d, old w = %d, new w = %d", task_pid_nr(p), rq->cpu, oldprio, oven_se->weight);
	if (task_current(rq, p)) {
		if (oven_se->weight > oldprio)
			resched_curr(rq);
	} else {
		if (oven_se->weight < rq->curr->oven.weight)
			resched_curr(rq);
	}
	return;
}

// some task just moved into my sched_class
// task is currently running，resched_curr
// task on rq, rcheck_preempt_curr
static void switched_to_oven(struct rq *rq, struct task_struct *p)
{
	//printk(KERN_INFO "switched_to_oven(), pid = %d, cpu = %d", task_pid_nr(p), rq->cpu);
	if (task_on_rq_queued(p)) {
		if (task_current(rq, p))
			resched_curr(rq);
		else
			check_preempt_curr(rq, p, 0);
	}
}





// Question: should  each function be static or not?

DEFINE_SCHED_CLASS(oven) = {

	.enqueue_task		= enqueue_task_oven, // TODO
	.dequeue_task		= dequeue_task_oven, // TODO
    .yield_task		= yield_task_oven, // Leave empty

	.check_preempt_curr	= check_preempt_curr_oven, // TODO for second iter

	.pick_next_task		= pick_next_task_oven, // TODO
	.put_prev_task		= put_prev_task_oven, // curr (may?) stop running, notify
	.set_next_task          = set_next_task_oven, // for changing params, update metadata, do nothing

#ifdef CONFIG_SMP
	.balance		= balance_oven,
	.pick_task		= pick_task_oven, // TODO, same as pick_next_task
	.select_task_rq		= select_task_rq_oven, // TODO, can just return cpu zero
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif

	.task_tick		= task_tick_oven, // TODO

    // .get_rr_interval	= get_rr_interval_oven, // TODO? For RR?

	.prio_changed		= prio_changed_oven, // TODO
	.switched_to		= switched_to_oven, // TODO
	.update_curr		= update_curr_oven, // leave it empty
};
