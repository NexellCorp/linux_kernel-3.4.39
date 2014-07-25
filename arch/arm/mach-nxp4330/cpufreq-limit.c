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
#include <linux/init.h>
#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/oom.h>
#include <linux/platform_device.h>
#include <mach/platform.h>
#include <mach/devices.h>

/*
#define pr_debug	printk
*/

struct cpufreq_limit_data {
    char **limit_name;
    int limit_num;
    long aval_max_freq;     /* unit Khz */
    long op_max_freq; 		/* unit Khz */
    long timer_duration;	/* unit ms */
    long op_timeout;		/* unit ms */
    long time_stamp;
    long frequency;			/* unit Khz */
    long pre_max_freq;		/* unit Khz */
    struct hrtimer limit_timer;
    struct work_struct limit_work;
    struct notifier_block limit_nb;
};

#define	LM_TASK_CHECK_SLEEP_TIME	(5 * USEC_PER_MSEC)		/* sec */
#define	DEFAULT_LM_CHECK_TIME		(500)					/* msec */

#define	ms_to_ktime(m)		ns_to_ktime((u64)m * 1000 * 1000)

static char *sys_scaling_path[] = {
	"/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
	"/sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq",
	"/sys/devices/system/cpu/cpu2/cpufreq/scaling_max_freq",
	"/sys/devices/system/cpu/cpu3/cpufreq/scaling_max_freq",
};

static char task_comm[PAGE_SIZE];	/* refer cpufreq_cmdline */

static int cpufreq_cmdline(struct task_struct *task, char * buffer)
{
	int res = 0;
	unsigned int len;
	struct mm_struct *mm = get_task_mm(task);
	if (!mm)
		goto out;
	if (!mm->arg_end)
		goto out_mm;	/* Shh! No looking before we're done */

 	len = mm->arg_end - mm->arg_start;

	if (len > PAGE_SIZE)
		len = PAGE_SIZE;

	res = access_process_vm(task, mm->arg_start, buffer, len, 0);

	// If the nul at the end of args has been overwritten, then
	// assume application is using setproctitle(3).
	if (res > 0 && buffer[res-1] != '\0' && len < PAGE_SIZE) {
		len = strnlen(buffer, res);
		if (len < res) {
		    res = len;
		} else {
			len = mm->env_end - mm->env_start;
			if (len > PAGE_SIZE - res)
				len = PAGE_SIZE - res;
			res += access_process_vm(task, mm->env_start, buffer+res, len, 0);
			res = strnlen(buffer, res);
		}
	}
out_mm:
	mmput(mm);
out:
	return res;
}

static void cpufreq_set_max_frequency(struct cpufreq_limit_data *limit, int boost)
{
	int fd;
	mm_segment_t old_fs;
	char buf[16];
	long sc_max_freq = 0, max_freq = 0;
	int cpu;

	max_freq = boost ? limit->aval_max_freq : limit->op_max_freq;
	if (limit->pre_max_freq == max_freq)
		return;

	for_each_possible_cpu(cpu) {
		fd = sys_open(sys_scaling_path[cpu], O_RDWR, 0);
    	old_fs = get_fs();
   		if (0 > fd)
   			continue;

		set_fs(KERNEL_DS);
		sys_read(fd, (void*)buf, sizeof(buf));

		sc_max_freq = simple_strtoul(buf, NULL, 10);
		limit->pre_max_freq = max_freq;

		if (max_freq != sc_max_freq) {
			sprintf(buf, "%ld", max_freq);
			sys_write(fd, (void*)buf, sizeof(buf));
		}

		set_fs(old_fs);
		sys_close(fd);

		pr_debug("[cpu.%d scaling max: %ld khz -> %ld khz][%s]\n",
			cpu, sc_max_freq, max_freq, boost?"boost":"normal");
	}
}

