/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/sysrq.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/tags.h>

/*
#define pr_debug	printk
*/

/*
 * DVFS info
 */
struct cpufreq_asv_ops {
	int  (*setup_table)(unsigned long (*tables)[2]);
	void (*modify_vol)(unsigned long (*tables)[2], long val, bool dn, bool percent);
	int  (*current_label)(char *string);
};

#include "nxp-cpufreq.h"

struct cpufreq_dvfs_time {
	unsigned long start;
	unsigned long duration;
};

struct cpufreq_dvfs_limits {
	int  run_monitor;
	long target_freq;
    long new_freq;		/* khz */
    long max_freq;		/* khz */
    long max_retent;	/* msec */
    long rest_freq;		/* khz */
    long rest_retent;	/* msec */
    long rest_period;
   	ktime_t rest_ktime;
   	struct hrtimer rest_hrtimer;
   	struct hrtimer restore_hrtimer;
   	struct task_struct *proc;
};

struct cpufreq_dvfs_info {
	struct cpufreq_frequency_table *freq_table;
	unsigned long (*freq_volts)[2];	/* khz freq (khz): voltage(uV): voltage (us) */
	struct clk *clk;
	cpumask_var_t cpus;
	int cpu;
	struct mutex lock;
	/* limited max frequency */
	struct cpufreq_dvfs_limits limits;
    /* voltage control */
    struct regulator *volt;
    int table_size;
    long supply_delay_us;
    /* for suspend/resume */
    struct notifier_block pm_notifier;
    unsigned long resume_state;
    long reset_freq;
    int reset_voltage;
    /* check frequency duration */
	int  curr_index;
	int  prev_index;
    unsigned long check_state;
    struct cpufreq_dvfs_time *freq_times;
    /* ASV operation */
    struct cpufreq_asv_ops *asv_ops;
};

#define	FREQ_TABLE_MAX			(30)
#define	FREQ_STATE_RESUME 		(0)	/* bit num */
#define	FREQ_STATE_TIME_RUN   	(0)	/* bit num */

static struct cpufreq_dvfs_info	*ptr_cpufreq_dvfs = NULL;
static unsigned long st_freq_tables[FREQ_TABLE_MAX][2];
static struct cpufreq_dvfs_time st_freq_times[FREQ_TABLE_MAX] = { {0,}, };
#define	ms_to_ktime(m)			 ns_to_ktime((u64)m * 1000 * 1000)

static inline void set_dvfs_ptr(void *ptr)
{
	 ptr_cpufreq_dvfs = ptr;
}

static inline void *get_dvfs_ptr(void)
{
	 return ptr_cpufreq_dvfs;
}

static int nxp_cpufreq_get_index(unsigned long frequency)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	unsigned long (*freq_tables)[2] = (unsigned long(*)[2])dvfs->freq_volts;
	int len = dvfs->table_size;
	int index = 0;

	for (index = 0; len > index; index++) {
		if (frequency == freq_tables[index][0])
			break;
	}

	if (index == len) {
		index = CPUFREQ_ENTRY_INVALID;
		printk("Fail : invalid frequency (%ld) index !!!\n", frequency);
	}
	return index;
}

static enum hrtimer_restart nxp_cpufreq_restore_timer(struct hrtimer *hrtimer)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();

	dvfs->limits.rest_ktime = ktime_set(0, 0);	/* clear */
	dvfs->limits.new_freq = dvfs->limits.target_freq;	/* restore */

	pr_debug("cpufreq : restore %ldkhz after rest %4ldms\n",
		dvfs->limits.target_freq, dvfs->limits.rest_retent);

	if (dvfs->limits.target_freq > dvfs->limits.rest_freq) {
		wake_up_process(dvfs->limits.proc);

		/* to rest frequency after end of rest time */
		hrtimer_start(&dvfs->limits.rest_hrtimer,
			ms_to_ktime(dvfs->limits.max_retent), HRTIMER_MODE_REL_PINNED);
	}
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart nxp_cpufreq_rest_timer(struct hrtimer *hrtimer)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();

	dvfs->limits.rest_ktime = ktime_get();
	dvfs->limits.new_freq = dvfs->limits.rest_freq;

	pr_debug("cpufreq : %ldkhz (%4ldms) -> %ldkhz rest (%4ldms) \n",
		dvfs->limits.max_freq, dvfs->limits.max_retent,
		dvfs->limits.new_freq, dvfs->limits.rest_retent);

	wake_up_process(dvfs->limits.proc);

	/* to restore frequency after end of rest time */
	hrtimer_start(&dvfs->limits.restore_hrtimer,
		ms_to_ktime(dvfs->limits.rest_retent), HRTIMER_MODE_REL_PINNED);

	return HRTIMER_NORESTART;
}

