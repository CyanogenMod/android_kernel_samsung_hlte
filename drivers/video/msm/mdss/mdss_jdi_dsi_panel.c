/* Copyright (c) 2012, Samsung Electronics. All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/lcd.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "mdss_dsi.h"
#include "jdi_dsi_panel.h"
#include "mdss_fb.h"

#if defined(CONFIG_MDNIE_LITE_TUNING)
#include "mdnie_lite_tuning.h"
#endif

#include <linux/gpio.h>
#include <linux/irq.h>

#define DDI_VIDEO_ENHANCE_TUNING
#if defined(DDI_VIDEO_ENHANCE_TUNING)
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#endif

#include <asm/system_info.h>

#define DT_CMD_HDR 6

static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;

static struct dsi_cmd display_qcom_on_cmds;
static struct dsi_cmd display_qcom_off_cmds;
static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_cmd;
static struct dsi_cmd display_blank_cmd;
static struct dsi_cmd display_unblank_cmd;
static struct dsi_cmd late_on_cmds;
static struct dsi_cmd early_off_cmds;
static struct dsi_cmd brightness_cmds;
static struct dsi_cmd mtp_enable_cmds;
static struct dsi_cmd mtp_disable_cmds;
static struct dsi_cmd cabc_on_cmds;
static struct dsi_cmd cabc_off_cmds;
static struct dsi_cmd hsync_on_cmds;
static struct dsi_cmd hsync_off_cmds;
static struct dsi_cmd manufacture_id_cmds;
static struct candella_lux_map candela_map_table;

static struct mipi_samsung_driver_data msd;
/*List of supported Panels with HW revision detail
 * (one structure per project)
 * {hw_rev,"label string given in panel dtsi file"}
 * */
static struct  panel_hrev panel_supp_cdp[]= {
	{"JDI_ACX454AKM", PANEL_TFT_FHD_S6D6FA0},
	{NULL}
};

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_dsi_phy_ctrl phy_params;

char board_rev;

static int lcd_attached = 1;

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct work_struct  err_fg_work;
static int err_fg_gpio;	/* PM_GPIO4 */
static int esd_count;
static int err_fg_working;
#define ESD_DEBUG
#endif

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock);

extern void mdss_dsi_panel_touchsensing(int enable);
int get_lcd_attached(void);

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
			ctrl->pwm_pmic_gpio);
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
		if (ret) {
			pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
					ctrl->pwm_pmic_gpio);
			return;
		}

		ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
		if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
				pr_err("%s: lpg_chan=%d pwm request failed", __func__,
					ctrl->pwm_lpg_chan);
				gpio_free(ctrl->pwm_pmic_gpio);
				ctrl->pwm_pmic_gpio = -1;
		}
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_pmic_gpio=%d pwm_lpg_chan=%d\n",
		__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
		ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
		ctrl->ndx, level, duty);

	ret = pwm_config(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(ctrl->pwm_bl);
	if (ret)
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

static void dcs_read_cb(u32 data)
{
	pr_info("%s: bklt_ctrl=%x\n", __func__, data);
}

u32 mdss_dsi_dcs_read(struct mdss_dsi_ctrl_pdata *ctrl,
			char cmd0, char cmd1)
{
	struct dcs_cmd_req cmdreq;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = 1;
	cmdreq.cb = dcs_read_cb; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, untill call back called
	 */
	return 0;
}

void mdss_dsi_samsung_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);


	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, enable line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}


	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		udelay(200);
		wmb();
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		wmb();
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
#if defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
			gpio_direction_output((ctrl_pdata->disp_en_gpio), 1);
			pr_info("%s disp_en on: %d\n", __func__, gpio_get_value(ctrl_pdata->disp_en_gpio));
#else
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
			wmb();
#endif
		}
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
		}
	}
}

static int get_candela_value(int bl_level)
{
	return candela_map_table.lux_tab[candela_map_table.bkl[bl_level]];
}

static char get_pwm_value(int bl_level)
{
	return candela_map_table.pwm_tab[candela_map_table.bkl[bl_level]];
}
static int get_cmd_idx(int bl_level)
{
	return candela_map_table.cmd_idx[candela_map_table.bkl[bl_level]];
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static ssize_t mipi_samsung_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int rc;

	if (unlikely(!mfd))
		return -ENODEV;
	if (unlikely(mfd->key != MFD_KEY))
		return -EINVAL;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", mfd->panel_power_on);
	pr_info("mipi_samsung_disp_get_power(%d)\n", mfd->panel_power_on);

	return rc;
}

static ssize_t mipi_samsung_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int power;
	struct msm_fb_data_type *mfd = msd.mfd;

	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;

	if (power == mfd->panel_power_on)
		return 0;

	if (power) {
		mfd->fbi->fbops->fb_blank(FB_BLANK_UNBLANK, mfd->fbi);
		mfd->fbi->fbops->fb_pan_display(&mfd->fbi->var, mfd->fbi);
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");

	return size;
}

static ssize_t mipi_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[MAX_PANEL_NAME_SIZE];
	snprintf(temp, strnlen(msd.panel_name, MAX_PANEL_NAME_SIZE), "%s\n", msd.panel_name);
	strlcat(buf, temp, MAX_PANEL_NAME_SIZE);
	return strnlen(buf, MAX_PANEL_NAME_SIZE);
}

static ssize_t mipi_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp[15];
	int id1, id2, id3;
	id1 = (msd.manufacture_id & 0x00FF0000) >> 16;
	id2 = (msd.manufacture_id & 0x0000FF00) >> 8;
	id3 = msd.manufacture_id & 0xFF;

	snprintf(temp, sizeof(temp), "%x %x %x\n",	id1, id2, id3);
	strlcat(buf, temp, 15);
	return strnlen(buf, 15);
}

static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

static int make_brightcontrol_set(int bl_level)
{
	int cd_idx = get_cmd_idx(bl_level);
	int cd_level = get_candela_value(bl_level);
	char backlight_pwm = get_pwm_value(bl_level);

	brightness_cmds.cmd_desc[0].payload[1] = backlight_pwm;

	LCD_DEBUG("bright_level: %d, candela_idx: %d( %d cd ), pwm set: 0x%x\n",\
		msd.dstat.bright_level, cd_idx, cd_level, backlight_pwm);

	return brightness_cmds.num_of_cmds;
}

