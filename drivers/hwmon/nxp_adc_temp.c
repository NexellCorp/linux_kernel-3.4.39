/*
 * (C) Copyright 2010
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-vid.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <mach/devices.h>
#include "../staging/iio/consumer.h"

#include <linux/cpufreq.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/delay.h>


//#define	pr_debug	printk


#define DRVNAME	"nxp-adc-tmp"
#define STEP_FREQ	100000
#define CORE_DOWN_TEMP_LEVEL	74

struct nxp_adc_tmp_event {
	int  temp;
	long period;
	long freq;
	unsigned long expire;
	bool poweroff;
};

struct nxp_adc_tmp {
	struct device *hwmon_dev;
	const char *name;
	struct iio_channel *iio;
	u32 id;
	u32 adc_value;
	int channel;	/* use adc channel */
	int delay_ms;
	void (*callback)(int ch, int adc, int temp, bool run);

	struct notifier_block nb;
	struct cpumask allowed_cpus;
	/* TMU func */

	struct delayed_work mon_work, core_down_work;
	struct workqueue_struct *core_voltage_down_workqueue;
	struct nxp_adc_tmp_event *event;
	int eventsize;
	int	step_up;	/* freq stup_up or direct up */
	int up_max;
	unsigned long state;
	/* ADC tmu info */
	int temperature;
	int tmp_max;
	int tmp_value;
	int tmp_offset;
	int tmp_previous;
	long max_freq;
	long min_freq;
	long new_freq;
	struct regulator *core_1_1V;
	bool voltage_down;
};

#define	STATE_SUSPEND_ENTER		(0)		/* bit position */
#define	STATE_STOP_ENTER		(1)		/* bit position */
#define STATE_CHECK_TIME		(100)	/* ms */

/*
 * TMU operation
 */

/* initialize table for register value matching with tmp_value */
static int tmp_table[][2] = {
// R1 : 4.7K
    [0]  = {2786, 25},
    [1]  = {2615, 30},
    [2]  = {2440, 35},
    [3]  = {2263, 40},
    [4]  = {2088, 45},
    [5]  = {1918, 50},
    [6]  = {1753, 55},
    [7]  = {1597, 60},
    [8]  = {1450, 65},
    [9]  = {1311, 70},
    [10] = {1187, 75},
    [11] = {1071, 80},
    [12] = { 966, 85},
    [13] = { 871, 90},
    [14] = { 786, 95},
    [15] = { 709, 100}
};

#define TEMP_TABLAE_SIZE	ARRAY_SIZE(tmp_table)

int stopped = 0;
bool down_flag = false;

/*
 * CPU Down
 */
static int cpu_down_force_byResource(void)
{
    int cpu, cur = raw_smp_processor_id();
    int stopped = 0;
    int err;

    for_each_present_cpu(cpu) {
        if (cpu == 0 || cpu == 3)
            continue;
        if (cpu == cur)
            continue;
        if (!cpu_online(cpu))
            continue;
        stopped |= 1<<cpu;
        err = cpu_down(cpu);

		printk("cpu=%d, cur=%d, stopped=%x\n", cpu, cur, stopped);
        if(err == 0)
	        return stopped;
    }

    return 0;
}

/*
 * CPU up
 */
static void cpu_up_force_byResource(int stopped)
{
    int cpu;
    for_each_present_cpu(cpu) {
		printk("cpu=%d\n", cpu);
        if (stopped & 1<<cpu) {
			printk("cpu down =%d\n", cpu);
            cpu_up(cpu);
		}
    }
}

/*
 * cpu frequency
 */
static int tmp_cpufreq_notifier(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct nxp_adc_tmp *thermal =
			container_of(nb, struct nxp_adc_tmp, nb);
	struct cpufreq_policy *policy = data;
	unsigned long max_freq = 0;

	if (event != CPUFREQ_ADJUST || 0 == thermal->eventsize)
		return 0;

	if (cpumask_test_cpu(policy->cpu, &thermal->allowed_cpus))
		max_freq = thermal->new_freq;

	pr_debug("Thermal.%d notify max = %lu (%ld)kHz, policy max = %u (EVENT:%lu)\n",
		thermal->channel, max_freq, thermal->new_freq, policy->max, event);

	cpufreq_verify_within_limits(policy, 0, max_freq);

	return 0;
}

