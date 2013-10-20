/* Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2012, Synaptics Incorporated
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <mach/cpufreq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "synaptics_i2c_rmi.h"

#include <linux/i2c/synaptics_rmi.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/qpnp/pin.h>

#define DRIVER_NAME "synaptics_rmi4_i2c"
#undef USE_SENSOR_SLEEP

#define SYNAPTICS_PM_GPIO_STATE_WAKE	0
#define SYNAPTICS_PM_GPIO_STATE_SLEEP	1

struct qpnp_pin_cfg synaptics_int_set[] = {
	{
		.mode = 0,
		.pull = 5,
		.output_type = 0,
		.invert = 0,
		.vin_sel = 2,
		.out_strength = 2,
		.src_sel = 0,
		.master_en =1,
	},
	{
		.mode = 0,
		.pull = 4,
		.output_type = 0,
		.invert = 0,
		.vin_sel = 2,
		.out_strength = 2,
		.src_sel = 0,
		.master_en =1,
	},
};
/*#define REPORT_2D_Z*/
#define REPORT_2D_W

#define F12_FINGERS_TO_SUPPORT 10

#define INVALID_X 65535
#define INVALID_Y 65535

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#ifdef PROXIMITY
#define F51_FINGER_TIMEOUT 50 /* ms */
#define HOVER_Z_MAX (255)

#define EDGE_SWIPE_DEGREES_MAX (90)
#define EDGE_SWIPE_DEGREES_MIN (-89)
#define EDGE_SWIPE_WIDTH_SCALING_FACTOR (9)

#define F51_PROXIMITY_ENABLES_OFFSET (0)
#define F51_GPIP_EDGE_EXCLUSION_RX_OFFSET (47)

#define FINGER_HOVER_DIS (0 << 0)
#define FINGER_HOVER_EN (1 << 0)
#define AIR_SWIPE_EN (1 << 1)
#define LARGE_OBJ_EN (1 << 2)
#define HOVER_PINCH_EN (1 << 3)
/* This command is reserved..
#define NO_PROXIMITY_ON_TOUCH_EN (1 << 5)
#define CONTINUOUS_LOAD_REPORT_EN (1 << 6)
*/
#define ENABLE_HANDGRIP_RECOG (1 << 6)
#define SLEEP_PROXIMITY (1 << 7)

#define F51_GENERAL_CONTROL_OFFSET (1)
#define JIG_TEST_EN	(1 << 0)
#define JIG_COMMAND_EN	(1 << 1)
#define NO_PROXIMITY_ON_TOUCH (1 << 2)
#define CONTINUOUS_LOAD_REPORT (1 << 3)
#define HOST_REZERO_COMMAND (1 << 4)
#define EDGE_SWIPE_EN (1 << 5)
#define HSYNC_STATUS	(1 << 6)
#define F51_GENERAL_CONTROL (NO_PROXIMITY_ON_TOUCH | HOST_REZERO_COMMAND | EDGE_SWIPE_EN)
#define F51_GENERAL_CONTROL_NO_HOST_REZERO (NO_PROXIMITY_ON_TOUCH | EDGE_SWIPE_EN)


#define HAS_FINGER_HOVER (1 << 0)
#define HAS_AIR_SWIPE (1 << 1)
#define HAS_LARGE_OBJ (1 << 2)
#define HAS_HOVER_PINCH (1 << 3)
#define HAS_EDGE_SWIPE (1 << 4)
#define HAS_SINGLE_FINGER (1 << 5)
#define HAS_GRIP_SUPPRESSION (1 << 6)
#define HAS_PALM_REJECTION (1 << 7)

#ifdef EDGE_SWIPE
#define EDGE_SWIPE_DATA_OFFSET	9

#define EDGE_SWIPE_WIDTH_MAX	255
#define EDGE_SWIPE_ANGLE_MIN	(-90)
#define EDGE_SWIPE_ANGLE_MAX	90
#define EDGE_SWIPE_PALM_MAX		1

#define F51_DATA_1_SIZE (4)
#define F51_DATA_2_SIZE (1)
#define F51_DATA_3_SIZE (1)
#define F51_DATA_4_SIZE (1)
#define F51_DATA_5_SIZE (1)
#define F51_DATA_6_SIZE (1)
#endif
#endif

#define SYN_I2C_RETRY_TIMES 10
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F01_PACKAGE_ID_LEN	4
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CHARGER_CONNECTED (1 << 5)
#define CHARGER_DISCONNECTED	0xDF

#define CONFIGURED (1 << 7)

#define TSP_NEEDTO_REBOOT	(-ECONNREFUSED)
#define MAX_TSP_REBOOT		3

void synaptics_power_ctrl(struct synaptics_rmi4_data *rmi4_data, bool enable);

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_stop_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_start_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_init_exp_fn(struct synaptics_rmi4_data *rmi4_data);
static void synaptics_rmi4_remove_exp_fn(struct synaptics_rmi4_data *rmi4_data);

#ifdef CONFIG_HAS_EARLYSUSPEND
static ssize_t synaptics_rmi4_full_pm_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_full_pm_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static void synaptics_rmi4_early_suspend(struct early_suspend *h);

static void synaptics_rmi4_late_resume(struct early_suspend *h);

#else

static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);
#endif

#ifdef PROXIMITY
static void synaptics_rmi4_f51_finger_timer(unsigned long data);

static ssize_t synaptics_rmi4_f51_enables_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f51_enables_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f51_general_control_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f51_general_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
#endif

#ifdef USE_OPEN_CLOSE
static int synaptics_rmi4_input_open(struct input_dev *dev);
static void synaptics_rmi4_input_close(struct input_dev *dev);
#ifdef USE_OPEN_DWORK
static void synaptics_rmi4_open_work(struct work_struct *work);
#endif
#endif

static ssize_t synaptics_rmi4_register_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_global_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_global_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_glove_mode_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_glove_mode_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_closed_cover_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_closed_cover_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_fast_detect_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_fast_detect_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
		};
		unsigned char data[3];
	};
};

struct synaptics_rmi4_f12_query_10 {
	union {
		struct {
			unsigned char f12_query10_b0__4:5;
			unsigned char glove_mode_feature:1;
			unsigned char f12_query10_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_9 {
	union {
		struct {
			unsigned char touch_threshold;
			unsigned char lift_hysteresis;
			unsigned char small_z_scale_factor_lsb;
			unsigned char small_z_scale_factor_msb;
			unsigned char large_z_scale_factor_lsb;
			unsigned char large_z_scale_factor_msb;
			unsigned char small_large_boundary;
			unsigned char wx_scale;
			unsigned char wx_offset;
			unsigned char wy_scale;
			unsigned char wy_offset;
			unsigned char x_size_lsb;
			unsigned char x_size_msb;
			unsigned char y_size_lsb;
			unsigned char y_size_msb;
			unsigned char gloved_finger;
		};
		unsigned char data[16];
	};
};

struct synaptics_rmi4_f12_ctrl_11 {
	union {
		struct {
			unsigned char small_corner;
			unsigned char large_corner;
			unsigned char jitter_filter_strength;
			unsigned char x_minimum_z;
			unsigned char y_minimum_z;
			unsigned char x_maximum_z;
			unsigned char y_maximum_z;
			unsigned char x_amplitude;
			unsigned char y_amplitude;
			unsigned char gloved_finger_jitter_filter_strength;
		};
		unsigned char data[10];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char obj_type_enable;
			unsigned char max_reported_objects;
		};
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char reserved:5;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char button_int_enable;
	unsigned char multi_button;
	unsigned char *txrx_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_f34_ctrl_3 {
	union {
		struct {
			unsigned char fw_release_month;
			unsigned char fw_release_date;
			unsigned char fw_release_revision;
			unsigned char fw_release_version;
		};
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f34_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

struct synaptics_rmi4_f34_handle {
	unsigned char status;
	unsigned char cmd;
	unsigned short bootloaderid;
	unsigned short blocksize;
	unsigned short imageblockcount;
	unsigned short configblockcount;
	unsigned short blocknum;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	bool inflashprogmode;
	unsigned char intr_mask;
	struct mutex attn_mutex;
	struct synaptics_rmi4_f34_fn_ptr *fn_ptr;
};

#ifdef PROXIMITY
struct synaptics_rmi4_f51_query {
	union {
		struct {
			unsigned char query_register_count;
			unsigned char data_register_count;
			unsigned char control_register_count;
			unsigned char command_register_count;
			unsigned char features;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f51_data {
	union {
		struct {
			unsigned char finger_hover_det:1;
			unsigned char air_swipe_det:1;
			unsigned char large_obj_det:1;
			unsigned char f1a_data0_b3:1;
			unsigned char hover_pinch_det:1;
			unsigned char profile_handedness_status:2;
			unsigned char f1a_data0_b5__7:1;
			unsigned char hover_finger_x_4__11;
			unsigned char hover_finger_y_4__11;
			unsigned char hover_finger_xy_0__3;
			unsigned char hover_finger_z;
			unsigned char f1a_data2_b0__6:7;
			unsigned char hover_pinch_dir:1;
			unsigned char air_swipe_dir_0:1;
			unsigned char air_swipe_dir_1:1;
			unsigned char f1a_data3_b2__4:3;
			unsigned char object_present:1;
			unsigned char large_obj_act:2;
		} __packed;
		unsigned char proximity_data[7];
	};
#ifdef EDGE_SWIPE
	union {
		struct {
			unsigned char edge_swipe_x_lsb;
			unsigned char edge_swipe_x_msb;
			unsigned char edge_swipe_y_lsb;
			unsigned char edge_swipe_y_msb;
			unsigned char edge_swipe_z;
			unsigned char edge_swipe_wx;
			unsigned char edge_swipe_wy;
			unsigned char edge_swipe_mm;
			signed char edge_swipe_dg;
		} __packed;
		unsigned char edge_swipe_data[9];
	};
#endif
};

#ifdef EDGE_SWIPE
struct synaptics_rmi4_surface {
	int width_major;
	int palm;
	int angle;
	int wx;
	int wy;
};
#endif

struct synaptics_rmi4_f51_handle {
	unsigned char edge_swipe_data_offset;
	unsigned char proximity_enables;
	unsigned char general_control;
	unsigned short proximity_enables_addr;
	unsigned short general_control_addr;
	struct synaptics_rmi4_surface surface_data;
	struct synaptics_rmi4_data *rmi4_data;
};
#endif

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	bool initialized;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

static struct device_attribute attrs[] = {
	__ATTR(regval, (S_IRUGO | S_IWUSR | S_IWGRP),
			NULL,
			synaptics_rmi4_register_value_store),
	__ATTR(global, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_global_show,
			synaptics_rmi4_global_store),
#ifdef CONFIG_HAS_EARLYSUSPEND
	__ATTR(full_pm_cycle, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_full_pm_cycle_show,
			synaptics_rmi4_full_pm_cycle_store),
#endif
#ifdef PROXIMITY
	__ATTR(proximity_enables, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_f51_enables_show,
			synaptics_rmi4_f51_enables_store),
	__ATTR(general_control, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_f51_general_control_show,
			synaptics_rmi4_f51_general_control_store),
#endif
	__ATTR(glove_mode_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_glove_mode_enable_show,
			synaptics_rmi4_glove_mode_enable_store),
	__ATTR(closed_cover_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_closed_cover_enable_show,
			synaptics_rmi4_closed_cover_enable_store),
	__ATTR(fast_detect_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_fast_detect_enable_show,
			synaptics_rmi4_fast_detect_enable_store),
	__ATTR(reset, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUGO | S_IWUSR | S_IWGRP),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(suspend, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			synaptics_rmi4_suspend_store),
};

extern int system_rev;

static struct list_head exp_fn_list;

#ifdef PROXIMITY
static struct synaptics_rmi4_f51_handle *f51;
#endif

#ifdef READ_LCD_ID
static int synaptics_lcd_id;
static int __init synaptics_read_lcd_id(char *mode)
{
	int retval = 0, count = 0;

	if (mode == NULL)
		return -EINVAL;

	while ((mode[count] != (int)NULL) && (mode[count] >= '0') && \
			(mode[count] <= 'z')) {
		retval = retval * 0x10 + mode[count] - '0';
		++count;
	}

	synaptics_lcd_id = ((retval >> 8) & 0xFF) >> 4;
	printk(KERN_ERR "%s: string is %s, integer is %x, ID2[0x%02x], system_rev is %d\n",
			__func__, mode, retval, synaptics_lcd_id, system_rev );

	return 0;
}
__setup("lcd_id=0x", synaptics_read_lcd_id);
#endif

#ifdef CONFIG_OF
static int synaptics_parse_dt(struct device *dev,
		struct synaptics_rmi4_platform_data *pdata)
{
	int i, rc;
	u32 coords[2];
	struct device_node *np = dev->of_node;
	struct property *prop;
	struct synaptics_rmi_f1a_button_map *f1a_button_map;

	/* vdd, irq gpio info */
	pdata->vdd_io_1p8 = of_get_named_gpio(np, "synaptics,tsppwr_en", 0);
	pdata->tsp_int = of_get_named_gpio(np, "synaptics,irq-gpio", 0);

	/* led info */
#ifdef TOUCKEY_ENABLE
	pdata->tkey_led_vdd_on = of_get_named_gpio(np, "synaptics,tkeyled-gpio", 0);
#endif

	rc = of_property_read_u32_array(np, "synaptics,tsp-coords", coords, 2);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read synaptics,tsp-coords\n", __func__);
		return rc;
	}
	pdata->sensor_max_x = coords[0];
	pdata->sensor_max_y = coords[1];

	/* f1a button info */
	prop = of_find_property(np, "synaptics,tsp-keycodes", NULL);
	if (prop && prop->value) {
		f1a_button_map = kzalloc(sizeof(*f1a_button_map), GFP_KERNEL);
		if (!f1a_button_map) {
			dev_err(dev, "Failed to allocate f1a memory\n");
			return -ENOMEM;
		}

		f1a_button_map->nbuttons = prop->length / sizeof(u32);

		rc = of_property_read_u32_array(np, "synaptics,tsp-keycodes",
			f1a_button_map->map, f1a_button_map->nbuttons);
		if (rc && (rc != -EINVAL)) {
			dev_info(dev, "%s: Unable to read %s\n", __func__,
					"synaptics,tsp-keycodes");
			return rc;
		}

		pdata->f1a_button_map = f1a_button_map;

		pr_err("%s tkey enabled! ", __func__);
		for (i = 0; i < pdata->f1a_button_map->nbuttons; i++)
			pr_cont("keycode[%d] = %d ", i,
				pdata->f1a_button_map->map[i]);
		pr_cont("\n");
	}

	/* project info */
	rc = of_property_read_string(np, "synaptics,tsp-project", &pdata->project_name);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read synaptics,tsp-project\n", __func__);
		pdata->project_name = "0";
	}

	pr_err("%s vdd_1p8= %d, tsp_int= %d, max_x= %d, max_y= %d, project= %s\n",
		__func__, pdata->vdd_io_1p8, pdata->tsp_int,
			pdata->sensor_max_x, pdata->sensor_max_y, pdata->project_name);

	return 0;
}
#else
static int synaptics_parse_dt(struct device *dev,
		struct synaptics_rmi4_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static void synaptics_request_gpio(struct synaptics_rmi4_platform_data *pdata)
{
	int ret;
	pr_info("[TSP] synaptics_request_gpio\n");

	ret = gpio_request(pdata->vdd_io_1p8, "synaptics,tsppwr_en");
	if (ret) {
		pr_err("[TSP]%s: unable to request vdd_io_1p8 [%d]\n",
				__func__, pdata->vdd_io_1p8);
		return;
	}

#ifdef TOUCKEY_ENABLE
	ret = gpio_request(pdata->tkey_led_vdd_on, "synaptics,tkey_led_vdd_on");
	if (ret) {
		pr_err("[TSP]%s: unable to request tkey_led_vdd_on [%d]\n",
				__func__, pdata->tkey_led_vdd_on);
		return;
	}
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static ssize_t synaptics_rmi4_full_pm_cycle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->full_pm_cycle);
}

static ssize_t synaptics_rmi4_full_pm_cycle_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->full_pm_cycle = input > 0 ? 1 : 0;

	return count;
}
#endif

#ifdef PROXIMITY
static ssize_t synaptics_rmi4_f51_enables_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!f51)
		return -ENODEV;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n",
			f51->proximity_enables);
}

static ssize_t synaptics_rmi4_f51_enables_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!f51)
		return -ENODEV;