static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int	acl_set;

	acl_set = msd.dstat.acl_on;
	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (mfd->panel_power_on) {
		if (acl_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.acl_on = true;
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			mipi_samsung_disp_send_cmd(PANEL_ACL_ON, true);
		} else if (!acl_set && msd.dstat.acl_on && !msd.dstat.siop_status) {
			msd.dstat.acl_on = false;
			msd.dstat.curr_acl_idx = -1;
			pr_info("%s: acl off : acl %d, siop %d", __func__,
					msd.dstat.acl_on, msd.dstat.siop_status);
			mipi_samsung_disp_send_cmd(PANEL_ACL_OFF, true);
		} else {
			msd.dstat.acl_on = acl_set;
			pr_info("%s: skip but acl update!! acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);
		}
	}else {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		msd.dstat.acl_on = acl_set;
	}

	return size;
}

static ssize_t mipi_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.siop_status);
	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;
	int siop_set;

	siop_set = msd.dstat.siop_status;
	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (mfd->panel_power_on) {
		if (siop_set && !(msd.dstat.acl_on||msd.dstat.siop_status)) {
			msd.dstat.siop_status = true;
			mipi_samsung_disp_send_cmd(PANEL_ACL_ON, true);
			pr_info("%s: acl on  : acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);

		} else if (!siop_set && !msd.dstat.acl_on && msd.dstat.siop_status) {
			mutex_lock(&msd.lock);
			msd.dstat.siop_status = false;
			msd.dstat.curr_acl_idx = -1;
			mipi_samsung_disp_send_cmd(PANEL_ACL_OFF, false);
			mutex_unlock(&msd.lock);
			pr_info("%s: acl off : acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);

		} else {
			msd.dstat.siop_status = siop_set;
			pr_info("%s: skip but siop update!! acl %d, siop %d", __func__,
				msd.dstat.acl_on, msd.dstat.siop_status);
		}
	}else {
	msd.dstat.siop_status = siop_set;
	pr_info("%s: panel is off state. updating state value.\n",
		__func__);
	}

	return size;
}

static ssize_t mipi_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("auto_brightness: %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	static int first_auto_br = 0;

	if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		msd.dstat.auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		msd.dstat.auto_brightness = 3;
	else if (sysfs_streq(buf, "4"))
		msd.dstat.auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		msd.dstat.auto_brightness = 5;
	else if (sysfs_streq(buf, "6")) // HBM mode (HBM + PSRE) will be changed to // leve 6 : no HBM , RE
		msd.dstat.auto_brightness = 6;
	else if (sysfs_streq(buf, "7")) // HBM mode (HBM + PSRE)
		msd.dstat.auto_brightness = 7;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (!first_auto_br) {
		pr_info("%s : skip first auto brightness store (%d) (%d)!!\n",
				__func__, msd.dstat.auto_brightness, msd.dstat.bright_level);
		first_auto_br++;
		return size;
	}

	if (msd.mfd->resume_state == MIPI_RESUME_STATE) {
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
		pr_info("%s %d %d\n", __func__, msd.dstat.auto_brightness, msd.dstat.bright_level);
	} else {
		pr_info("%s : panel is off state!!\n", __func__);
	}

	return size;
}

/*pwm backlight tunning*/
static ssize_t pwm_backlight_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int pwm_buf;

	if (sscanf(buf, "%u", &pwm_buf) != 1)
		return -EINVAL;

	if (msd.mfd->resume_state == MIPI_RESUME_STATE)
	{
		mutex_lock(&msd.lock);
		brightness_cmds.cmd_desc[0].payload[1] = (char)pwm_buf;
		mdss_dsi_cmds_send(msd.ctrl_pdata, brightness_cmds.cmd_desc, brightness_cmds.num_of_cmds, 0);
		mutex_unlock(&msd.lock);
		pr_info("%s, pwm set : 0x%x\n", __func__, (char)pwm_buf);

	} else {
		pr_info("%s : panel is off state!!\n", __func__);
	}
	return size;
}


static struct lcd_ops mipi_samsung_disp_props = {
	.get_power = NULL,
	.set_power = NULL,
};

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);

static DEVICE_ATTR(lcd_type, S_IRUGO,
			mipi_samsung_disp_lcdtype_show, NULL);

static DEVICE_ATTR(window_type, S_IRUGO,
			mipi_samsung_disp_windowtype_show, NULL);

static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);

static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_siop_show,
			mipi_samsung_disp_siop_store);

static DEVICE_ATTR(pwm_backlight, S_IRUGO | S_IWUSR | S_IWGRP,
			NULL,
			pwm_backlight_store);

#endif

#if !defined(CONFIG_FB_MSM_EDP_SAMSUNG)
static int __init current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		msd.dstat.recovery_boot_mode = 1;
	else
		msd.dstat.recovery_boot_mode = 0;

	pr_debug("%s %s", __func__, msd.dstat.recovery_boot_mode == 1 ?
						"recovery" : "normal");
	return 1;
}
__setup("androidboot.boot_recovery=", current_boot_mode);
#endif

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt,int flag)
{
	struct dcs_cmd_req cmdreq;
	if (get_lcd_attached() == 0)
	{
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));

	if (flag & CMD_REQ_SINGLE_TX) {
		cmdreq.flags = CMD_REQ_SINGLE_TX | CMD_CLK_CTRL | CMD_REQ_COMMIT;
	}else
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	/*
	 * This mutex is to sync up with dynamic FPS changes
	 * so that DSI lockups shall not happen
	 */
	BUG_ON(msd.ctrl_pdata == NULL);
	mutex_lock(&msd.ctrl_pdata->dfps_mutex);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
}