static void tmp_cpufreq_register(struct nxp_adc_tmp *thermal)
{
	struct notifier_block *nb;
	struct cpumask mask_val;

	if (0 == thermal->eventsize)
		return;

	cpumask_set_cpu(0, &mask_val);
	cpumask_copy(&thermal->allowed_cpus, &mask_val);

	nb = &thermal->nb;
	nb->notifier_call = &tmp_cpufreq_notifier;

	cpufreq_register_notifier(nb, CPUFREQ_POLICY_NOTIFIER);
}

#define MAX_NUM_ADC_DATA	10
#define SORT_C_NUM			3
static int nxp_sort_adc(struct nxp_adc_tmp *tmp)
{
	int i,j,temp;
	int sample_val[MAX_NUM_ADC_DATA];
	int err=0;
	int avg=0;

	memset(sample_val, 0, sizeof(sample_val));

	for(i=0;i<MAX_NUM_ADC_DATA;i++) {
		err = iio_st_read_channel_raw(tmp->iio, &sample_val[i]);
		pr_debug("%s : %d \n",__FUNCTION__, sample_val[i]);
		mdelay(1);
		if (0 > err)
			return -1;
	}

	for(i=0;i<MAX_NUM_ADC_DATA-1;i++) {
		for(j=0;j<MAX_NUM_ADC_DATA-1;j++) {
			if(sample_val[j] > sample_val[j+1]) {
                temp=sample_val[j];
                sample_val[j]=sample_val[j+1];
                sample_val[j+1]=temp;
            }
        }
    }

	for(i=SORT_C_NUM; i<(MAX_NUM_ADC_DATA-SORT_C_NUM) ; i++) {
		avg += sample_val[i];
		pr_debug("%s : %d \n",__FUNCTION__, sample_val[i]);
	}

	return (int) (avg / (MAX_NUM_ADC_DATA-(SORT_C_NUM*2) ));
}

static long nxp_read_adc_tmp(struct nxp_adc_tmp *tmp)
{
	int i = 0, j = 0, val = 0;
	int voltage;
	int err = 0;

	/* read adc and convert tmp */
	#if 0
	err = iio_st_read_channel_raw(tmp->iio, &val);
	if (0 > err)
		return -1;

	#else
	val = nxp_sort_adc(tmp);
	#endif
	tmp->adc_value = val;
//	voltage = (18*val*1000)/4096;
	voltage = tmp->adc_value;

	/*
	 * according to Register Voltage table,
	 * calculate board tmp_value.
	 */
	for (i = 0, j = 0; TEMP_TABLAE_SIZE > i; i++) {
		if (voltage > tmp_table[i][0]) {
			if (0 != i) {
				j = (tmp_table[i-1][0] - tmp_table[i][0])/5;
				break;
			}
		}
	}

	if (i == TEMP_TABLAE_SIZE) {
		tmp->tmp_value = 105;
	} else if (j == 0) {
		tmp->tmp_value = 20;
	} else {
		int n = tmp_table[i-1][0] - j;
		tmp->tmp_value = tmp_table[i-1][1];

		for (; n>tmp_table[i][0]; n -= j) {
			if (voltage > n)
				break;
			tmp->tmp_value++;
		}

		if (tmp->tmp_value > tmp_table[i][1])
			tmp->tmp_value = tmp_table[i][1];
	}
	/* adjust the tmp_value value . */
	if(tmp->tmp_previous){
		if (tmp->tmp_value >= tmp->tmp_previous) {
			int df = tmp->tmp_value - tmp->tmp_previous;
			if (df >= 5) // ignore.
				tmp->tmp_value = tmp->tmp_previous;
			else
				tmp->tmp_previous = tmp->tmp_value;
		} else {
			int df = tmp->tmp_previous - tmp->tmp_value;
			if (df >= 5)  // ignore.
				tmp->tmp_value = tmp->tmp_previous;
			else
				tmp->tmp_previous = tmp->tmp_value;
		}
	} else {
		tmp->tmp_previous = tmp->tmp_value;
	}


	if (tmp->tmp_value > tmp->tmp_max)
		tmp->tmp_max = tmp->tmp_value;

	tmp->temperature = (tmp->tmp_value - tmp->tmp_offset);
	pr_debug("TMU [%d] %3d:%3d (%4d)\n",
		tmp->channel, tmp->tmp_value, tmp->tmp_max, tmp->adc_value);

	return 	tmp->temperature;

}
static inline long cpufreq_get_max(struct nxp_adc_tmp *tmp)
{
	unsigned int cpuid = 0;
	return cpufreq_quick_get_max(cpuid);
}