	if (sscanf(buf, "%x", &input) != 1)
		return -EINVAL;

	f51->proximity_enables = (unsigned char)input;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			f51->proximity_enables_addr,
			&f51->proximity_enables,
			sizeof(f51->proximity_enables));
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to write proximity enables, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f51_general_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int retval;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!f51)
		return -ENODEV;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			f51->general_control_addr,
			&data,
			sizeof(data));
	if (retval < 0)
		dev_err(dev,
				"%s: Failed to read general control, error = %d\n",
				__func__, retval);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n",
			data);
}

static ssize_t synaptics_rmi4_f51_general_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!f51)
		return -ENODEV;

	if (sscanf(buf, "%x", &input) != 1)
		return -EINVAL;

	f51->general_control = (unsigned char)input;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			f51->general_control_addr,
			&f51->general_control,
			sizeof(f51->general_control));
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to write general control, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}
#endif

static ssize_t synaptics_rmi4_register_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input, i;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned char data[10];
	unsigned int retval;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			input,
			&data[0],
			sizeof(data));

	for (i = 0; i < 10; i++)
		dev_info(&rmi4_data->i2c_client->dev, "%s:[%d] register address=0x%X, register value= %X\n",
				__func__, i, input, data[i]);

	return count;
}

static ssize_t synaptics_rmi4_global_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int retval;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned char data;

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			0x0c,
			&data,
			sizeof(data));
	dev_info(&rmi4_data->i2c_client->dev, "%s: device_control: %x\n[%d]",
			__func__, data, retval);
	data = 0;
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			0x0d,
			&data,
			sizeof(data));
	dev_info(&rmi4_data->i2c_client->dev, "%s: device_status: %x\n[%d]",
			__func__, data, retval);
	return snprintf(buf, PAGE_SIZE, "%u\n",
			retval);
}

static ssize_t synaptics_rmi4_global_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char data[8];
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			0x06,
			&data[0],
			sizeof(data));
	dev_info(&rmi4_data->i2c_client->dev, "%s: %x, %x, %x, %x, %x, %x, %x, %x\n",
			__func__, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	return count;
}

static ssize_t synaptics_rmi4_glove_mode_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int output;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (rmi4_data->feature_enable & GLOVE_MODE_EN)
		output = 1;
	else
		output = 0;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			output);
}

static ssize_t synaptics_rmi4_glove_mode_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->feature_enable &= ~(GLOVE_MODE_EN);
	if (input)
		rmi4_data->feature_enable |= GLOVE_MODE_EN;

	retval = synaptics_rmi4_f12_set_feature(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to set glove mode enable, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_closed_cover_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int output;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (rmi4_data->feature_enable & CLOSED_COVER_EN)
		output = 1;
	else
		output = 0;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			output);
}

static ssize_t synaptics_rmi4_closed_cover_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->feature_enable &= ~(CLOSED_COVER_EN);
	if (input)
		rmi4_data->feature_enable |= CLOSED_COVER_EN;

	retval = synaptics_rmi4_f12_set_feature(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to set closed cover enable, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_fast_detect_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int output;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (rmi4_data->feature_enable & FAST_DETECT_EN)
		output = 1;
	else
		output = 0;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			output);
}

static ssize_t synaptics_rmi4_fast_detect_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (!rmi4_data->has_glove_mode)
		return -ENODEV;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmi4_data->feature_enable &= ~(FAST_DETECT_EN);
	if (input)
		rmi4_data->feature_enable |= FAST_DETECT_EN;

	retval = synaptics_rmi4_f12_set_feature(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to set fast detect enable, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

#ifdef TSP_BOOSTER
static void synaptics_change_dvfs_lock(struct work_struct *work)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(work,
				struct synaptics_rmi4_data, work_dvfs_chg.work);
	int retval = 0;

	mutex_lock(&rmi4_data->dvfs_lock);

	if (rmi4_data->dvfs_boost_mode == DVFS_STAGE_DUAL) {
		if (rmi4_data->stay_awake) {
			dev_info(&rmi4_data->i2c_client->dev,
					"%s: do fw update, do not change cpu frequency.\n",
					__func__);
		} else {
			retval = set_freq_limit(DVFS_TOUCH_ID,
					MIN_TOUCH_LIMIT_SECOND);
			rmi4_data->dvfs_freq = MIN_TOUCH_LIMIT_SECOND;
		}
	} else if (rmi4_data->dvfs_boost_mode == DVFS_STAGE_SINGLE ||
			rmi4_data->dvfs_boost_mode == DVFS_STAGE_TRIPLE) {
		retval = set_freq_limit(DVFS_TOUCH_ID, -1);
		rmi4_data->dvfs_freq = -1;
	}

	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: booster change failed(%d).\n",
				__func__, retval);
	mutex_unlock(&rmi4_data->dvfs_lock);
}

static void synaptics_set_dvfs_off(struct work_struct *work)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(work,
				struct synaptics_rmi4_data, work_dvfs_off.work);
	int retval;

	if (rmi4_data->stay_awake) {
		dev_info(&rmi4_data->i2c_client->dev,
				"%s: do fw update, do not change cpu frequency.\n",
				__func__);
	} else {
		mutex_lock(&rmi4_data->dvfs_lock);

		retval = set_freq_limit(DVFS_TOUCH_ID, -1);
		rmi4_data->dvfs_freq = -1;

		if (retval < 0)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: booster stop failed(%d).\n",
					__func__, retval);
		rmi4_data->dvfs_lock_status = false;

		mutex_unlock(&rmi4_data->dvfs_lock);
	}
}

static void synaptics_set_dvfs_lock(struct synaptics_rmi4_data *rmi4_data,
		int on)
{
	int ret = 0;

	if (rmi4_data->dvfs_boost_mode == DVFS_STAGE_NONE) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: DVFS stage is none(%d)\n",
				__func__, rmi4_data->dvfs_boost_mode);
		return;
	}

	mutex_lock(&rmi4_data->dvfs_lock);
	if (on == 0) {
		if (rmi4_data->dvfs_lock_status)
			schedule_delayed_work(&rmi4_data->work_dvfs_off,
					msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
	} else if (on > 0) {
		cancel_delayed_work(&rmi4_data->work_dvfs_off);

		if ((!rmi4_data->dvfs_lock_status) || (rmi4_data->dvfs_old_stauts < on)) {
			cancel_delayed_work(&rmi4_data->work_dvfs_chg);

			if (rmi4_data->dvfs_freq != MIN_TOUCH_LIMIT) {
				if (rmi4_data->dvfs_boost_mode == DVFS_STAGE_TRIPLE)
					ret = set_freq_limit(DVFS_TOUCH_ID,
						MIN_TOUCH_LIMIT_SECOND);
				else
					ret = set_freq_limit(DVFS_TOUCH_ID,
						MIN_TOUCH_LIMIT);
				rmi4_data->dvfs_freq = MIN_TOUCH_LIMIT;

				if (ret < 0)
					dev_err(&rmi4_data->i2c_client->dev,
							"%s: cpu first lock failed(%d)\n",
							__func__, ret);
			} 
			schedule_delayed_work(&rmi4_data->work_dvfs_chg,
					msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));

			rmi4_data->dvfs_lock_status = true;
		}
	} else if (on < 0) {
		if (rmi4_data->dvfs_lock_status) {
			cancel_delayed_work(&rmi4_data->work_dvfs_off);
			cancel_delayed_work(&rmi4_data->work_dvfs_chg);
			schedule_work(&rmi4_data->work_dvfs_off.work);
		}
	}
	rmi4_data->dvfs_old_stauts = on;
	mutex_unlock(&rmi4_data->dvfs_lock);
}

static void synaptics_init_dvfs(struct synaptics_rmi4_data *rmi4_data)
{
	mutex_init(&rmi4_data->dvfs_lock);

	rmi4_data->dvfs_boost_mode = DVFS_STAGE_DUAL;

	INIT_DELAYED_WORK(&rmi4_data->work_dvfs_off, synaptics_set_dvfs_off);
	INIT_DELAYED_WORK(&rmi4_data->work_dvfs_chg, synaptics_change_dvfs_lock);

	rmi4_data->dvfs_lock_status = false;
}
#endif

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int build_id;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	build_id = (unsigned int)rmi->build_id[0] +
		(unsigned int)rmi->build_id[1] * 0x100 +
		(unsigned int)rmi->build_id[2] * 0x10000;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			build_id);
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->button_0d_enabled);
}

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	if (list_empty(&rmi->support_fn_list))
		return -ENODEV;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
				return retval;
			}

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_i2c_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		synaptics_rmi4_suspend(dev);
	else if (input == 0)
		synaptics_rmi4_resume(dev);
	else
		return -EINVAL;

	return count;
}

/**
 * synaptics_rmi4_set_page()
 *
 * Called by synaptics_rmi4_i2c_read() and synaptics_rmi4_i2c_write().
 *
 * This function writes to the page select register to switch to the
 * assigned page.
 */
