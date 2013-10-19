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
#include <linux/lcd.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "mdss_dsi.h"
#include "samsung_dsi_panel.h"
#include "mdss_fb.h"

#define MAGNA_LCD
#define DT_CMD_HDR 6

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock);
		
static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;

static struct dsi_cmd_desc *dsi_panel_on_cmds;
static struct dsi_cmd_desc *dsi_panel_off_cmds;
static int num_of_on_cmds;
static int num_of_off_cmds;
static char *on_cmds, *off_cmds;

#ifdef  MAGNA_LCD
static struct dsi_cmd_desc *dsi_panel_on_cmds_magna;
static int num_of_on_cmds_magna;
static char *on_cmds_magna;
#endif

static struct dsi_cmd nv_cmds;
static struct dsi_cmd nv_enable_cmds;
static struct dsi_cmd nv_disable_cmds;
static struct dsi_cmd manufacture_id_cmds;
static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_cmd;
static struct dsi_cmd acl_off_cmd;

/*
contains the list of acl commands
available for different brightness levels
*/
static struct dsi_cmd acl_cmds_list;
static struct dsi_cmd elvss_cmds_list;
static struct dsi_cmd aid_cmds_list;

/*
contains mapping between bl_level and
index number of corresponding acl command
in acl command list
*/
static struct cmd_map acl_map_table;
static struct cmd_map elvss_map_table;
static struct cmd_map aid_map_table;

static char *nv_mem_data;

static struct mipi_samsung_driver_data msd;
/*List of supported Panels with HW revision detail
 * (one structure per project)
 * {hw_rev,"label string given in panel dtsi file"}
 * */
struct  panel_hrev panel_supp_cdp[]= {
	{"samsung amoled 720p video mode dsi S6E8AA3X01 panel"},
	{NULL}
};

static struct dsi_cmd_desc brightness_packet[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0, NULL},
};

DEFINE_LED_TRIGGER(bl_led_trigger);

static struct mdss_dsi_phy_ctrl phy_params;

static int rst_gpio;

static int disp_en;

static char board_rev;

#ifdef MAGNA_LCD
static int oled_id_gpio;
static int disp_is_magna;
#endif

void mdss_dsi_magna_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
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
		gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		wmb();
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}

#ifdef MAGNA_LCD
	disp_is_magna = !gpio_get_value( oled_id_gpio );
	LCD_DEBUG( "disp_is_magna = %d", disp_is_magna );
#endif
}
#if 1
static int get_candela_index(int bl_level)
{
	int backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */

	switch (bl_level) {
	case 0 ... 19:
		backlightlevel = GAMMA_10CD; /* 0*/
		break;
	case 20 ... 29:
		backlightlevel = GAMMA_20CD; /* 1*/
		break;
	case 30 ... 39:
		backlightlevel = GAMMA_30CD; /* 2*/
		break;
	case 40 ... 49:
		backlightlevel = GAMMA_40CD; /* 3 */
		break;
	case 50 ... 59:
		backlightlevel = GAMMA_50CD; /* 4 */
		break;
	case 60 ... 69:
		backlightlevel = GAMMA_60CD; /* 5 */
		break;
	case 70 ... 79:
		backlightlevel = GAMMA_70CD; /* 6 */
		break;
	case 80 ... 89:
		backlightlevel = GAMMA_80CD; /* 7 */
		break;
	case 90 ... 99:
		backlightlevel = GAMMA_90CD; /* 8 */
		break;
	case 100 ... 109:
		backlightlevel = GAMMA_100CD; /* 9 */
		break;
	case 110 ... 119:
		backlightlevel = GAMMA_110CD; /* 10 */
		break;
	case 120 ... 129:
		backlightlevel = GAMMA_120CD; /* 11 */
		break;
	case 130 ... 139:
		backlightlevel = GAMMA_130CD; /* 12 */
		break;
	case 140 ... 149:
		backlightlevel = GAMMA_140CD; /* 13 */
		break;
	case 150 ... 159:
		backlightlevel = GAMMA_150CD; /* 14 */
		break;
	case 160 ... 169:
		backlightlevel = GAMMA_160CD; /* 15 */
		break;
	case 170 ... 179:
		backlightlevel = GAMMA_170CD; /* 16 */
		break;
	case 180 ... 189:
		backlightlevel = GAMMA_180CD; /* 17 */
		break;
	case 190 ... 199:
		backlightlevel = GAMMA_190CD; /* 18 */
		break;
	case 200 ... 209:
		backlightlevel = GAMMA_200CD; /* 19 */
		break;
	case 210 ... 219:
		backlightlevel = GAMMA_210CD; /* 20 */
		break;
	case 220 ... 229:
		backlightlevel = GAMMA_220CD; /* 21 */
		break;
	case 230 ... 239:
		backlightlevel = GAMMA_230CD; /* 22 */
		break;
	case 240 ... 249:
		backlightlevel = GAMMA_240CD; /* 23 */
		break;
	case 250 ... 254:
		backlightlevel = GAMMA_250CD; /* 24 */
		break;
	case 255:
		if (msd.dstat.auto_brightness == 1)
			backlightlevel = GAMMA_300CD; /* 29 */
		else
			backlightlevel = GAMMA_250CD; /* 24 */
		break;
	default:
		backlightlevel = GAMMA_10CD; /* 1 */
		break;
	}
	return backlightlevel;
}
#endif


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
	char temp[20];

	snprintf(temp, strnlen(msd.panel_name, 20) + 1,
						msd.panel_name);
	strlcat(buf, temp, 20);
	return strnlen(buf, 20);
}



