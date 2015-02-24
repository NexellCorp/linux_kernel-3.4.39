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
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/hwmon.h>
#include <linux/hwmon-vid.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>

#include <mach/platform.h>
#include <mach/devices.h>

/*
#define	pr_debug	printk
*/

#define DRVNAME	"nxp-tmu"

struct tmu_trigger {
	int	 trig_degree;
	long trig_duration;
	long trig_cpufreq;
	long process_time;
   	bool is_trigger;
};

struct tmu_info {
	struct device *hwmon_dev;
	const char *name;
	int channel;
	struct tmu_trigger *triggers;
	int trigger_size;
	int poll_duration;
	int temperature;
	struct mutex mlock;
	long max_cpufreq;
	void (*callback)(int ch, int temp, bool run);
	/* TMU func */
	struct delayed_work mon_work;
	unsigned long state;
};

#define	STATE_SUSPEND_ENTER		(0)		/* bit position */
#define	STATE_STOP_ENTER		(1)		/* bit position */
#define TMU_POLL_TIME			(500)	/* ms */

/*
 * Sysfs
 */
enum { SHOW_TEMP, SHOW_LABEL, SHOW_NAME };

static ssize_t tmu_show_temp(struct device *dev,
			 struct device_attribute *devattr, char *buf)
{
	struct tmu_info *info = dev_get_drvdata(dev);
	char *s = buf;

	s += sprintf(s, "%4d\n", info->temperature);
	if (s != buf)
		*(s-1) = '\n';
	return (s - buf);
}

static SENSOR_DEVICE_ATTR(temp_label, 0666, tmu_show_temp , NULL, SHOW_LABEL);

static struct attribute *adc_temp_attr[] = {
	&sensor_dev_attr_temp_label.dev_attr.attr,
	NULL
};

static const struct attribute_group tmu_attr_group = {
	.attrs = adc_temp_attr,
};

/*
 * TMU operation
 */
#define	TIME_100US	0x6B3	// 0x4305
#define	TIME_20us 	0x16A	// 0xE29
#define	TIME_2us 	0x024	// 0x170

static int nxp_tmu_start(int channel)
{
	u32 mode = 7;
	u32 mask = 0;
	int time = 1000;
	//	((0x1<<28) | (0x1<<24) | (0x1<<20) | (0x1<<16) |
	// (0x1<<12) | (0x1<<8) | (0x1<<4) | (0x1<<0));

	NX_TMU_SetBaseAddress(channel, IO_ADDRESS(NX_TMU_GetPhysicalAddress(channel)));
	NX_TMU_ClearInterruptPendingAll(channel);
	NX_TMU_SetInterruptEnableAll(channel, CFALSE);

	// Set CounterValue0, CounterValue1
	NX_TMU_SetCounterValue0(channel, ((TIME_20us<<16) | TIME_100US));
	NX_TMU_SetCounterValue1(channel, ((TIME_100US<<16) | TIME_2us));
	NX_TMU_SetSamplingInterval(channel, 0x1);

	// Emulstion mode enable
	NX_TMU_SetTmuEmulEn(channel, CFALSE);

	// Interrupt Enable <--- disable
	NX_TMU_SetP0IntEn(channel, mask);

	// Thermal tripping mode selection
	NX_TMU_SetTmuTripMode(channel, mode);

	// Thermal tripping enable
	NX_TMU_SetTmuTripEn(channel, 0x0);

	// Check sensing operation is idle
	while (time-- > 0 && NX_TMU_IsBusy(channel)) { msleep(1); }

	NX_TMU_SetTmuStart(channel, CTRUE);
	return 0;
}

static void nxp_tmu_stop(int channel)
{
	NX_TMU_SetTmuStart(channel, CFALSE);
}

static int nxp_tmu_temp(int channel)
{
//	int time = 1000;
//	while (time-- > 0 && NX_TMU_IsBusy(channel)) { msleep(1); }
	NX_TMU_ClearInterruptPendingAll(channel);

	return NX_TMU_GetCurrentTemp0(channel);	// can't use temp1
}

static int nxp_tmu_triggers(struct nxp_tmu_platdata *plat, struct tmu_info *info)
{
	struct tmu_trigger *trig = NULL;
	struct nxp_tmu_trigger *data = plat->triggers;
	int i = 0;

	if (!plat->triggers || !plat->trigger_size)
		return 0;

	trig = kzalloc(sizeof(*trig)*plat->trigger_size, GFP_KERNEL);
	if (!trig) {
		pr_err("%s: Out of memory\n", __func__);
		return -ENOMEM;
	}

	info->triggers = trig;
	info->trigger_size = plat->trigger_size;

	for (i = 0; plat->trigger_size > i; i++, trig++, data++) {
		trig->trig_degree = data->trig_degree;
		trig->trig_duration = data->trig_duration;
		trig->trig_cpufreq = data->trig_cpufreq;
		trig->process_time = 0;
		pr_debug("TMU[%d] = %3d (%6ldms) -> %8ld kzh\n",
			i, trig->trig_degree, trig->trig_duration,
			trig->trig_cpufreq);
	}

	return 0;
}

static int nxp_tmu_max_freq(long new)
{
	char *file = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq";
	mm_segment_t old_fs;
	char buf[32];
	long max = 0;

	int fd = sys_open(file, O_RDWR, 0);
   	old_fs = get_fs();
	if (0 > fd)
		return -EINVAL;

	set_fs(KERNEL_DS);
	sys_read(fd, (void*)buf, sizeof(buf));

	max = simple_strtoul(buf, NULL, 10);
	if (max != new) {
		sprintf(buf, "%ld", new);
		sys_write(fd, (void*)buf, sizeof(buf));
		pr_debug("Max Freq %8ld khz\n", new);
	}

	set_fs(old_fs);
	sys_close(fd);
	return 0;
}