static inline void cpufreq_set_max(struct nxp_adc_tmp *tmp, long new)
{
	unsigned int cpuid = 0;
	tmp->new_freq = new;
	cpufreq_update_policy(cpuid);
}

static void nxp_adc_tmp_monfn(struct work_struct *work)
{
	struct nxp_adc_tmp *tmp = container_of(work, struct nxp_adc_tmp, mon_work.work);
	struct nxp_adc_tmp_event *event = tmp->event;
	int event_size = tmp->eventsize;
	long curr = 0, next =0 ; /* Frequncy */
	long temp,delay;
	unsigned long time = 0;
	int up_max = 0, down_work = 0;
	int i =0;

	if (test_bit(STATE_SUSPEND_ENTER, &tmp->state))
		goto exit_mon;

	temp = nxp_read_adc_tmp(tmp);
	if(temp <tmp_table[0][1] ){
		goto exit_mon;
	}

	time = ktime_to_ms(ktime_get());
	curr =  cpufreq_get_max(tmp);
	next = curr;
	delay= tmp->delay_ms;
	if (tmp->min_freq > next)
		next = tmp->min_freq;

	for(i=0; i < event_size; i++, event++)
	{
		if(event->temp > temp){	/* freq up Max */
			if(event->freq >= next )
			{
				up_max = 1;
				next =  event->freq;
			}
			if(tmp->up_max)
				up_max = 1;
		} else  { /* freq down */
			down_work = 1; up_max = 0;
			if (next > event->freq) {
				next = event->freq;
				delay = event->period;
			} else {
			}
			if(event->poweroff) {
				printk("Thermal critical temperature reached (%d C)\n",
					 event->temp);
				printk("shutting down ...\n");
				orderly_poweroff(true);
				down_work = 0; up_max = 0;
			}
		}
	}

	if(down_work) {
		if(next < curr) {
			cpufreq_set_max(tmp, next);
			schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(delay));
			return;
		} else if ( next > curr) { /* freq up */
			if(tmp->step_up){
				cpufreq_set_max(tmp, curr + STEP_FREQ);
			}
			else
				cpufreq_set_max(tmp, next);

			schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(delay));

		}
	} else if (up_max) {
		if( tmp->max_freq > next) {
			if(tmp->step_up){
				cpufreq_set_max(tmp,curr + STEP_FREQ);
				if(tmp->max_freq > curr + STEP_FREQ )
					tmp->up_max = 1;
				else
					tmp->up_max = 0;

			} else {
				cpufreq_set_max(tmp,tmp->max_freq);
			}
			schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(tmp->delay_ms));
		return;
		}
	}
exit_mon:
	if (tmp->callback)
		tmp->callback(tmp->channel, tmp->adc_value, tmp->tmp_value, true);

	schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(tmp->delay_ms));

	return;
}

static void nxp_core_down(struct work_struct *work) {
	struct nxp_adc_tmp *tmp = container_of(work, struct nxp_adc_tmp, core_down_work.work);


	if (tmp->temperature > CORE_DOWN_TEMP_LEVEL) {
		// Core voltage down
		printk("Down core voltage!\n");
		if (tmp->voltage_down == false) {
			regulator_set_voltage(tmp->core_1_1V,  1050000, 1050000);
			regulator_put(tmp->core_1_1V);
			tmp->voltage_down = true;
		}
#if 0
		// Core down
		stopped |= cpu_down_force_byResource();
		if (stopped != 0)
			down_flag = true;
#endif
	} else {
		// Core voltage up
		printk("Up core voltage!\n");
		if (tmp->voltage_down == true) {
			regulator_set_voltage(tmp->core_1_1V,  1100000, 1100000);
			regulator_put(tmp->core_1_1V);
			tmp->voltage_down = false;
		}
#if 0
		// Core up
		if (down_flag == true) {
			cpu_up_force_byResource(stopped);
			stopped = 0;
			down_flag = false;
		}
#endif
	}

	queue_delayed_work(tmp->core_voltage_down_workqueue, &tmp->core_down_work, HZ);
}