static ssize_t mipi_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, sizeof(buf), "%d\n", msd.dstat.acl_on);
	pr_info("acl status: %d\n", *buf);

	return rc;
}

/*
	This function takes aid_map_table and uses cd_idx,
	to get the index of the command in aid command list.

*/
static struct dsi_cmd get_aid_aor_control_set(int cd_idx)
{
	struct dsi_cmd aid_control = {0, 0, 0, 0};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_aid_idx;

	if (!aid_map_table.size || !(cd_idx < aid_map_table.size))
		goto end;

	/* Get index in the aid command list*/
	cmd_idx = aid_map_table.cmd_idx[cd_idx];

	c_payload = aid_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = aid_cmds_list.cmd_desc[p_idx].payload;
		payload_size = aid_cmds_list.cmd_desc[p_idx].dlen;

		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	aid_control.cmd_desc = &(aid_cmds_list.cmd_desc[cmd_idx]);
	aid_control.num_of_cmds = 1;
	msd.dstat.curr_aid_idx = cmd_idx;

	switch (msd.panel) {
	case PANEL_720P_AMOLED_S6E8AA3X01:
		/* Do any panel specfific customization here */
		break;
	}
end:
	return aid_control;

}

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/
static struct dsi_cmd get_acl_control_set(int cd_idx)
{
	struct dsi_cmd acl_control = {0, 0, 0, 0};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_acl_idx;

	if (!acl_map_table.size || !(cd_idx < acl_map_table.size))
		goto end;

	/* Get index in the acl command list*/
	cmd_idx = acl_map_table.cmd_idx[cd_idx];

	c_payload = acl_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = acl_cmds_list.cmd_desc[p_idx].payload;
		payload_size = acl_cmds_list.cmd_desc[p_idx].dlen;
		if (!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	acl_control.cmd_desc = &(acl_cmds_list.cmd_desc[cmd_idx]);
	acl_control.num_of_cmds = 1;
	msd.dstat.curr_acl_idx = cmd_idx;

	switch (msd.panel) {
	case PANEL_720P_AMOLED_S6E8AA3X01:
		/* Do any panel specfific customization here */
		break;
	}


end:
	return acl_control;
}

/*
	This function takes acl_map_table and uses cd_idx,
	to get the index of the command in elvss command list.

*/
static struct dsi_cmd get_elvss_control_set(int cd_idx)
{
	struct dsi_cmd elvss_control = {0, 0, 0, 0};
	int cmd_idx = 0, payload_size = 0;
	char *p_payload, *c_payload;
	int p_idx = msd.dstat.curr_elvss_idx;

	if (!elvss_map_table.size || !(cd_idx < elvss_map_table.size))
		goto end;


	/* Get index in the acl command list*/
	cmd_idx = elvss_map_table.cmd_idx[cd_idx];

	c_payload = elvss_cmds_list.cmd_desc[cmd_idx].payload;

	/* Check if current & previous commands are same */
	if (p_idx >= 0) {
		p_payload = elvss_cmds_list.cmd_desc[p_idx].payload;
		payload_size = elvss_cmds_list.cmd_desc[p_idx].dlen;

		if (msd.dstat.curr_elvss_idx == cmd_idx ||
			!memcmp(p_payload, c_payload, payload_size))
			goto end;
	}