u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmd, int rlen)
{
        struct dcs_cmd_req cmdreq;

        memset(&cmdreq, 0, sizeof(cmdreq));
        cmdreq.cmds = cmd;
        cmdreq.cmds_cnt = 1;
        cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
        cmdreq.rlen = rlen;
        cmdreq.cb = NULL; /* call back */
        /*
         * This mutex is to sync up with dynamic FPS changes
         * so that DSI lockups shall not happen
         */
        BUG_ON(msd.ctrl_pdata == NULL);
        mutex_lock(&msd.ctrl_pdata->dfps_mutex);
        mdss_dsi_cmdlist_put(ctrl, &cmdreq);
        mutex_unlock(&msd.ctrl_pdata->dfps_mutex);
        /*
         * blocked here, untill call back called
         */
        return ctrl->rx_buf.len;
}

static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	struct dsi_buf *rp, *tp;

	unsigned int id = 0 ;

	if (get_lcd_attached() == 0)
	{
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return id;
	}

	if (!manufacture_id_cmds.num_of_cmds)
		return 0;

	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);

	tp = &dsi_panel_tx_buf;
	rp = &dsi_panel_rx_buf;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[0],
		manufacture_id_cmds.read_size[0]);

	pr_info("%s: manufacture_id1=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);

	id = (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[1],
		manufacture_id_cmds.read_size[1]);
	pr_info("%s: manufacture_id2=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[2],
		manufacture_id_cmds.read_size[2]);
	pr_info("%s: manufacture_id3=%x\n", __func__, *msd.ctrl_pdata->rx_buf.data);
	id |= (*((unsigned int *)msd.ctrl_pdata->rx_buf.data) & 0xFF);


	mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, true);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

	return id;
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	/*Dont need to send backlight command if display off*/
	if (msd.mfd->resume_state != MIPI_RESUME_STATE)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	switch (ctrl_pdata->bklt_ctrl) {
		case BL_WLED:
			led_trigger_event(bl_led_trigger, bl_level);
			break;
		case BL_PWM:
			mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
			break;
		case BL_DCS_CMD:
			msd.dstat.bright_level = bl_level;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
			break;
		default:
			pr_err("%s: Unknown bl_ctrl configuration\n",
				__func__);
			break;
	}
}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
/* fix build error temporary */
/*	struct msm_fb_data_type *mfd = msd.mfd;*/
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;
	int flag = 0;

	if (get_lcd_attached() == 0)
	{
		printk("%s: get_lcd_attached(0)!\n",__func__);
		return -ENODEV;
	}

	if (lock)
		mutex_lock(&msd.lock);

	switch (cmd) {
		case PANEL_READY_TO_ON:
			cmd_desc = display_qcom_on_cmds.cmd_desc;
			cmd_size = display_qcom_on_cmds.num_of_cmds;
			break;
		case PANEL_DISP_OFF:
			cmd_desc = display_qcom_off_cmds.cmd_desc;
			cmd_size = display_qcom_off_cmds.num_of_cmds;
			break;
		case PANEL_DISPLAY_ON:
			cmd_desc = display_on_cmd.cmd_desc;
			cmd_size = display_on_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_OFF:
			cmd_desc = display_off_cmd.cmd_desc;
			cmd_size = display_off_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_UNBLANK:
			cmd_desc = display_unblank_cmd.cmd_desc;
			cmd_size = display_unblank_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_BLANK:
			cmd_desc = display_blank_cmd.cmd_desc;
			cmd_size = display_blank_cmd.num_of_cmds;
			break;
		case PANEL_BRIGHT_CTRL:
			cmd_desc = brightness_cmds.cmd_desc;
			cmd_size = make_brightcontrol_set(msd.dstat.bright_level);
			/* Single Tx use for DSI_VIDEO_MODE Only */
#if 0
			if(msd.pdata->panel_info.mipi.mode == DSI_VIDEO_MODE)
				flag = CMD_REQ_SINGLE_TX;
			else
				flag = 0;
#endif
			break;
		case PANEL_MTP_ENABLE:
			cmd_desc = mtp_enable_cmds.cmd_desc;
			cmd_size = mtp_enable_cmds.num_of_cmds;
			break;
		case PANEL_MTP_DISABLE:
			cmd_desc = mtp_disable_cmds.cmd_desc;
			cmd_size = mtp_disable_cmds.num_of_cmds;
			break;
		case PANEL_LATE_ON:
			cmd_desc = late_on_cmds.cmd_desc;
			cmd_size = late_on_cmds.num_of_cmds;
			break;
		case PANEL_EARLY_OFF:
			cmd_desc = early_off_cmds.cmd_desc;
			cmd_size = early_off_cmds.num_of_cmds;
			break;
		case PANEL_ACL_ON:
			cmd_desc = cabc_on_cmds.cmd_desc;
			cmd_size = cabc_on_cmds.num_of_cmds;
			break;
		case PANEL_ACL_OFF:
			cmd_desc = cabc_off_cmds.cmd_desc;
			cmd_size = cabc_off_cmds.num_of_cmds;
			break;
		case PANEL_HSYNC_ON:
			cmd_desc = hsync_on_cmds.cmd_desc;
			cmd_size = hsync_on_cmds.num_of_cmds;
			break;
		case PANEL_HSYNC_OFF:
			cmd_desc = hsync_off_cmds.cmd_desc;
			cmd_size = hsync_off_cmds.num_of_cmds;
			break;
		default:
			goto unknown_command;
			;
	}

	if (!cmd_size)
		goto unknown_command;



	pr_info("%s cmd=%d cnt=%d\n", __func__, cmd, cmd_size);
	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size, flag);

	if (lock)
		mutex_unlock(&msd.lock);

	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

	if (lock)
		mutex_unlock(&msd.lock);

	return -EINVAL;
}

void mdss_dsi_panel_touchsensing(int enable)
{
	if(!msd.dstat.on)
	{
		pr_err("%s: No panel on! %d\n", __func__, enable);
		return;
	}
/*
	if(enable)
		mipi_samsung_disp_send_cmd(PANEL_TOUCHSENSING_ON, true);
	else
		mipi_samsung_disp_send_cmd(PANEL_TOUCHSENSING_OFF, true);
		*/
}

