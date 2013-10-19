/*
 * Samsung TZIC Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define KMSG_COMPONENT "TZIC"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/android_pmem.h>
#include <linux/io.h>
#include <mach/scm.h>
#include <linux/types.h>

#define TZIC_DEV "tzic"

static DEFINE_MUTEX(tzic_mutex);

static struct class *driver_class;
static dev_t tzic_device_no;
static struct cdev tzic_cdev;

#define HLOS_IMG_TAMPER_FUSE    0
#define SCM_BLOW_SW_FUSE_ID     0x01
#define SCM_IS_SW_FUSE_BLOWN_ID 0x02
#define TZIC_IOC_MAGIC          0x9E
#define TZIC_IOCTL_GET_FUSE_REQ _IO(TZIC_IOC_MAGIC, 0)
#define TZIC_IOCTL_SET_FUSE_REQ _IO(TZIC_IOC_MAGIC, 1)
#define TZIC_IOCTL_SET_CERT_INDEX_FUSE_REQ _IO(TZIC_IOC_MAGIC, 2)

#define STATE_IC_BAD    1
#define STATE_IC_GOOD   0

#define LOG printk

#define TZBSP_SVC_OEM_CERT_INDEX	246
#define OEM_CERT_INDEX		 0x01

typedef struct {
	int index;
	int is_write_req;
	void *read_val;
	int qfprom_api_status;
} qfprom_cert_index_req;

static int set_tamper_fuse_cmd(void)
{
	uint32_t fuse_id = HLOS_IMG_TAMPER_FUSE;

	return scm_call(SCM_SVC_FUSE, SCM_BLOW_SW_FUSE_ID, &fuse_id,
		sizeof(fuse_id), NULL, 0);
}

static uint8_t get_tamper_fuse_cmd(void)
{
	uint32_t fuse_id = HLOS_IMG_TAMPER_FUSE;
	uint8_t resp_buf;

	scm_call(SCM_SVC_FUSE, SCM_IS_SW_FUSE_BLOWN_ID, &fuse_id,
		sizeof(fuse_id), &resp_buf, sizeof(resp_buf));
	return resp_buf;
}

static uint8_t  csb_set_cert_index(int index)
{
	int ret = -1;

	qfprom_cert_index_req cmd;

	memset(&cmd,0,sizeof(cmd));
	cmd.index = index;
	cmd.is_write_req = 1;

	LOG(KERN_INFO "csb_set_cert_index : %d\n", index);	
	
	ret = scm_call(TZBSP_SVC_OEM_CERT_INDEX, OEM_CERT_INDEX, &cmd, sizeof(cmd), NULL, 0);

	return ret;
}


static long tzic_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int ret = 0;
	int index = 0;
	void __user *argp = (void __user *) arg;

	LOG(KERN_INFO "tzic_ioctl called");

	switch (cmd) {
	case TZIC_IOCTL_GET_FUSE_REQ: {
		ret = get_tamper_fuse_cmd();
		LOG(KERN_INFO "tamper_fuse value = %x\n", ret);

		break;
	}
	case TZIC_IOCTL_SET_FUSE_REQ: {
		mutex_lock(&tzic_mutex);
		ret = set_tamper_fuse_cmd();
		mutex_unlock(&tzic_mutex);
		
		if (ret)
			LOG(KERN_INFO "failed tzic_set_fuse_cmd: %d\n", ret);
		ret = get_tamper_fuse_cmd();
		LOG(KERN_INFO "tamper_fuse value = %x\n", ret);
		
		break;
	}
	case TZIC_IOCTL_SET_CERT_INDEX_FUSE_REQ: {
		ret = copy_from_user((void *)&index, argp, sizeof(int));  
		if (ret) {
			LOG(KERN_INFO "copy_from_user failed\n");
			return ret;
		}

		mutex_lock(&tzic_mutex);
		ret = csb_set_cert_index(index);
		mutex_unlock(&tzic_mutex);
		
		if (ret)
			LOG(KERN_INFO "failed tzic_set_fuse_cmd: %d\n", ret);
		else
			LOG(KERN_INFO "succeeded tzic_set_fuse_cmd: %d\n", ret);

		break;
	}
	default:
		return -EINVAL;
	}
	return ret;
}

static ssize_t tzic_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	uint8_t tzic_value;
	int ret = sizeof(tzic_value);
	LOG(KERN_INFO "tzic_read called");

	tzic_value = get_tamper_fuse_cmd();
	memcpy((void *)buf, (void *)&tzic_value, sizeof(tzic_value));
	
	return ret;
}

static const struct file_operations tzic_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tzic_ioctl,
	.read = tzic_read,
};

static int __init tzic_init(void)
{
	int rc;
	struct device *class_dev;

	LOG(KERN_INFO "init tzic");

	rc = alloc_chrdev_region(&tzic_device_no, 0, 1, TZIC_DEV);
	if (rc < 0) {
		LOG(KERN_INFO "alloc_chrdev_region failed %d", rc);
		return rc;
	}

	driver_class = class_create(THIS_MODULE, TZIC_DEV);
	if (IS_ERR(driver_class)) {
		rc = -ENOMEM;
		LOG(KERN_INFO "class_create failed %d", rc);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, tzic_device_no, NULL,
			TZIC_DEV);
	if (!class_dev) {
		LOG(KERN_INFO "class_device_create failed %d", rc);
		rc = -ENOMEM;
		goto class_destroy;
	}

	cdev_init(&tzic_cdev, &tzic_fops);
	tzic_cdev.owner = THIS_MODULE;

	rc = cdev_add(&tzic_cdev, MKDEV(MAJOR(tzic_device_no), 0), 1);
	if (rc < 0) {
		LOG(KERN_INFO "cdev_add failed %d", rc);
		goto class_device_destroy;
	}

	return 0;

class_device_destroy:
	device_destroy(driver_class, tzic_device_no);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(tzic_device_no, 1);
	return rc;
}

static void __exit tzic_exit(void)
{
	LOG(KERN_INFO "exit tzic");
	device_destroy(driver_class, tzic_device_no);
	class_destroy(driver_class);
	unregister_chrdev_region(tzic_device_no, 1);
}

int tzic_get_tamper_flag(void)
{
	return get_tamper_fuse_cmd();
}
EXPORT_SYMBOL(tzic_get_tamper_flag);

int tzic_set_tamper_flag(void)
{
	int ret;
	
	mutex_lock(&tzic_mutex);
	ret = set_tamper_fuse_cmd();
	mutex_unlock(&tzic_mutex);
		
	if (ret)
	{
		LOG(KERN_INFO "failed tzic_set_fuse_cmd: %d\n", ret);
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL(tzic_set_tamper_flag);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Samsung TZIC Driver");
MODULE_VERSION("1.00");

module_init(tzic_init);
module_exit(tzic_exit);