	/* Get the command desc */
	elvss_control.cmd_desc = &(elvss_cmds_list.cmd_desc[cmd_idx]);
	elvss_control.num_of_cmds = 1;
	msd.dstat.curr_elvss_idx = cmd_idx;

	switch (msd.panel) {
	case PANEL_720P_AMOLED_S6E8AA3X01:
		/* Do any panel specfific customization here */
		break;
	}
end:
	return elvss_control;

}

static struct dsi_cmd get_gamma_control_set(int bl_level)
{
	struct dsi_cmd gamma_control = {0, 0, 0, 0};

	return gamma_control;
}

static int update_bright_packet(int cmd_count, struct dsi_cmd *cmd_set)
{
	int i = 0;
	for (i = 0; i < cmd_set->num_of_cmds; i++) {
		brightness_packet[cmd_count].payload = \
			cmd_set->cmd_desc[i].payload;
		brightness_packet[cmd_count].dlen = \
			cmd_set->cmd_desc[i].dlen;
		brightness_packet[cmd_count].dtype = \
		cmd_set->cmd_desc[i].dtype;
		brightness_packet[cmd_count].wait = \
		cmd_set->cmd_desc[i].wait;
		cmd_count++;
	}

	return cmd_count;
}
#if 1
static int make_brightcontrol_set(int bl_level)
{

	struct dsi_cmd elvss_control = {0, 0, 0, 0};
	struct dsi_cmd acl_control = {0, 0, 0, 0};
	struct dsi_cmd aid_control = {0, 0, 0, 0};
	struct dsi_cmd gamma_control = {0, 0, 0, 0};
	int cmd_count = 0, cd_idx = 0;

	cd_idx = get_candela_index(bl_level);

	/* aid/aor */
	aid_control = get_aid_aor_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &aid_control);


	/* acl */
	if (msd.dstat.acl_on) {
		acl_control = get_acl_control_set(cd_idx);
		cmd_count = update_bright_packet(cmd_count, &acl_control);
	}

	/*elvss*/
	elvss_control = get_elvss_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &elvss_control);

	/*gamma*/
	gamma_control = get_gamma_control_set(cd_idx);
	cmd_count = update_bright_packet(cmd_count, &gamma_control);

	LCD_DEBUG("bright_level: %d, candela_idx: %d, "\
		"cmd_count(aid,acl,elvss,gamma)::(%d,%d,%d,%d)%d\n",\
		msd.dstat.bright_level, cd_idx,
		aid_control.num_of_cmds,
		acl_control.num_of_cmds,
		elvss_control.num_of_cmds,
		gamma_control.num_of_cmds,
		cmd_count);
	return cmd_count;

}
#endif
static ssize_t mipi_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd = msd.mfd;

	if (!mfd->panel_power_on) {
		pr_info("%s: panel is off state. updating state value.\n",
			__func__);
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on)
			msd.dstat.acl_on = true;
		else if (sysfs_streq(buf, "0") && msd.dstat.acl_on)
			msd.dstat.acl_on = false;
		else
			pr_info("%s: Invalid argument!!", __func__);
	} else {
		if (sysfs_streq(buf, "1") && !msd.dstat.acl_on) {
			mutex_lock(&msd.lock);
			msd.dstat.acl_on = true;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, false);
			mutex_unlock(&msd.lock);

		} else if (sysfs_streq(buf, "0") && msd.dstat.acl_on) {
			mutex_lock(&msd.lock);
			mipi_samsung_disp_send_cmd(PANEL_ACL_OFF, false);
			msd.dstat.acl_on = false;
			msd.dstat.curr_acl_idx = -1;
			mutex_unlock(&msd.lock);

		} else {
			pr_info("%s: Invalid argument!!", __func__);
		}
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
	else
		pr_info("%s: Invalid argument!!", __func__);

	return size;
}


static struct lcd_ops mipi_samsung_disp_props = {
	.get_power = NULL,
	.set_power = NULL,
};


static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR,
			mipi_samsung_disp_get_power,
			mipi_samsung_disp_set_power);

static DEVICE_ATTR(lcd_type, S_IRUGO, mipi_samsung_disp_lcdtype_show, NULL);

static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_disp_acl_show,
			mipi_samsung_disp_acl_store);
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_auto_brightness_show,
			mipi_samsung_auto_brightness_store);

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