void mdss_dsi_panel_hsync_onoff(bool onoff)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	if (mfd->panel_power_on)
	{
		if( onoff )
		{
			msleep(30);
			mipi_samsung_disp_send_cmd(PANEL_HSYNC_ON, true);
			pr_info("%s : HSYNC On\n",__func__);
		}
		else
		{
			mipi_samsung_disp_send_cmd(PANEL_HSYNC_OFF, true);
			msleep(10);
			pr_info("%s : HSYNC Off\n",__func__);
		}
	}
	else
		pr_err("%s : panel power off\n",__func__);

	return;
}
EXPORT_SYMBOL(mdss_dsi_panel_hsync_onoff);

static int mdss_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
			pr_err("%s: Invalid input data\n", __func__);
			return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	msd.pdata = pdata;
	msd.ctrl_pdata = ctrl_pdata;
#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! init class called!\n",
		__func__);
	mdnie_lite_tuning_init(&msd);
#endif
	/* Set the initial state to Suspend until it is switched on */
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:%d, Panel registered succesfully\n", __func__, __LINE__);
	return 0;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	mipi  = &pdata->panel_info.mipi;

	pr_info("mdss_dsi_panel_on DSI_MODE = %d ++\n",mipi->mode);
	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (0 && !msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);

	mipi_samsung_disp_send_cmd(PANEL_READY_TO_ON, true);

	/* Recovery Mode : Set some default brightness */
	if (msd.dstat.recovery_boot_mode) {
		msd.dstat.bright_level = RECOVERY_BRIGHTNESS;
	}
	mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

	/* Init Index Values */
	msd.dstat.curr_elvss_idx = -1;
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.hbm_mode = 0;
	msd.dstat.on = 1;
	msd.dstat.wait_disp_on = 1;
	msd.mfd->resume_state = MIPI_RESUME_STATE;

	if(	msd.dstat.acl_on )
		mipi_samsung_disp_send_cmd(PANEL_ACL_ON, true);

#if defined(CONFIG_MDNIE_LITE_TUNING)
	is_negative_on();
#endif

	pr_info("%s--\n", __func__);
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	printk(KERN_INFO "[lcd] mipi_samsung_disp_on_in_video_engine end %d\n", gpio_get_value(err_fg_gpio));
	enable_irq(gpio_to_irq(err_fg_gpio));
#endif

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_info("mdss_dsi_panel_off ++\n");
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mipi  = &pdata->panel_info.mipi;
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!err_fg_working) {
		disable_irq_nosync(gpio_to_irq(err_fg_gpio));
		cancel_work_sync(&err_fg_work);
	}
#endif

	mutex_lock(&msd.lock);
	msd.dstat.on = 0;
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
#if defined(CONFIG_MACH_JVELTEEUR)
	mdss_dsi_panel_hsync_onoff(false);
#endif
	mipi_samsung_disp_send_cmd(PANEL_DISP_OFF, false);

	mutex_unlock(&msd.lock);
	pr_info("mdss_dsi_panel_off --\n");

	return 0;
}

static int mdss_samsung_parse_candela_lux_mapping_table(struct platform_device *pdev,
		struct candella_lux_map *table, char *keystring)
{
		const __be32 *data;
		int  data_offset, len = 0 , i = 0;
		int  cdmap_start=0, cdmap_end=0;

		struct device_node *np = pdev->dev.of_node;
		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read table %s ",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 4) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s",
						__func__, __LINE__, keystring);
			return -EINVAL;
		}
		table->lux_tab_size = len / (sizeof(int)*5);
		table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
		if (!table->lux_tab)
			return -ENOMEM;

		table->pwm_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
		if (!table->pwm_tab)
			return -ENOMEM;

		table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
		if (!table->cmd_idx)
			goto error;

		data_offset = 0;
		for (i = 0 ; i < table->lux_tab_size; i++) {
			table->cmd_idx[i]= be32_to_cpup(&data[data_offset++]);	/* 1rst field => <idx> */
			cdmap_start = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
			cdmap_end = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <till> */
			table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);	/* 4th field => <candella> */
			table->pwm_tab[i]= be32_to_cpup(&data[data_offset++]);		/* 5th field => <pwm> */
			/* Fill the backlight level to lux mapping array */
			do{
				table->bkl[cdmap_start++] = i;
			}while(cdmap_start <= cdmap_end);
		}
		return 0;
error:
	kfree(table->lux_tab);

	return -ENOMEM;
}

static int mdss_samsung_parse_panel_cmd(struct platform_device *pdev,
		struct dsi_cmd *commands, char *keystring)
{

		const char *data;
		int type, len = 0, i = 0;
		char *bp;
		struct dsi_ctrl_hdr *dchdr;
		struct device_node *np = pdev->dev.of_node;
		int is_read = 0;

		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read %s",
				__func__, __LINE__, keystring);
			return -ENOMEM;
		}

		commands->cmds_buff = kzalloc(sizeof(char) * len, GFP_KERNEL);
		if (!commands->cmds_buff)
			return -ENOMEM;

		memcpy(commands->cmds_buff, data, len);
		commands->cmds_len = len;

		/* scan dcs commands */
		bp = commands->cmds_buff;
		while (len > sizeof(*dchdr)) {
			dchdr = (struct dsi_ctrl_hdr *)bp;
			dchdr->dlen = ntohs(dchdr->dlen);

		if (dchdr->dlen >200)
			goto error2;

			bp += sizeof(*dchdr);
			len -= sizeof(*dchdr);
			bp += dchdr->dlen;
			len -= dchdr->dlen;
			commands->num_of_cmds++;

			type = dchdr->dtype;
			if (type == DTYPE_GEN_READ ||
				type == DTYPE_GEN_READ1 ||
				type == DTYPE_GEN_READ2 ||
				type == DTYPE_DCS_READ)	{
				/* Read command :last byte contain read size, read start */
				bp += 2;
				len -= 2;
				is_read = 1;
			}
		}

		if (len != 0) {
			pr_err("%s: dcs OFF command byte Error, len=%d", __func__, len);
			commands->cmds_len = 0;
			commands->num_of_cmds = 0;
			goto error2;
		}

		if (is_read) {
			/*
				Allocate an array which will store the number
				for bytes to read for each read command
			*/
			commands->read_size = kzalloc(sizeof(char) * \
					commands->num_of_cmds, GFP_KERNEL);
			if (!commands->read_size) {
				pr_err("%s:%d, Unable to read NV cmds",
					__func__, __LINE__);
				goto error2;
			}
			commands->read_startoffset = kzalloc(sizeof(char) * \
					commands->num_of_cmds, GFP_KERNEL);
			if (!commands->read_startoffset) {
				pr_err("%s:%d, Unable to read NV cmds",
					__func__, __LINE__);
				goto error1;
			}
		}

		commands->cmd_desc = kzalloc(commands->num_of_cmds
					* sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
		if (!commands->cmd_desc)
			goto error1;

		bp = commands->cmds_buff;
		len = commands->cmds_len;
		for (i = 0; i < commands->num_of_cmds; i++) {
			dchdr = (struct dsi_ctrl_hdr *)bp;
			len -= sizeof(*dchdr);
			bp += sizeof(*dchdr);
			commands->cmd_desc[i].dchdr = *dchdr;
			commands->cmd_desc[i].payload = bp;
			bp += dchdr->dlen;
			len -= dchdr->dlen;
			if (is_read)
			{
				commands->read_size[i] = *bp++;
				commands->read_startoffset[i] = *bp++;
				len -= 2;
			}
		}

		return 0;

error1:
	kfree(commands->read_size);
error2:
	kfree(commands->cmds_buff);

	return -EINVAL;

}

