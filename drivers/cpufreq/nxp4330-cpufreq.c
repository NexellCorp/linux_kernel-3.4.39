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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/devices.h>

/*
#define pr_debug	printk
*/

struct cpufreq_dvfs_data {
	struct cpufreq_frequency_table *freq_table;
	unsigned long (*freq_volts)[2];
	struct clk *clk;
	cpumask_var_t cpus;
	int cpu;
	long target_freq;
    long max_cpufreq;		/* khz */
    long max_retention;		/* msec */
    long rest_cpufreq;		/* khz */
    long rest_retention;	/* msec */
    long rest_period;
   	ktime_t rest_ktime;
    struct delayed_work rest_work;
    struct delayed_work restore_work;
    int  run_monitor;
};

static struct cpufreq_dvfs_data	*cpufreq_dvfs;
#define	set_cpufreq_data(d)		(cpufreq_dvfs = d)
#define	get_cpufreq_data()		(cpufreq_dvfs)

static void nxp4330_cpufreq_restore_thread(struct work_struct *work)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct cpufreq_freqs freqs;
	struct clk *clk = dvfs->clk;
	int cpu = dvfs->cpu;

	dvfs->rest_ktime = ktime_get();

	freqs.new = dvfs->target_freq;	/* restore */
	freqs.old = clk_get_rate(clk) / 1000;
	freqs.cpu = cpu;

	pr_debug("cpufreq : restore after rest %4ldms  %u -> %u khz \n",
		dvfs->rest_retention, freqs.old, freqs.new);

	for_each_cpu(cpu, dvfs->cpus)
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	clk_set_rate(clk, freqs.new*1000);

	for_each_cpu(cpu, dvfs->cpus)
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	/* to rest frequency after end of rest time */
	schedule_delayed_work_on(dvfs->cpu, &dvfs->rest_work,
		msecs_to_jiffies(dvfs->max_retention));
}

static void nxp4330_cpufreq_rest_thread(struct work_struct *work)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct cpufreq_freqs freqs;
	struct clk *clk = dvfs->clk;
	int cpu = dvfs->cpu;

	dvfs->rest_ktime = ktime_get();

	freqs.new = dvfs->rest_cpufreq;
	freqs.old = clk_get_rate(clk) / 1000;
	freqs.cpu = cpu;

	pr_debug("cpufreq : max %ldkhz rest %4ldms, %u -> %u khz \n",
		dvfs->max_cpufreq, dvfs->max_retention, freqs.old, freqs.new);

	for_each_cpu(cpu, dvfs->cpus)
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	clk_set_rate(clk, freqs.new*1000);

	for_each_cpu(cpu, dvfs->cpus)
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	/* to restore frequency after end of rest time */
	schedule_delayed_work_on(dvfs->cpu, &dvfs->restore_work,
		msecs_to_jiffies(dvfs->rest_retention));
}

static inline void nxp4330_cpufreq_setup(void)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();

	dvfs->run_monitor = 0;
	INIT_DELAYED_WORK_DEFERRABLE(&dvfs->rest_work, nxp4330_cpufreq_rest_thread);
	INIT_DELAYED_WORK_DEFERRABLE(&dvfs->restore_work, nxp4330_cpufreq_restore_thread);
}

static struct freq_attr *nxp4330_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int nxp4330_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;

	if (!freq_table)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, freq_table);
}

static unsigned int nxp4330_cpufreq_getspeed(unsigned int cpu)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct clk *clk = dvfs->clk;

	long rate_khz = clk_get_rate(clk)/1000;
	return rate_khz;
}

static int nxp4330_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;
	struct cpufreq_frequency_table *table;
	struct cpufreq_freqs freqs;
	struct clk *clk = dvfs->clk;
	long ts;
	int ret = 0, i = 0;

	ret = cpufreq_frequency_table_target(policy, freq_table,
				target_freq, relation, &i);
	if (ret) {
		pr_err("%s: cpu%d: no freq match for %d khz(ret=%d)\n",
			__func__, policy->cpu, target_freq, ret);
		return ret;
	}

	table = &freq_table[i];
	freqs.new = table->frequency;
	if (!freqs.new) {
		pr_err("%s: cpu%d: no match for freq %d khz\n",
			__func__, policy->cpu, target_freq);
		return -EINVAL;
	}

	freqs.old = nxp4330_cpufreq_getspeed(policy->cpu);
	freqs.cpu = policy->cpu;
	pr_debug("cpufreq : target %u -> %u khz mon(%s) ",
		freqs.old, freqs.new, dvfs->run_monitor?"run":"no");

	if (freqs.old == freqs.new && policy->cur == freqs.new) {
		pr_debug("PASS\n");
		return ret;
	}

	dvfs->cpu = policy->cpu;
	dvfs->target_freq = freqs.new;

	/* rest period */
	if (ktime_to_ms(dvfs->rest_ktime) && freqs.new > dvfs->rest_cpufreq) {
		ts = (ktime_to_ms(ktime_get()) - ktime_to_ms(dvfs->rest_ktime));
		if (dvfs->rest_retention > ts) {
			freqs.new = dvfs->rest_cpufreq;
			pr_debug("rest %4ld:%4ldms (%u khz)\n", dvfs->rest_retention, ts, freqs.new);
			ret = -EINVAL;
			goto _cpu_freq;	/* retry */
		}
		dvfs->rest_ktime = ktime_set(0, 0);	/* clear rest time */
	}

	if (dvfs->max_cpufreq && dvfs->run_monitor && freqs.new < dvfs->max_cpufreq ) {
		dvfs->run_monitor = 0;
		cancel_delayed_work_sync(&dvfs->rest_work);
		cancel_delayed_work_sync(&dvfs->restore_work);
		pr_debug("stop monitor\n");
	}

	if (dvfs->max_cpufreq && !dvfs->run_monitor && freqs.new >= dvfs->max_cpufreq) {
		dvfs->run_monitor = 1;
		schedule_delayed_work_on(dvfs->cpu, &dvfs->rest_work,
			msecs_to_jiffies(dvfs->max_retention));
		pr_debug("run  monitor\n");
	}