static int samsung_nv_read(struct dsi_cmd_desc *desc, char *destBuffer,
		int srcLength, struct mdss_panel_data *pdata)
{
	int loop_limit = 0;
	/* first byte is size of Register */
	static char packet_size[] = { 0x07, 0 };
	static struct dsi_cmd_desc s6e8aa0_packet_size_cmd = {
		DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size),
		packet_size };

	/* second byte is Read-position */
	static char reg_read_pos[] = { 0xB0, 0x00 };
	static struct dsi_cmd_desc s6e8aa0_read_pos_cmd = {
		DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos),
		reg_read_pos };

	int read_pos = 0;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256];
	int show_buffer_pos = 0;
	int read_size = 0;

	show_buffer_pos +=
		snprintf(show_buffer, 256, "read_reg : %X[%d] : ",
		desc[0].payload[0], srcLength);

	loop_limit = (srcLength + packet_size[0] - 1)
				/ packet_size[0];
	mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf,
				&(s6e8aa0_packet_size_cmd), 1);
	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		reg_read_pos[1] = read_pos;
		read_size = ((srcLength - read_pos) < packet_size[0]) ?
					(srcLength - read_pos) : packet_size[0];

		if (mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf,
					&(s6e8aa0_read_pos_cmd), 1) < 1) {
			show_buffer_pos += snprintf(show_buffer
						+ show_buffer_pos, 256,
						"Tx command FAILED");
			break;
		}

		read_count = mdss_dsi_cmds_rx(pdata, &dsi_panel_tx_buf,
					&dsi_panel_rx_buf, desc,
					 read_size);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer +
						show_buffer_pos, 256, "%02x ",
						dsi_panel_rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					dsi_panel_rx_buf.data[i];
			}
		}
		show_buffer_pos += snprintf(show_buffer +
				show_buffer_pos, 256, ".");
		read_pos += read_count;

		if (read_pos >= srcLength)
			break;
	}

	pr_info("%s\n", show_buffer);
	return read_pos;
}

static int mipi_samsung_read_nv_mem(struct mdss_panel_data *pdata)
{
	int nv_size = 0;
	int nv_read_cnt = 0;
	int i = 0;

	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, false);

	for (i = 0; i < nv_cmds.num_of_cmds; i++)
		nv_size += nv_cmds.read_size[i];

	 nv_mem_data = kzalloc(sizeof(char) * nv_size, GFP_KERNEL);
	 if (!nv_mem_data)
		return -ENOMEM;

	for (i = 0; i < nv_cmds.num_of_cmds; i++) {
		int count = 0;
		int read_size = nv_cmds.read_size[i];
		count = samsung_nv_read(&(nv_cmds.cmd_desc[i]),
				&nv_mem_data[nv_read_cnt], read_size, pdata);
		nv_read_cnt += count;
		if (count != read_size)
			pr_err("Error reading LCD NV data !!!!\n");
	}

	mipi_samsung_disp_send_cmd(PANEL_MTP_DISABLE, false);

	return nv_read_cnt;
}