static void nxp_tmu_monitor(struct work_struct *work)
{
	struct tmu_info *info = container_of(work, struct tmu_info, mon_work.work);
	struct tmu_trigger *trig = info->triggers;
	int size = info->trigger_size;
	int channel = info->channel;
	int i = 0;

	if (test_bit(STATE_SUSPEND_ENTER, &info->state)) {
		if (trig->is_trigger) {
			nxp_tmu_max_freq(info->max_cpufreq);	/* restore */
			trig->is_trigger = false;
		}
		goto exit_mon;
	}

	info->temperature = nxp_tmu_temp(channel);

	for (i = 0; size > i; i++, trig++) {
		pr_debug("TMU[%d] = %3d : %3d ~ %6ldms (trigger %s)\n",
			i, info->temperature, trig->trig_degree, trig->process_time,
			trig->is_trigger?"O":"X");

		if (info->temperature >= trig->trig_degree
			&& 0 == trig->is_trigger)
		{
			trig->process_time += info->poll_duration;
			/* limit cpu max frequency */
			if (trig->process_time > trig->trig_duration) {
				if (0 > nxp_tmu_max_freq(trig->trig_cpufreq))
					goto exit_mon;
				trig->is_trigger = true;
				trig->process_time = 0;
			}
		} else {
			/* relax cpu max frequency */
			if (trig->trig_degree > info->temperature &&
				trig->is_trigger) {
				if (0 > nxp_tmu_max_freq(info->max_cpufreq))
					goto exit_mon;
				trig->is_trigger = false;
				trig->process_time = 0;
			}
		}
	}
	pr_debug("\n");

exit_mon:
	schedule_delayed_work(&info->mon_work,
			msecs_to_jiffies(info->poll_duration));

	return;
}

#ifdef CONFIG_PM
static int nxp_tmu_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tmu_info *info = platform_get_drvdata(pdev);
	set_bit(STATE_SUSPEND_ENTER, &info->state);
	return 0;
}

static int nxp_tmu_resume(struct platform_device *pdev)
{
	struct tmu_info *info = platform_get_drvdata(pdev);

	nxp_tmu_start(info->channel);
	clear_bit(STATE_SUSPEND_ENTER, &info->state);

	return 0;
}
#else
#define nxp_tmu_suspend NULL
#define nxp_tmu_resume NULL
#endif

static int __devinit nxp_tmu_probe(struct platform_device *pdev)
{
	struct nxp_tmu_platdata *plat = pdev->dev.platform_data;
	struct tmu_info *info = NULL;
	int err = -1;
	char name[16] ;

	if (!plat) {
		pr_err("%s: no platform data ....\n", __func__);
		return -EINVAL;
	}

	info = kzalloc(sizeof(struct tmu_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: Out of memory\n", __func__);
		return -ENOMEM;
	}

	sprintf(name, "tmu.%d", plat->channel);

	info->channel =	plat->channel;
	info->name = DRVNAME;
	info->channel = plat->channel;
	info->poll_duration = plat->poll_duration ? plat->poll_duration : TMU_POLL_TIME;
	info->callback = plat->callback;
	info->max_cpufreq = cpufreq_quick_get_max(raw_smp_processor_id());
	clear_bit(STATE_SUSPEND_ENTER, &info->state);
	mutex_init(&info->mlock);

	if (0 > nxp_tmu_triggers(plat, info))
		goto exit_free;

	platform_set_drvdata(pdev, info);

	err = sysfs_create_group(&pdev->dev.kobj, &tmu_attr_group);
	if (err)
		goto exit_free;

	info->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(info->hwmon_dev)) {
		err = PTR_ERR(info->hwmon_dev);
		pr_err("%s: Class registration failed (%d)\n", __func__, err);
		goto exit_remove;
	}
	nxp_tmu_start(info->channel);

	INIT_DELAYED_WORK(&info->mon_work, nxp_tmu_monitor);
	schedule_delayed_work(&info->mon_work, msecs_to_jiffies(500));
	printk("TMU: register %s to hwmon (max %ldkhz)\n", name, info->max_cpufreq);

	return 0;

exit_remove:
	sysfs_remove_group(&pdev->dev.kobj, &tmu_attr_group);

exit_free:
	platform_set_drvdata(pdev, NULL);
	if (info->triggers)
		kfree(info->triggers);
	kfree(info);

	return err;
}

static int __devexit nxp_tmu_remove(struct platform_device *pdev)
{
	struct tmu_info *info = platform_get_drvdata(pdev);
	struct tmu_trigger *trig = info->triggers;

	nxp_tmu_stop(info->channel);
	hwmon_device_unregister(info->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &tmu_attr_group);
	platform_set_drvdata(pdev, NULL);
	kfree(info);
	kfree(trig);

	return 0;
}

static struct platform_driver nxp_tmu_driver = {
	.driver = {
		.name   = DRVNAME,
		.owner  = THIS_MODULE,
	},
	.probe = nxp_tmu_probe,
	.remove	= __devexit_p(nxp_tmu_remove),
	.suspend = nxp_tmu_suspend,
	.resume = nxp_tmu_resume,
};

static int __init nxp_tmp_init(void) {
    return platform_driver_register(&nxp_tmu_driver);
}
late_initcall(nxp_tmp_init);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("SLsiAP temperature monitor");
MODULE_LICENSE("GPL");