static int mdss_panel_parse_dt(struct platform_device *pdev,
			       struct mdss_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	u32 fbc_res[7];
	int rc, i, len;
	const char *data;
	static const char *bl_ctrl_type, *pdest;
	static const char *on_cmds_state, *off_cmds_state;
	bool fbc_enabled = false;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-size", res, 2);
	if (rc == 0) {
		panel_data->panel_info.width = res[0];
		panel_data->panel_info.height = res[1];
	}

#if 0
	rc = of_property_read_u32_array(np, "qcom,mdss-clk-rate", res, 1);
	if (rc) {
		pr_err("%s:%d, panel clock rate not specified\n",
						__func__, __LINE__);
	}
	panel_data->panel_info.clk_rate = (!rc ? res[0] : 0);
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	panel_data->esd_gpio = of_get_named_gpio(pdev->dev.of_node,
						     "qcom,esd-irq-gpio", 0);
	if (!gpio_is_valid(panel_data->esd_gpio)) {
		pr_err("%s:%d, esd gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(panel_data->esd_gpio, "esd_enable");
		if (rc) {
			pr_err("request esd gpio failed, rc=%d\n",
			       rc);
			gpio_free(panel_data->esd_gpio);
			return -ENODEV;
		}
	}
	err_fg_gpio = panel_data->esd_gpio;
#endif

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-active-res", res, 2);
	if (rc == 0) {
		panel_data->panel_info.lcdc.xres_pad =
			panel_data->panel_info.xres - res[0];
		panel_data->panel_info.lcdc.yres_pad =
			panel_data->panel_info.yres - res[1];
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);

	pdest = of_get_property(pdev->dev.of_node,
				"qcom,mdss-pan-dest", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		panel_data->panel_info.pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		panel_data->panel_info.pdest = DISPLAY_2;
	else {
		pr_debug("%s: pdest not specified. Set Default\n",
							__func__);
		panel_data->panel_info.pdest = DISPLAY_1;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-porch-values", res, 6);
	panel_data->panel_info.lcdc.h_back_porch = (!rc ? res[0] : 6);
	panel_data->panel_info.lcdc.h_pulse_width = (!rc ? res[1] : 2);
	panel_data->panel_info.lcdc.h_front_porch = (!rc ? res[2] : 6);
	panel_data->panel_info.lcdc.v_back_porch = (!rc ? res[3] : 6);
	panel_data->panel_info.lcdc.v_pulse_width = (!rc ? res[4] : 2);
	panel_data->panel_info.lcdc.v_front_porch = (!rc ? res[5] : 6);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-underflow-clr", &tmp);
	panel_data->panel_info.lcdc.underflow_clr = (!rc ? tmp : 0xff);

	bl_ctrl_type = of_get_property(pdev->dev.of_node,
				  "qcom,mdss-pan-bl-ctrl", NULL);

	if ((bl_ctrl_type) && (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12))) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);

		panel_data->panel_info.bklt_ctrl = BL_WLED;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_pwm", 11)) {
		panel_data->panel_info.bklt_ctrl = BL_PWM;

		rc = of_property_read_u32(np, "qcom,dsi-pwm-period", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, dsi pwm_period\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		panel_data->panel_info.pwm_period = tmp;

		rc = of_property_read_u32(np, "qcom,dsi-lpg-channel", &tmp);
		if (rc) {
			pr_err("%s:%d, Error, dsi lpg channel\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		panel_data->panel_info.pwm_lpg_chan = tmp;

		tmp = of_get_named_gpio(np, "qcom,dsi-pwm-gpio", 0);
		panel_data->panel_info.pwm_pmic_gpio =  tmp;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_dcs_cmds", 12)) {
		pr_debug("%s: SUCCESS-> DCS CMD BACKLIGHT register\n",
			 __func__);
		panel_data->panel_info.bklt_ctrl = BL_DCS_CMD;
	} else {
		pr_debug("%s: Unknown backlight control\n", __func__);
		panel_data->panel_info.bklt_ctrl = UNKNOWN_CTRL;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	panel_data->panel_info.mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

	rc = of_property_read_u32(np, "qcom,mdss-vsync-enable", &tmp);
	panel_data->panel_info.mipi.vsync_enable = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-hw-vsync-mode", &tmp);
	panel_data->panel_info.mipi.hw_vsync_mode = (!rc ? tmp : 0);

#if 0
#ifdef	CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FULL_HD_PT_PANEL
	panel_data->panel_info.mipi.vsync_enable = 1;
	if (!(board_rev > 0x02)) {
		panel_data->panel_info.mipi.hw_vsync_mode = 0;
	}
#endif
#endif

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-h-pulse-mode", &tmp);
	panel_data->panel_info.mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-h-power-stop", res, 3);
	panel_data->panel_info.mipi.hbp_power_stop = (!rc ? res[0] : false);
	panel_data->panel_info.mipi.hsa_power_stop = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.hfp_power_stop = (!rc ? res[2] : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-bllp-power-stop", res, 2);
	panel_data->panel_info.mipi.bllp_power_stop =
					(!rc ? res[0] : false);
	panel_data->panel_info.mipi.eof_bllp_power_stop =
					(!rc ? res[1] : false);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-traffic-mode", &tmp);
	panel_data->panel_info.mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-insert-dcs-cmd", &tmp);
	panel_data->panel_info.mipi.insert_dcs_cmd =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-continue", &tmp);
	panel_data->panel_info.mipi.wr_mem_continue =
		(!rc ? tmp : 0x3c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-wr-mem-start", &tmp);
	panel_data->panel_info.mipi.wr_mem_start =
		(!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-te-sel", &tmp);
	panel_data->panel_info.mipi.te_sel =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-dst-format", &tmp);
	panel_data->panel_info.mipi.dst_format =
			(!rc ? tmp : DSI_VIDEO_DST_FORMAT_RGB888);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-vc", &tmp);
	panel_data->panel_info.mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-rgb-swap", &tmp);
	panel_data->panel_info.mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	panel_data->panel_info.mipi.force_clk_lane_hs = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "samsung,mdss-early-lcd-on", &tmp);
			panel_data->panel_info.early_lcd_on = (!rc ? tmp : 0);
	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-data-lanes", res, 4);
	panel_data->panel_info.mipi.data_lane0 = (!rc ? res[0] : true);
	panel_data->panel_info.mipi.data_lane1 = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.data_lane2 = (!rc ? res[2] : false);
	panel_data->panel_info.mipi.data_lane3 = (!rc ? res[3] : false);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dlane-swap", &tmp);
	panel_data->panel_info.mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-dsi-t-clk", res, 2);
	panel_data->panel_info.mipi.t_clk_pre = (!rc ? res[0] : 0x24);
	panel_data->panel_info.mipi.t_clk_post = (!rc ? res[1] : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-stream", &tmp);
	panel_data->panel_info.mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mdp-tr", &tmp);
	panel_data->panel_info.mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dma-tr", &tmp);
	panel_data->panel_info.mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-frame-rate", &tmp);
	panel_data->panel_info.mipi.frame_rate = (!rc ? tmp : 60);

	rc = of_property_read_u32(np, "qcom,mdss-pan-clk-rate", &tmp);
	panel_data->panel_info.clk_rate = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,panel-phy-regulatorSettings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.regulator[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-timingSettings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.timing[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-strengthCtrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	phy_params.strength[0] = data[0];
	phy_params.strength[1] = data[1];

	data = of_get_property(np, "qcom,panel-phy-bistCtrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.bistCtrl[i] = data[i];

	data = of_get_property(np, "qcom,panel-phy-laneConfig", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
		       __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_params.laneCfg[i] = data[i];

	panel_data->panel_info.mipi.dsi_phy_db = &phy_params;

		fbc_enabled = of_property_read_bool(np,
			"qcom,fbc-enabled");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_data->panel_info.fbc.enabled = 1;

		rc = of_property_read_u32_array(np,
				"qcom,fbc-mode", fbc_res, 7);
		panel_data->panel_info.fbc.target_bpp =
			(!rc ?	fbc_res[0] : panel_data->panel_info.bpp);
		panel_data->panel_info.fbc.comp_mode = (!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.qerr_enable =
			(!rc ? fbc_res[2] : 0);
		panel_data->panel_info.fbc.cd_bias = (!rc ? fbc_res[3] : 0);
		panel_data->panel_info.fbc.pat_enable = (!rc ? fbc_res[4] : 0);
		panel_data->panel_info.fbc.vlc_enable = (!rc ? fbc_res[5] : 0);
		panel_data->panel_info.fbc.bflc_enable =
			(!rc ? fbc_res[6] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-budget-ctl", fbc_res, 3);
		panel_data->panel_info.fbc.line_x_budget =
			(!rc ? fbc_res[0] : 0);
		panel_data->panel_info.fbc.block_x_budget =
			(!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.block_budget =
			(!rc ? fbc_res[2] : 0);

		rc = of_property_read_u32_array(np,
				"qcom,fbc-lossy-mode", fbc_res, 4);
		panel_data->panel_info.fbc.lossless_mode_thd =
			(!rc ? fbc_res[0] : 0);
		panel_data->panel_info.fbc.lossy_mode_thd =
			(!rc ? fbc_res[1] : 0);
		panel_data->panel_info.fbc.lossy_rgb_thd =
			(!rc ? fbc_res[2] : 0);
		panel_data->panel_info.fbc.lossy_mode_idx =
			(!rc ? fbc_res[3] : 0);

	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_data->panel_info.fbc.enabled = 0;
		panel_data->panel_info.fbc.target_bpp =
			panel_data->panel_info.bpp;
	}

	on_cmds_state = of_get_property(pdev->dev.of_node,
				"qcom,on-cmds-dsi-state", NULL);
	if (!strncmp(on_cmds_state, "DSI_LP_MODE", 11)) {
		panel_data->dsi_on_state = DSI_LP_MODE;
	} else if (!strncmp(on_cmds_state, "DSI_HS_MODE", 11)) {
		panel_data->dsi_on_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		panel_data->dsi_on_state = DSI_LP_MODE;
	}

	off_cmds_state = of_get_property(pdev->dev.of_node,
				"qcom,off-cmds-dsi-state", NULL);
	if (!strncmp(off_cmds_state, "DSI_LP_MODE", 11)) {
		panel_data->dsi_off_state = DSI_LP_MODE;
	} else if (!strncmp(off_cmds_state, "DSI_HS_MODE", 11)) {
		panel_data->dsi_off_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
							__func__);
		panel_data->dsi_off_state = DSI_LP_MODE;
	}

	mdss_samsung_parse_panel_cmd(pdev, &display_qcom_on_cmds,
				"qcom,panel-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_qcom_off_cmds,
				"qcom,panel-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &late_on_cmds,
				"samsung,panel-late-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &early_off_cmds,
				"samsung,panel-early-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &brightness_cmds,
				"samsung,panel-brightness-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &mtp_enable_cmds,
				"samsung,panel-mtp-enable-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &mtp_disable_cmds,
				"samsung,panel-mtp-disable-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &cabc_on_cmds,
				"samsung,panel-cabc-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &cabc_off_cmds,
				"samsung,panel-cabc-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &hsync_on_cmds,
				"samsung,panel-hsync-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &hsync_off_cmds,
				"samsung,panel-hsync-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &manufacture_id_cmds,
				"samsung,panel-manufacture-id-read-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_on_cmd,
				"qcom,panel-display-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_off_cmd,
				"qcom,panel-display-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_unblank_cmd,
				"qcom,panel-display-unblank-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_blank_cmd,
				"qcom,panel-display-blank-cmds");

	/* Process the lux value table */
	mdss_samsung_parse_candela_lux_mapping_table(pdev, &candela_map_table,
					"samsung,panel-candela-mapping-table");
	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void mipi_samsung_disp_early_suspend(struct early_suspend *h)
{
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;

	LCD_DEBUG("------");
}

static void mipi_samsung_disp_late_resume(struct early_suspend *h)
{

	msd.mfd->resume_state = MIPI_RESUME_STATE;

	LCD_DEBUG("------");
}
#endif

static int is_panel_supported(const char *panel_name)
{
	int i = 0;

	if (panel_name == NULL)
		return -EINVAL;

	if(!of_machine_is_compatible("qcom,msm8974-cdp"))
		return -EINVAL;

	while(panel_supp_cdp[i].name != NULL)	{
		if(!strcmp(panel_name,panel_supp_cdp[i].name))
			break;
		i++;
	}

	if( i < ARRAY_SIZE(panel_supp_cdp)) {
		memcpy(msd.panel_name, panel_name, MAX_PANEL_NAME_SIZE);
		msd.panel = panel_supp_cdp[i].panel_code;
		return 0;
	}
	return -EINVAL;
}

#ifdef DDI_VIDEO_ENHANCE_TUNING
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
#define TUNE_FIRST_SIZE 5
#define TUNE_SECOND_SIZE 108
static char tuning_file[MAX_FILE_NAME];

static char mdni_tuning1[TUNE_FIRST_SIZE];
static char mdni_tuning2[TUNE_SECOND_SIZE];

static struct dsi_cmd_desc mdni_tune_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning2)}, mdni_tuning2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(mdni_tuning1)}, mdni_tuning1},
};

static char char_to_dec(char data1, char data2)
{
	char dec;

	dec = 0;

	if (data1 >= 'a') {
		data1 -= 'a';
		data1 += 10;
	} else if (data1 >= 'A') {
		data1 -= 'A';
		data1 += 10;
	} else
		data1 -= '0';

	dec = data1 << 4;

	if (data2 >= 'a') {
		data2 -= 'a';
		data2 += 10;
	} else if (data2 >= 'A') {
		data2 -= 'A';
		data2 += 10;
	} else
		data2 -= '0';

	dec |= data2;

	return dec;
}
static void sending_tune_cmd(char *src, int len)
{
	int data_pos;
	int cmd_step;
	int cmd_pos;

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step) {
					mdni_tuning1[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				} else {
					mdni_tuning2[cmd_pos] =
					char_to_dec(*(src + data_pos + 2),
							*(src + data_pos + 3));
				}

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == TUNE_FIRST_SIZE && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_FIRST_SIZE ; data_pos++)
		printk(KERN_INFO "0x%x ", mdni_tuning1[data_pos]);
	printk(KERN_INFO "\n");
	for (data_pos = 0; data_pos < TUNE_SECOND_SIZE ; data_pos++)
		printk(KERN_INFO"0x%x ", mdni_tuning2[data_pos]);
	printk(KERN_INFO "\n");



	mutex_lock(&msd.lock);


	mdss_dsi_cmds_send(msd.ctrl_pdata, mdni_tune_cmd, ARRAY_SIZE(mdni_tune_cmd), 0);

	mutex_unlock(&msd.lock);

}

static void load_tuning_file(char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs;

	pr_info("%s called loading file name : [%s]\n", __func__,
	       filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		return;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return;
	}
	pos = 0;
	memset(dp, 0, l);

	pr_info("%s before vfs_read()\n", __func__);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	pr_info("%s after vfs_read()\n", __func__);

	if (ret != l) {
		pr_info("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dp, l);

	kfree(dp);
}

static int samsung_dsi_panel_event_handler(int event)
{
	pr_debug("SS DSI Event Handler");
	switch (event) {
		case MDSS_EVENT_FRAME_UPDATE:
			if(msd.dstat.wait_disp_on) {
				mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON, true);
				msd.dstat.wait_disp_on = 0;
			}
		break;
	}

	return 0;
}

static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n",
								tuning_file);

	return ret;
}

static ssize_t tuning_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
	char *pt;
	memset(tuning_file, 0, sizeof(tuning_file));
	snprintf(tuning_file, MAX_FILE_NAME, "%s%s", TUNING_FILE_PATH, buf);

	pt = tuning_file;
	while (*pt) {
		if (*pt == '\r' || *pt == '\n') {
			*pt = 0;
			break;
		}
		pt++;
	}

	pr_info("%s:%s\n", __func__, tuning_file);

	load_tuning_file(tuning_file);

	return size;
}

static int mdss_dsi_panel_blank(struct mdss_panel_data *pdata, int blank)
{
	if(blank) {
		pr_info("%s:%d, blanking panel\n", __func__, __LINE__);
		mipi_samsung_disp_send_cmd(PANEL_DISPLAY_BLANK, false);
	}
	else {
		pr_info("%s:%d, unblanking panel\n", __func__, __LINE__);
		mipi_samsung_disp_send_cmd(PANEL_DISPLAY_UNBLANK, false);
	}
	return 0;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
static irqreturn_t err_fg_irq_handler(int irq, void *handle)
{
	pr_info("%s handler start", __func__);
	disable_irq_nosync(gpio_to_irq(err_fg_gpio));
	schedule_work(&err_fg_work);
	pr_info("%s : handler end", __func__);

	return IRQ_HANDLED;
}

static void err_fg_work_func(struct work_struct *work)
{
	int bl_backup;
	struct msm_fb_data_type *mfd = msd.mfd;
	bl_backup = mfd->bl_level;
	pr_info("%s : start", __func__);

	err_fg_working = 1;
	esd_recovery();
	esd_count++;
	err_fg_working = 0;
	mdelay(50);

	/* brightness off */
	msd.dstat.bright_level = 0;
	mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

	/* Restore brightness */
	msd.dstat.bright_level = bl_backup;
	mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);

	pr_info("%s esd end", __func__);
	return;
}

#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "esd count \n");

	return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	err_fg_irq_handler(0, mfd);
	return 1;
}

static DEVICE_ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store);
#endif
#endif

static int __devinit mdss_dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct mdss_panel_common_pdata vendor_pdata;
	static const char *panel_name;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#endif

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
				struct backlight_device *bd = NULL;
#endif


	pr_info("%s:%d", __func__, __LINE__);
	if (!pdev->dev.of_node)
		return -ENODEV;

	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	if(is_panel_supported(panel_name)) {
		LCD_DEBUG("Panel : %s is not supported:",panel_name);
		return -1;
	}

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata);
	if (rc)
		return rc;

#if defined(CONFIG_MIPI_LCD_S6E3FA0_FORCE_VIDEO_MODE)
	force_dsi_video_mode(&vendor_pdata);
#endif

	vendor_pdata.on = mdss_dsi_panel_on;
	vendor_pdata.off = mdss_dsi_panel_off;
	vendor_pdata.bl_fnc = mdss_dsi_panel_bl_ctrl;
	vendor_pdata.panel_reset = mdss_dsi_samsung_panel_reset;
	vendor_pdata.panel_registered = mdss_dsi_panel_registered;
#ifdef DDI_VIDEO_ENHANCE_TUNING
	vendor_pdata.event_handler = samsung_dsi_panel_event_handler;
	vendor_pdata.blank = mdss_dsi_panel_blank;
#endif

	/* Init driver  specific data */
	mutex_init(&msd.lock);

	rc = dsi_panel_device_register(pdev, &vendor_pdata);
	if (rc)
		return rc;

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	INIT_WORK(&err_fg_work, err_fg_work_func);

	rc = request_threaded_irq(gpio_to_irq(err_fg_gpio),
		NULL, err_fg_irq_handler,  IRQF_TRIGGER_RISING | IRQF_ONESHOT, "esd_detect", NULL);
	if (rc) {
		pr_err("%s : Failed to request_irq.:ret=%d", __func__, rc);
	}

	disable_irq(gpio_to_irq(err_fg_gpio));
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return rc;
	}


	sysfs_remove_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);

	rc = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_power.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_power.attr.name);
	}


	rc = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_lcd_type.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_lcd_type.attr.name);
	}

	rc = sysfs_create_file(&lcd_device->dev.kobj,
			&dev_attr_window_type.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_window_type.attr.name);
	}

	rc = sysfs_create_file(&lcd_device->dev.kobj,
					&dev_attr_power_reduce.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_power_reduce.attr.name);
	}

	rc = sysfs_create_file(&lcd_device->dev.kobj,
						&dev_attr_siop_enable.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_siop_enable.attr.name);
	}

	/*pwm backlight tunning*/
	rc = sysfs_create_file(&lcd_device->dev.kobj,
						&dev_attr_pwm_backlight.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_pwm_backlight.attr.name);
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return rc;
	}

	rc = sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}