/*
 * Sysfs
 */
//enum { SHOW_TEMP, SHOW_LABEL, SHOW_NAME };
enum { SHOW_TEMP, SHOW_LABEL,SHOW_MAX, SHOW_NAME };

static ssize_t show_value(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	struct nxp_adc_tmp *tmp = dev_get_drvdata(dev);
	char *s = buf;

	s += sprintf(s, "%3d:%4d\n", tmp->tmp_value, tmp->adc_value);
	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}

static ssize_t show_temp(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	struct nxp_adc_tmp *tmp = dev_get_drvdata(dev);
	char *s = buf;

	s += sprintf(s, "%d\n", tmp->temperature);
	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}
static ssize_t show_max(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	struct nxp_adc_tmp *tmp = dev_get_drvdata(dev);
	char *s = buf;

	s += sprintf(s, "%d\n", tmp->tmp_max);
	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}
static ssize_t store_temp(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct nxp_adc_tmp *tmp = dev_get_drvdata(dev);
	const char *s = buf;

	if (0 == strncmp(s, "run", strlen("run"))) {
		set_bit(STATE_STOP_ENTER, &tmp->state);
		if (test_bit(STATE_STOP_ENTER, &tmp->state)) {
			if (tmp->callback)
				tmp->callback(tmp->channel, tmp->adc_value, tmp->tmp_value, true);
			schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(tmp->delay_ms));
		}
	}
	else if (0 == strncmp(s, "stop", strlen("stop"))) {
		clear_bit(STATE_STOP_ENTER, &tmp->state);
		cancel_delayed_work_sync(&tmp->mon_work);
		if (tmp->callback)
			tmp->callback(tmp->channel, tmp->adc_value, tmp->tmp_value, false);
	} else {
		count = -1;
	}

	return count;
}

static SENSOR_DEVICE_ATTR(temp_value, S_IRUGO, show_value, NULL, SHOW_TEMP);
static SENSOR_DEVICE_ATTR(temp_max, S_IRUGO, show_max, NULL, SHOW_MAX);
static SENSOR_DEVICE_ATTR(temp_label, 0666, show_temp , store_temp, SHOW_LABEL);

static struct attribute *adc_temp_attr[] = {
	&sensor_dev_attr_temp_value.dev_attr.attr,
	&sensor_dev_attr_temp_max.dev_attr.attr,
	&sensor_dev_attr_temp_label.dev_attr.attr,
	NULL
};

static const struct attribute_group adc_temp_group = {
	.attrs = adc_temp_attr,
};

#ifdef CONFIG_PM
static int nxp_adc_tmp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct nxp_adc_tmp *tmp =  platform_get_drvdata(pdev);
	set_bit(STATE_SUSPEND_ENTER, &tmp->state);
	return 0;
}

static int nxp_adc_tmp_resume(struct platform_device *pdev)
{
	struct nxp_adc_tmp *tmp =  platform_get_drvdata(pdev);
	clear_bit(STATE_SUSPEND_ENTER, &tmp->state);
	return 0;
}
#else
#define nxp_adc_tmp_suspend NULL
#define nxp_adc_tmp_resume NULL
#endif

