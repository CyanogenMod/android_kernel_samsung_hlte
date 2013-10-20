/*
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef _TSU6721_H_
#define _TSU6721_H_

#include <linux/mfd/pm8xxx/pm8921-charger.h>

enum {
	TSU6721_DETACHED,
	TSU6721_ATTACHED
};

enum {
	DISABLE,
	ENABLE
};

enum {
	TSU6721_DETACHED_DOCK = 0,
	TSU6721_ATTACHED_DESK_DOCK,
	TSU6721_ATTACHED_CAR_DOCK,
};

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_MISC,
	CABLE_TYPE_CARDOCK,
	CABLE_TYPE_UARTOFF,
	CABLE_TYPE_JIG,
	CABLE_TYPE_UNKNOWN,
	CABLE_TYPE_CDP,
	CABLE_TYPE_SMART_DOCK,
	CABLE_TYPE_OTG,
	CABLE_TYPE_AUDIO_DOCK,
#ifdef CONFIG_WIRELESS_CHARGING
	CABLE_TYPE_WPC,
#endif
	CABLE_TYPE_INCOMPATIBLE,
};

enum tsu6721_cb_support {
	UNSUPPORTED = 0,
	SUPPORTED
};

struct tsu6721_cb_struct {
	const char *name;
	enum tsu6721_cb_support is_support;
};

struct tsu6721_platform_data {
	void (*callback)(enum cable_type_t cable_type, int state);
	void (*cfg_gpio) (void);
	void (*reset_cb) (void);
	void (*set_init_flag) (void);
	int (*dock_init) (void);
	void (*mhl_sel) (bool onoff);
	void (*mhl_notify) (int attached);
	void (*oxp_callback) (int state);
	bool i2c_pull_up;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
};
enum {
	SWITCH_PORT_AUTO = 0,
	SWITCH_PORT_USB,
	SWITCH_PORT_AUDIO,
	SWITCH_PORT_UART,
	SWITCH_PORT_VAUDIO,
	SWITCH_PORT_USB_OPEN,
	SWITCH_PORT_ALL_OPEN,
};
extern void muic_callback(enum cable_type_t cable_type, int state);
extern void tsu6721_manual_switching(int path);
extern void tsu6721_otg_detach(void);
extern int check_jig_state(void);
#if defined(CONFIG_VIDEO_MHL_V2)
extern int dock_det(void);

#endif
extern struct class *sec_class;

#endif /* _TSU6721_H_ */