static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	struct dsi_buf *rp, *tp;

	unsigned int id = 0 ;


	if (!manufacture_id_cmds.num_of_cmds)
		return 0;

	tp = &dsi_panel_tx_buf;
	rp = &dsi_panel_rx_buf;
	mdss_dsi_cmds_rx(pdata, tp, rp,
		&manufacture_id_cmds.cmd_desc[0],
		manufacture_id_cmds.read_size[0]);

	pr_info("%s: manufacture_id1=%x\n", __func__, *rp->data);
	id = (*((unsigned int *)rp->data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmds_rx(pdata, tp, rp,
		&manufacture_id_cmds.cmd_desc[1],
		manufacture_id_cmds.read_size[1]);
	pr_info("%s: manufacture_id2=%x\n", __func__, *rp->data);
	id |= (*((unsigned int *)rp->data) & 0xFF);
	id <<= 8;

	mdss_dsi_cmds_rx(pdata, tp, rp,
		&manufacture_id_cmds.cmd_desc[2],
		manufacture_id_cmds.read_size[2]);
	pr_info("%s: manufacture_id3=%x\n", __func__, *rp->data);
	id |= (*((unsigned int *)rp->data) & 0xFF);

	pr_info("%s: manufacture_id=%x\n", __func__, id);

	return id;
}


static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (ctrl_pdata->bl_ctrl) {
		switch (ctrl_pdata->bl_ctrl) {
		case BL_WLED:
			led_trigger_event(bl_led_trigger, bl_level);
			break;
		case BL_DCS_CMD:
			mutex_lock(&msd.lock);
			msd.dstat.bright_level = bl_level;
			mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, false);
			mutex_unlock(&msd.lock);
			break;

		default:
			pr_err("%s: Unknown bl_ctrl configuration\n",
				__func__);
			break;
		}
	} else
		pr_err("%s:%d, bl_ctrl not configured", __func__, __LINE__);

}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
/* fix build error temporary */
/*	struct msm_fb_data_type *mfd = msd.mfd;*/
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;

	if (lock)
		mutex_lock(&msd.lock);


		switch (cmd) {
		case PANEL_READY_TO_ON:
#ifdef MAGNA_LCD
			if( disp_is_magna )
			{
				cmd_desc = dsi_panel_on_cmds_magna;
				cmd_size = num_of_on_cmds_magna;
				break;
			}
#endif
			cmd_desc = dsi_panel_on_cmds;
			cmd_size = num_of_on_cmds;
			break;
		case PANEL_DISP_OFF:
			cmd_desc = dsi_panel_off_cmds;
			cmd_size = num_of_off_cmds;
			break;
		case PANEL_DISPLAY_ON:
			cmd_desc = display_on_cmd.cmd_desc;
			cmd_size = display_on_cmd.num_of_cmds;
			break;
		case PANEL_DISPLAY_OFF:
			cmd_desc = display_off_cmd.cmd_desc;
			cmd_size = display_off_cmd.num_of_cmds;
			break;
		case PANEL_BRIGHT_CTRL:
			cmd_desc = brightness_packet;
			cmd_size = make_brightcontrol_set(msd.dstat.bright_level);
			break;
		case PANEL_MTP_ENABLE:
			cmd_desc = nv_enable_cmds.cmd_desc;
			cmd_size = nv_enable_cmds.num_of_cmds;
			break;
		case PANEL_MTP_DISABLE:
			cmd_desc = nv_disable_cmds.cmd_desc;
			cmd_size = nv_disable_cmds.num_of_cmds;
			break;
		case PANEL_NEED_FLIP:
			/*
				May be required by Panel Like Fusion3
			*/
			break;
		case PANEL_ACL_ON:
			/*
				May be required by panel like D2,Commanche
			*/
			break;
		case PANEL_ACL_OFF:
			cmd_desc = acl_off_cmd.cmd_desc;
			cmd_size = acl_off_cmd.num_of_cmds;
			break;

		default:
			goto unknown_command;
			;
	}

	if (!cmd_size)
		return 0;
/* fix build error temporary */
//	if (mfd->panel_info.type == MIPI_CMD_PANEL) {
		/* mdp4_dsi_cmd_busy_wait: will turn on dsi clock also
			TODO:
		*/

//	}

	mdss_dsi_cmds_tx(msd.pdata, &dsi_panel_tx_buf, cmd_desc, cmd_size);

	if (lock)
		mutex_unlock(&msd.lock);


	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

	if (lock)
		mutex_unlock(&msd.lock);

	return -EINVAL;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;

	mipi  = &pdata->panel_info.mipi;


	msd.pdata = pdata;

	pr_debug("%s:%d, debug info (mode) : %d\n", __func__, __LINE__,
		 mipi->mode);
	if (!msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);

	if (!msd.dstat.is_smart_dim_loaded) {
		mipi_samsung_read_nv_mem(pdata);
		/* Initialize smart dimming related
			things here */
		msd.dstat.is_smart_dim_loaded = true;
	}

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_samsung_disp_send_cmd(PANEL_READY_TO_ON, false);

	} else {
		pr_err("%s:%d, CMD MODE NOT SUPPORTED", __func__, __LINE__);
		return -EINVAL;
	}

	/* Recovery Mode : Set some default brightness */
	if (msd.dstat.recovery_boot_mode) {
		msd.dstat.bright_level = RECOVERY_BRIGHTNESS;
		mipi_samsung_disp_send_cmd(PANEL_BRIGHT_CTRL, false);
	}
	/* Init Index Values */
	msd.dstat.curr_elvss_idx = -1;
	msd.dstat.curr_acl_idx = -1;
	msd.dstat.curr_aid_idx = -1;

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;

	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info\n", __func__, __LINE__);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_samsung_disp_send_cmd(PANEL_DISP_OFF, false);
	} else {
		pr_debug("%s:%d, CMD mode not supported", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mdss_samsung_parse_panel_table(struct platform_device *pdev,
		struct cmd_map *table, char *keystring)
{
		const unsigned short *data;
		int  data_offset, len = 0 , i = 0;

		struct device_node *np = pdev->dev.of_node;
		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read table %s ",
				__func__, __LINE__, keystring);
			return -EINVAL;
		}
		if ((len % 2) != 0) {
			pr_err("%s:%d, Incorrect table entries for %s",
						__func__, __LINE__, keystring);
			return -EINVAL;
		}
		table->size = len / (sizeof(short)*2);
		table->bl_level = kzalloc(sizeof(int) * table->size,
					GFP_KERNEL);
		if (!table->bl_level)
			return -ENOMEM;

		table->cmd_idx = kzalloc(sizeof(int) * table->size, GFP_KERNEL);
		if (!table->cmd_idx)
			goto error;

		data_offset = 0;
		for (i = 0 ; i < table->size; i++) {
			table->bl_level[i] = data[data_offset++];
			table->cmd_idx[i] = data[data_offset++];
		}

		return 0;
error:
	kfree(table->cmd_idx);

	return -ENOMEM;
}

