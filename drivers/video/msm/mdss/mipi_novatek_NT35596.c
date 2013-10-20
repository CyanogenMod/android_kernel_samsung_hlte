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
#include "mipi_novatek_NT35596.h"
#include "mdss_fb.h"

#include <mach/gpiomux.h>

#ifdef CONFIG_SAMSUNG_CMC624
#include <linux/i2c/samsung_cmc624.h>
#endif

#include <asm/system_info.h>

#define DT_CMD_HDR 6

#define GPIO_LCD_BOOSTER_EN			51

static struct dsi_cmd display_qcom_on_cmds;
static struct dsi_cmd display_qcom_off_cmds;
static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_cmd;
static struct dsi_cmd acl_off_cmd;
static struct dsi_cmd touchsensing_on_cmd;
static struct dsi_cmd touchsensing_off_cmd;

//melius
//static struct dsi_cmd display_ready_on_cmds;
static struct dsi_cmd display_ready_off_cmds;
static struct dsi_cmd display_panel_on_cmds;
// static struct dsi_cmd display_panel_off_cmds;
static struct dsi_cmd display_late_on_cmds;
static struct dsi_cmd display_early_off_cmds;
static struct dsi_cmd backlight_cont_cmd;

static struct mipi_samsung_driver_data msd;
/*List of supported Panels with HW revision detail
 * (one structure per project)
 * {hw_rev,"label string given in panel dtsi file"}
 * */
static struct  panel_hrev panel_supp_cdp[]= {
	{"samsung amoled 720p video mode dsi S6E8AA3X01 panel", PANEL_720P_AMOLED_S6E8AA3X01},
	{"samsung amoled 1080p video mode dsi S6E8FA0 panel", PANEL_1080P_OCTA_S6E8FA0},
	{"samsung amoled 1080p video mode dsi S6E3FA0 panel", PANEL_1080P_OCTA_S6E3FA0},
	{"samsung amoled 1080p command mode dsi S6E3FA0 panel", PANEL_1080P_OCTA_S6E3FA0_CMD},
	{"novatek tft hd video mode dsi nt35596 panel", PANEL_HD_TFT_NT35596},
	{NULL}
};

//#define CONFIG_NO_CMC624 //Melius test

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_dsi_phy_ctrl phy_params;
char board_rev;
static int lcd_attached = 1;

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock);

void mdss_dsi_cmds_send(
	struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt);

extern void mdss_dsi_panel_touchsensing(int enable);

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

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (!gpio_is_valid(ctrl_pdata->lcd_en)) {
		pr_debug("%s:%d, enable line not configured\n",  __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n", __func__, __LINE__);
	}

	if (enable) {
                pr_info("[LCD] %s: LCD_RST High !!! \n", __func__);
		gpio_set_value((ctrl_pdata->rst_gpio), 1);
		msleep(20);
		wmb();

                #if 1 // def CONFIG_SAMSUNG_CMC624 /* Enable CMC Chip */
		{
        		int retry=0;
        		for(retry=0; retry<3; retry++)	{
        			cmc_power(1);
        			if(samsung_cmc624_on(1)) {
        				break;
        			}	
        			cmc_power(0);
        			msleep(500);
        		}
		}
                msleep(20);
                #endif

                /*
                if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
			wmb();
		}*/
	} else {
                pr_info("[LCD] %s: LCD_RST LOW !!! \n", __func__);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
                if (gpio_is_valid(ctrl_pdata->lcd_en)) {
                        pr_info("[LCD] %s: LCD_EN LOW !!! \n", __func__);
			gpio_set_value((ctrl_pdata->lcd_en), 0);
		}
	}
}