static int synaptics_rmi4_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned int address)
{
	int retval = 0;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = rmi4_data->i2c_client;

	page = ((address >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				if((rmi4_data->tsp_probe != true)&&(retry>=1)){
					dev_err(&i2c->dev,
							"%s: TSP needs to reboot \n",__func__);
					retval = TSP_NEEDTO_REBOOT;
					return retval;
				}
				dev_err(&i2c->dev,
						"%s: I2C retry = %d, i2c_master_send retval = %d\n",
						__func__, retry + 1, retval);
				if (retval == 0)
					retval = -EAGAIN;
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

/**
 * synaptics_rmi4_i2c_read()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf;
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &buf,
		},
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	buf = addr & MASK_8BIT;

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	if (rmi4_data->touch_stopped) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: Sensor stopped\n",
				__func__);
		retval = 0;
		goto exit;
	}

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 2) == 2) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev, "%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C read over retry limit\n",
				__func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

/**
 * synaptics_rmi4_i2c_write()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf[length + 1];
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	if (rmi4_data->touch_stopped) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: Sensor stopped\n",
				__func__);
		retval = 0;
		goto exit;
	}

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	buf[0] = addr & MASK_8BIT;
	memcpy(&buf[1], &data[0], length);

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C write over retry limit\n",
				__func__);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

/**
 * synaptics_rmi4_f11_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $11
 * finger data has been detected.
 *
 * This function reads the Function $11 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char data_reg_blk_size;
	unsigned char finger_status_reg[3];
	unsigned char data[F11_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	int temp;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;
	data_reg_blk_size = fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
		return 0;
	}

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
			& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status);

		if (finger_status) {
			data_offset = data_addr +
				num_of_finger_status_regs +
				(finger * data_reg_blk_size);
			retval = synaptics_rmi4_i2c_read(rmi4_data,
					data_offset,
					data,
					data_reg_blk_size);
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
				return 0;
			}

			x = (data[0] << 4) | (data[2] & MASK_4BIT);
			y = (data[1] << 4) | ((data[2] >> 4) & MASK_4BIT);
			wx = (data[3] & MASK_4BIT);
			wy = (data[3] >> 4) & MASK_4BIT;

			if (rmi4_data->board->swap_axes) {
				temp = x;
				x = y;
				y = temp;
				temp = wx;
				wx = wy;
				wy = temp;
			}

			if (rmi4_data->board->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->board->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Finger %d:\n"
					"status = 0x%02x\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			touch_count++;
		}
	}
/*
	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
	}
*/
	input_sync(rmi4_data->input_dev);

	return touch_count;
}

/**
 * synaptics_rmi4_f12_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $12
 * finger data has been detected.
 *
 * This function reads the Function $12 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned short data_addr;
	int x;
	int y;
	int wx;
	int wy;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
	unsigned char device_status;

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return 0;
	}
	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;

	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = (finger_data->object_type_and_status & MASK_2BIT);
		x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
		y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
		if ((x == INVALID_X) && (y == INVALID_Y))
			finger_status = 0;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status);

		if (finger_status) {
#ifdef GLOVE_MODE
			if ((finger_status == 0x02) &&
				!rmi4_data->touchkey_glove_mode_status) {
				rmi4_data->touchkey_glove_mode_status = true;
				input_report_switch(rmi4_data->input_dev,
						SW_GLOVE, true);
			} else if ((finger_status != 0x02) &&
				rmi4_data->touchkey_glove_mode_status) {
				rmi4_data->touchkey_glove_mode_status = false;
				input_report_switch(rmi4_data->input_dev,
						SW_GLOVE, false);
			}
#endif
#ifdef REPORT_2D_W
			wx = finger_data->wx;
			wy = finger_data->wy;
#ifdef EDGE_SWIPE
			if ((finger_status == 0x03) &&
				(f51->surface_data.palm == 0)) {
				dev_info(&rmi4_data->i2c_client->dev,
						"%s: finger status 0x3, but No Palm. Reject!\n",
						__func__);
				return 0;
			}

			if (f51) {
				if ((f51->general_control & HAS_EDGE_SWIPE)
					&& f51->surface_data.palm) {
					wx = f51->surface_data.wx;
					wy = f51->surface_data.wy;
				}
			}
#endif
#endif
			if (rmi4_data->board->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->board->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifdef EDGE_SWIPE
			if (f51) {
				if (f51->general_control & HAS_EDGE_SWIPE) {
					input_report_abs(rmi4_data->input_dev,
							ABS_MT_WIDTH_MAJOR, f51->surface_data.width_major);
					input_report_abs(rmi4_data->input_dev,
							ABS_MT_ANGLE, f51->surface_data.angle);
					input_report_abs(rmi4_data->input_dev,
							ABS_MT_PALM, f51->surface_data.palm);
				}
			}
#endif
			if (!rmi4_data->finger[finger].state) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&rmi4_data->i2c_client->dev,
						"[%d][P] 0x%02x, x = %d, y = %d, wx = %d, wy = %d\n",
						finger, finger_status, x, y, wx, wy);
#else
				dev_info(&rmi4_data->i2c_client->dev,
						"[%d][P] 0x%02x\n",
						finger, finger_status);
#endif
			} else {
				rmi4_data->finger[finger].mcount++;
			}
			touch_count++;
		}

		if (rmi4_data->finger[finger].state && (!finger_status)) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			dev_info(&rmi4_data->i2c_client->dev, "[%d][R] 0x%02x M[%d], Ver[%02X][%X/%d]\n",
					finger, finger_status, rmi4_data->finger[finger].mcount,
					rmi4_data->fw_version_of_ic, rmi4_data->lcd_id, system_rev);
#else
			dev_info(&rmi4_data->i2c_client->dev, "[%d][R] 0x%02x M[%d], Ver[ %02X][%X/%d]\n",
					finger, finger_status, rmi4_data->finger[finger].mcount,
					rmi4_data->fw_version_of_ic, rmi4_data->lcd_id, system_rev);
#endif
			rmi4_data->finger[finger].mcount = 0;
		}

		rmi4_data->finger[finger].state = finger_status;

	}

		/* Clear BTN_TOUCH when All touch are released  */
	if (touch_count == 0)
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);

	input_sync(rmi4_data->input_dev);

#ifdef TSP_BOOSTER
	if (touch_count)
		synaptics_set_dvfs_lock(rmi4_data, touch_count);
	else
		synaptics_set_dvfs_lock(rmi4_data, 0);
#endif

	if (touch_count == 0) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				&device_status,
				sizeof(device_status));
		if ((retval < 0) || (device_status != 0))
			dev_info(&rmi4_data->i2c_client->dev, "%s: status: %x, retval: %d\n",
					__func__, device_status, retval);
	}
	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static unsigned char do_once = 1;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (do_once) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		do_once = 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	data = f1a->button_data_buffer;

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status)
			continue;
		else
			current_status[button] = status;

		dev_info(&rmi4_data->i2c_client->dev,
				"%s: Button %d (code %d) ->%d\n",
				__func__, button,
				f1a->button_map[button],
				status);
#ifdef NO_0D_WHILE_2D
		if (rmi4_data->fingers_on_2d == false) {
			if (status == 1) {
				before_2d_status[button] = 1;
			} else {
				if (while_2d_status[button] == 1) {
					while_2d_status[button] = 0;
					continue;
				} else {
					before_2d_status[button] = 0;
				}
			}
			touch_count++;
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button],
					status);
		} else {
			if (before_2d_status[button] == 1) {
				before_2d_status[button] = 0;
				touch_count++;
				input_report_key(rmi4_data->input_dev,
						f1a->button_map[button],
						status);
			} else {
				if (status == 1)
					while_2d_status[button] = 1;
				else
					while_2d_status[button] = 0;
			}
		}
#else
		touch_count++;
		input_report_key(rmi4_data->input_dev,
				f1a->button_map[button],
				status);
#endif
	}

	if (touch_count)
		input_sync(rmi4_data->input_dev);

	return;
}

#ifdef PROXIMITY
#ifdef EDGE_SWIPE
static int synaptics_rmi4_f51_edge_swipe(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned short data_base_addr;
	struct synaptics_rmi4_f51_data *data;

	data_base_addr = fhandler->full_addr.data_base;
	data = (struct synaptics_rmi4_f51_data *)fhandler->data;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_base_addr + EDGE_SWIPE_DATA_OFFSET,
			data->edge_swipe_data,
			sizeof(data->edge_swipe_data));

	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
					__func__, retval);
		return retval;
	}

	if (!f51)
		return -ENODEV;

	if (data->edge_swipe_dg >= 90 && data->edge_swipe_dg <= 180)
		f51->surface_data.angle = data->edge_swipe_dg - 180;
	else if (data->edge_swipe_dg < 90)
		f51->surface_data.angle = data->edge_swipe_dg;
	else
		dev_err(&rmi4_data->i2c_client->dev, "Skip wrong edge swipe angle [%d]\n",
				data->edge_swipe_dg);

	f51->surface_data.width_major = data->edge_swipe_mm;
	f51->surface_data.wx = data->edge_swipe_wx;
	f51->surface_data.wy = data->edge_swipe_wy;
	f51->surface_data.palm = data->edge_swipe_z;

	dev_dbg(&rmi4_data->i2c_client->dev,
		"%s: edge_data : x[%d], y[%d], z[%d] ,wx[%d], wy[%d], area[%d], angle[%d][%d]\n", __func__,
		data->edge_swipe_x_msb << 8 | data->edge_swipe_x_lsb,
		data->edge_swipe_y_msb << 8 | data->edge_swipe_y_lsb,
		data->edge_swipe_z, data->edge_swipe_wx, data->edge_swipe_wy,
		data->edge_swipe_mm, data->edge_swipe_dg, f51->surface_data.angle);

	return retval;
}
#endif

static void synaptics_rmi4_f51_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned short data_base_addr;
	int x;
	int y;
	int z;
	struct synaptics_rmi4_f51_data *data;

#ifdef EDGE_SWIPE
	/* Read edge swipe data */
	if (f51->general_control & EDGE_SWIPE_EN) {
		retval = synaptics_rmi4_f51_edge_swipe(rmi4_data, fhandler);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to read edge swipe data\n",
					__func__);
			return;
		}
	}
#endif

	data_base_addr = fhandler->full_addr.data_base;
	data = (struct synaptics_rmi4_f51_data *)fhandler->data;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_base_addr,
			data->proximity_data,
			sizeof(data->proximity_data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read proximity data registers\n",
				__func__);
		return;
	}

	if (data->proximity_data[0] == 0x00)
		return;

#ifdef HAND_GRIP_MODE
	/* enable hover event + handgrip event */
	if (rmi4_data->hand_grip_mode/* && data->profile_handedness_status*/) {
		dev_dbg(&rmi4_data->i2c_client->dev,
		"%s: profile_handedness_status, 0x%x\n",
		__func__, data->profile_handedness_status);

		if (rmi4_data->old_code != data->profile_handedness_status) {
			if (data->profile_handedness_status == 0x2)
				rmi4_data->hand_grip = SW_RIGHT_HAND;
			else if (data->profile_handedness_status == 0x1)
				rmi4_data->hand_grip = SW_LEFT_HAND;
			else
				rmi4_data->hand_grip = SW_BOTH_HAND;

			input_report_switch(rmi4_data->input_dev, rmi4_data->old_hand_grip, 0);
			input_sync(rmi4_data->input_dev);

			input_report_switch(rmi4_data->input_dev,
					rmi4_data->hand_grip, 1);

			input_sync(rmi4_data->input_dev);
			dev_info(&rmi4_data->i2c_client->dev, "%s: (%d) %s Hand grip.(old = %s)\n",
					__func__, data->profile_handedness_status,
					rmi4_data->hand_grip == SW_RIGHT_HAND ? "RIGHT" :
					rmi4_data->hand_grip == SW_LEFT_HAND ? "LEFT" : "BOTH",
					rmi4_data->old_hand_grip == SW_RIGHT_HAND ? "RIGHT" :
					rmi4_data->old_hand_grip == SW_LEFT_HAND ? "LEFT" : "BOTH");

		}

	rmi4_data->old_code = data->profile_handedness_status;
	rmi4_data->old_hand_grip = rmi4_data->hand_grip;

	if (rmi4_data->hand_grip_mode && !rmi4_data->hover_status_in_normal_mode)
		return;

	}
#endif

	if (data->finger_hover_det && (data->hover_finger_z > 0)) {
		x = (data->hover_finger_x_4__11 << 4) |
			(data->hover_finger_xy_0__3 & 0x0f);
		y = (data->hover_finger_y_4__11 << 4) |
			(data->hover_finger_xy_0__3 >> 4);
		z = HOVER_Z_MAX - data->hover_finger_z;

		input_mt_slot(rmi4_data->input_dev, 0);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 1);

		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 1);
		input_report_abs(rmi4_data->input_dev,
				ABS_MT_POSITION_X, x);
		input_report_abs(rmi4_data->input_dev,
				ABS_MT_POSITION_Y, y);
		input_report_abs(rmi4_data->input_dev,
				ABS_MT_DISTANCE, z);

		input_sync(rmi4_data->input_dev);

		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Hover finger:\n"
				"x = %d\n"
				"y = %d\n"
				"z = %d\n",
				__func__, x, y, z);

		rmi4_data->f51_finger = true;
		rmi4_data->fingers_on_2d = false;
		synaptics_rmi4_f51_finger_timer((unsigned long)rmi4_data);
	}

	if (data->air_swipe_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Swipe direction 0 = %d\n",
				__func__, data->air_swipe_dir_0);
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Swipe direction 1 = %d\n",
				__func__, data->air_swipe_dir_1);
	}

	if (data->large_obj_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Large object activity = %d\n",
				__func__, data->large_obj_act);
	}

	if (data->hover_pinch_det) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Hover pinch direction = %d\n",
				__func__, data->hover_pinch_dir);
	}

	if (data->object_present) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Object presence detected\n",
				__func__);
	}

	return;
}
#endif

