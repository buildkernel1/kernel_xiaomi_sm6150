// SPDX-License-Identifier: GPL-2.0
/*
 * Energy Model of CPUs
 *
 * Copyright (c) 2018, Arm ltd.
 * Written by: Quentin Perret, Arm ltd.
 */

#define pr_fmt(fmt) "energy_model: " fmt

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/energy_model.h>
#include <linux/sched.h>
#include <linux/slab.h>

/* Mapping of each CPU to the performance domain to which it belongs. */
static DEFINE_PER_CPU(struct em_perf_domain *, em_data);

/*
 * Mutex serializing the registrations of performance domains and letting
 * callbacks defined by drivers sleep.
 */
static DEFINE_MUTEX(em_pd_mutex);

static struct em_perf_domain *em_create_pd(cpumask_t *span, int nr_states,
						struct em_data_callback *cb)
{
	unsigned long power, freq, prev_freq = 0, prev_cost = ULONG_MAX;
	int i, ret, cpu = cpumask_first(span);
	struct em_cap_state *table;
	struct em_perf_domain *pd;
	u64 fmax;

	if (!cb->active_power)
		return NULL;

	pd = kzalloc(sizeof(*pd) + cpumask_size(), GFP_KERNEL);
	if (!pd)
		return NULL;

	table = kcalloc(nr_states, sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_pd;

	/* Build the list of capacity states for this performance domain */
	for (i = 0, freq = 0; i < nr_states; i++, freq++) {
		/*
		 * active_power() is a driver callback which ceils 'freq' to
		 * lowest capacity state of 'cpu' above 'freq' and updates
		 * 'power' and 'freq' accordingly.
		 */
		ret = cb->active_power(&power, &freq, cpu);
		if (ret) {
			pr_err("pd%d: invalid cap. state: %d\n", cpu, ret);
			goto free_cs_table;
		}

		/*
		 * We expect the driver callback to increase the frequency for
		 * higher capacity states.
		 */
		if (freq <= prev_freq) {
			pr_err("pd%d: non-increasing freq: %lu\n", cpu, freq);
			goto free_cs_table;
		}

		/*
		 * The power returned by active_state() is expected to be
		 * positive, in milli-watts and to fit into 16 bits.
		 */
		if (!power || power > EM_CPU_MAX_POWER) {
			pr_err("pd%d: invalid power: %lu\n", cpu, power);
			goto free_cs_table;
		}

		table[i].power = power;
		table[i].frequency = prev_freq = freq;

	}

	/* Compute the cost of each capacity_state. */
	fmax = (u64) table[nr_states - 1].frequency;
	for (i = nr_states - 1; i >= 0; i--) {
		unsigned long power_res = em_scale_power(table[i].power);

		table[i].cost = div64_u64(fmax * power_res,
					  table[i].frequency);

		if (table[i].cost >= prev_cost) {
			pr_warn("pd%d: EM: OPP:%lu is inefficient\n",
				cpu, table[i].frequency);
		} else {
			prev_cost = table[i].cost;

		}
	}

	pd->table = table;
	pd->nr_cap_states = nr_states;
	cpumask_copy(to_cpumask(pd->cpus), span);

	return pd;

free_cs_table:
	kfree(table);
free_pd:
	kfree(pd);

	return NULL;
}

/**
 * em_cpu_get() - Return the performance domain for a CPU
 * @cpu : CPU to find the performance domain for
 *
 * Return: the performance domain to which 'cpu' belongs, or NULL if it doesn't
 * exist.
 */
struct em_perf_domain *em_cpu_get(int cpu)
{
	return READ_ONCE(per_cpu(em_data, cpu));
}
EXPORT_SYMBOL_GPL(em_cpu_get);

/**
 * em_register_perf_domain() - Register the Energy Model of a performance domain
 * @span	: Mask of CPUs in the performance domain
 * @nr_states	: Number of capacity states to register
 * @cb		: Callback functions providing the data of the Energy Model
 *
 * Create Energy Model tables for a performance domain using the callbacks
 * defined in cb.
 *
 * If multiple clients register the same performance domain, all but the first
 * registration will be ignored.
 *
 * Return 0 on success
 */
int em_register_perf_domain(cpumask_t *span, unsigned int nr_states,
						struct em_data_callback *cb)
{
	unsigned long cap, prev_cap = 0;
	struct em_perf_domain *pd;
	int cpu, ret = 0;

	if (!span || !nr_states || !cb)
		return -EINVAL;

	/*
	 * Use a mutex to serialize the registration of performance domains and
	 * let the driver-defined callback functions sleep.
	 */
	mutex_lock(&em_pd_mutex);

	for_each_cpu(cpu, span) {
		/* Make sure we don't register again an existing domain. */
		if (READ_ONCE(per_cpu(em_data, cpu))) {
			ret = -EEXIST;
			goto unlock;
		}

		/*
		 * All CPUs of a domain must have the same micro-architecture
		 * since they all share the same table.
		 */
		cap = arch_scale_cpu_capacity(NULL, cpu);
		if (prev_cap && prev_cap != cap) {
			pr_err("CPUs of %*pbl must have the same capacity\n",
							cpumask_pr_args(span));
			ret = -EINVAL;
			goto unlock;
		}
		prev_cap = cap;
	}

	/* Create the performance domain and add it to the Energy Model. */
	pd = em_create_pd(span, nr_states, cb);
	if (!pd) {
		ret = -EINVAL;
		goto unlock;
	}

	for_each_cpu(cpu, span) {
		/*
		 * The per-cpu array can be read concurrently from em_cpu_get().
		 * The barrier enforces the ordering needed to make sure readers
		 * can only access well formed em_perf_domain structs.
		 */
		smp_store_release(per_cpu_ptr(&em_data, cpu), pd);
	}

	pr_debug("Created perf domain %*pbl\n", cpumask_pr_args(span));
unlock:
	mutex_unlock(&em_pd_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(em_register_perf_domain);