unsigned int nxp_cpufreq_voltage(unsigned long freqhz)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
 	unsigned long (*freq_volts)[2] = (unsigned long(*)[2])dvfs->freq_volts;
	long mS = 0, uS = 0, uV = 0, wT = 0;
	int index = dvfs->curr_index;

	if (!dvfs->volt)
		return 0;

	uV = freq_volts[index][1];
	wT = dvfs->supply_delay_us;

	regulator_set_voltage(dvfs->volt, uV, uV);

	if (wT) {
		mS = wT/1000;
		uS = wT%1000;
		if (mS) mdelay(mS);
		if (uS) udelay(uS);
	}

#ifdef CONFIG_ARM_NXP_CPUFREQ_VOLTAGE_DEBUG
	printk(" volt (%lukhz %ld.%06ld V, %ld.%03ld us)\n",
			freq_volts[index][0], uV/1000000, uV%1000000, mS, uS);
#endif
	return uV;
}

static unsigned long nxp_cpufreq_update(struct cpufreq_dvfs_info *dvfs,
				struct cpufreq_freqs *freqs)
{
	struct clk *clk = dvfs->clk;
	unsigned long rate = 0;
	int index = dvfs->curr_index;

	if (!test_bit(FREQ_STATE_RESUME, &dvfs->resume_state))
		return freqs->old;

	/* pre voltage */
	if (freqs->new > freqs->old)
		nxp_cpufreq_voltage(freqs->new*1000);

	for_each_cpu(freqs->cpu, dvfs->cpus)
		cpufreq_notify_transition(freqs, CPUFREQ_PRECHANGE);

	rate = clk_set_rate(clk, freqs->new*1000);

	if (test_bit(FREQ_STATE_TIME_RUN, &dvfs->check_state)) {
		int prev = dvfs->prev_index;
		long ms = ktime_to_ms(ktime_get());

		dvfs->freq_times[prev].duration += (ms - dvfs->freq_times[prev].start);
		dvfs->freq_times[index].start = ms;
		dvfs->prev_index = index;
	}

	for_each_cpu(freqs->cpu, dvfs->cpus)
		cpufreq_notify_transition(freqs, CPUFREQ_POSTCHANGE);

	/* post voltage */
	if (freqs->old > freqs->new)
		nxp_cpufreq_voltage(freqs->new*1000);

	return rate;
}

static int nxp_cpufreq_pm_notify(struct notifier_block *this,
        unsigned long mode, void *unused)
{
	struct cpufreq_dvfs_info *dvfs = container_of(this,
					struct cpufreq_dvfs_info, pm_notifier);
	struct clk *clk = dvfs->clk;
	struct cpufreq_freqs freqs;

    switch(mode) {
    case PM_SUSPEND_PREPARE:	/* set initial frequecny */
		mutex_lock(&dvfs->lock);

		freqs.new = dvfs->reset_freq;
		freqs.old = clk_get_rate(clk)/1000;
		if (!dvfs->limits.target_freq)
			dvfs->limits.target_freq = freqs.new;

		dvfs->curr_index = nxp_cpufreq_get_index(freqs.new);

		nxp_cpufreq_update(dvfs, &freqs);

    	clear_bit(FREQ_STATE_RESUME, &dvfs->resume_state);
		mutex_unlock(&dvfs->lock);
    	break;

    case PM_POST_SUSPEND:	/* set restore frequecny */
		mutex_lock(&dvfs->lock);
    	set_bit(FREQ_STATE_RESUME, &dvfs->resume_state);

		freqs.new = dvfs->limits.target_freq;
		freqs.old = clk_get_rate(clk)/1000;
		dvfs->curr_index = nxp_cpufreq_get_index(freqs.new);

		nxp_cpufreq_update(dvfs, &freqs);

		mutex_unlock(&dvfs->lock);
    	break;
    }
    return 0;
}