#endif
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
#ifdef ESD_DEBUG
	rc= sysfs_create_file(&lcd_device->dev.kobj,
							&dev_attr_esd_check.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_esd_check.attr.name);
	}
#endif
#endif

#if defined(CONFIG_MDNIE_LITE_TUNING)
	pr_info("[%s] CONFIG_MDNIE_LITE_TUNING ok ! init class called!\n",
		__func__);
	init_mdnie_class();
#endif

#if defined(DDI_VIDEO_ENHANCE_TUNING)
	rc = sysfs_create_file(&lcd_device->dev.kobj,
			&dev_attr_tuning.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_tuning.attr.name);
	}
#endif


#if defined(CONFIG_HAS_EARLYSUSPEND)
	msd.early_suspend.suspend = mipi_samsung_disp_early_suspend;
	msd.early_suspend.resume = mipi_samsung_disp_late_resume;
	msd.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN-1;
	register_early_suspend(&msd.early_suspend);
#endif
	/*
	 * unless panel is powered on don't set the state to true
	 *  This is to handle cases like cont splash or lpm in which panel
	 *  will be powered on later on.
	 */
	msd.dstat.on = 0;
	msd.dstat.bright_level = RECOVERY_BRIGHTNESS;

	return 0;
}

static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "jdi,mdss-tft-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
		.name   = "jdi_tft_dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

