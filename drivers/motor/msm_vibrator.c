/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2011 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
//shkang_mf07
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/msm_rpcrouter.h>
#include <linux/vibrator.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include "../staging/android/timed_output.h"

#define VIBRATION_ON			1
#define VIBRATION_OFF			0

/* Error and Return value codes */
#define VIBE_S_SUCCESS			0	/*!< Success */
#define VIBE_E_FAIL			-4	/*!< Generic error */

/* need to change for each project */
#define PMIC_LDO_NAME		"8941_l22"

static struct workqueue_struct *msm_vibrator_workqueue;
static struct work_struct work_vibrator_on;
static struct work_struct work_vibrator_off;
static struct hrtimer vibe_timer;

static int msm_vibrator_suspend(struct platform_device *pdev, pm_message_t state);
static int msm_vibrator_resume(struct platform_device *pdev);
static int msm_vibrator_probe(struct platform_device *pdev);
static int msm_vibrator_exit(struct platform_device *pdev);
/* for the suspend/resume VIBRATOR Module */
static struct platform_driver msm_vibrator_platdrv =
{
	.probe   = msm_vibrator_probe,
	.suspend = msm_vibrator_suspend,
	.resume  = msm_vibrator_resume,
	.remove  = msm_vibrator_exit,
	.driver =
	{
		.name = "msm_vibrator",
		.owner = THIS_MODULE,
	},
};

static void set_vibrator(int on)
{
	int ret = 0;
	static struct regulator *reg_ldo_num;

	printk(KERN_DEBUG"[VIB] set_vibrator, on= %d\n", on);

	if (!reg_ldo_num) {
		reg_ldo_num = regulator_get(NULL, PMIC_LDO_NAME);
		ret = regulator_set_voltage(reg_ldo_num, 3300000, 3300000);

		if (IS_ERR(reg_ldo_num)) {
			printk(KERN_ERR"could not get vib ldo, ret= %ld\n", PTR_ERR(reg_ldo_num));
			return;
		}
	}

	if (on) {
		ret = regulator_enable(reg_ldo_num);
		if (ret) {
			printk(KERN_ERR"enable vib ldo enable failed, ret= %d\n", ret);
			return;
		}
	} else {
		if (regulator_is_enabled(reg_ldo_num)) {
			ret = regulator_disable(reg_ldo_num);
			if (ret) {
				printk(KERN_ERR"disable vib ldo disable failed, ret= %d\n", ret);
				return;
			}
		}
	}
}

static int msm_vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("[VIB] Susepend \n");

	set_vibrator(VIBRATION_OFF);

	return VIBE_S_SUCCESS;
}

static int msm_vibrator_resume(struct platform_device *pdev)
{
	printk("[VIB] Resume \n");

	return VIBE_S_SUCCESS;
}

static int msm_vibrator_exit(struct platform_device *pdev)
{
	printk("[VIB] Exit\n");

	return VIBE_S_SUCCESS;
}
static void msm_vibrator_on(struct work_struct *work)
{
	set_vibrator(VIBRATION_ON);
}

static void msm_vibrator_off(struct work_struct *work)
{
	set_vibrator(VIBRATION_OFF);
}

static void timed_vibrator_on(struct timed_output_dev *sdev)
{
	printk("[VIB] %s\n",__func__);

	queue_work(msm_vibrator_workqueue, &work_vibrator_on);
}

static void timed_vibrator_off(struct timed_output_dev *sdev)
{
	printk("[VIB] %s\n",__func__);

	queue_work(msm_vibrator_workqueue, &work_vibrator_off);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	hrtimer_cancel(&vibe_timer);

	if (value == VIBRATION_OFF) {
		printk("[VIB] OFF\n");

		timed_vibrator_off(dev);
	}
	else {
		printk("[VIB] ON, Duration : %d msec\n" , value);

		timed_vibrator_on(dev);

		if (value == 0x7fffffff){
			printk("[VIB} No Use Timer %d \n", value);
		}
		else	{
			value = (value > 15000 ? 15000 : value);
		        hrtimer_start(&vibe_timer, ktime_set(value / 1000, (value % 1000) * 1000000), HRTIMER_MODE_REL);
                }
	}
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		struct timeval t = ktime_to_timeval(r);

		return (t.tv_sec * 1000 + t.tv_usec / 1000);
	}

	return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	printk("[VIB] %s\n",__func__);

	timed_vibrator_off(NULL);

	return HRTIMER_NORESTART;
}

static struct timed_output_dev msm_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int msm_vibrator_probe(struct platform_device *pdev)
{
	int rc = 0;

	printk("[VIB] %s\n",__func__);

	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	rc = timed_output_dev_register(&msm_vibrator);
	if (rc < 0) {
		goto err_read_vib;
	}
	return 0;

err_read_vib:
	printk(KERN_ERR "[VIB] timed_output_dev_register fail (rc=%d)\n", rc);

	return rc;
}

static int __init msm_timed_vibrator_init(void)
{
	int rc = 0;

	printk("[VIB] %s\n",__func__);

	rc = platform_driver_register(&msm_vibrator_platdrv);
	if (rc)	{
		printk("[VIB] platform_driver_register failed : %d\n",rc);
	}

	msm_vibrator_workqueue = create_workqueue("msm_vibrator");

	if (!msm_vibrator_workqueue) {
		pr_err("%s(): can't create workqueue\n", __func__);
		rc = -ENOMEM;
		goto err_create_work_queue;
	}
	
	INIT_WORK(&work_vibrator_on, msm_vibrator_on);
	INIT_WORK(&work_vibrator_off, msm_vibrator_off);
	
	return 0;

err_create_work_queue:
	destroy_workqueue(msm_vibrator_workqueue);

	return rc;
}

void __exit msm_timed_vibrator_exit(void)
{
	printk("[VIB] %s\n",__func__);

	platform_driver_unregister(&msm_vibrator_platdrv);
	destroy_workqueue(msm_vibrator_workqueue);
}

module_init(msm_timed_vibrator_init);
module_exit(msm_timed_vibrator_exit);

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_LICENSE("GPL");