/**
 * synaptics_rmi4_report_touch()
 *
 * Called by synaptics_rmi4_sensor_report().
 *
 * This function calls the appropriate finger data reporting function
 * based on the function handler it receives and returns the number of
 * fingers detected.
 */
static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count_2d;

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
		case SYNAPTICS_RMI4_F11:
			touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
					fhandler);

			if (touch_count_2d)
				rmi4_data->fingers_on_2d = true;
			else
				rmi4_data->fingers_on_2d = false;
			break;
		case SYNAPTICS_RMI4_F12:
			touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
					fhandler);

			if (touch_count_2d)
				rmi4_data->fingers_on_2d = true;
			else
				rmi4_data->fingers_on_2d = false;
			break;
		case SYNAPTICS_RMI4_F1A:
			synaptics_rmi4_f1a_report(rmi4_data, fhandler);
			break;
#ifdef PROXIMITY
		case SYNAPTICS_RMI4_F51:
			synaptics_rmi4_f51_report(rmi4_data, fhandler);
			break;
#endif
		default:
			dev_info(&rmi4_data->i2c_client->dev,
					"%s: fn_number : %d\n",
					__func__, fhandler->fn_number);
			break;
	}

	return;
}

/**
 * synaptics_rmi4_sensor_report()
 *
 * Called by synaptics_rmi4_irq().
 *
 * This function determines the interrupt source(s) from the sensor
 * and calls synaptics_rmi4_report_touch() with the appropriate
 * function handler for each function with valid data inputs.
 */
static int synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fn *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	status.data[0] = data[0];
	if (status.unconfigured) {
		if (rmi4_data->doing_reflash) {
			dev_err(&rmi4_data->i2c_client->dev,
					"Spontaneous reset detected during reflash.\n");
			return 0;
		}

		dev_info(&rmi4_data->i2c_client->dev,
				"Spontaneous reset detected\n");
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to reinit device\n",
					__func__);
		}
		return 0;
	}

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
						intr[fhandler->intr_reg_num]) {
					synaptics_rmi4_report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}

	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
			if (exp_fhandler->initialized &&
					(exp_fhandler->func_attn != NULL))
				exp_fhandler->func_attn(rmi4_data, intr[0]);
		}
	}

	return 0;
}

/**
 * synaptics_rmi4_irq()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = data;

	do {
		retval = synaptics_rmi4_sensor_report(rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s: Failed to read",
					__func__);
			goto out;
		}

		if (!rmi4_data->touch_stopped)
			goto out;

	} while (!gpio_get_value(rmi4_data->board->tsp_int));
out:
	return IRQ_HANDLED;
}

/**
 * synaptics_rmi4_irq_enable()
 *
 * Called by synaptics_rmi4_probe() and the power management functions
 * in this driver and also exported to other expansion Function modules
 * such as rmi_dev.
 *
 * This function handles the enabling and disabling of the attention
 * irq including the setting up of the ISR thread.
 */
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char intr_status[MAX_INTR_REGISTERS];

	//	memset(rmi4_data->finger, 0, sizeof(rmi4_data->finger) * 10);
	int i;
	for (i = 0; i < 10; i++) {
		rmi4_data->finger[i].mcount = 0;
		rmi4_data->finger[i].state= 0;
	}
	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr + 1,
				intr_status,
				rmi4_data->num_of_intr_regs);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
					__func__, retval);
			return retval;
		}

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				synaptics_rmi4_irq, IRQF_TRIGGER_FALLING,
				DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}

	return retval;
}

/**
 * synaptics_rmi4_f11_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 11 registers
 * and determines the number of fingers supported, x and y data ranges,
 * offset to the associated interrupt status register, interrupt bit
 * mask, and gathers finger data acquisition capabilities from the query
 * registers.
 */
static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	unsigned char abs_data_size;
	unsigned char abs_data_blk_size;
	unsigned char query[F11_STD_QUERY_LEN];
	unsigned char control[F11_STD_CTRL_LEN];

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			query,
			sizeof(query));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%d read fail[%d]\n",
					__func__, __LINE__, retval);
		return retval;
	}

	/* Maximum number of fingers supported */
	if ((query[1] & MASK_3BIT) <= 4)
		fhandler->num_of_data_points = (query[1] & MASK_3BIT) + 1;
	else if ((query[1] & MASK_3BIT) == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			control,
			sizeof(control));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%d read fail[%d]\n",
					__func__, __LINE__, retval);
		return retval;
	}

	/* Maximum x and y */
	rmi4_data->sensor_max_x = ((control[6] & MASK_8BIT) << 0) |
		((control[7] & MASK_4BIT) << 8);
	rmi4_data->sensor_max_y = ((control[8] & MASK_8BIT) << 0) |
		((control[9] & MASK_4BIT) << 8);
	dev_info(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->max_touch_width = MAX_F11_TOUCH_WIDTH;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
				intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	abs_data_size = query[5] & MASK_2BIT;
	abs_data_blk_size = 3 + (2 * (abs_data_size == 0 ? 1 : 0));
	fhandler->size_of_data_register_block = abs_data_blk_size;
	fhandler->data = NULL;
	fhandler->extra = NULL;

	return retval;
}

int synaptics_rmi4_f12_ctrl11_set (struct synaptics_rmi4_data *rmi4_data,
		unsigned char data)
{
	struct synaptics_rmi4_f12_ctrl_11 ctrl_11;

	int retval;

	if (rmi4_data->touch_stopped)
		return 0;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f12_ctrl11_addr,
			ctrl_11.data,
			sizeof(ctrl_11.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
				__func__, retval);
		return retval;
	}

	ctrl_11.data[2] = data; /* set a value of jitter filter */

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f12_ctrl11_addr,
			ctrl_11.data,
			sizeof(ctrl_11.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: write fail[%d]\n",
				__func__, retval);
		return retval;
	}

	return 0;
}

int synaptics_rmi4_f12_set_feature(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	if (rmi4_data->has_glove_mode) {
		retval = synaptics_rmi4_i2c_write(rmi4_data,
				rmi4_data->f12_ctrl26_addr,
				&rmi4_data->feature_enable,
				sizeof(rmi4_data->feature_enable));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s: write fail[%d]\n",
				__func__, retval);
			return retval;
		}
	}

	return 0;
}

static int synaptics_rmi4_f12_set_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f12_ctrl28_addr,
			&rmi4_data->report_enable,
			sizeof(rmi4_data->report_enable));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: write fail[%d]\n",
				__func__, retval);
		return retval;
	}

	return retval;
}

/**
 * synaptics_rmi4_f12_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 12 registers and
 * determines the number of fingers supported, offset to the data1
 * register, x and y data ranges, offset to the associated interrupt
 * status register, interrupt bit mask, and allocates memory resources
 * for finger data acquisition.
 */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	unsigned char size_of_2d_data;
	unsigned char size_of_query8;
	unsigned char ctrl_8_offset;
	unsigned char ctrl_9_offset;
	unsigned char ctrl_11_offset;
	unsigned char ctrl_23_offset;
	unsigned char ctrl_26_offset;
	unsigned char ctrl_28_offset;
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_query_5 query_5;
	struct synaptics_rmi4_f12_query_8 query_8;
	struct synaptics_rmi4_f12_query_10 query_10;
	struct synaptics_rmi4_f12_ctrl_8 ctrl_8;
	struct synaptics_rmi4_f12_ctrl_9 ctrl_9;
	struct synaptics_rmi4_f12_ctrl_11 ctrl_11;
	struct synaptics_rmi4_f12_ctrl_23 ctrl_23;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;
	fhandler->extra = kzalloc(sizeof(*extra_data), GFP_KERNEL);
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base + 5,
			query_5.data,
			sizeof(query_5.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	ctrl_8_offset = query_5.ctrl0_is_present +
		query_5.ctrl1_is_present +
		query_5.ctrl2_is_present +
		query_5.ctrl3_is_present +
		query_5.ctrl4_is_present +
		query_5.ctrl5_is_present +
		query_5.ctrl6_is_present +
		query_5.ctrl7_is_present;

	ctrl_9_offset = ctrl_8_offset +
		query_5.ctrl8_is_present;

	ctrl_11_offset = ctrl_9_offset +
		query_5.ctrl9_is_present +
		query_5.ctrl10_is_present;

	ctrl_23_offset = ctrl_11_offset +
		query_5.ctrl11_is_present +
		query_5.ctrl12_is_present +
		query_5.ctrl13_is_present +
		query_5.ctrl14_is_present +
		query_5.ctrl15_is_present +
		query_5.ctrl16_is_present +
		query_5.ctrl17_is_present +
		query_5.ctrl18_is_present +
		query_5.ctrl19_is_present +
		query_5.ctrl20_is_present +
		query_5.ctrl21_is_present +
		query_5.ctrl22_is_present;

	ctrl_26_offset = ctrl_23_offset +
		query_5.ctrl23_is_present +
		query_5.ctrl24_is_present +
		query_5.ctrl25_is_present;

	ctrl_28_offset = ctrl_26_offset +
		query_5.ctrl26_is_present +
		query_5.ctrl27_is_present;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_23_offset,
			ctrl_23.data,
			sizeof(ctrl_23.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	/* Maximum number of fingers supported */
	fhandler->num_of_data_points = min(ctrl_23.max_reported_objects,
			(unsigned char)F12_FINGERS_TO_SUPPORT);

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base + 7,
			&size_of_query8,
			sizeof(size_of_query8));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base + 8,
			query_8.data,
			size_of_query8);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	/* Determine the presence of the Data0 register */
	extra_data->data1_offset = query_8.data0_is_present;

	if ((size_of_query8 >= 3) && (query_8.data15_is_present)) {
		extra_data->data15_offset = query_8.data0_is_present +
			query_8.data1_is_present +
			query_8.data2_is_present +
			query_8.data3_is_present +
			query_8.data4_is_present +
			query_8.data5_is_present +
			query_8.data6_is_present +
			query_8.data7_is_present +
			query_8.data8_is_present +
			query_8.data9_is_present +
			query_8.data10_is_present +
			query_8.data11_is_present +
			query_8.data12_is_present +
			query_8.data13_is_present +
			query_8.data14_is_present;
		extra_data->data15_size = (rmi4_data->num_of_fingers + 7) / 8;
	} else {
		extra_data->data15_size = 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base + 10,
			query_10.data,
			sizeof(query_10.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	if (query_10.glove_mode_feature)
		rmi4_data->has_glove_mode = true;
	else
		rmi4_data->has_glove_mode = false;

	rmi4_data->report_enable = RPT_DEFAULT;
#ifdef REPORT_2D_Z
	rmi4_data->report_enable |= RPT_Z;
#endif
#ifdef REPORT_2D_W
	rmi4_data->report_enable |= (RPT_WX | RPT_WY);
#endif

/*
 * Set f12 control register address.
 * control register address : cntrol base register address + offset
 */
	rmi4_data->f12_ctrl11_addr = fhandler->full_addr.ctrl_base + ctrl_11_offset;
	rmi4_data->f12_ctrl26_addr = fhandler->full_addr.ctrl_base + ctrl_26_offset;
	rmi4_data->f12_ctrl28_addr = fhandler->full_addr.ctrl_base + ctrl_28_offset;

	if (rmi4_data->has_glove_mode) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f12_ctrl26_addr,
				&rmi4_data->feature_enable,
				sizeof(rmi4_data->feature_enable));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
			return retval;
		}

		retval = synaptics_rmi4_f12_set_feature(rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s: f12_set_feature fail[%d]\n",
					__func__, retval);
			return retval;
		}
	}

	retval = synaptics_rmi4_f12_set_report(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: f12_set_report fail[%d]\n",
				__func__, retval);
		return retval;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_8_offset,
			ctrl_8.data,
			sizeof(ctrl_8.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	/* Maximum x and y */
	rmi4_data->sensor_max_x =
		((unsigned short)ctrl_8.max_x_coord_lsb << 0) |
		((unsigned short)ctrl_8.max_x_coord_msb << 8);
	rmi4_data->sensor_max_y =
		((unsigned short)ctrl_8.max_y_coord_lsb << 0) |
		((unsigned short)ctrl_8.max_y_coord_msb << 8);
	dev_info(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	rmi4_data->num_of_rx = ctrl_8.num_of_rx;
	rmi4_data->num_of_tx = ctrl_8.num_of_tx;
	rmi4_data->num_of_node = ctrl_8.num_of_rx * ctrl_8.num_of_tx;

	rmi4_data->max_touch_width = max(rmi4_data->num_of_rx,
			rmi4_data->num_of_tx);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base + ctrl_9_offset,
			ctrl_9.data,
			sizeof(ctrl_9.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	rmi4_data->touch_threshold = (int)ctrl_9.touch_threshold;
	rmi4_data->gloved_sensitivity = (int)ctrl_9.gloved_finger;
	dev_info(&rmi4_data->i2c_client->dev,
			"%s: %02x num_Rx:%d num_Tx:%d, node:%d, threshold:%d, gloved sensitivity:%x\n",
			__func__, fhandler->fn_number,
			rmi4_data->num_of_rx, rmi4_data->num_of_tx,
			rmi4_data->num_of_node, rmi4_data->touch_threshold,
			rmi4_data->gloved_sensitivity);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f12_ctrl11_addr,
			ctrl_11.data,
			sizeof(ctrl_11.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	dev_info(&rmi4_data->i2c_client->dev,
			"%s: jitter filter strenth = %d, glove jitter filter strenght = %d\n",
			__func__, (int)ctrl_11.jitter_filter_strength,
			(int)ctrl_11.gloved_finger_jitter_filter_strength);

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
				intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	/* Allocate memory for finger data storage space */
	fhandler->data_size = rmi4_data->num_of_fingers * size_of_2d_data;
	fhandler->data = kzalloc(fhandler->data_size, GFP_KERNEL);

	return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for function handle\n",
				__func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;
	fhandler->extra = NULL;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read query registers\n",
				__func__);
		return retval;
	}

	f1a->max_count = f1a->button_query.max_button_count + 1;

	f1a->button_control.txrx_map = kzalloc(f1a->max_count * 2, GFP_KERNEL);
	if (!f1a->button_control.txrx_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for tx rx mapping\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_bitmask_size = (f1a->max_count + 7) / 8;

	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for data buffer\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->max_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for button map\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}

static int synaptics_rmi4_f1a_button_map(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char ii;
	unsigned char mapping_offset = 0;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	const struct synaptics_rmi4_platform_data *pdata = rmi4_data->board;

	mapping_offset = f1a->button_query.has_general_control +
		f1a->button_query.has_interrupt_enable +
		f1a->button_query.has_multibutton_select;

	if (f1a->button_query.has_tx_rx_map) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				fhandler->full_addr.ctrl_base + mapping_offset,
				f1a->button_control.txrx_map,
				sizeof(f1a->button_control.txrx_map));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to read tx rx mapping\n",
					__func__);
			return retval;
		}

		rmi4_data->button_txrx_mapping = f1a->button_control.txrx_map;
	}

	if (!pdata->f1a_button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: f1a_button_map is NULL in board file\n",
				__func__);
		return -ENODEV;
	} else if (!pdata->f1a_button_map->map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Button map is missing in board file\n",
				__func__);
		return -ENODEV;
	} else {
		if (pdata->f1a_button_map->nbuttons != f1a->max_count) {
			f1a->valid_button_count = min(f1a->max_count,
					pdata->f1a_button_map->nbuttons);
		} else {
			f1a->valid_button_count = f1a->max_count;
		}

		for (ii = 0; ii < f1a->valid_button_count; ii++)
			f1a->button_map[ii] = pdata->f1a_button_map->map[ii];
	}

	return 0;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_control.txrx_map);
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}

	return;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned short intr_offset;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
				intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_f1a_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;