static void cpufreq_limit_work(struct work_struct *work)
{
	struct cpufreq_limit_data *limit = container_of(work,
						struct cpufreq_limit_data, limit_work);
	struct task_struct *p = NULL, *t = NULL;
	char **s = limit->limit_name;
	char *comm = task_comm;
	int cpu, i = 0, len = limit->limit_num;

	for_each_possible_cpu(cpu) {
     	p = curr_task(cpu);
	   	t = find_task_by_vpid(task_tgid_vnr(p));	/* parent */
     	if (t)
     		p = t;

   		if (p->flags & PF_KTHREAD)
			continue;

		if (!likely(p->mm))
			continue;

		cpufreq_cmdline(p, comm);
     	pr_debug("cpu %d current (%d) %s", cpu, p->pid, comm);

		for (i = 0; len > i; i++) {
			/* boost : task is running */
			if (!strncmp(comm, s[i], strlen(s[i]))) {
				limit->time_stamp = 0;
				cpufreq_set_max_frequency(limit, 1);
				pr_debug(": run %s\n", s[i]);
				goto _exit;
			}
		}
		pr_debug("\n");
	}

	for_each_process(p) {
   		if (p->flags & PF_KTHREAD)
			continue;

		if (!likely(p->mm))
			continue;

		cpufreq_cmdline(p, comm);
		for (i = 0; len > i; i++) {
			if (!strncmp(comm, s[i], strlen(s[i]))) {
				pr_debug("detect %s:%s [%ld.%ld ms]\n",
					s[i], comm, limit->time_stamp, limit->time_stamp%1000);
				if (0 == limit->time_stamp) {
					limit->time_stamp = ktime_to_ms(ktime_get());
				} else {
					/* restore : task is sleep status */
					if ((ktime_to_ms(ktime_get()) - limit->time_stamp) >
						limit->op_timeout)
						cpufreq_set_max_frequency(limit, 0);
				}
				goto _exit;
			}
		}
	}

	/* restore : not find task */
	cpufreq_set_max_frequency(limit, 0);
	limit->time_stamp = 0;

_exit:
	hrtimer_start(&limit->limit_timer, ms_to_ktime(limit->timer_duration),
		HRTIMER_MODE_REL_PINNED);
}

static enum hrtimer_restart cpufreq_limit_timer(struct hrtimer *hrtimer)
{
	struct cpufreq_limit_data *limit = container_of(hrtimer,
						struct cpufreq_limit_data, limit_timer);
	schedule_work(&limit->limit_work);
	return HRTIMER_NORESTART;
}

static int cpufreq_limit_transition(struct notifier_block *nb,
					  unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_limit_data *limit = container_of(nb,
						struct cpufreq_limit_data, limit_nb);

	if (freq->cpu == 0 && val == CPUFREQ_POSTCHANGE)
		limit->frequency = freq->new;
	return 0;
}

static int cpufreq_limit_probe(struct platform_device *pdev)
{
	struct nxp_cpufreq_limit_data *plat = pdev->dev.platform_data;
	struct cpufreq_limit_data *limit;
	struct hrtimer *hrtimer;

	if (!plat || !plat->limit_name ||
		0 == plat->limit_num) {
		return -EINVAL;
	}

	limit = kzalloc(sizeof(*limit), GFP_KERNEL);
	if (!limit) {
		dev_err(&pdev->dev, "%s: failed allocate freq limit data !!!\n", __func__);
		return -ENOMEM;
	}

	limit->limit_name = plat->limit_name;
	limit->limit_num = plat->limit_num;
	limit->aval_max_freq = plat->aval_max_freq;
	limit->op_max_freq = plat->op_max_freq;
	limit->timer_duration = plat->sched_duration ? : DEFAULT_LM_CHECK_TIME;
	limit->op_timeout = plat->sched_timeout ? : LM_TASK_CHECK_SLEEP_TIME;
	limit->limit_nb.notifier_call = cpufreq_limit_transition;

	INIT_WORK(&limit->limit_work, cpufreq_limit_work);

	hrtimer = &limit->limit_timer;
	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = cpufreq_limit_timer;

	hrtimer_start(hrtimer, ms_to_ktime(limit->timer_duration),
		HRTIMER_MODE_REL_PINNED);
	printk("LIMIT: timer %ld ms, op %ld ms\n",
		limit->timer_duration, limit->op_timeout);
/*
	return cpufreq_register_notifier(&limit->limit_nb,
			CPUFREQ_TRANSITION_NOTIFIER);
*/
	return 0;
}

static struct platform_driver cpufreq_limit_driver = {
	.driver	= {
	.name	= "cpufreq-limit",
	.owner	= THIS_MODULE,
	},
	.probe	= cpufreq_limit_probe,
};
module_platform_driver(cpufreq_limit_driver);