int get_lcd_attached(void)
{
#if 1
	return 1;
#else
	return lcd_attached;
#endif
}
EXPORT_SYMBOL(get_lcd_attached);

static int __init lcd_attached_status(char *mode)
{
	/*
	*	1 is lcd attached
	*	0 is lcd detached
	*/

	if (strncmp(mode, "1", 1) == 0)
		lcd_attached = 1;
	else
		lcd_attached = 0;
	lcd_attached = 1;

	pr_info("%s %s", __func__, lcd_attached == 1 ?
				"lcd_attached" : "lcd_detached");
	return 1;
}
__setup("lcd_attached=", lcd_attached_status);

static int __init mdss_dsi_panel_init(void)
{
	printk("%s: get_lcd_attached(%d)!\n",
				__func__, get_lcd_attached());
//	if (get_lcd_attached() == 0)
//		return -ENODEV;

	return platform_driver_register(&this_driver);
}

static int __init mdss_panel_current_hw_rev(char *rev)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	board_rev = *rev - '0';

	pr_info("%s : %d", __func__, board_rev);

	return 1;
}

__setup("samsung.board_rev=", mdss_panel_current_hw_rev);

MODULE_DESCRIPTION("Samsung DSI panel driver");
MODULE_AUTHOR("Krishna Kishor Jha <krishna.jha@samsung.com>");
MODULE_LICENSE("GPL");
module_init(mdss_dsi_panel_init);