error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

int synaptics_rmi4_tsp_read_test_result(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char data;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f34_ctrl_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
				__func__, retval);
		return retval;
	}

	dev_info(&rmi4_data->i2c_client->dev, "%s: test_result : [%x]\n",
			__func__, data >> 4);

	return data >> 4;
}

static int synaptics_rmi4_f34_read_version(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f34_ctrl_3 ctrl_3;

	rmi4_data->f34_ctrl_base_addr = fhandler->full_addr.ctrl_base;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			ctrl_3.data,
			sizeof(ctrl_3.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
				__func__, retval);
		return retval;
	}

	dev_info(&rmi4_data->i2c_client->dev, "%s: [IC] test_result[%x][date, revision, version] [%02d/%02d, 0x%02X, 0x%02X ]\n",
			__func__, ctrl_3.fw_release_month >> 4, ctrl_3.fw_release_month & 0xF, ctrl_3.fw_release_date,
			ctrl_3.fw_release_revision, ctrl_3.fw_release_version );

	rmi4_data->fw_release_date_of_ic =
		(((ctrl_3.fw_release_month & 0xF) << 8) | ctrl_3.fw_release_date);
	rmi4_data->ic_revision_of_ic = ctrl_3.fw_release_revision;
	rmi4_data->fw_version_of_ic = ctrl_3.fw_release_version;

	return retval;
}

static int synaptics_rmi4_f34_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	retval = synaptics_rmi4_f34_read_version(rmi4_data, fhandler);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
				__func__, retval);
		return retval;
	}

	fhandler->data = NULL;

	return retval;
}

#ifdef PROXIMITY
#ifdef USE_HOVER_REZERO
static void synaptics_rmi4_rezero_work(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(work, struct synaptics_rmi4_data,
			rezero_work.work);

	unsigned char custom_rezero;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			f51->general_control_addr,
			&custom_rezero, sizeof(custom_rezero));

	custom_rezero |= 0x10;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			f51->general_control_addr,
			&custom_rezero, sizeof(custom_rezero));

	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: fail to write custom rezero\n",
			__func__);
		return;
	}
	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);
}
#endif

int synaptics_rmi4_f51_grip_edge_exclusion_rx(bool enables)
{
	int retval;
	unsigned char grip_edge = 0;

	if (!f51) {
		printk(KERN_ERR "%s: f51 is not set!\n", __func__);
		return -ENODEV;
	}

	if (enables)
		grip_edge = 0x8;
	else
		grip_edge = 0x10;

	retval = synaptics_rmi4_i2c_write(f51->rmi4_data,
			f51->proximity_enables_addr + F51_GPIP_EDGE_EXCLUSION_RX_OFFSET,
			&grip_edge,
			sizeof(grip_edge));
	if (retval <= 0)
		dev_err(&f51->rmi4_data->i2c_client->dev, "%s: fail to set grip_edge[%X][ret:%d]\n",
				__func__, grip_edge, retval);
	else
		dev_info(&f51->rmi4_data->i2c_client->dev, "%s: value is 0x%02x\n",
				__func__, grip_edge);

	return retval;
}

int synaptics_proximity_no_sleep_set(bool enables)
{
	int retval;
	unsigned char no_sleep = 0;

	if (!f51) {
		printk(KERN_ERR "%s: f51 is not set!\n", __func__);
		return -ENODEV;
	}

	retval = synaptics_rmi4_i2c_read(f51->rmi4_data,
			f51->rmi4_data->f01_ctrl_base_addr,
			&no_sleep,
			sizeof(no_sleep));

	if (retval <= 0)
		dev_err(&f51->rmi4_data->i2c_client->dev, "%s: fail to read no_sleep[ret:%d]\n",
				__func__, retval);

	if (enables)
		no_sleep |= NO_SLEEP_ON;
	else
		no_sleep &= ~(NO_SLEEP_ON);

	retval = synaptics_rmi4_i2c_write(f51->rmi4_data,
			f51->rmi4_data->f01_ctrl_base_addr,
			&no_sleep,
			sizeof(no_sleep));
	if (retval <= 0)
		dev_err(&f51->rmi4_data->i2c_client->dev, "%s: fail to set no_sleep[%X][ret:%d]\n",
				__func__, no_sleep, retval);

	return retval;
}
EXPORT_SYMBOL(synaptics_proximity_no_sleep_set);

static int synaptics_rmi4_f51_set_enables(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	if (!f51) {
		printk(KERN_ERR "%s: f51 is not set!\n", __func__);
		return 0;
	}

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			f51->proximity_enables_addr,
			&f51->proximity_enables,
			sizeof(f51->proximity_enables));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write proximity enable\n",
				__func__);
		return retval;
	}
#ifdef USE_HOVER_REZERO
	if (f51->proximity_enables & FINGER_HOVER_EN)
		schedule_delayed_work(&rmi4_data->rezero_work,
						msecs_to_jiffies(300));
#endif
	return 0;
}

static int synaptics_rmi4_f51_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned short intr_offset;
	struct synaptics_rmi4_f51_data *data;
	struct synaptics_rmi4_f51_query query;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = fd->intr_src_count;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) + intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	fhandler->data_size = sizeof(*data);
	data = kzalloc(fhandler->data_size, GFP_KERNEL);
	fhandler->data = (void *)data;
	fhandler->extra = NULL;

	f51 = kzalloc(sizeof(*f51), GFP_KERNEL);
	f51->rmi4_data = rmi4_data;
	f51->proximity_enables = AIR_SWIPE_EN | SLEEP_PROXIMITY;

	f51->general_control = F51_GENERAL_CONTROL;

	f51->proximity_enables_addr = fhandler->full_addr.ctrl_base +
		F51_PROXIMITY_ENABLES_OFFSET;
	f51->general_control_addr = fhandler->full_addr.ctrl_base +
		F51_GENERAL_CONTROL_OFFSET;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			query.data,
			sizeof(query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	f51->edge_swipe_data_offset = 1;

	if (query.features & HAS_FINGER_HOVER)
		f51->edge_swipe_data_offset += F51_DATA_1_SIZE;

	if (query.features & HAS_HOVER_PINCH)
		f51->edge_swipe_data_offset += F51_DATA_2_SIZE;

	if (query.features & (HAS_AIR_SWIPE | HAS_LARGE_OBJ))
		f51->edge_swipe_data_offset += F51_DATA_3_SIZE;

	f51->edge_swipe_data_offset += F51_DATA_4_SIZE;
	f51->edge_swipe_data_offset += F51_DATA_5_SIZE;
	f51->edge_swipe_data_offset += F51_DATA_6_SIZE;

	if (query.features & HAS_EDGE_SWIPE) {
		rmi4_data->has_edge_swipe = true;
		f51->general_control |= EDGE_SWIPE_EN;
	} else {
		rmi4_data->has_edge_swipe = false;
	}

	retval = synaptics_rmi4_f51_set_enables(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: f51_set_enables fail[%d]\n",
				__func__, retval);
		return retval;
	}

	return 0;
}

int synaptics_rmi4_proximity_enables(unsigned char enables)
{
	int retval;

	if (!f51)
		return -ENODEV;

	if (f51->rmi4_data->hand_grip_mode)
		f51->proximity_enables |= ENABLE_HANDGRIP_RECOG;
	else
		f51->proximity_enables &= ~(ENABLE_HANDGRIP_RECOG);

	if (enables) {
		f51->proximity_enables |= FINGER_HOVER_EN;
		f51->proximity_enables &= ~(SLEEP_PROXIMITY);
	} else {
		f51->proximity_enables |= SLEEP_PROXIMITY;
		f51->proximity_enables &= ~(FINGER_HOVER_EN);
	}

	retval = synaptics_rmi4_f51_set_enables(f51->rmi4_data);
	if (retval < 0)
		return retval;

	return 0;
}
EXPORT_SYMBOL(synaptics_rmi4_proximity_enables);
#endif

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int timeout = CHECK_STATUS_TIMEOUT_MS;
	unsigned char command = 0x01;
	unsigned char intr_status;
	struct synaptics_rmi4_f01_device_status status;

	/* Do a device reset first */
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d write fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	msleep(rmi4_data->board->reset_delay_ms);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
		return retval;
	}

	dev_info(&rmi4_data->i2c_client->dev, "%s: Device status[0x%02x] status.code[%d]\n",
			__func__, status.data[0], status.status_code);

	while (status.status_code == STATUS_CRC_IN_PROGRESS) {
		if (timeout > 0)
			msleep(20);
		else
			return -1;

		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
				__func__, __LINE__, retval);
			return retval;
		}

		timeout -= 20;
	}

	if (status.flash_prog == 1) {
		rmi4_data->firmware_cracked = rmi4_data->flash_prog_mode = true;
		dev_info(&rmi4_data->i2c_client->dev, "%s: In flash prog mode, status = 0x%02x\n",
				__func__, status.status_code);
	} else {
		rmi4_data->flash_prog_mode = false;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			&intr_status,
			sizeof(intr_status));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read interrupt status\n",
				__func__);
		return retval;
	}

	return 0;
}