static int nxp_cpufreq_proc_update(void *unused)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	struct cpufreq_freqs freqs;
	struct clk *clk = dvfs->clk;

	set_current_state(TASK_INTERRUPTIBLE);

	while (!kthread_should_stop()) {

		if (dvfs->limits.new_freq) {
			mutex_lock(&dvfs->lock);
			set_current_state(TASK_UNINTERRUPTIBLE);

			freqs.new = dvfs->limits.new_freq;
			freqs.old = clk_get_rate(clk)/1000;;
			freqs.cpu = dvfs->cpu;
			dvfs->curr_index = nxp_cpufreq_get_index(freqs.new);

			nxp_cpufreq_update(dvfs, &freqs);

			set_current_state(TASK_INTERRUPTIBLE);
			mutex_unlock(&dvfs->lock);
		}

		/* wait */
		schedule();

		if (kthread_should_stop())
			break;

		set_current_state(TASK_INTERRUPTIBLE);
	}

	set_current_state(TASK_RUNNING);
	return 0;
}

static inline int nxp_cpufreq_setup(struct cpufreq_dvfs_info *dvfs)
{
	struct hrtimer *hrtimer = &dvfs->limits.rest_hrtimer;
	int cpu = 0;
	struct task_struct *p;

	dvfs->limits.run_monitor = 0;
	mutex_init(&dvfs->lock);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = nxp_cpufreq_rest_timer;

	hrtimer = &dvfs->limits.restore_hrtimer;
	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = nxp_cpufreq_restore_timer;

	p = kthread_create_on_node(nxp_cpufreq_proc_update,
				NULL, cpu_to_node(cpu), "cpufreq-update");
	if (IS_ERR(p)) {
		pr_err("%s: cpu%d: failed rest thread for cpufreq\n", __func__, cpu);
		return PTR_ERR(p);
	}
	kthread_bind(p, cpu);
	wake_up_process(p);

	dvfs->limits.proc = p;

	return 0;
}

static ssize_t show_speed_duration(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	int index = dvfs->curr_index;
	ssize_t count = 0;
	int i = 0;

	if (test_bit(FREQ_STATE_TIME_RUN, &dvfs->check_state)) {
		long ms = ktime_to_ms(ktime_get());
		if (dvfs->freq_times[index].start)
			dvfs->freq_times[index].duration += (ms - dvfs->freq_times[index].start);
		dvfs->freq_times[index].start = ms;
		dvfs->prev_index = index;
	}

	for (; dvfs->table_size > i; i++)
		count += sprintf(&buf[count], "%8ld ", dvfs->freq_times[i].duration);

	count += sprintf(&buf[count], "\n");
	return count;
}

static ssize_t store_speed_duration(struct cpufreq_policy *policy,
			const char *buf, size_t count)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	int index = dvfs->curr_index;
	long ms = ktime_to_ms(ktime_get());
	const char *s = buf;

	mutex_lock(&dvfs->lock);

	if (0 == strncmp(s, "run", strlen("run"))) {
		dvfs->prev_index = index;
		dvfs->freq_times[index].start = ms;
		set_bit(FREQ_STATE_TIME_RUN, &dvfs->check_state);
	}
	else if (0 == strncmp(s, "stop", strlen("stop"))) {
		clear_bit(FREQ_STATE_TIME_RUN, &dvfs->check_state);
	}
	else if (0 == strncmp(s, "clear", strlen("clear"))) {
		memset(dvfs->freq_times, 0, sizeof(st_freq_times));
		if (test_bit(FREQ_STATE_TIME_RUN, &dvfs->check_state)) {
			dvfs->freq_times[index].start = ms;
			dvfs->prev_index = index;
		}
	} else {
		count = -1;
	}

	mutex_unlock(&dvfs->lock);

	return count;
}