static void mipi_novatek_disp_set_backlight(struct msm_fb_data_type *mfd)
{
//	struct mipi_panel_info *mipi;
	static int bl_level_old;

	pr_info("%s : level (%d)\n", __func__, mfd->bl_level);

#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
	if (msd.esd_refresh == true) {
		pr_debug("ESD Refresh on going cannot set backlight\n");
		goto end;
	}
#endif
//	mipi  = &mfd->panel_info.mipi;
	if (bl_level_old == mfd->bl_level)
		goto end;
	if (!mfd->panel_power_on)
		goto end;
#ifdef CONFIG_NO_CMC624
	mutex_lock(&msd.lock);
	/* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also */
//	mipi_dsi_mdp_busy_wait();
//	WRDISBV[1] = (unsigned char)mfd->bl_level;
	mipi_samsung_disp_send_cmd(BACKLIGHT_CONTROL_ON, true);
	mutex_unlock(&msd.lock);
#else
	cmc624_pwm_control_cabc(mfd->bl_level);
#endif
	bl_level_old = mfd->bl_level;

end:
	return;
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
		mipi_samsung_disp_send_cmd(PANEL_READY_TO_ON, true);
		mipi_novatek_disp_set_backlight(mfd); //melius
	} else {
		mfd->fbi->fbops->fb_blank(FB_BLANK_POWERDOWN, mfd->fbi);
	}

	pr_info("mipi_samsung_disp_set_power\n");

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
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, true);
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

	if (msd.mfd->resume_state == MIPI_RESUME_STATE) {
#ifdef CONFIG_SAMSUNG_CMC624
		cabc_onoff_ctrl(msd.dstat.auto_brightness);
#endif
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

static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_siop_show,
			mipi_samsung_disp_siop_store);

#endif


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

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
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
        mdss_dsi_cmdlist_put(ctrl, &cmdreq);
        /*
         * blocked here, untill call back called
         */
        return ctrl->rx_buf.len;
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
	cmc624_pwm_control_cabc(bl_level);
}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
/* fix build error temporary */
/*	struct msm_fb_data_type *mfd = msd.mfd;*/
	struct dsi_cmd_desc *cmd_desc;
//	struct msm_fb_data_type *mfd = msd.mfd;
	int cmd_size = 0;

	if (lock)
		mutex_lock(&msd.lock);

	switch (cmd) {
		case PANEL_READY_TO_ON: //this is the on cmds.
			cmd_desc = display_qcom_on_cmds.cmd_desc;
			cmd_size = display_qcom_on_cmds.num_of_cmds;
			break;
		case PANEL_DISP_OFF:   // PANEL_READY_TO_OFF:
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
		case PANEL_LATE_ON:
			cmd_desc = display_late_on_cmds.cmd_desc;
			cmd_size = display_late_on_cmds.num_of_cmds;
			break;
		case PANEL_EARLY_OFF:
			cmd_desc = display_early_off_cmds.cmd_desc;
			cmd_size = display_early_off_cmds.num_of_cmds;
			break;
		case PANEL_ACL_OFF:
			cmd_desc = acl_off_cmd.cmd_desc;
			cmd_size = acl_off_cmd.num_of_cmds;
			break;
		case PANEL_TOUCHSENSING_ON:
			cmd_desc = touchsensing_on_cmd.cmd_desc;
			cmd_size = touchsensing_on_cmd.num_of_cmds;
			break;
		case PANEL_TOUCHSENSING_OFF:
			cmd_desc = touchsensing_off_cmd.cmd_desc;
			cmd_size = touchsensing_off_cmd.num_of_cmds;
			break;
		case PANEL_READY_TO_OFF_2:
			cmd_desc =  display_ready_off_cmds.cmd_desc;
			cmd_size =  display_ready_off_cmds.num_of_cmds;
			break;
		case DISPLAY_PANEL_ON:
			cmd_desc = display_panel_on_cmds.cmd_desc;
			cmd_size = display_panel_on_cmds.num_of_cmds;
			break;
		case BACKLIGHT_CONTROL_ON:
			cmd_desc = backlight_cont_cmd.cmd_desc;
			cmd_size = backlight_cont_cmd.num_of_cmds;
			break;

		default:
			goto unknown_command;
			;
	}
	pr_info("%s : cmd is the [%d]! ",__func__,cmd);
	if (!cmd_size)
		goto unknown_command;

	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size);

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

	/* Set the initial state to Suspend until it is switched on */
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pr_info("%s:%d, Panel registered succesfully\n", __func__, __LINE__);
	return 0;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_info("[LCD] mdss_dsi_panel_on ++\n");