static void synaptics_rmi4_set_configured(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to set configured\n",
				__func__);
		return;
	}

	device_ctrl |= CONFIGURED;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to set configured\n",
				__func__);
	}

	return;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kzalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
		(rmi_fd->data_base_addr |
		 (page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
		(rmi_fd->ctrl_base_addr |
		 (page_number << 8));
	(*fhandler)->full_addr.cmd_base =
		(rmi_fd->cmd_base_addr |
		 (page_number << 8));
	(*fhandler)->full_addr.query_base =
		(rmi_fd->query_base_addr |
		 (page_number << 8));

	return 0;
}

static void synaptics_rmi_select_ta_mode(struct synaptics_rmi4_data *rmi4_data)
{
	/* ta_con_mode true : I2C (RMI)
	 * ta_con_mode false : INT (GPIO)
	 */

	if (system_rev >= TA_CON_REVISION)
		rmi4_data->ta_con_mode = false;
	else
		rmi4_data->ta_con_mode = true;
}

/**
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This funtion scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii = 0;
	unsigned char page_number;
	unsigned char intr_count = 0;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned char f01_package_id[F01_PACKAGE_ID_LEN];
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev, "%s: read fail[%d]\n",
							__func__, retval);
				return retval;
			}

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(&rmi4_data->i2c_client->dev,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			/* Display function description infomation */
			dev_info(&rmi4_data->i2c_client->dev, "%s: F%02x found (page %d): INT_SRC[%02X] BASE_ADDRS[%02X,%02X,%02X,%02x]\n",
					__func__, rmi_fd.fn_number, page_number,
					rmi_fd.intr_src_count, rmi_fd.data_base_addr,
					rmi_fd.ctrl_base_addr, rmi_fd.cmd_base_addr,
					rmi_fd.query_base_addr);

			switch (rmi_fd.fn_number) {
				case SYNAPTICS_RMI4_F01:
					rmi4_data->f01_query_base_addr =
						rmi_fd.query_base_addr;
					rmi4_data->f01_ctrl_base_addr =
						rmi_fd.ctrl_base_addr;
					rmi4_data->f01_data_base_addr =
						rmi_fd.data_base_addr;
					rmi4_data->f01_cmd_base_addr =
						rmi_fd.cmd_base_addr;

					retval = synaptics_rmi4_check_status(rmi4_data);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to check status\n",
								__func__);
						return retval;
					}

					if (rmi4_data->flash_prog_mode)
						goto flash_prog_mode;

					break;
				case SYNAPTICS_RMI4_F11:
					if (rmi_fd.intr_src_count == 0)
						break;

					retval = synaptics_rmi4_alloc_fh(&fhandler,
							&rmi_fd, page_number);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to alloc for F%d\n",
								__func__,
								rmi_fd.fn_number);
						return retval;
					}

					retval = synaptics_rmi4_f11_init(rmi4_data,
							fhandler, &rmi_fd, intr_count);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: f11_init fail[%d]\n",
								__func__, retval);
						return retval;
					}
					break;
				case SYNAPTICS_RMI4_F12:
					if (rmi_fd.intr_src_count == 0)
						break;

					retval = synaptics_rmi4_alloc_fh(&fhandler,
							&rmi_fd, page_number);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to alloc for F%d\n",
								__func__,
								rmi_fd.fn_number);
						return retval;
					}

					retval = synaptics_rmi4_f12_init(rmi4_data,
							fhandler, &rmi_fd, intr_count);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: f12_init fail[%d]\n",
								__func__, retval);
						return retval;
					}
					break;
				case SYNAPTICS_RMI4_F1A:
					if (rmi_fd.intr_src_count == 0)
						break;

					retval = synaptics_rmi4_alloc_fh(&fhandler,
							&rmi_fd, page_number);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to alloc for F%d\n",
								__func__,
								rmi_fd.fn_number);
						return retval;
					}

					retval = synaptics_rmi4_f1a_init(rmi4_data,
							fhandler, &rmi_fd, intr_count);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: f1a_init fail[%d]\n",
								__func__, retval);
						return retval;
					}
					break;
				case SYNAPTICS_RMI4_F34:
					if (rmi_fd.intr_src_count == 0)
						break;

					retval = synaptics_rmi4_alloc_fh(&fhandler,
							&rmi_fd, page_number);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to alloc for F%d\n",
								__func__,
								rmi_fd.fn_number);
						return retval;
					}

					retval = synaptics_rmi4_f34_init(rmi4_data,
							fhandler, &rmi_fd, intr_count);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: f34_init fail[%d]\n",
								__func__, retval);
						return retval;
					}
					break;

#ifdef PROXIMITY
				case SYNAPTICS_RMI4_F51:
					if (rmi_fd.intr_src_count == 0)
						break;

					retval = synaptics_rmi4_alloc_fh(&fhandler,
							&rmi_fd, page_number);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: Failed to alloc for F%d\n",
								__func__,
								rmi_fd.fn_number);
						return retval;
					}

					retval = synaptics_rmi4_f51_init(rmi4_data,
							fhandler, &rmi_fd, intr_count);
					if (retval < 0) {
						dev_err(&rmi4_data->i2c_client->dev,
								"%s: f51_init fail[%d]\n",
								__func__, retval);
						return retval;
					}
					break;
#endif
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

flash_prog_mode:
	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_info(&rmi4_data->i2c_client->dev,
			"%s: Number of interrupt registers = %d sum of intr_count = %d\n",
			__func__, rmi4_data->num_of_intr_regs, intr_count);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr,
			f01_query,
			sizeof(f01_query));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
		return retval;
	}

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	rmi->product_info[0] = f01_query[2] & MASK_7BIT;
	rmi->product_info[1] = f01_query[3] & MASK_7BIT;
	rmi->date_code[0] = f01_query[4] & MASK_5BIT;
	rmi->date_code[1] = f01_query[5] & MASK_4BIT;
	rmi->date_code[2] = f01_query[6] & MASK_5BIT;
	rmi->tester_id = ((f01_query[7] & MASK_7BIT) << 8) |
		(f01_query[8] & MASK_7BIT);
	rmi->serial_number = ((f01_query[9] & MASK_7BIT) << 8) |
		(f01_query[10] & MASK_7BIT);
	memcpy(rmi->product_id_string, &f01_query[11], 10);

	if (strncmp(rmi->product_id_string, "s5050", 5) == 0)
		rmi4_data->ic_version = SYNAPTICS_PRODUCT_ID_S5050;

	else if (strncmp(rmi->product_id_string, "S5050", 5) == 0)
		rmi4_data->ic_version = SYNAPTICS_PRODUCT_ID_S5050;
	
	else if (strncmp(rmi->product_id_string, "S5000", 5) == 0)
		rmi4_data->ic_version = SYNAPTICS_PRODUCT_ID_S5000;
	else
		rmi4_data->ic_version = SYNAPTICS_PRODUCT_ID_NONE;

/* below code is only S5000 IC temporary code. will be removed..*/
	if (rmi4_data->ic_version == SYNAPTICS_PRODUCT_ID_S5000)
		rmi4_data->ic_revision_of_ic = 0xB0;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			0x31, f01_package_id,
			sizeof(f01_package_id));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
		return retval;
	}

	rmi->package_id = (f01_package_id[1] << 8) | f01_package_id[0];
	rmi->package_rev = (f01_package_id[3] << 8) | f01_package_id[2];
	dev_info(&rmi4_data->i2c_client->dev,
			"%s: [%s] package_id is %d, package_rev is %d\n",
			__func__, rmi->product_id_string,
			rmi->package_id, rmi->package_rev);

	if (rmi->manufacturer_id != 1) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
				__func__, rmi->manufacturer_id);
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_BUID_ID_OFFSET,
			rmi->build_id,
			sizeof(rmi->build_id));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s:%4d read fail[%d]\n",
					__func__, __LINE__, retval);
		return retval;
	}

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
					fhandler->intr_mask;
			}

			/* To display each fhandler data */
			dev_info(&rmi4_data->i2c_client->dev, "%s: F%02x : NUM_SOURCE[%02X] NUM_INT_REG[%02X] INT_MASK[%02X]\n",
					__func__, fhandler->fn_number,
					fhandler->num_of_data_sources, fhandler->intr_reg_num,
					fhandler->intr_mask);
		}
	}

	/* Enable the interrupt sources */
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_info(&rmi4_data->i2c_client->dev,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev, "%s:%4d write fail[%d]\n",
					__func__, __LINE__, retval);
				return retval;
			}
		}
	}

	synaptics_rmi4_set_configured(rmi4_data);

	return 0;
}

static void synaptics_rmi4_release_support_fn(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler, *n;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (list_empty(&rmi->support_fn_list)) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: support_fn_list is empty\n",
				__func__);
		goto out;
	}

	list_for_each_entry_safe(fhandler, n, &rmi->support_fn_list, link) {
		dev_dbg(&rmi4_data->i2c_client->dev, "%s: fn_number = %x\n",
				__func__, fhandler->fn_number);
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
			synaptics_rmi4_f1a_kfree(fhandler);
		else
			kfree(fhandler->data);

		kfree(fhandler);
	}

out:
#ifdef PROXIMITY
	kfree(f51);
	f51 = NULL;
#endif
}

static int synaptics_rmi4_set_input_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	int temp;
	unsigned char ii;
	struct synaptics_rmi4_f1a_handle *f1a;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	rmi4_data->input_dev = input_allocate_device();
	if (rmi4_data->input_dev == NULL) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to allocate input device\n",
				__func__);
		retval = -ENOMEM;
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	if (rmi4_data->has_edge_swipe) {
		rmi4_data->max_touch_width *= EDGE_SWIPE_WIDTH_SCALING_FACTOR;
		rmi4_data->num_of_fingers++; /* extra finger for edge swipe */
	}

	rmi4_data->input_dev->name = "sec_touchscreen";
	rmi4_data->input_dev->id.bustype = BUS_I2C;
	rmi4_data->input_dev->dev.parent = &rmi4_data->i2c_client->dev;
#ifdef USE_OPEN_CLOSE
	rmi4_data->input_dev->open = synaptics_rmi4_input_open;
	rmi4_data->input_dev->close = synaptics_rmi4_input_close;
#endif
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);
#ifdef GLOVE_MODE
	input_set_capability(rmi4_data->input_dev, EV_SW, SW_GLOVE);
#endif
#ifdef HAND_GRIP_MODE
	input_set_capability(rmi4_data->input_dev, EV_SW, SW_LEFT_HAND);
	input_set_capability(rmi4_data->input_dev, EV_SW, SW_RIGHT_HAND);
	input_set_capability(rmi4_data->input_dev, EV_SW, SW_BOTH_HAND);
#endif

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);
	set_bit(EV_KEY, rmi4_data->input_dev->evbit);
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);
	set_bit(BTN_TOUCH, rmi4_data->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, rmi4_data->input_dev->keybit);
	set_bit(EV_LED, rmi4_data->input_dev->evbit);
	set_bit(LED_MISC, rmi4_data->input_dev->ledbit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif

	if (rmi4_data->board->swap_axes) {
		temp = rmi4_data->sensor_max_x;
		rmi4_data->sensor_max_x = rmi4_data->sensor_max_y;
		rmi4_data->sensor_max_y = temp;
	}

	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_X, 0,
			rmi4_data->board->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_Y, 0,
			rmi4_data->board->sensor_max_y, 0, 0);
#ifdef REPORT_2D_W
#ifdef EDGE_SWIPE
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			EDGE_SWIPE_WIDTH_MAX, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			EDGE_SWIPE_WIDTH_MAX, 0, 0);
#else
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			rmi4_data->board->max_width, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			rmi4_data->board->max_width, 0, 0);
#endif
#endif

#ifdef PROXIMITY
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_DISTANCE, 0,
			HOVER_Z_MAX, 0, 0);
#ifdef EDGE_SWIPE
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_WIDTH_MAJOR, 0,
			EDGE_SWIPE_WIDTH_MAX, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_ANGLE, 0,
			EDGE_SWIPE_ANGLE_MAX, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PALM, 0,
			EDGE_SWIPE_PALM_MAX, 0, 0);
#endif
	setup_timer(&rmi4_data->f51_finger_timer,
			synaptics_rmi4_f51_finger_timer,
			(unsigned long)rmi4_data);
#endif

	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers);

	f1a = NULL;
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
				f1a = fhandler->data;
		}
	}

	if (f1a) {
		for (ii = 0; ii < f1a->valid_button_count; ii++) {
			set_bit(f1a->button_map[ii],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, f1a->button_map[ii]);
		}
	}

	retval = input_register_device(rmi4_data->input_dev);
	if (retval) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register input device\n",
				__func__);
		goto err_register_input;
	}

	return 0;

err_register_input:
	input_free_device(rmi4_data->input_dev);

err_query_device:
	synaptics_rmi4_release_support_fn(rmi4_data);

err_input_device:
	return retval;
}

static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char ii;

	for (ii = 0; ii < rmi4_data->num_of_fingers; ii++) {
		input_mt_slot(rmi4_data->input_dev, ii);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 0);
	}

	input_report_key(rmi4_data->input_dev,
			BTN_TOUCH, 0);
	input_report_key(rmi4_data->input_dev,
			BTN_TOOL_FINGER, 0);
#ifdef GLOVE_MODE
	input_report_switch(rmi4_data->input_dev,
			SW_GLOVE, false);
	rmi4_data->touchkey_glove_mode_status = false;
#endif
	input_sync(rmi4_data->input_dev);

	rmi4_data->fingers_on_2d = false;
#ifdef PROXIMITY
	rmi4_data->f51_finger = false;
#endif

#ifdef TSP_BOOSTER
	synaptics_set_dvfs_lock(rmi4_data, -1);
#endif
#ifdef USE_HOVER_REZERO
	cancel_delayed_work(&rmi4_data->rezero_work);
#endif
	return 0;
}

static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii = 0;
	unsigned char f51_general_control;
	unsigned short intr_addr;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	synaptics_rmi4_free_fingers(rmi4_data);

	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->fn_number == SYNAPTICS_RMI4_F12) {
				synaptics_rmi4_f12_set_feature(rmi4_data);
				synaptics_rmi4_f12_set_report(rmi4_data);
				break;
			}
		}
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			f51->general_control_addr,
			&f51_general_control, sizeof(f51_general_control));

	/* Read the Hsync's status */
	dev_info(&rmi4_data->i2c_client->dev, "%s: Hsync [%s[0x%x]]\n", __func__,
		(f51_general_control & HSYNC_STATUS) ? "GD" : "NG", f51_general_control);

	/* general control register
	 * register bit	: 1 active	: 0 active
	 * HOST ID		: I2C(RMI)	: INT(GPIO)
	 * HSYNC status	: GOOD		: NG
	 */
	if (rmi4_data->ta_con_mode)
		f51_general_control |= 0x80;	/* set default I2C(RMI) */
	else
		f51_general_control &= ~0x80;	/* set default INT(GPIO) */

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			f51->general_control_addr,
			&f51_general_control, sizeof(f51_general_control));