static ssize_t show_voltage_level(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
 	unsigned long (*freq_volts)[2] = (unsigned long(*)[2])dvfs->freq_volts;
	char *s = buf;
	int i = 0;

	for (; dvfs->table_size > i; i++)
		s += sprintf(s, "%8ld", freq_volts[i][1]);

	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}

static ssize_t store_voltage_level(struct cpufreq_policy *policy,
			const char *buf, size_t count)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	bool percent = false, down = false;
	const char *s = strchr(buf, '-');
	long val;

	if (s)
		down = true;
	else
		s = strchr(buf, '+');

	if (!s)
		s = buf;
	else
		s++;

	if (strchr(buf, '%'))
		percent = 1;

	val = simple_strtol(s, NULL, 10);

	if (dvfs->asv_ops->modify_vol)
		dvfs->asv_ops->modify_vol(st_freq_tables, val, down, percent);

	return count;
}

static ssize_t show_asv_level(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	int ret = 0;

	if (dvfs->asv_ops->current_label)
		ret = dvfs->asv_ops->current_label(buf);

	return ret;
}

static struct freq_attr cpufreq_freq_attr_scaling_speed_duration = {
    .attr = {
    	.name = "scaling_speed_duration",
		.mode = 0666,
	},
    .show  = show_speed_duration,
    .store = store_speed_duration,
};

static struct freq_attr cpufreq_freq_attr_scaling_asv_level = {
    .attr = {
    	.name = "scaling_asv_level",
		.mode = 0666,
	},
    .show  = show_asv_level,
};

static struct freq_attr cpufreq_freq_attr_scaling_voltage_level = {
    .attr = {
    	.name = "scaling_voltage_level",
		.mode = 0666,
	},
    .show  = show_voltage_level,
    .store = store_voltage_level,
};

static struct freq_attr *nxp_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&cpufreq_freq_attr_scaling_voltage_level,
	&cpufreq_freq_attr_scaling_asv_level,
	&cpufreq_freq_attr_scaling_speed_duration,
	NULL,
};

static int nxp_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;

	if (!freq_table)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, freq_table);
}

static unsigned int nxp_cpufreq_get_speed(unsigned int cpu)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	struct clk *clk = dvfs->clk;
	long rate_khz = clk_get_rate(clk)/1000;

	return rate_khz;
}

static int nxp_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;
	struct cpufreq_frequency_table *table;
	struct cpufreq_freqs freqs;
	unsigned long rate = 0;
	long ts;
	int ret = 0, i = 0;

	ret = cpufreq_frequency_table_target(policy, freq_table,
						target_freq, relation, &i);
	if (ret) {
		pr_err("%s: cpu%d: no freq match for %d khz(ret=%d)\n",
			__func__, policy->cpu, target_freq, ret);
		return ret;
	}

	mutex_lock(&dvfs->lock);

	table = &freq_table[i];
	freqs.new = table->frequency;

	if (!freqs.new) {
		pr_err("%s: cpu%d: no match for freq %d khz\n",
			__func__, policy->cpu, target_freq);
		mutex_unlock(&dvfs->lock);
		return -EINVAL;
	}

	freqs.old = nxp_cpufreq_get_speed(policy->cpu);
	freqs.cpu = policy->cpu;
	pr_debug("cpufreq : target %u -> %u khz mon(%s) ",
		freqs.old, freqs.new, dvfs->limits.run_monitor?"run":"no");

	if (freqs.old == freqs.new && policy->cur == freqs.new) {
		pr_debug("PASS\n");
		mutex_unlock(&dvfs->lock);
		return ret;
	}

	dvfs->cpu = policy->cpu;
	dvfs->limits.target_freq = freqs.new;
	dvfs->curr_index = table->index;

	/* rest period */
	if (ktime_to_ms(dvfs->limits.rest_ktime) && freqs.new > dvfs->limits.rest_freq) {
		ts = (ktime_to_ms(ktime_get()) - ktime_to_ms(dvfs->limits.rest_ktime));
		if (dvfs->limits.rest_retent > ts) {
			freqs.new = dvfs->limits.rest_freq;
			pr_debug("rest %4ld:%4ldms (%u khz)\n", dvfs->limits.rest_retent, ts, freqs.new);
			goto _cpu_freq;	/* retry */
		}
		dvfs->limits.rest_ktime = ktime_set(0, 0);	/* clear rest time */
	}

	if (dvfs->limits.max_freq && dvfs->limits.run_monitor && freqs.new < dvfs->limits.max_freq ) {
		dvfs->limits.run_monitor = 0;
		hrtimer_cancel(&dvfs->limits.rest_hrtimer);
		pr_debug("stop monitor");
	}

	if (dvfs->limits.max_freq && !dvfs->limits.run_monitor && freqs.new >= dvfs->limits.max_freq) {
		dvfs->limits.run_monitor = 1;
		hrtimer_start(&dvfs->limits.rest_hrtimer, ms_to_ktime(dvfs->limits.max_retent),
			      HRTIMER_MODE_REL_PINNED);
		pr_debug("run  monitor");
	}