static int mdss_samsung_parse_panel_cmd(struct platform_device *pdev,
		struct dsi_cmd *commands, char *keystring)
{

		const char *data;
		int cmd_plen, data_offset, len = 0, i = 0;
		int is_read = 0;
		struct device_node *np = pdev->dev.of_node;
		data = of_get_property(np, keystring, &len);
		if (!data) {
			pr_err("%s:%d, Unable to read %s",
				__func__, __LINE__, keystring);
			goto error;
		}

		commands->cmds_buff = kzalloc(sizeof(char) * len, GFP_KERNEL);
		if (!commands->cmds_buff)
			return -ENOMEM;

		memcpy(commands->cmds_buff, data, len);

		data_offset = 0;
		cmd_plen = 0;
		while ((len - data_offset) >= DT_CMD_HDR) {
			int type = commands->cmds_buff[data_offset];

			data_offset += (DT_CMD_HDR - 1);
			cmd_plen = commands->cmds_buff[data_offset++];
			data_offset += cmd_plen;

			if (type == DTYPE_GEN_READ ||
				type == DTYPE_GEN_READ1 ||
				type == DTYPE_GEN_READ2 ||
				type == DTYPE_DCS_READ)	{
				/* Read command :last byte contain read size*/
				data_offset++;
				is_read = 1;
			}

			commands->num_of_cmds++;
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

		}

		if (!commands->num_of_cmds) {
			pr_err("%s:%d, No %s cmds specified",
				__func__, __LINE__, keystring);
			goto error;
		}

		commands->cmd_desc = kzalloc(commands->num_of_cmds
					* sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
		if (!commands->cmd_desc)
			goto error1;

		data_offset = 0;
		for (i = 0; i < commands->num_of_cmds; i++) {
			commands->cmd_desc[i].dtype =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].last =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].vc =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].ack =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].wait =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].dlen =
					commands->cmds_buff[data_offset++];
			commands->cmd_desc[i].payload =
					&(commands->cmds_buff[data_offset]);

			data_offset += (commands->cmd_desc[i].dlen);
			if (is_read)
				commands->read_size[i] =
					commands->cmds_buff[data_offset++];

		}

		if (data_offset != len) {
			pr_err("%s:%d, Incorrect OFF command entries",
							__func__, __LINE__);
			goto error;
		}

		return 0;

error:
	kfree(commands->cmd_desc);
error1:
	kfree(commands->read_size);
error2:
	kfree(commands->cmds_buff);

	return -EINVAL;

}

static int mdss_panel_parse_dt(struct platform_device *pdev,
			       struct mdss_panel_common_pdata *panel_data,
			       char *bl_ctrl)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc, i, len;
	int cmd_plen, data_offset;
	const char *data;
	static const char *bl_ctrl_type;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-active-res", res, 2);
	if (rc == 0) {
		panel_data->panel_info.lcdc.xres_pad =
			panel_data->panel_info.xres - res[0];
		panel_data->panel_info.lcdc.yres_pad =
			panel_data->panel_info.yres - res[1];
	}

	disp_en = of_get_named_gpio(np, "qcom,enable-gpio", 0);
	if (!gpio_is_valid(disp_en)) {
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);
		return -ENODEV;
	}

	rc = gpio_request(disp_en, "disp_enable");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		gpio_free(disp_en);
		return -ENODEV;
	}
	rc = gpio_direction_output(disp_en, 1);
	if (rc) {
		pr_err("set_direction for disp_en gpio failed, rc=%d\n",
			rc);
		gpio_free(disp_en);
		return -ENODEV;
	}

	rst_gpio = of_get_named_gpio(np, "qcom,rst-gpio", 0);
	if (!gpio_is_valid(rst_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(rst_gpio, "disp_rst_n");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
				rc);
			gpio_free(rst_gpio);
			gpio_free(disp_en);
			return -ENODEV;
		}
	}