#ifdef PROXIMITY
	retval = synaptics_rmi4_f51_set_enables(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: f51_set_enables fail[%d]\n",
					__func__, retval);
		goto exit;
	}
#endif
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_info(&rmi4_data->i2c_client->dev,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev, "%s: write fail[%d]\n",
					__func__, retval);
				goto exit;
			}
		}
	}

	synaptics_rmi4_set_configured(rmi4_data);

	retval = 0;

exit:
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));
	return retval;
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char command = 0x01;

	mutex_lock(&(rmi4_data->rmi4_reset_mutex));

	disable_irq(rmi4_data->i2c_client->irq);

	synaptics_rmi4_free_fingers(rmi4_data);

	if (!rmi4_data->stay_awake) {
		retval = synaptics_rmi4_i2c_write(rmi4_data,
				rmi4_data->f01_cmd_base_addr,
				&command,
				sizeof(command));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to issue reset command, error = %d\n",
					__func__, retval);
			goto out;
		}

		msleep(rmi4_data->board->reset_delay_ms);

	} else {
		synaptics_power_ctrl(rmi4_data,false);

		msleep(30);
		synaptics_power_ctrl(rmi4_data,true);

		msleep(SYNAPTICS_HW_RESET_TIME);

		if (rmi4_data->firmware_cracked) {
			synaptics_rmi4_remove_exp_fn(rmi4_data);
			dev_info(&rmi4_data->i2c_client->dev,
						"%s: removed exp_func..\n",
						__func__);

			retval = synaptics_rmi4_init_exp_fn(rmi4_data);
			if (retval < 0)
				dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to init_exp_fn at cracked fw reflash.\n",
						__func__);

			rmi4_data->firmware_cracked = false;
		}

		retval = synaptics_rmi4_f54_set_control(rmi4_data);
		if (retval < 0)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to set f54 control\n",
					__func__);
	}		

	synaptics_rmi4_release_support_fn(rmi4_data);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to query device\n",
				__func__);
	}

out:
	enable_irq(rmi4_data->i2c_client->irq);
	mutex_unlock(&(rmi4_data->rmi4_reset_mutex));

	return 0;
}

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
extern void synaptics_tsp_register_callback(struct synaptics_rmi_callbacks *cb);
static void synaptics_charger_conn(struct synaptics_rmi4_data *rmi4_data,
		int ta_status)
{
	int retval;
	unsigned char charger_connected;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&charger_connected,
			sizeof(charger_connected));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to set configured\n",
				__func__);
		return;
	}

	if (ta_status == 0x01 || ta_status == 0x03)
		charger_connected |= CHARGER_CONNECTED;
	else
		charger_connected &= CHARGER_DISCONNECTED;

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&charger_connected,
			sizeof(charger_connected));

	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to set configured\n",
				__func__);
	}

	dev_info(&rmi4_data->i2c_client->dev,
			"%s: device_control : %x, ta_status : %x\n",
			__func__, charger_connected, ta_status);

}

static void synaptics_ta_cb(struct synaptics_rmi_callbacks *cb, int ta_status)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(cb, struct synaptics_rmi4_data, callbacks);

	dev_info(&rmi4_data->i2c_client->dev,
			"%s: ta_status : %x\n", __func__, ta_status);

	rmi4_data->ta_status = ta_status;

	/* if do not completed driver loading, ta_cb will not run. */
#ifdef TSP_INIT_COMPLETE
	if (!rmi4_data->init_done.done) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: until driver loading done.\n",
				__func__);
		return;
	}
#endif
	if (rmi4_data->touch_stopped || rmi4_data->doing_reflash) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: device is in suspend state or reflash.\n",
				__func__);
		return;
	}

	synaptics_charger_conn(rmi4_data, ta_status);

}
#endif

#ifdef PROXIMITY
static void synaptics_rmi4_f51_finger_timer(unsigned long data)
{
	struct synaptics_rmi4_data *rmi4_data =
		(struct synaptics_rmi4_data *)data;

	if (rmi4_data->f51_finger) {
		rmi4_data->f51_finger = false;
		mod_timer(&rmi4_data->f51_finger_timer,
				jiffies + msecs_to_jiffies(F51_FINGER_TIMEOUT));
	} else if (!rmi4_data->fingers_on_2d) {
		input_mt_slot(rmi4_data->input_dev, 0);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
		input_sync(rmi4_data->input_dev);
	}

	return;
}
#endif

static void synaptics_rmi4_remove_exp_fn(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler, *n;

	if (list_empty(&exp_fn_list)) {
		dev_err(&rmi4_data->i2c_client->dev, "%s: exp_fn_list empty\n",
				__func__);
		return;
	}

	list_for_each_entry_safe(exp_fhandler, n, &exp_fn_list, link) {
		if (exp_fhandler->initialized &&
				(exp_fhandler->func_remove != NULL)) {
			dev_dbg(&rmi4_data->i2c_client->dev, "%s: [%d]\n",
					__func__, exp_fhandler->fn_type);
			exp_fhandler->func_remove(rmi4_data);
		}
		list_del(&exp_fhandler->link);
		kfree(exp_fhandler);
	}
}

/*
 *	RMI_DEV = 0
 *	RMI_F54 = 1
 *	RMI_FW_UPDATER = 2
 *	RMI_DB = 3
 */
static int synaptics_rmi4_init_exp_fn(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	INIT_LIST_HEAD(&exp_fn_list);

	retval = rmidev_module_register();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register rmidev module\n",
				__func__);
		if (rmi4_data->firmware_cracked)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: ffirmware_cracked, skip exit routine(rmidev)\n",
					__func__);
		else
			goto error_exit;
	}

	retval = rmi4_f54_module_register();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register f54 module\n",
				__func__);
		if (rmi4_data->firmware_cracked)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: firmware_cracked, skip exit routine(f54)\n",
					__func__);
		else
			goto error_exit;
	}

	retval = rmi4_fw_update_module_register();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register fw update module\n",
				__func__);
		if (rmi4_data->firmware_cracked)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: firmware_cracked, skip exit routine(fw)\n",
					__func__);
		else
			goto error_exit;
	}

	retval = rmidb_module_register();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register rmidb module\n",
				__func__);
		if (rmi4_data->firmware_cracked)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: firmware_cracked, skip exit routine(rmidb)\n",
					__func__);
		else
			goto error_exit;

	}

	if (list_empty(&exp_fn_list))
		return -ENODEV;

	list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
		if (exp_fhandler->func_init != NULL) {
			dev_dbg(&rmi4_data->i2c_client->dev, "%s: run [%d]'s init function\n",
					__func__, exp_fhandler->fn_type);
			retval = exp_fhandler->func_init(rmi4_data);
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to init exp fn\n",
						__func__);
				if (rmi4_data->firmware_cracked)
					dev_err(&rmi4_data->i2c_client->dev,
							"%s: firmware_cracked, list add exp_func.\n",
							__func__);
				else
					goto error_exit;
			} else {
				exp_fhandler->initialized = true;
			}
		}
	}

	return 0;

error_exit:
	synaptics_rmi4_remove_exp_fn(rmi4_data);

	return retval;
}

/**
 * synaptics_rmi4_new_function()
 *
 * Called by other expansion Function modules in their module init and
 * module exit functions.
 *
 * This function is used by other expansion Function modules such as
 * rmi_dev to register themselves with the driver by providing their
 * initialization and removal callback function pointers so that they
 * can be inserted or removed dynamically at module init and exit times,
 * respectively.
 */
int synaptics_rmi4_new_function(enum exp_fn fn_type,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask))
{
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
	if (!exp_fhandler) {
		pr_err("%s: Failed to alloc mem for expansion function\n",
				__func__);
		return -ENOMEM;
	}
	exp_fhandler->fn_type = fn_type;
	exp_fhandler->func_init = func_init;
	exp_fhandler->func_attn = func_attn;
	exp_fhandler->func_remove = func_remove;
	list_add_tail(&exp_fhandler->link, &exp_fn_list);

	return 0;
}


void synaptics_power_ctrl(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	int ret, retval = 0;
	static struct regulator *reg_l10;

	if (!reg_l10) {
		reg_l10 = regulator_get(NULL, "8941_l10");
		if (IS_ERR(reg_l10)) {
			pr_err("%s: could not get 8917_l10, rc = %ld\n",
					__func__, PTR_ERR(reg_l10));
			return;
		}
		ret = regulator_set_voltage(reg_l10, 1800000, 1800000);
		if (ret) {
			pr_err("%s: unable to set l10 voltage to 1.8V\n",
					__func__);
			return;
		}
	}

	ret = gpio_direction_output(rmi4_data->board->vdd_io_1p8, enable);
	if (ret) {
		pr_err("[TSP] %s: unable to set_direction for TSP_EN [%d]\n",
				__func__, rmi4_data->board->vdd_io_1p8);
	}

	if (enable) {
		if (regulator_is_enabled(reg_l10))
			pr_err(	"%s: L10(1.8V) is enabled\n", __func__);
		else
			ret = regulator_enable(reg_l10);
		if (ret) {
			pr_err("%s: enable l10 failed, rc=%d\n",
					__func__, ret);
			return;
		}
		retval = qpnp_pin_config(rmi4_data->board->tsp_int,
				&synaptics_int_set[SYNAPTICS_PM_GPIO_STATE_WAKE]);
		if (retval < 0)
			dev_info(&rmi4_data->i2c_client->dev,
				"%s: wakeup int config return: %d\n",
					__func__, retval);
		pr_info("%s: tsp 1.8V on is finished.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l10))
			ret = regulator_disable(reg_l10);
		else
			pr_err("%s: L10(1.8V) is disabled\n", __func__);
		if (ret) {
			pr_err("%s: disable l10 failed, rc=%d\n",
					__func__, ret);
			return;
		}
		retval = qpnp_pin_config(rmi4_data->board->tsp_int,
				&synaptics_int_set[SYNAPTICS_PM_GPIO_STATE_SLEEP]);
		if (retval < 0)
			dev_info(&rmi4_data->i2c_client->dev,
				"%s: sleep int config return: %d\n",
					__func__, retval);
		pr_info("%s: tsp 1.8V off is finished.\n", __func__);
	}
	pr_err("[synaptics] %s   enable(%d)\n", __func__,enable);
	return;
}

/**
 * synaptics_rmi4_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, handles
 * the registration of the early_suspend and late_resume functions,
 * and creates a work queue for detection of other expansion Function
 * modules.
 */
static int __devinit synaptics_rmi4_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;
	unsigned char attr_count;
	int attr_count_num;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_device_info *rmi;
	struct synaptics_rmi4_platform_data *pdata;

	dev_err(&client->dev, "%s start.\n", __func__);
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data not supported\n",
				__func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_rmi4_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		retval = synaptics_parse_dt(&client->dev, pdata);
		if (retval)
			return retval;
	} else	{
		pdata = client->dev.platform_data;
		printk(KERN_ERR "TSP failed to align dtsi %s",__func__);
	}

	if (!pdata) {
		dev_err(&client->dev,
				"%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}
	synaptics_request_gpio(pdata);

	rmi4_data = kzalloc(sizeof(*rmi4_data), GFP_KERNEL);
	if (!rmi4_data) {
		dev_err(&client->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	rmi = &(rmi4_data->rmi4_mod_info);

	client->irq = gpio_to_irq(pdata->tsp_int);
	rmi4_data->i2c_client = client;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->board = pdata;

	rmi4_data->touch_stopped = false;
	rmi4_data->sensor_sleep = false;
	rmi4_data->irq_enabled = false;
	rmi4_data->fingers_on_2d = false;
	rmi4_data->hand_edge_down = false;

	rmi4_data->i2c_read = synaptics_rmi4_i2c_read;
	rmi4_data->i2c_write = synaptics_rmi4_i2c_write;
	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;
	rmi4_data->reset_device = synaptics_rmi4_reset_device;
	rmi4_data->stop_device = synaptics_rmi4_stop_device;
	rmi4_data->start_device = synaptics_rmi4_start_device;
	rmi4_data->irq = client->irq;
	pr_err("%s: tsp : gpio_to_irq : %d\n", __func__, client->irq);

#ifdef READ_LCD_ID
	rmi4_data->lcd_id = synaptics_lcd_id;
#endif

	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->rmi4_reset_mutex));
	mutex_init(&(rmi4_data->rmi4_reflash_mutex));
	mutex_init(&(rmi4_data->rmi4_device_mutex));
#ifdef TSP_INIT_COMPLETE
	init_completion(&rmi4_data->init_done);
#endif

#ifdef USE_OPEN_DWORK
	INIT_DELAYED_WORK(&rmi4_data->open_work, synaptics_rmi4_open_work);
#endif

#ifdef USE_HOVER_REZERO
	INIT_DELAYED_WORK(&rmi4_data->rezero_work, synaptics_rmi4_rezero_work);
#endif
	i2c_set_clientdata(client, rmi4_data);

err_tsp_reboot:
	synaptics_power_ctrl(rmi4_data,true);
	msleep(SYNAPTICS_POWER_MARGIN_TIME);

#ifdef TSP_BOOSTER
	synaptics_init_dvfs(rmi4_data);
#endif

	retval = synaptics_rmi4_set_input_device(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to set up input device\n",
				__func__);

		if((retval == TSP_NEEDTO_REBOOT) && (rmi4_data->rebootcount < MAX_TSP_REBOOT)) {
			synaptics_power_ctrl(rmi4_data,false);
			msleep(SYNAPTICS_POWER_MARGIN_TIME);
			rmi4_data->rebootcount++;
			dev_err(&client->dev,
					"%s: reboot sequence by i2c fail\n",
					__func__);
			goto err_tsp_reboot;
		}
		else goto err_set_input_device;
	}

	retval = synaptics_rmi4_init_exp_fn(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to register rmidev module\n",
				__func__);
		goto err_init_exp_fn;
	}

	retval = synaptics_rmi4_irq_enable(rmi4_data, true);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to enable attention interrupt\n",
				__func__);
		goto err_enable_irq;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto err_sysfs;
		}
	}

	if (rmi4_data->ic_version == SYNAPTICS_PRODUCT_ID_S5050) {
		if (strncmp(rmi4_data->board->project_name, "H", 1) == 0) {
			if (rmi4_data->lcd_id == 0x03)
				rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5050_NONTR;
			else
				rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5050;
		} else if (strncmp(rmi4_data->board->project_name, "F", 1) == 0) {
			if(system_rev <= 6)
				rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5050_F;
			else
				rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5050_HSYNCF;	
		} else if (strncmp(rmi4_data->board->project_name, "J", 1) == 0) {
				rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5050_J;
		}
	else
		rmi4_data->board->firmware_name = FW_IMAGE_NAME_NONE;
	} else if (rmi4_data->ic_version == SYNAPTICS_PRODUCT_ID_S5000) {
		rmi4_data->board->firmware_name = FW_IMAGE_NAME_S5000;
	} else {
		rmi4_data->board->firmware_name = FW_IMAGE_NAME_NONE;
	}

	retval = synaptics_rmi4_fw_update_on_probe(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev, "%s: Failed to firmware update\n",
				__func__);
		goto err_fw_update;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	rmi4_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN -2;
	rmi4_data->early_suspend.suspend = synaptics_rmi4_early_suspend;
	rmi4_data->early_suspend.resume = synaptics_rmi4_late_resume;
	register_early_suspend(&rmi4_data->early_suspend);