_cpu_freq:

	pr_debug(" set rate %ukhz\n", freqs.new);
	rate = nxp_cpufreq_update(dvfs, &freqs);

	mutex_unlock(&dvfs->lock);

	return rate;
}

static int __cpuinit nxp_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_dvfs_info *dvfs = get_dvfs_ptr();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;
	int res;

	pr_debug("nxp-cpufreq: available frequencies cpus (%d) \n",
		num_online_cpus());

	/* get policy fields based on the table */
	res = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (!res) {
		cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	} else {
		pr_err("nxp-cpufreq: Failed to read policy table\n");
		return res;
	}

	policy->cur = nxp_cpufreq_get_speed(policy->cpu);
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	/*
	 * FIXME : Need to take time measurement across the target()
	 *	   function with no/some/all drivers in the notification
	 *	   list.
	 */
	policy->cpuinfo.transition_latency = 100000; /* in ns */

	/*
	 * multi-core processors has 2 cores
	 * that the frequency cannot be set independently.
	 * Each cpu is bound to the same speed.
	 * So the affected cpu is all of the cpus.
	 */
	if (num_online_cpus() == 1) {
		cpumask_copy(policy->related_cpus, cpu_possible_mask);
		cpumask_copy(policy->cpus, cpu_online_mask);
		cpumask_copy(dvfs->cpus, cpu_online_mask);
	} else {
		cpumask_setall(policy->cpus);
		cpumask_setall(dvfs->cpus);
	}

	return 0;
}

static struct cpufreq_driver nxp_cpufreq_driver = {
	.flags   = CPUFREQ_STICKY,
	.verify  = nxp_cpufreq_verify_speed,
	.target  = nxp_cpufreq_target,
	.get     = nxp_cpufreq_get_speed,
	.init    = nxp_cpufreq_init,
	.name    = "nxp-cpufreq",
	.attr    = nxp_cpufreq_attr,
};

static struct tag_asv_margin tag_margin = { 0, };

static int __init parse_tag_arm_margin(const struct tag *tag)
{
	struct tag_asv_margin *t = (struct tag_asv_margin *)&tag->u;
	struct tag_asv_margin *p = &tag_margin;

	p->value = t->value;
	p->minus = t->minus;
	p->percent = t->percent;
	printk("ASV: Arm margin:%s%d%s\n",
		p->minus?"-":"+", p->value, p->percent?"%":"mV");
	return 0;
}
__tagtable(ATAG_ARM_MARGIN, parse_tag_arm_margin);