static int __devinit nxp_adc_tmp_probe(struct platform_device *pdev)
{
	struct nxp_adc_tmp_platdata *plat = pdev->dev.platform_data;
	struct nxp_adc_tmp *tmp = NULL;
	//struct nxp_adc_tmp_event *event = NULL;
	struct cpufreq_policy policy = { .cpuinfo = { .min_freq = 0, .min_freq = 0 }, };
	int err = -1;
	char name[16] ;

	int i = 0;

	if (!plat) {
		dev_err(&pdev->dev, "no platform data ....\n");
		goto exit;
	}

	tmp = kzalloc(sizeof(struct nxp_adc_tmp), GFP_KERNEL);
	if (!tmp) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "Out of memory\n");
		goto exit;
	}

	cpufreq_get_policy(&policy, 0);
	sprintf(name, "adc.%d", plat->channel);
	tmp->channel =	plat->channel;
	tmp->id = pdev->id;
	tmp->name = DRVNAME;
	tmp->channel = plat->channel;
	tmp->delay_ms = plat->duration ? plat->duration : STATE_CHECK_TIME;
	tmp->tmp_offset = plat->tmp_offset;
	tmp->min_freq = policy.cpuinfo.min_freq;
	tmp->max_freq = policy.cpuinfo.max_freq;
	tmp->new_freq = tmp->max_freq;
	tmp->callback = plat->callback;
	tmp->iio = iio_st_channel_get(DEV_NAME_ADC, name);
	tmp->step_up = plat->step_up;
	clear_bit(STATE_SUSPEND_ENTER, &tmp->state);
	tmp->eventsize = plat->eventsize;
	tmp->event = kzalloc(sizeof(struct nxp_adc_tmp_event) * plat->eventsize, GFP_KERNEL);

	pr_debug ("Add ADC Temp Trigger . event size : [%d] \n", tmp->eventsize);
	for(i=0;i<tmp->eventsize; i++){
		tmp->event[i].temp = plat->event[i].temp;
		tmp->event[i].freq =  plat->event[i].freq;
		tmp->event[i].period = plat->event[i].period;
		tmp->event[i].expire = 0;
		tmp->event[i].poweroff = !tmp->event[i].freq ? 1 : 0;
		pr_debug(" [%d] Temperature : %3d  CPU Frequncy : %8ld  period : %8ld %s \n",i, tmp->event[i].temp, tmp->event[i].freq, tmp->event[i].period , tmp->event[i].poweroff ? "Critical " : "");
	}
	platform_set_drvdata(pdev, tmp);


	err = sysfs_create_group(&pdev->dev.kobj, &adc_temp_group);
	if (err)
		goto exit_free;

	tmp->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(tmp->hwmon_dev)) {
		err = PTR_ERR(tmp->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n",
			err);
		goto exit_remove;
	}

	tmp_cpufreq_register(tmp);
//	tmp->voltage_down = false;
//	tmp->core_1_1V = regulator_get(NULL, "vdd_core_1.2V");
//	if (IS_ERR(tmp->core_1_1V))
//		printk("%s: failed to regulator_get() for vdd_core_1.2V", __func__);

	INIT_DELAYED_WORK(&tmp->mon_work, nxp_adc_tmp_monfn);
//	tp->core_voltage_down_workqueue = create_singlethread_workqueue("Core voltage down monitor");
//	INIT_DELAYED_WORK_DEFERRABLE(&tmp->core_down_work, nxp_core_down);
	schedule_delayed_work(&tmp->mon_work, msecs_to_jiffies(1));
//	queue_delayed_work(tmp->core_voltage_down_workqueue, &tmp->core_down_work, 15*HZ);
	printk("TMU: register %s to hwmon\n", name);

	return 0;

exit_remove:
	sysfs_remove_group(&pdev->dev.kobj, &adc_temp_group);
exit_free:
	platform_set_drvdata(pdev, NULL);
	kfree(tmp);
exit:

	return err;
}

static int __devexit nxp_adc_tmp_remove(struct platform_device *pdev)
{
	struct nxp_adc_tmp *tmp = platform_get_drvdata(pdev);

	hwmon_device_unregister(tmp->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &adc_temp_group);
	platform_set_drvdata(pdev, NULL);
	kfree(tmp);

	return 0;
}

static struct platform_driver nxp_adc_tmp_driver = {
	.driver = {
		.name   = DRVNAME,
		.owner  = THIS_MODULE,
	},
	.probe = nxp_adc_tmp_probe,
	.remove	= __devexit_p(nxp_adc_tmp_remove),
	.suspend = nxp_adc_tmp_suspend,
	.resume = nxp_adc_tmp_resume,
};

static int __init nxp_adc_tmp_init(void)
{
    return platform_driver_register(&nxp_adc_tmp_driver);
}

static void __exit nxp_adc_tmp_exit(void)
{
    return platform_driver_unregister(&nxp_adc_tmp_driver);
}

late_initcall(nxp_adc_tmp_init);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("SLsiAP ADC tmp_value monitor");
MODULE_LICENSE("GPL");