#endif

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	synaptics_rmi_select_ta_mode(rmi4_data);
	if (rmi4_data->ta_con_mode) {
		rmi4_data->register_cb = synaptics_tsp_register_callback;

		rmi4_data->callbacks.inform_charger = synaptics_ta_cb;
		if (rmi4_data->register_cb){
			dev_err(&client->dev, "[TSP] Register TA Callback\n");
			rmi4_data->register_cb(&rmi4_data->callbacks);
		}
	}
#endif

	/* for blocking to be excuted open function until probing */
	rmi4_data->tsp_probe = true;
#ifdef TSP_INIT_COMPLETE
	complete_all(&rmi4_data->init_done);

#endif
	return retval;

err_fw_update:
err_sysfs:
	attr_count_num = (int)attr_count;
	for (attr_count_num--; attr_count_num >= 0; attr_count_num--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}
	synaptics_rmi4_irq_enable(rmi4_data, false);

err_enable_irq:
	synaptics_rmi4_remove_exp_fn(rmi4_data);

err_init_exp_fn:
	synaptics_rmi4_release_support_fn(rmi4_data);
#ifdef USE_OPEN_CLOSE
	rmi4_data->input_dev->open = NULL;
	rmi4_data->input_dev->close = NULL;
#endif
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
err_set_input_device:
	synaptics_power_ctrl(rmi4_data,false);
#ifdef TSP_INIT_COMPLETE
	complete_all(&rmi4_data->init_done);
#endif
	kfree(rmi4_data);

	return retval;
}

/**
 * synaptics_rmi4_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 * This funtion terminates the work queue, stops sensor data acquisition,
 * frees the interrupt, unregisters the driver from the input subsystem,
 * turns off the power to the sensor, and frees other allocated resources.
 */
static int __devexit synaptics_rmi4_remove(struct i2c_client *client)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = i2c_get_clientdata(client);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif
	synaptics_rmi4_irq_enable(rmi4_data, false);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	input_unregister_device(rmi4_data->input_dev);

	synaptics_rmi4_remove_exp_fn(rmi4_data);

	synaptics_power_ctrl(rmi4_data,false);
	rmi4_data->touch_stopped = true;

	synaptics_rmi4_release_support_fn(rmi4_data);

	input_free_device(rmi4_data->input_dev);

	kfree(rmi4_data);

	return 0;
}

#ifdef USE_SENSOR_SLEEP
/**
 * synaptics_rmi4_sensor_sleep()
 *
 * Called by synaptics_rmi4_early_suspend() and synaptics_rmi4_suspend().
 *
 * This function stops finger data acquisition and puts the sensor to sleep.
 */
static void synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | SENSOR_SLEEP);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	} else {
		rmi4_data->sensor_sleep = true;
	}

	return;
}

/**
 * synaptics_rmi4_sensor_wake()
 *
 * Called by synaptics_rmi4_resume() and synaptics_rmi4_late_resume().
 *
 * This function wakes the sensor from sleep.
 */
static void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	}

	device_ctrl = (device_ctrl & ~MASK_3BIT);
	device_ctrl = (device_ctrl | NO_SLEEP_OFF | NORMAL_OPERATION);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->i2c_client->dev),
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	} else {
		rmi4_data->sensor_sleep = false;
	}

	return;
}
#endif

static int synaptics_rmi4_stop_device(struct synaptics_rmi4_data *rmi4_data)
{
	mutex_lock(&rmi4_data->rmi4_device_mutex);

	if (rmi4_data->touch_stopped) {
		dev_err(&rmi4_data->i2c_client->dev, "%s already power off\n",
				__func__);
		goto out;
	}

	disable_irq(rmi4_data->i2c_client->irq);
	synaptics_rmi4_free_fingers(rmi4_data);
	rmi4_data->touch_stopped = true;
	synaptics_power_ctrl(rmi4_data,false);

	dev_dbg(&rmi4_data->i2c_client->dev, "%s\n", __func__);

out:
	mutex_unlock(&rmi4_data->rmi4_device_mutex);
	return 0;
}

static int synaptics_rmi4_start_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval = 0;
	mutex_lock(&rmi4_data->rmi4_device_mutex);

	if (!rmi4_data->touch_stopped) {
		dev_err(&rmi4_data->i2c_client->dev, "%s already power on\n",
				__func__);
		goto out;
	}

	synaptics_power_ctrl(rmi4_data,true);		
	rmi4_data->touch_stopped = false;

	msleep(SYNAPTICS_HW_RESET_TIME);

#if defined(CONFIG_SEC_FACTORY)
	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to query device\n",
			__func__);
	}
#else
	retval = synaptics_rmi4_reinit_device(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to reinit device\n",
			__func__);
	}
#endif

	if (rmi4_data->ta_con_mode)
		synaptics_charger_conn(rmi4_data, rmi4_data->ta_status);

	enable_irq(rmi4_data->i2c_client->irq);

	dev_dbg(&rmi4_data->i2c_client->dev, "%s\n", __func__);

out:
	mutex_unlock(&rmi4_data->rmi4_device_mutex);
	return retval;
}

#ifdef USE_OPEN_CLOSE
#ifdef USE_OPEN_DWORK
static void synaptics_rmi4_open_work(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data =
		container_of(work, struct synaptics_rmi4_data,
				open_work.work);

	retval = synaptics_rmi4_start_device(rmi4_data);
	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to start device\n", __func__);
}
#endif

static int synaptics_rmi4_input_open(struct input_dev *dev)
{
	struct synaptics_rmi4_data *rmi4_data = input_get_drvdata(dev);
	int retval;

#ifdef TSP_INIT_COMPLETE
	retval = wait_for_completion_interruptible_timeout(&rmi4_data->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"error while waiting for device to init (%d)\n", retval);
		retval = -ENXIO;
		goto err_open;
	}
	if (retval == 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"timedout while waiting for device to init\n");
		retval = -ENXIO;
		goto err_open;
	}
#endif

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	schedule_delayed_work(&rmi4_data->open_work,
			msecs_to_jiffies(TOUCH_OPEN_DWORK_TIME));
#else
	retval = synaptics_rmi4_start_device(rmi4_data);
	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to start device\n", __func__);
#endif

	return 0;

#ifdef TSP_INIT_COMPLETE
err_open:
	return retval;
#endif
}

static void synaptics_rmi4_input_close(struct input_dev *dev)
{
	struct synaptics_rmi4_data *rmi4_data = input_get_drvdata(dev);

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	cancel_delayed_work(&rmi4_data->open_work);
#endif
	synaptics_rmi4_stop_device(rmi4_data);
}
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
#define synaptics_rmi4_suspend NULL
#define synaptics_rmi4_resume NULL

/**
 * synaptics_rmi4_early_suspend()
 *
 * Called by the kernel during the early suspend phase when the system
 * enters suspend.
 *
 * This function calls synaptics_rmi4_sensor_sleep() to stop finger
 * data acquisition and put the sensor to sleep.
 */
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(h, struct synaptics_rmi4_data,
				early_suspend);

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	if (rmi4_data->stay_awake) {
		rmi4_data->staying_awake = true;
		dev_info(&rmi4_data->i2c_client->dev, "%s : return due to staying_awake\n",
				__func__);
		return;
	} else {
		rmi4_data->staying_awake = false;
	}

	synaptics_rmi4_stop_device(rmi4_data);

	return;
}

/**
 * synaptics_rmi4_late_resume()
 *
 * Called by the kernel during the late resume phase when the system
 * wakes up from suspend.
 *
 * This function goes through the sensor wake process if the system wakes
 * up from early suspend (without going into suspend).
 */
static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
	int retval = 0;
	struct synaptics_rmi4_data *rmi4_data =
		container_of(h, struct synaptics_rmi4_data,
				early_suspend);

	dev_info(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	if (rmi4_data->staying_awake) {
		dev_info(&rmi4_data->i2c_client->dev, "%s : return due to staying_awake\n",
				__func__);
		return;
	}

	retval = synaptics_rmi4_start_device(rmi4_data);
	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to start device\n", __func__);

	return;
}
#else

/**
 * synaptics_rmi4_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep (if not already done so during the early suspend phase),
 * disables the interrupt, and turns off the power to the sensor.
 */
static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	dev_dbg(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	if (rmi4_data->staying_awake) {
		dev_info(&rmi4_data->i2c_client->dev, "%s : return due to staying_awake\n",
				__func__);
		return 0;
	}

	mutex_lock(&rmi4_data->input_dev->mutex);

	if (rmi4_data->input_dev->users)
		synaptics_rmi4_stop_device(rmi4_data);

	mutex_unlock(&rmi4_data->input_dev->mutex);

	return 0;
}

/**
 * synaptics_rmi4_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 * This function turns on the power to the sensor, wakes the sensor
 * from sleep, enables the interrupt, and starts finger data
 * acquisition.
 */
static int synaptics_rmi4_resume(struct device *dev)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	dev_dbg(&rmi4_data->i2c_client->dev, "%s\n", __func__);

	mutex_lock(&rmi4_data->input_dev->mutex);

	if (rmi4_data->input_dev->users) {
		retval = synaptics_rmi4_start_device(rmi4_data);
		if (retval < 0)
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to start device\n", __func__);
	}

	mutex_unlock(&rmi4_data->input_dev->mutex);

	return 0;
}
#endif

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
	.suspend = synaptics_rmi4_suspend,
	.resume  = synaptics_rmi4_resume,
};
#endif

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);


#ifdef CONFIG_OF
static struct of_device_id synaptics_match_table[] = {
	{ .compatible = "synaptics,rmi4-ts",},
	{ },
};
#else
#define synaptics_match_table	NULL
#endif

static struct i2c_driver synaptics_rmi4_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = synaptics_match_table,
#ifdef CONFIG_PM
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = __devexit_p(synaptics_rmi4_remove),
	.id_table = synaptics_rmi4_id_table,
};

/**
 * synaptics_rmi4_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init synaptics_rmi4_init(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_notice("%s : LPM Charging Mode!!\n", __func__);
		return 0;
	}
#endif
	return i2c_add_driver(&synaptics_rmi4_driver);
}

/**
 * synaptics_rmi4_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit synaptics_rmi4_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_driver);
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics RMI4 I2C Touch Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SYNAPTICS_RMI4_DRIVER_VERSION);