//	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx); // blocked because of kernel panic
//	mipi_samsung_disp_send_cmd(DISPLAY_PANEL_ON, true);
	mipi_samsung_disp_send_cmd(PANEL_READY_TO_ON, true);

	ret = gpio_direction_output((ctrl->boost_gpio), 1);
	if (ret) {
		pr_err("[LCD]%s: unable to set_direction for ima_nRst [%d]\n", __func__, ctrl->boost_gpio);
	}
	pr_info("[LCD] %s: BOOST GPIO HIGH \n", __func__);
	gpio_set_value((ctrl->boost_gpio), 1);

	/* Init Index Values */
	msd.dstat.curr_elvss_idx = -1;
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_aid_idx = -1;
	msd.dstat.hbm_mode = 0;
	msd.dstat.on = 1;
	msd.mfd->resume_state = MIPI_RESUME_STATE;
	
	pr_info("[LCD] mdss_dsi_panel_on--\n");

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
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,	panel_data);

	mipi  = &pdata->panel_info.mipi;

	pr_info("%s: DISP_BL_CONT_GPIO Low\n", __func__);
	gpio_set_value((ctrl->boost_gpio), 0);

	mutex_lock(&msd.lock);

	msd.dstat.on = 0;
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;

	mipi_samsung_disp_send_cmd(PANEL_DISP_OFF, false);
	// mipi_samsung_disp_send_cmd(PANEL_READY_TO_OFF, false);

	mutex_unlock(&msd.lock);
	pr_info("mdss_dsi_panel_off --\n");

	return 0;
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

static int mdss_panel_parse_dt(struct platform_device *pdev, struct mdss_panel_common_pdata *panel_data)
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
		pr_err("%s:%d, panel resolution not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

#if 0
	rc = of_property_read_u32_array(np, "qcom,mdss-clk-rate", res, 1);
	if (rc) {
		pr_err("%s:%d, panel clock rate not specified\n",
						__func__, __LINE__);
	}
	panel_data->panel_info.clk_rate = (!rc ? res[0] : 0);
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


#if 1
	mdss_samsung_parse_panel_cmd(pdev, &display_qcom_on_cmds, "qcom,panel-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_qcom_off_cmds,"qcom,panel-off-cmds");
#endif
	mdss_samsung_parse_panel_cmd(pdev, &display_on_cmd, "qcom,panel-display-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_off_cmd,"qcom,panel-display-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &backlight_cont_cmd,	"qcom,panel-backlightcontrol-cmds");

	return 0;

	return -EINVAL;
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
		pr_info("%s:%d, panel name not specified\n", __func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);


	printk(KERN_DEBUG "Is_There_cmc624 : CMC624 is there!!!!");
	samsung_cmc624_init();

	if(is_panel_supported(panel_name)) {
		LCD_DEBUG("Panel : %s is not supported:",panel_name);
		return -1;
	}

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata);
	if (rc)
		return rc;

	vendor_pdata.on = mdss_dsi_panel_on;
	vendor_pdata.off = mdss_dsi_panel_off;
	vendor_pdata.bl_fnc = mdss_dsi_panel_bl_ctrl;
	vendor_pdata.panel_reset = mdss_dsi_samsung_panel_reset;
	vendor_pdata.panel_registered = mdss_dsi_panel_registered;

	/* Init driver  specific data */
	mutex_init(&msd.lock);

	rc = dsi_panel_device_register(pdev, &vendor_pdata);
	if (rc)
		return rc;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL, &mipi_samsung_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return rc;
	}

	sysfs_remove_file(&lcd_device->dev.kobj, &dev_attr_lcd_power.attr);

	rc = sysfs_create_file(&lcd_device->dev.kobj,&dev_attr_lcd_power.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n", dev_attr_lcd_power.attr.name);
	}

	rc = sysfs_create_file(&lcd_device->dev.kobj,&dev_attr_siop_enable.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_siop_enable.attr.name);
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
#ifdef CONFIG_SAMSUNG_CMC624
		rc = cmc624_sysfs_init();
		if (rc < 0)
			pr_debug("CMC624 sysfs initialize FAILED\n");

#endif
	msd.dstat.on = 0;
	return 0;
}

//melius
static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "novatek,mdss-tft-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
		.name   = "novatek_tft_dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

int get_lcd_attached(void)
{
	return lcd_attached;
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

	pr_info("%s %s", __func__, lcd_attached == 1 ?
				"lcd_attached" : "lcd_detached");
	return 1;
}
__setup("lcd_attached=", lcd_attached_status);

static int __init mdss_dsi_panel_init(void)
{
	printk("%s: get_lcd_attached(%d)!\n", __func__, get_lcd_attached());

	if (get_lcd_attached() == 0)    {
                printk("%s: get_lcd_attached(%d)!\n", __func__, get_lcd_attached());
		// return -ENODEV;
        }
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
