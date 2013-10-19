/*
 * cypress_touchkey.h - Platform data for cypress touchkey driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_CYPRESS_TOUCHKEY_H
#define __LINUX_CYPRESS_TOUCHKEY_H
extern struct class *sec_class;

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

/* DVFS feature : TOUCH BOOSTER */
#define TSP_BOOSTER
#ifdef TSP_BOOSTER
#include <linux/cpufreq.h>

#define DVFS_STAGE_DUAL		2
#define DVFS_STAGE_SINGLE		1
#define DVFS_STAGE_NONE		0
#define TOUCH_BOOSTER_OFF_TIME	500
#define TOUCH_BOOSTER_CHG_TIME	500
#endif

#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/mutex.h>

#if defined(CONFIG_GLOVE_TOUCH)
#define TK_BIT_GLOVE 0x40
#endif

/* Flip cover*/
#define TKEY_FLIP_MODE
#ifdef TKEY_FLIP_MODE
#define TK_BIT_FLIP		0x08
#endif

#define TK_INFORM_CHARGER
#define TK_BIT_TA_ON		0x10
#define TK_BIT_AUTOCAL		0x80

#define TK_CMD_LED_ON		0x10
#define TK_CMD_LED_OFF		0x20

#define CYPRESS_55_IC_MASK	0x20
#define CYPRESS_65_IC_MASK	0x04

#define NUM_OF_KEY		4

enum {
	CORERIVER_TOUCHKEY,
	CYPRESS_TOUCHKEY,
};

#ifdef TK_INFORM_CHARGER
struct touchkey_callbacks {
	void (*inform_charger)(struct touchkey_callbacks *, bool);
};
#endif

enum {
	UPDATE_NONE,
	DOWNLOADING,
	UPDATE_FAIL,
	UPDATE_PASS,
};

#define TKEY_REQUEST_FW_UPDATE

#ifdef TKEY_REQUEST_FW_UPDATE
#define TKEY_FW_BUILTIN_PATH	"tkey"
#define TKEY_FW_IN_SDCARD_PATH	"/sdcard/"

#if defined(CONFIG_MACH_JS01LTEDCM)
#define TKEY_CYPRESS_FW_NAME		"hltejs01_cypress_tkey"
#define TKEY_CORERIVER_FW_NAME		"hltejs01_coreriver_tkey"
#else
#define TKEY_CYPRESS_FW_NAME		"hlte_cypress_tkey"
#define TKEY_CORERIVER_FW_NAME		"hlte_coreriver_tkey"
#endif

enum {
	FW_BUILT_IN = 0,
	FW_IN_SDCARD,
};

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u8 data[0];
} __attribute__ ((packed));
#endif

struct cypress_touchkey_platform_data {
	unsigned	gpio_led_en;
	u32	touchkey_keycode[4];
	int	keycodes_size;
	void	(*power_onoff) (int);
	bool	skip_fw_update;
	bool	touchkey_order;
	void	(*register_cb)(void *);
	bool i2c_pull_up;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
	int gpio_touchkey_id;
	u32	gpio_touchkey_id_flags;
	int vdd_led;
};

struct cypress_touchkey_info {
	struct i2c_client			*client;
	struct cypress_touchkey_platform_data	*pdata;
	struct input_dev			*input_dev;
	struct early_suspend			early_suspend;
	char			phys[32];
	unsigned char			keycode[NUM_OF_KEY];
	u8			sensitivity[NUM_OF_KEY];
	int			irq;
	void (*power_onoff)(int);
	u8			touchkey_update_status;
	u8			ic_vendor;
	struct regulator *vcc_en;
	struct regulator *vdd_led;
#if defined(CONFIG_GLOVE_TOUCH)
	struct workqueue_struct		*glove_wq;
	struct work_struct		glove_work;
#endif
	bool is_powering_on;
	bool enabled;
	bool done_ta_setting;

#ifdef TKEY_FLIP_MODE
	bool enabled_flip;
#endif

#ifdef TSP_BOOSTER
	struct delayed_work	work_dvfs_off;
	struct delayed_work	work_dvfs_chg;
	struct mutex		dvfs_lock;
	bool dvfs_lock_status;
	int dvfs_old_stauts;
	int dvfs_boost_mode;
	int dvfs_freq;
#endif

#ifdef TK_INFORM_CHARGER
	struct touchkey_callbacks callbacks;
	bool charging_mode;
#endif
#ifdef TKEY_REQUEST_FW_UPDATE
	const struct firmware		*fw;	
	struct fw_image	*fw_img;
	u8	cur_fw_path;
#endif
	int	src_fw_ver;
	int	ic_fw_ver;
	int	module_ver;
	u32 fw_id;
#if defined(CONFIG_GLOVE_TOUCH)
	int glove_value;
#endif
	u8	touchkeyid;
	bool	support_fw_update;
};

void touchkey_charger_infom(bool en);


#define PM8921_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)
#define GPIO_TOUCHKEY_SDA	95
#define GPIO_TOUCHKEY_SCL	96
#define PMIC_GPIO_TKEY_INT	79
extern void cypress_power_onoff(struct cypress_touchkey_info *info, int onoff);
#endif /* __LINUX_CYPRESS_TOUCHKEY_H */
