/*
 * sec_pm_debug.c
 *
 * driver supporting debug functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2013 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>

static struct dentry *debugfs_base;
static u32 debug_suspend = 1;

struct power_device_id {
	char name[32];
	void __iomem *base;
	phys_addr_t addr;
	size_t size;
};

static struct power_device_id power_device[] = {
	{ .name = "bimc",		.addr = 0xfc401104,	.size = SZ_4 },
	{ .name = "venus0",		.addr = 0xfd8c1024,	.size = SZ_4 },
	{ .name = "ocmemcx",	.addr = 0xfd8c4054,	.size = SZ_4 },
	{ .name = "mdss",		.addr = 0xfd8c2304,	.size = SZ_4 },
	{ .name = "vfe",		.addr = 0xfd8c36a4,	.size = SZ_4 },
	{ .name = "jpeg",		.addr = 0xfd8c35a4,	.size = SZ_4 },
	{ .name = "lpass",		.addr = 0xfe007000,	.size = SZ_4 },
	{ .name = "oxili",		.addr = 0xfd8c4024,	.size = SZ_4 },
	{ .name = "oxicx",		.addr = 0xfd8c4034,	.size = SZ_4 },
};

/**
 * power_debug_active_show() - Show GDSC register of each IPs
 * @m: seq_file *
 * @unused: unused
 */
static int power_debug_active_show(struct seq_file *m, void *unused)
{
	int i;
	unsigned int temp;

	seq_puts(m, "Show GDSCRs:\n");
	seq_puts(m, "name\t\taddr\t\traw\t\tenable\n");

	for (i = 0; i < ARRAY_SIZE(power_device); i++) {
		if (!power_device[i].base)
			power_device[i].base = ioremap(power_device[i].addr, power_device[i].size);

		temp = readl_relaxed(power_device[i].base);

		seq_printf(m, "%s\t\t0x%08x\t", power_device[i].name, power_device[i].addr);
		seq_printf(m, "0x%08x\t%d\n", temp, (temp >> 31) & 0x1);
	}

	return 0;
}

static int power_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, power_debug_active_show, inode->i_private);
}

static const struct file_operations power_debug_fops = {
	.open		= power_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

/**
 * sec_pm_debug_init() - Initialize sec_pm debugfs
 */
int __init sec_pm_debug_init(void)
{
	debugfs_base = debugfs_create_dir("sec_pm", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_u32("debug_suspend", S_IRUGO | S_IWUSR,
				debugfs_base, &debug_suspend)) {
		debugfs_remove_recursive(debugfs_base);
		return -ENOMEM;
	}

	debugfs_create_file("showenable", S_IRUGO,
			debugfs_base, NULL, &power_debug_fops);

	return 0;
}

arch_initcall(sec_pm_debug_init);