_cpu_freq:
	/* pre-change notification */
	for_each_cpu(freqs.cpu, policy->cpus)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* Change frequency */
	ret = clk_set_rate(clk, freqs.new * 1000);

	/* post change notification */
	for_each_cpu(freqs.cpu, policy->cpus)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int __cpuinit nxp4330_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_dvfs_data *dvfs = get_cpufreq_data();
	struct cpufreq_frequency_table *freq_table = dvfs->freq_table;
	int res;

	pr_info("nxp4330-cpufreq: Available frequencies cpus (%d) \n",
		num_online_cpus());

	/* get policy fields based on the table */
	res = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (!res) {
		cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	} else {
		pr_err("nxp4330-cpufreq: Failed to read policy table\n");
		return res;
	}

	policy->cur = nxp4330_cpufreq_getspeed(policy->cpu);
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	/*
	 * FIXME : Need to take time measurement across the target()
	 *	   function with no/some/all drivers in the notification
	 *	   list.
	 */
	policy->cpuinfo.transition_latency = 100000; /* in ns */

	/*
	 * NXP4330 multi-core processors has 2 cores
	 * that the frequency cannot be set independently.
	 * Each cpu is bound to the same speed.
	 * So the affected cpu is all of the cpus.
	 */
	if (num_online_cpus() == 1) {
		cpumask_copy(policy->related_cpus, cpu_possible_mask);
		cpumask_copy(policy->cpus, cpu_online_mask);
	} else {
		cpumask_setall(policy->cpus);
	}

	return 0;
}

static struct cpufreq_driver nxp4330_cpufreq_driver = {
	.flags  = CPUFREQ_STICKY,
	.verify = nxp4330_cpufreq_verify_speed,
	.target = nxp4330_cpufreq_target,
	.get    = nxp4330_cpufreq_getspeed,
	.init   = nxp4330_cpufreq_init,
	.name   = "nxp4330-cpufreq",
	.attr   = nxp4330_cpufreq_attr,
};

static int nxp4330_cpufreq_probe(struct platform_device *pdev)
{
	struct nxp_cpufreq_plat_data *plat = pdev->dev.platform_data;
	struct cpufreq_dvfs_data *dvfs;
	struct cpufreq_frequency_table *table;
	char name[16];
	int size = 0, i = 0;

	if (!plat ||
		!plat->freq_table ||
		!plat->table_size) {
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

	size  = plat->table_size;
	table = kzalloc((sizeof(*table)*size) + 1, GFP_KERNEL);
	if (!table) {
		dev_err(&pdev->dev, "%s: failed allocate freq table !!!\n", __func__);
		return -ENOMEM;
	}

	dvfs->freq_table = table;
	dvfs->freq_volts = (unsigned long(*)[2])plat->freq_table;
	dvfs->max_cpufreq = plat->max_cpufreq;
	dvfs->max_retention = plat->max_retention;
	dvfs->rest_cpufreq = plat->rest_cpufreq;
	dvfs->rest_retention = plat->rest_retention;
	dvfs->rest_ktime = ktime_set(0, 0);
	dvfs->run_monitor = 0;

	/*
     * make frequency table with platform data
	 */
	for (; size > i; i++) {
		table->index = i;
		table->frequency = dvfs->freq_volts[i][0];
		table++;
		pr_debug("[%s] %2d = %8ldkhz, %8ld mV\n",
			name, i, dvfs->freq_volts[i][0], dvfs->freq_volts[i][1]);
	}
	table->index = i;
	table->frequency = CPUFREQ_TABLE_END;

	set_cpufreq_data(dvfs);
	nxp4330_cpufreq_setup();

	printk("DVFS: cpu dvfs with PLL.%d\n", plat->pll_dev);
	return cpufreq_register_driver(&nxp4330_cpufreq_driver);
}

static struct platform_driver cpufreq_driver = {
	.driver	= {
	.name	= DEV_NAME_CPUFREQ,
	.owner	= THIS_MODULE,
	},
	.probe	= nxp4330_cpufreq_probe,
};
module_platform_driver(cpufreq_driver);