static int nxp_cpufreq_probe(struct platform_device *pdev)
{
	struct nxp_cpufreq_plat_data *plat = pdev->dev.platform_data;
	struct cpufreq_asv_ops *asv_ops = &asv_freq_ops;
	static struct notifier_block *pm_notifier;
	struct cpufreq_dvfs_info *dvfs;
	struct cpufreq_frequency_table *table;
	char name[16];
	int tb_size = 0;
	int size = 0, i = 0;

	struct tag_asv_margin *p = &tag_margin;

	/*
	 * check asv support
	 */
	if (asv_ops->setup_table) {
		tb_size = asv_ops->setup_table(st_freq_tables);
		/* for TEST */
		if (p->value && asv_ops->modify_vol)
			asv_ops->modify_vol(st_freq_tables,
				p->value, p->minus, p->percent);
	}
	if (0 >= tb_size &&
		(!plat || !plat->freq_table || !plat->table_size)) {
		dev_err(&pdev->dev, "%s: failed no freq table !!!\n", __func__);
		return -EINVAL;
	}

	dvfs = kzalloc(sizeof(*dvfs), GFP_KERNEL);
	if (!dvfs) {
		dev_err(&pdev->dev, "%s: failed allocate DVFS data !!!\n", __func__);
		return -ENOMEM;
	}

	sprintf(name, "pll%d", plat->pll_dev);
	dvfs->clk = clk_get(NULL, name);
	if (IS_ERR(dvfs->clk))
		return PTR_ERR(dvfs->clk);

	size  = (tb_size > 0 ? tb_size : plat->table_size);
	table = kzalloc((sizeof(*table)*size) + 1, GFP_KERNEL);
	if (!table) {
		dev_err(&pdev->dev, "%s: failed allocate freq table !!!\n", __func__);
		return -ENOMEM;
	}

	set_dvfs_ptr(dvfs);

	dvfs->asv_ops = asv_ops;
	dvfs->freq_table = table;
	dvfs->freq_volts = (unsigned long(*)[2])(tb_size > 0 ? st_freq_tables : plat->freq_table);
	dvfs->limits.max_freq = plat->max_cpufreq;
	dvfs->limits.max_retent = plat->max_retention;
	dvfs->limits.rest_freq = plat->rest_cpufreq;
	dvfs->limits.rest_retent = plat->rest_retention;
	dvfs->limits.rest_ktime = ktime_set(0, 0);
	dvfs->limits.run_monitor = 0;
	dvfs->table_size = size;
	dvfs->supply_delay_us = plat->supply_delay_us;
	dvfs->reset_freq = nxp_cpufreq_get_speed(0);
	dvfs->prev_index = -1;
	dvfs->check_state = 0;
	dvfs->freq_times = st_freq_times;

	/*
     * make frequency table with platform data
	 */
	for (; size > i; i++) {
		table->index = i;
		table->frequency = dvfs->freq_volts[i][0];
		table++;
		pr_debug("[%s] %2d = %8ldkhz, %8ld uV (%lu us)\n",
			name, i, dvfs->freq_volts[i][0], dvfs->freq_volts[i][1],
			dvfs->supply_delay_us);
	}

	table->index = i;
	table->frequency = CPUFREQ_TABLE_END;

	/*
     * get voltage regulator table with platform data
	 */
	if (plat->supply_name) {
		dvfs->volt = regulator_get(NULL, plat->supply_name);
		if (IS_ERR(dvfs->volt)) {
			dev_err(&pdev->dev, "%s: Cannot get regulator for DVS supply %s\n",
				__func__, plat->supply_name);
			kfree(table);
			kfree(dvfs);
			return -1;
		}
		dvfs->reset_voltage = regulator_get_voltage(dvfs->volt);

		pm_notifier = &dvfs->pm_notifier;
		pm_notifier->notifier_call = nxp_cpufreq_pm_notify;
		if (register_pm_notifier(pm_notifier)) {
			dev_err(&pdev->dev, "%s: Cannot pm notifier %s\n",
				__func__, plat->supply_name);
			return -1;
		}
		set_bit(FREQ_STATE_RESUME, &dvfs->resume_state);
	}

	if (0 > nxp_cpufreq_setup(dvfs))
		return -EINVAL;

	printk("DVFS: cpu %s with PLL.%d [tables=%d]\n",
		dvfs->volt?"DVFS":"DFS", plat->pll_dev, dvfs->table_size);

	return cpufreq_register_driver(&nxp_cpufreq_driver);
}

static struct platform_driver cpufreq_driver = {
	.driver	= {
	.name	= DEV_NAME_CPUFREQ,
	.owner	= THIS_MODULE,
	},
	.probe	= nxp_cpufreq_probe,
};
module_platform_driver(cpufreq_driver);