#ifdef MAGNA_LCD
	oled_id_gpio = of_get_named_gpio(np, "qcom,oled-id-gpio", 0);
	if (!gpio_is_valid(oled_id_gpio)) {
		pr_err("%s:%d, oled_id_gpio gpio not specified\n",
						__func__, __LINE__);
		return -ENODEV;
	}

	rc = gpio_request(oled_id_gpio, "disp_oled_id");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		gpio_free(oled_id_gpio);
		return -ENODEV;
	}
	rc = gpio_direction_input(oled_id_gpio);
	if (rc) {
		pr_err("set_direction for oled_idgpio failed, rc=%d\n",
			rc);
		gpio_free(oled_id_gpio);
		return -ENODEV;
	}
#endif

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);

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
	if (!strncmp(bl_ctrl_type, "bl_ctrl_wled", 12)) {
		led_trigger_register_simple("bkl-trigger", &bl_led_trigger);
		pr_debug("%s: SUCCESS-> WLED TRIGGER register\n", __func__);
		*bl_ctrl = BL_WLED;
	} else if (!strncmp(bl_ctrl_type, "bl_ctrl_dcs_cmds", 12)) {
		pr_debug("%s: SUCCESS-> DCS CMD BACKLIGHT register\n",
			 __func__);
		*bl_ctrl = BL_DCS_CMD;
	}

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	panel_data->panel_info.mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

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

	data = of_get_property(np, "qcom,panel-on-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read ON cmds", __func__, __LINE__);
		return -EINVAL;
	}

	on_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!on_cmds)
		return -ENOMEM;

	memcpy(on_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = on_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_on_cmds++;
	}
	if (!num_of_on_cmds) {
		pr_err("%s:%d, No ON cmds specified", __func__, __LINE__);
		goto error3;
	}

	dsi_panel_on_cmds =
		kzalloc((num_of_on_cmds * sizeof(struct dsi_cmd_desc)),
						GFP_KERNEL);
	if (!dsi_panel_on_cmds)
		goto error3;

	data_offset = 0;
	for (i = 0; i < num_of_on_cmds; i++) {
		dsi_panel_on_cmds[i].dtype = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].last = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].vc = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].ack = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].wait = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].dlen = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].payload = &on_cmds[data_offset];
		data_offset += (dsi_panel_on_cmds[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect ON command entries",
						__func__, __LINE__);
		goto error2;
	}

#ifdef MAGNA_LCD
	data = of_get_property(np, "qcom,magnaPanel-on-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read magnaPanel-on-cmds", __func__, __LINE__);
		return -EINVAL;
	}

	on_cmds_magna = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!on_cmds_magna)
		return -ENOMEM;

	memcpy(on_cmds_magna, data, len);

	data_offset = 0;
	cmd_plen = 0;
	num_of_on_cmds_magna = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = on_cmds_magna[data_offset++];
		data_offset += cmd_plen;
		num_of_on_cmds_magna++;
	}
	if (!num_of_on_cmds_magna) {
		pr_err("%s:%d, No magnaPanel-on-cmds specified", __func__, __LINE__);
		goto error3;
	}

	dsi_panel_on_cmds_magna =
		kzalloc((num_of_on_cmds_magna * sizeof(struct dsi_cmd_desc)),
						GFP_KERNEL);
	if (!dsi_panel_on_cmds_magna)
		goto error3;

	data_offset = 0;
	for (i = 0; i < num_of_on_cmds_magna; i++) {
		dsi_panel_on_cmds_magna[i].dtype = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].last = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].vc = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].ack = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].wait = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].dlen = on_cmds_magna[data_offset++];
		dsi_panel_on_cmds_magna[i].payload = &on_cmds_magna[data_offset];
		data_offset += (dsi_panel_on_cmds_magna[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect magnaPanel-on-cmds command entries",
						__func__, __LINE__);
		goto error2;
	}
#endif

	data = of_get_property(np, "qcom,panel-off-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read OFF cmds", __func__, __LINE__);
		goto error2;
	}

	off_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!off_cmds)
		goto error2;

	memcpy(off_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = off_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_off_cmds++;
	}
	if (!num_of_off_cmds) {
		pr_err("%s:%d, No OFF cmds specified", __func__, __LINE__);
		goto error1;
	}

	dsi_panel_off_cmds = kzalloc(num_of_off_cmds
				* sizeof(struct dsi_cmd_desc),
					GFP_KERNEL);
	if (!dsi_panel_off_cmds)
		goto error1;

	data_offset = 0;
	for (i = 0; i < num_of_off_cmds; i++) {
		dsi_panel_off_cmds[i].dtype = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].last = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].vc = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].ack = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].wait = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].dlen = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].payload = &off_cmds[data_offset];
		data_offset += (dsi_panel_off_cmds[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect OFF command entries",
						__func__, __LINE__);
		goto error;
	}
	mdss_samsung_parse_panel_cmd(pdev, &nv_cmds,
				"samsung,panel-nv-read-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &nv_enable_cmds,
				"samsung,panel-nv-read-enable-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &nv_disable_cmds,
				"samsung,panel-nv-read-disable-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &manufacture_id_cmds,
				"samsung,panel-manufacture-id-read-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_on_cmd,
				"qcom,panel-display-on-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &display_off_cmd,
				"qcom,panel-display-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &acl_off_cmd,
				"samsung,panel-acl-off-cmds");
	mdss_samsung_parse_panel_cmd(pdev, &acl_cmds_list,
				"samsung,panel-acl-cmds-list");
	mdss_samsung_parse_panel_cmd(pdev, &elvss_cmds_list,
				"samsung,panel-elvss-cmds-list");
	mdss_samsung_parse_panel_cmd(pdev, &aid_cmds_list,
				"samsung,panel-aid-cmds-list");
	mdss_samsung_parse_panel_table(pdev, &aid_map_table,
				"samsung,panel-aid-map-table");
	mdss_samsung_parse_panel_table(pdev, &acl_map_table,
				"samsung,panel-acl-map-table");
	mdss_samsung_parse_panel_table(pdev, &elvss_map_table,
				"samsung,panel-elvss-map-table");

	return 0;

error:
	kfree(dsi_panel_off_cmds);
error1:
	kfree(off_cmds);
error2:
	kfree(dsi_panel_on_cmds);
error3:
	kfree(on_cmds);
	if (rst_gpio)
		gpio_free(rst_gpio);
	if (disp_en)
		gpio_free(disp_en);

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
		return 0;
	}
	return -EINVAL;
}

static int __devinit mdss_dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct mdss_panel_common_pdata vendor_pdata;
	static const char *panel_name;
	char bl_ctrl = UNKNOWN_CTRL;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#endif

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
				struct backlight_device *bd = NULL;
#endif

	if (pdev->dev.parent == NULL) {
		pr_err("%s: parent device missing\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
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

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata, &bl_ctrl);
	if (rc)
		return rc;

	vendor_pdata.on = mdss_dsi_panel_on;
	vendor_pdata.off = mdss_dsi_panel_off;
	vendor_pdata.bl_fnc = mdss_dsi_panel_bl_ctrl;
	vendor_pdata.panel_reset = mdss_dsi_magna_panel_reset;

	/* Init driver  specific data */
	mutex_init(&msd.lock);

	rc = dsi_panel_device_register(pdev, &vendor_pdata, bl_ctrl);
	if (rc)
		return rc;

	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;

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
					&dev_attr_power_reduce.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_power_reduce.attr.name);
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


	return 0;
}

static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "samsung,mdss-amoled-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
			.name   = "magna_amoled_dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

static int __init mdss_dsi_panel_init(void)
{
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT)\
	|| defined(CONFIG_MACH_KS01LGT)
	if (board_rev >= 2)
		return 0;
#elif defined(CONFIG_MACH_JACTIVESKT)
	return 0;
#else
	return 0;
#endif

	mdss_dsi_buf_alloc(&dsi_panel_tx_buf, ALIGN(DSI_BUF_SIZE, SZ_4K));
	mdss_dsi_buf_alloc(&dsi_panel_rx_buf, ALIGN(DSI_BUF_SIZE, SZ_4K));

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


