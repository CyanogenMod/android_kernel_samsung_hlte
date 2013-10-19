/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm.h>
#include <linux/qpnp/pin.h>

#include <linux/clk.h>
#include <linux/spinlock_types.h>
#include <asm/system.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/dma.h>
#include <linux/kthread.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_mdp.h"
#include "mdss_edp.h"
#include "mdss_debug.h"

#if defined(CONFIG_EDP_TCON_MDNIE)
#include "edp_tcon_mdnie.h"
#endif

#define RGB_COMPONENTS		3
#define VDDA_MIN_UV			1800000	/* uV units */
#define VDDA_MAX_UV			1800000	/* uV units */
#define VDDA_UA_ON_LOAD		100000	/* uA units */
#define VDDA_UA_OFF_LOAD	100		/* uA units */

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
extern void edp_backlight_enable(void);
extern void edp_backlight_disable(void);
static struct completion edp_power_sync;
static int edp_power_state;
static int recovery_mode;
static int edp_power_state;

DEFINE_MUTEX(edp_power_state_chagne);
DEFINE_MUTEX(edp_event_state_chagne);

int get_edp_power_state(void)
{
	return edp_power_state;
}
#endif

#if defined(CONFIG_EDP_ESD_FUNCTION)
static int edp_esd_power_state;
#endif

#define EXTRA_POWER_REVSION 0x08
#define MIN_BL_LEVEL 3

static struct qpnp_pin_cfg  LCD_EN_PM_GPIO_WAKE =
{
	.mode = 1, /*QPNP_PIN_MODE_DIG_OUT*/
	.output_type = 0, /*QPNP_PIN_OUT_BUF_CMOS*/
	.invert = 0, /*QPNP_PIN_INVERT_DISABLE*/
	.pull = 5, /*QPNP_PIN_PULL_NO*/
	.vin_sel = 2,
	.out_strength = 3, /*QPNP_PIN_OUT_STRENGTH_HIGH*/
	.src_sel = 0, /*QPNP_PIN_SEL_FUNC_CONSTANT*/
	.master_en = 1,
};


static struct qpnp_pin_cfg  LCD_EN_PM_GPIO_SLEEP =
{
	.mode = 1, /*QPNP_PIN_MODE_DIG_OUT*/
	.output_type = 0, /*QPNP_PIN_OUT_BUF_CMOS*/
	.invert = 0, /*QPNP_PIN_INVERT_DISABLE*/
	.pull = 4, /*QPNP_PIN_PULL_DN*/
	.vin_sel = 2,
	.out_strength = 3, /*QPNP_PIN_OUT_STRENGTH_HIGH*/
	.src_sel = 0, /*QPNP_PIN_SEL_FUNC_CONSTANT*/
	.master_en = 1,
};

static struct qpnp_pin_cfg  LCD_PWM_PM_GPIO_WAKE = 
{
	.mode = 1, /*QPNP_PIN_MODE_DIG_OUT*/
	.output_type = 0, /*QPNP_PIN_OUT_BUF_CMOS*/
	.invert = 0, /*QPNP_PIN_INVERT_DISABLE*/
	.pull = 5, /*QPNP_PIN_PULL_NO*/
	.vin_sel = 2,
	.out_strength = 3, /*QPNP_PIN_OUT_STRENGTH_HIGH*/
	.src_sel = 3, /*QPNP_PIN_SEL_FUNC_2*/
	.master_en = 1,
};

static struct qpnp_pin_cfg  LCD_PWM_PM_GPIO_SLEEP = 
{
	.mode = 1, /*QPNP_PIN_MODE_DIG_OUT*/
	.output_type = 0, /*QPNP_PIN_OUT_BUF_CMOS*/
	.invert = 0, /*QPNP_PIN_INVERT_DISABLE*/
	.pull = 5, /*QPNP_PIN_PULL_NO*/
	.vin_sel = 2,
	.out_strength = 3, /*QPNP_PIN_OUT_STRENGTH_HIGH*/
	.src_sel = 0, /*QPNP_PIN_SEL_FUNC_CONSTANT*/
	.master_en = 1,
};

/*
 * Set uA and enable vdda
 */
static int mdss_edp_regulator_on(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	ret = regulator_set_optimum_mode(edp_drv->vdda_vreg, VDDA_UA_ON_LOAD);
	if (ret < 0) {
		pr_err("%s: vdda_vreg set regulator mode failed.\n", __func__);
		return ret;
	}

	ret = regulator_enable(edp_drv->vdda_vreg);
	if (ret) {
		pr_err("%s: Failed to enable vdda_vreg regulator.\n", __func__);
		return ret;
	}

#if defined(CONFIG_MACH_VIENNAEUR)
	if (system_rev >= EXTRA_POWER_REVSION) {
		ret = regulator_set_optimum_mode(edp_drv->i2c_vreg, VDDA_UA_ON_LOAD);
		if (ret < 0) {
			pr_err("%s: i2c_vreg set regulator mode failed.\n", __func__);
			return ret;
		}

		ret = regulator_enable(edp_drv->i2c_vreg);
		if (ret) {
			pr_err("%s: Failed to enable i2c_vreg regulator.\n", __func__);
			return ret;
		}
	}
#endif

	return 0;
}

/*
 * Init regulator needed for edp, 8974_l12
 */
static int mdss_edp_regulator_init(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	edp_drv->vdda_vreg = devm_regulator_get(&(edp_drv->pdev->dev), "vdda");
	if (IS_ERR(edp_drv->vdda_vreg)) {
		pr_err("%s: Could not get 8941_l12, ret = %ld\n", __func__,
				PTR_ERR(edp_drv->vdda_vreg));
		return -ENODEV;
	}

	ret = regulator_set_voltage(edp_drv->vdda_vreg,
			VDDA_MIN_UV, VDDA_MAX_UV);
	if (ret) {
		pr_err("%s: vdda_vreg set_voltage failed, ret=%d\n", __func__,
				ret);
		return -EINVAL;
	}

#if defined(CONFIG_MACH_VIENNAEUR)
	if (system_rev >= EXTRA_POWER_REVSION) {
		edp_drv->i2c_vreg = devm_regulator_get(&(edp_drv->pdev->dev), "i2c_vreg");
		if (IS_ERR(edp_drv->vdda_vreg)) {
			pr_err("%s: Could not get i2c_vreg, ret = %ld\n", __func__,
					PTR_ERR(edp_drv->i2c_vreg));
			return -ENODEV;
		}

		ret = regulator_set_voltage(edp_drv->i2c_vreg, 2500000, 2500000);
		if (ret) {
			pr_err("%s: i2c_vreg set_voltage failed, ret=%d\n", __func__,
					ret);
			return -EINVAL;
		}

		config_i2c_lane(true);
	}
#endif

	return 0;
}


/*
 * Disable vdda and set uA
 */
static int mdss_edp_regulator_off(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	ret = regulator_disable(edp_drv->vdda_vreg);
	if (ret) {
		pr_err("%s: Failed to disable vdda_vreg regulator.\n",
				__func__);
		return ret;
	}

	ret = regulator_set_optimum_mode(edp_drv->vdda_vreg, VDDA_UA_OFF_LOAD);
	if (ret < 0) {
		pr_err("%s: vdda_vreg set regulator mode failed.\n",
				__func__);
		return ret;
	}

#if defined(CONFIG_MACH_VIENNAEUR)
	if (system_rev >= EXTRA_POWER_REVSION) {
		ret = regulator_disable(edp_drv->i2c_vreg);
		if (ret) {
			pr_err("%s: Failed to disable i2c_vreg regulator.\n",
					__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(edp_drv->i2c_vreg, VDDA_UA_OFF_LOAD);
		if (ret < 0) {
			pr_err("%s: i2c_vreg set regulator mode failed.\n",
					__func__);
			return ret;
		}

		config_i2c_lane(false);
	}
#endif

	return 0;
}

/*
 * Enables the gpio that supply power to the panel and enable the backlight
 */
static int mdss_edp_gpio_panel_en(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;

	edp_drv->gpio_panel_en = of_get_named_gpio(edp_drv->pdev->dev.of_node,
			"gpio-panel-en", 0);
	if (!gpio_is_valid(edp_drv->gpio_panel_en)) {
		pr_err("%s: gpio_panel_en=%d not specified\n", __func__,
				edp_drv->gpio_panel_en);
		goto gpio_err;
	}

	ret = gpio_request(edp_drv->gpio_panel_en, "disp_enable");
	if (ret) {
		pr_err("%s: Request reset gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		return ret;
	}

	ret = gpio_direction_output(edp_drv->gpio_panel_en, 1);
	if (ret) {
		pr_err("%s: Set direction for gpio_panel_en failed, ret=%d\n",
				__func__, ret);
		goto gpio_free;
	}

	return 0;

gpio_free:
	gpio_free(edp_drv->gpio_panel_en);
gpio_err:
	return -ENODEV;
}


static int mdss_edp_pwm_config(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;

	ret = of_property_read_u32(edp_drv->pdev->dev.of_node,
			"qcom,panel-pwm-period", &edp_drv->pwm_period);
	if (ret) {
		pr_err("%s: panel pwm period is not specified, %d", __func__,
				edp_drv->pwm_period);
		return -EINVAL;
	}

	ret = of_property_read_u32(edp_drv->pdev->dev.of_node,
			"qcom,panel-lpg-channel", &edp_drv->lpg_channel);
	if (ret) {
		pr_err("%s: panel lpg channel is not specified, %d", __func__,
				edp_drv->lpg_channel);
		return -EINVAL;
	}

	edp_drv->bl_pwm = pwm_request(edp_drv->lpg_channel, "lcd-backlight");
	if (edp_drv->bl_pwm == NULL || IS_ERR(edp_drv->bl_pwm)) {
		pr_err("%s: pwm request failed", __func__);
		edp_drv->bl_pwm = NULL;
		return -EIO;
	}

	edp_drv->gpio_panel_pwm = of_get_named_gpio(edp_drv->pdev->dev.of_node,
			"gpio-panel-pwm", 0);
	if (!gpio_is_valid(edp_drv->gpio_panel_pwm)) {
		pr_err("%s: gpio_panel_pwm=%d not specified\n", __func__,
				edp_drv->gpio_panel_pwm);
		goto edp_free_pwm;
	}

	ret = gpio_request(edp_drv->gpio_panel_pwm, "disp_pwm");
	if (ret) {
		pr_err("%s: Request reset gpio_panel_pwm failed, ret=%d\n",
				__func__, ret);
		goto edp_free_pwm;
	}

	return 0;

edp_free_pwm:
	pwm_free(edp_drv->bl_pwm);
	return -ENODEV;
}

#define FACTOR_FOR_DUTY 210

int get_duty_level(u32 bl_level, int bl_max)
{
	int duty_level;

	duty_level = (FACTOR_FOR_DUTY * bl_level) / bl_max;

	return duty_level;
}

void mdss_edp_set_backlight(struct mdss_panel_data *pdata, u32 bl_level)
{
	int ret = 0;
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int bl_max;
	int duty_level; /* 0~200 */
	int duty;

	if (bl_level < MIN_BL_LEVEL) {
		pr_err("%s : bl_level(%d) is too low.. MIN (3)\n", __func__, bl_level);
		return;
	}

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata, panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (edp_drv->bl_pwm == NULL) {
		pr_err("%s: edp_drv->bl_pwm=NULL.\n", __func__);
		return;
	}

	bl_max = edp_drv->panel_data.panel_info.bl_max;
	if (bl_level > bl_max)
		bl_level = bl_max;

	duty_level = get_duty_level(bl_level, bl_max);

	if (edp_drv->duty_level == duty_level) {
		pr_err("%s : same duty level..(%d) do not pwm_config..\n", __func__, duty_level);
		return;
	}

	duty = (duty_level * edp_drv->pwm_period) / FACTOR_FOR_DUTY;	

	ret = pwm_config(edp_drv->bl_pwm, duty,	edp_drv->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(edp_drv->bl_pwm);
	if (ret) {
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
		return;
	}
#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	tcon_pwm_duty(duty_level * 100 / FACTOR_FOR_DUTY, 1);
#endif

#if defined(CONFIG_EDP_ESD_FUNCTION)
	edp_drv->current_bl = bl_level;
#endif
	edp_drv->duty_level = duty_level;

	pr_info("%s bl_level : %d duty_level : %d duty : %d period : %d", 
				__func__, bl_level, duty_level, duty, edp_drv->pwm_period);
}

void mdss_edp_config_sync(unsigned char *base)
{
	int ret = 0;

	ret = edp_read(base + 0xc); /* EDP_CONFIGURATION_CTRL */
	ret &= ~0x733;
	ret |= (0x75 & 0x733);
	edp_write(base + 0xc, ret);
	edp_write(base + 0xc, 0x177); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_config_sw_div(unsigned char *base)
{
	edp_write(base + 0x14, 0x217); /* EDP_SOFTWARE_MVID */
	edp_write(base + 0x18, 0x21a); /* EDP_SOFTWARE_NVID */
}

static void mdss_edp_config_static_mdiv(unsigned char *base)
{
	int ret = 0;

	ret = edp_read(base + 0xc); /* EDP_CONFIGURATION_CTRL */
	edp_write(base + 0xc, ret | 0x2); /* EDP_CONFIGURATION_CTRL */
	edp_write(base + 0xc, 0x77); /* EDP_CONFIGURATION_CTRL */
}

static void mdss_edp_enable(struct mdss_edp_drv_pdata *ep, int enable)
{
	struct display_timing_desc *dp = &ep->edid.timing[0];

	if (enable) {
		edp_write(ep->base + 0x1c, (dp->v_addressable + dp->v_blank) << 16 | (dp->h_addressable + dp->h_blank)); /*MDSS_EDP_TOTAL_HOR_VER-*/
		edp_write(ep->base + 0x20, (dp->v_blank - dp->v_fporch) << 16 | (dp->h_blank - dp->h_fporch)); /*MDSS_EDP_START_HOR_VER_FROM_SYNC-*/
		edp_write(ep->base + 0x24, (dp->vsync_pol << 31) | (dp->v_sync_pulse << 16) | (dp->hsync_pol << 15) | (dp->h_sync_pulse)); /*MDSS_EDP_HSYNC_VSYNC_WIDTH_POLARITY-*/
		edp_write(ep->base + 0x28, (dp->v_addressable << 16) | (dp->h_addressable)); /*MDSS_EDP_ACTIVE_HOR_VER-*/

		edp_write(ep->base + 0xc, 0x137); /*MDSS_EDP_CONFIGURATION_CTRL*/
		edp_write(ep->base + 0x14, 0x217); /*MDSS_EDP_SOFTWARE_MVID*/
		edp_write(ep->base + 0x18, 0x21a); /*MDSS_EDP_SOFTWARE_NVID*/

		edp_write(ep->base + 0x518, 0x6c); /* EDP_PHY_EDPPHY_GLB_MISC9 */
		edp_write(ep->base + 0x2c, 0x21); /* EDP_MISC1_MISC0 */

		edp_write(ep->base + 0x144, 0x5); /*MDSS_EDP_TPG_VIDEO_CONFIG*/

		edp_write(ep->base + EDP_STATE_CTRL, 0x40); /* EDP_STATE_CTRL */
	} else {
		edp_write(ep->base + EDP_STATE_CTRL, 0x0); /* EDP_STATE_CTRL */
	}

	edp_write(ep->base + 0x94, enable); /* EDP_TIMING_ENGINE_EN */
	edp_write(ep->base + 0x4, enable); /* EDP_MAINLINK_CTRL */
}

int mdss_edp_on(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int i;

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_lock(&edp_power_state_chagne);
	INIT_COMPLETION(edp_power_sync);
#endif

	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
			panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_info("%s:+\n", __func__);

	qpnp_pin_config(edp_drv->gpio_panel_en, &LCD_EN_PM_GPIO_WAKE);
	qpnp_pin_config(edp_drv->gpio_panel_pwm, &LCD_PWM_PM_GPIO_WAKE);

	gpio_set_value(edp_drv->gpio_panel_en, 0);

	mdss_edp_regulator_on(edp_drv);
	mdss_edp_prepare_clocks(edp_drv);
	mdss_edp_phy_sw_reset(edp_drv->base);
	mdss_edp_hw_powerup(edp_drv->base, 1);
	mdss_edp_pll_configure(edp_drv->base, edp_drv->edid.timing[0].pclk);
	mdss_edp_clk_enable(edp_drv);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	for (i = 0; i < edp_drv->dpcd.max_lane_count; ++i)
		mdss_edp_enable_lane_bist(edp_drv->base, i, 1);

	mdss_edp_enable_mainlink(edp_drv->base, 1);
	mdss_edp_config_clk(edp_drv->base, edp_drv->mmss_cc_base);

	mdss_edp_phy_misc_cfg(edp_drv->base);
	mdss_edp_config_sync(edp_drv->base);
	mdss_edp_config_sw_div(edp_drv->base);
	mdss_edp_config_static_mdiv(edp_drv->base);

	edp_write(edp_drv->base + 0x304, 0x01); /* MDSS_EDP_HPD_CTRL */
	mdss_edp_enable_aux(edp_drv->base, 1);


	mdss_edp_irq_enable();

	gpio_set_value(edp_drv->gpio_panel_en, 1);
	
#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_unlock(&edp_power_state_chagne);

	if (!wait_for_completion_timeout(&edp_power_sync, 3 * HZ)) {
		pr_err("%s: timeout error\n", __func__);
	}

#if defined(CONFIG_EDP_ESD_FUNCTION)
	edp_esd_power_state = 1;
#endif
#endif
	pr_info("%s:-\n", __func__);
	return 0;
}

int mdss_edp_off(struct mdss_panel_data *pdata)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;
	int ret = 0;
	int i;

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_lock(&edp_power_state_chagne);
	edp_power_state = 0;
#if defined(CONFIG_EDP_ESD_FUNCTION)
	edp_esd_power_state = 0;
#endif
#endif
	edp_drv = container_of(pdata, struct mdss_edp_drv_pdata,
					panel_data);
	if (!edp_drv) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_info("%s:+\n", __func__);

	mdss_edp_irq_disable();

	edp_write(edp_drv->base + 0x308, 0);
	edp_write(edp_drv->base + 0x30c, 0);
	edp_write(edp_drv->base  + 0x300, 0); /* EDP_AUX_CTRL */

	
	pwm_disable(edp_drv->bl_pwm);
#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	edp_backlight_disable();
#endif
	gpio_set_value(edp_drv->gpio_panel_en, 0);
	
	mdss_edp_enable(edp_drv, 0);
	mdss_edp_unconfig_clk(edp_drv->base, edp_drv->mmss_cc_base);
	mdss_edp_enable_mainlink(edp_drv->base, 0);

	for (i = 0; i < edp_drv->dpcd.max_lane_count; ++i)
		mdss_edp_enable_lane_bist(edp_drv->base, i, 0);

	mdss_edp_clk_disable(edp_drv);
	mdss_edp_hw_powerup(edp_drv->base, 0);
	mdss_edp_unprepare_clocks(edp_drv);
	mdss_edp_enable_aux(edp_drv->base, 0);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mdss_edp_regulator_off(edp_drv);

	qpnp_pin_config(edp_drv->gpio_panel_en, &LCD_EN_PM_GPIO_SLEEP);
	qpnp_pin_config(edp_drv->gpio_panel_pwm, &LCD_PWM_PM_GPIO_SLEEP);

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_unlock(&edp_power_state_chagne);
#endif
	edp_drv->duty_level = 0;

	pr_info("%s:-\n", __func__);
	return ret;
}

static int mdss_edp_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_lock(&edp_event_state_chagne);
#endif
	pr_info("%s: event=%d\n", __func__, event);
	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_edp_on(pdata);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = mdss_edp_off(pdata);
		break;
	}

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	mutex_unlock(&edp_event_state_chagne);
#endif

	return rc;
}

/*
 * Converts from EDID struct to mdss_panel_info
 */
static void mdss_edp_edid2pinfo(struct mdss_edp_drv_pdata *edp_drv)
{
	struct display_timing_desc *dp;
	struct mdss_panel_info *pinfo;
	struct edp_edid *edid = &edp_drv->edid;

	dp = &edp_drv->edid.timing[0];
	pinfo = &edp_drv->panel_data.panel_info;

	pinfo->clk_rate = dp->pclk;

	pinfo->xres = dp->h_addressable + dp->h_border * 2;
	pinfo->yres = dp->v_addressable + dp->v_border * 2;

	pinfo->width = edid->timing[0].width_mm;
	pinfo->height = edid->timing[0].height_mm;
	
	pinfo->lcdc.h_back_porch = dp->h_blank - dp->h_fporch \
		- dp->h_sync_pulse;
	pinfo->lcdc.h_front_porch = dp->h_fporch;
	pinfo->lcdc.h_pulse_width = dp->h_sync_pulse;

	pinfo->lcdc.v_back_porch = dp->v_blank - dp->v_fporch \
		- dp->v_sync_pulse;
	pinfo->lcdc.v_front_porch = dp->v_fporch;
	pinfo->lcdc.v_pulse_width = dp->v_sync_pulse;

	pinfo->type = EDP_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = edp_drv->edid.color_depth * RGB_COMPONENTS;
	pinfo->fb_num = 2;

	pinfo->lcdc.border_clr = 0;	 /* black */
	pinfo->lcdc.underflow_clr = 0xff; /* blue */
	pinfo->lcdc.hsync_skew = 0;
}

static int __devexit mdss_edp_remove(struct platform_device *pdev)
{
	struct mdss_edp_drv_pdata *edp_drv = NULL;

	edp_drv = platform_get_drvdata(pdev);

	gpio_free(edp_drv->gpio_panel_en);
	mdss_edp_regulator_off(edp_drv);
	iounmap(edp_drv->base);
	iounmap(edp_drv->mmss_cc_base);
	edp_drv->base = NULL;

	return 0;
}

static int mdss_edp_device_register(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	mdss_edp_edid2pinfo(edp_drv);
	edp_drv->panel_data.panel_info.bl_min = 1;
	edp_drv->panel_data.panel_info.bl_max = 255;

	edp_drv->panel_data.event_handler = mdss_edp_event_handler;
	edp_drv->panel_data.set_backlight = mdss_edp_set_backlight;

	ret = mdss_register_panel(edp_drv->pdev, &edp_drv->panel_data);
	if (ret) {
		dev_err(&(edp_drv->pdev->dev), "unable to register eDP\n");
		return ret;
	}

	pr_info("%s: eDP initialized\n", __func__);

	return 0;
}

/*
 * Retrieve edp base address
 */
static int mdss_edp_get_base_address(struct mdss_edp_drv_pdata *edp_drv)
{
	struct resource *res;

	res = platform_get_resource_byname(edp_drv->pdev, IORESOURCE_MEM,
			"edp_base");
	if (!res) {
		pr_err("%s: Unable to get the MDSS EDP resources", __func__);
		return -ENOMEM;
	}

	edp_drv->base_size = resource_size(res);
	edp_drv->base = ioremap(res->start, resource_size(res));
	if (!edp_drv->base) {
		pr_err("%s: Unable to remap EDP resources",  __func__);
		return -ENOMEM;
	}

	pr_info("%s: drv=%x base=%x size=%x\n", __func__,
		(int)edp_drv, (int)edp_drv->base, edp_drv->base_size);

	mdss_debug_register_base("edp",
			edp_drv->base, edp_drv->base_size);
	return 0;
}

static int mdss_edp_get_mmss_cc_base_address(struct mdss_edp_drv_pdata
		*edp_drv)
{
	struct resource *res;

	res = platform_get_resource_byname(edp_drv->pdev, IORESOURCE_MEM,
			"mmss_cc_base");
	if (!res) {
		pr_err("%s: Unable to get the MMSS_CC resources", __func__);
		return -ENOMEM;
	}

	edp_drv->mmss_cc_base = ioremap(res->start, resource_size(res));
	if (!edp_drv->mmss_cc_base) {
		pr_err("%s: Unable to remap MMSS_CC resources",  __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mdss_edp_fill_edid_data(struct mdss_edp_drv_pdata *edp_drv)
{
	struct edp_edid *edid = &edp_drv->edid;
	unsigned int res[2];

	edid->id_name[0] = 'A';
	edid->id_name[0] = 'U';
	edid->id_name[0] = 'O';
	edid->id_name[0] = 0;
	edid->id_product = 0x305D;
	edid->version = 1;
	edid->revision = 4;
	edid->ext_block_cnt = 0;
	edid->video_digital = 0x5;
	edid->color_depth = 8;
	edid->dpm = 0;
	edid->color_format = 0;
	edid->timing[0].pclk = 268500000;

	edid->timing[0].h_addressable = 2560;
	edid->timing[0].h_blank = 165;
	edid->timing[0].h_fporch = 48;
	edid->timing[0].h_sync_pulse = 32;

	edid->timing[0].v_addressable = 1600;
	edid->timing[0].v_blank = 70;
	edid->timing[0].v_fporch = 3;
	edid->timing[0].v_sync_pulse = 6;

	edid->timing[0].width_mm = 271;
	edid->timing[0].height_mm = 172;
	edid->timing[0].h_border = 0;
	edid->timing[0].v_border = 0;
	edid->timing[0].interlaced = 0;
	edid->timing[0].stereo = 0;
	edid->timing[0].sync_type = 1;
	edid->timing[0].sync_separate = 1;
	edid->timing[0].vsync_pol = 0;
	edid->timing[0].hsync_pol = 0;

	if (!of_property_read_u32_array(edp_drv->pdev->dev.of_node, "qcom,mdss-pan-size", res, 2)) {
		edid->timing[0].width_mm = res[0];
		edid->timing[0].height_mm = res[1];
	}
}

static void mdss_edp_fill_dpcd_data(struct mdss_edp_drv_pdata *edp_drv)
{
	struct dpcd_cap *cap = &edp_drv->dpcd;

	cap->max_lane_count = 4;
	cap->max_link_clk = 270;
}

#if defined(CONFIG_EDP_ESD_FUNCTION)
void edp_esd_work_func(struct work_struct *work)
{
	struct fb_info *info = registered_fb[0];
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_edp_drv_pdata *edp_drv = NULL;

	edp_drv = container_of(work, struct mdss_edp_drv_pdata, edp_esd_work);

	if (!edp_drv) {
		pr_err("%s: Invalid input data edp_drv", __func__);
		return ;
	}

	if (!mfd) {
		pr_err("%s: Invalid input data mfd", __func__);
		return ;
	}

	if (!edp_power_state) {
		pr_err("%s: edp_power_state is off", __func__);
		return ;
	}

	pr_info("%s start", __func__);

	edp_drv->panel_data.event_handler(&edp_drv->panel_data, MDSS_EVENT_PANEL_OFF, NULL);
	edp_drv->panel_data.event_handler(&edp_drv->panel_data, MDSS_EVENT_UNBLANK, NULL);

	mdss_edp_set_backlight(&edp_drv->panel_data, edp_drv->current_bl);

	pr_info("%s end", __func__);
}
#endif

static int edp_event_thread(void *data)
{
	struct mdss_edp_drv_pdata *ep;
	unsigned long flag;
	u32 todo = 0;
#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	static int first_boot;
#endif
	ep = (struct mdss_edp_drv_pdata *)data;

	pr_info("%s: start\n", __func__);

	while (1) {
		wait_event(ep->event_q, (ep->event_pndx != ep->event_gndx));
		while (1) {
			spin_lock_irqsave(&ep->event_lock, flag);
			if (ep->event_pndx == ep->event_gndx) {
				spin_unlock_irqrestore(&ep->event_lock, flag);
				break;
			}
			todo = ep->event_todo_list[ep->event_gndx];
			ep->event_todo_list[ep->event_gndx++] = 0;
			ep->event_gndx %= HPD_EVENT_MAX;
			spin_unlock_irqrestore(&ep->event_lock, flag);

			pr_info("%s: todo=%x\n", __func__, todo);

			if (todo == 0)
				continue;

			if (todo & EV_EDID_READ)
				mdss_edp_edid_read(ep, 0);

			if (todo & EV_DPCD_CAP_READ)
				mdss_edp_dpcd_cap_read(ep);

			if (todo & EV_DPCD_STATUS_READ)
				mdss_edp_dpcd_status_read(ep);

			if (todo & EV_LINK_TRAIN) {
				msleep(350);
				mdss_edp_link_train(ep);
				mdss_edp_enable(ep, 1);				
#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
				edp_power_state = 1;

				if (!first_boot) {
					msleep(20);
					mdss_edp_set_backlight(&ep->panel_data, 180);
					first_boot = 1;
				}

				if(gpio_get_value(ep->gpio_panel_hpd))
					edp_backlight_enable();
				else
					pr_err("%s : hpd is not detected, do not edp_backlight_enable()\n", __func__);

				complete(&edp_power_sync);
#endif
			}
		}
	}

	return 0;
}

static void edp_send_events(struct mdss_edp_drv_pdata *ep, u32 events)
{
	spin_lock(&ep->event_lock);
	ep->event_todo_list[ep->event_pndx++] = events;
	ep->event_pndx %= HPD_EVENT_MAX;
	wake_up(&ep->event_q);
	spin_unlock(&ep->event_lock);

}

irqreturn_t edp_isr(int irq, void *ptr)
{
	struct mdss_edp_drv_pdata *ep = (struct mdss_edp_drv_pdata *)ptr;
	unsigned char *base = ep->base;
	u32 isr, isr2, mask1, mask2;
	u32 ack;

	isr = edp_read(base + 0x308);
	isr2 = edp_read(base + 0x30c);

	mask1 = isr & 0x24924924;	/* fix later */
	mask2 = isr2 & 0x920;

	pr_debug("%s: isr=%x isr2=%x", __func__, isr, isr2);

	ack = isr & EDP_INTR_STATUS_BITS;
	ack <<= 1;	/* ack bits */
	ack |= mask1;
	edp_write(base + 0x308, ack);

	ack = isr2 & EDP_INTR_STATUS_2_BITS;
	ack <<= 1;	/* ack bits */
	ack |= mask2;
	edp_write(base + 0x30c, ack);

	if (ep->aux_cmd_busy) {
		/* clear EDP_AUX_TRANS_CTRL */
		edp_write(base + 0x318, 0);
		/* read EDP_INTERRUPT_TRANS_NUM */
		ep->aux_trans_num = edp_read(base + 0x310);

		if (ep->aux_cmd_i2c)
			edp_aux_i2c_handler(ep, isr);
		else
			edp_aux_native_handler(ep, isr);
	}

	if (isr & EDP_INTR_HPD) {
#if defined(CONFIG_EDP_ESD_FUNCTION)
		/* FOR ESD FUNCTION */
		if (edp_esd_power_state)
			schedule_work(&ep->edp_esd_work);
		else
			edp_send_events(ep, EV_LINK_TRAIN);
#else
		edp_send_events(ep, EV_EDID_READ |
				EV_DPCD_CAP_READ | EV_LINK_TRAIN);
#endif
	}

	if (isr2 & EDP_INTR_READY_FOR_VIDEO) {
		/*
		 * need to do something
		 */
	}

	return IRQ_HANDLED;
}

struct mdss_hw mdss_edp_hw = {
	.hw_ndx = MDSS_HW_EDP,
	.ptr = NULL,
	.irq_handler = edp_isr,
};

void mdss_edp_irq_enable(void)
{
	mdss_enable_irq(&mdss_edp_hw);
}

void mdss_edp_irq_disable(void)
{
	mdss_disable_irq(&mdss_edp_hw);
}

static int mdss_edp_irq_setup(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret = 0;


	edp_drv->gpio_panel_hpd = of_get_named_gpio_flags(
			edp_drv->pdev->dev.of_node, "gpio-panel-hpd", 0,
			&edp_drv->hpd_flags);

	if (!gpio_is_valid(edp_drv->gpio_panel_hpd)) {
		pr_err("%s gpio_panel_hpd %d is not valid ", __func__,
				edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_request(edp_drv->gpio_panel_hpd, "edp_hpd_irq_gpio");
	if (ret) {
		pr_err("%s unable to request gpio_panel_hpd %d", __func__,
				edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_tlmm_config(GPIO_CFG(
					edp_drv->gpio_panel_hpd,
					1,
					GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL,
					GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	if (ret) {
		pr_err("%s: unable to config tlmm = %d\n", __func__,
				edp_drv->gpio_panel_hpd);
		gpio_free(edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	ret = gpio_direction_input(edp_drv->gpio_panel_hpd);
	if (ret) {
		pr_err("%s unable to set direction for gpio_panel_hpd %d",
				__func__, edp_drv->gpio_panel_hpd);
		return -ENODEV;
	}

	mdss_edp_hw.ptr = (void *)(edp_drv);

	if (mdss_register_irq(&mdss_edp_hw))
		pr_err("%s: mdss_register_irq failed.\n", __func__);

	return 0;
}


static void mdss_edp_event_setup(struct mdss_edp_drv_pdata *ep)
{
	init_waitqueue_head(&ep->event_q);
	spin_lock_init(&ep->event_lock);

	kthread_run(edp_event_thread, (void *)ep, "mdss_edp_hpd");
}

static int __devinit mdss_edp_probe(struct platform_device *pdev)
{
	int ret;
	struct mdss_edp_drv_pdata *edp_drv;

	pr_info("%s", __func__);

	if (!pdev->dev.of_node) {
		pr_err("%s: Failed\n", __func__);
		return -EPERM;
	}

	edp_drv = devm_kzalloc(&pdev->dev, sizeof(*edp_drv), GFP_KERNEL);
	if (edp_drv == NULL) {
		pr_err("%s: Failed, could not allocate edp_drv", __func__);
		return -ENOMEM;
	}

	set_global_ep(edp_drv);

	edp_drv->pdev = pdev;
	edp_drv->pdev->id = 1;
	edp_drv->clk_on = 0;

	ret = mdss_edp_get_base_address(edp_drv);
	if (ret)
		goto probe_err;

	ret = mdss_edp_get_mmss_cc_base_address(edp_drv);
	if (ret)
		goto edp_base_unmap;

	ret = mdss_edp_regulator_init(edp_drv);
	if (ret)
		goto mmss_cc_base_unmap;

	ret = mdss_edp_clk_init(edp_drv);
	if (ret)
		goto edp_clk_deinit;

	ret = mdss_edp_gpio_panel_en(edp_drv);
	if (ret)
		goto edp_clk_deinit;

	ret = mdss_edp_pwm_config(edp_drv);
	if (ret)
		goto edp_free_gpio_panel_en;

	mdss_edp_fill_edid_data(edp_drv);
	mdss_edp_fill_dpcd_data(edp_drv);
	mdss_edp_device_register(edp_drv);

	mdss_edp_irq_setup(edp_drv);

	mdss_edp_aux_setup(edp_drv);

	mdss_edp_event_setup(edp_drv);

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
	init_completion(&edp_power_sync);
#endif

#if defined(CONFIG_EDP_ESD_FUNCTION)
	INIT_WORK(&edp_drv->edp_esd_work, edp_esd_work_func);
#endif

#if defined(CONFIG_EDP_TCON_MDNIE)
	init_mdnie_class();
#endif
	return 0;


edp_free_gpio_panel_en:
	gpio_free(edp_drv->gpio_panel_en);
edp_clk_deinit:
	mdss_edp_clk_deinit(edp_drv);
	mdss_edp_regulator_off(edp_drv);
mmss_cc_base_unmap:
	iounmap(edp_drv->mmss_cc_base);
edp_base_unmap:
	iounmap(edp_drv->base);
probe_err:
	return ret;

}

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
static int __init edp_current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		recovery_mode = 1;
	else
		recovery_mode = 0;

	pr_info("%s %s", __func__, recovery_mode ?
						"recovery" : "normal");
	return 1;
}
__setup("androidboot.boot_recovery=", edp_current_boot_mode);

#endif

static const struct of_device_id msm_mdss_edp_dt_match[] = {
	{.compatible = "qcom,mdss-edp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_mdss_edp_dt_match);

static struct platform_driver mdss_edp_driver = {
	.probe = mdss_edp_probe,
	.remove = __devexit_p(mdss_edp_remove),
	.shutdown = NULL,
	.driver = {
		.name = "mdss_edp",
		.of_match_table = msm_mdss_edp_dt_match,
	},
};

static int __init mdss_edp_init(void)
{
	int ret;

	ret = platform_driver_register(&mdss_edp_driver);
	if (ret) {
		pr_err("%s driver register failed", __func__);
		return ret;
	}

	return ret;
}
module_init(mdss_edp_init);

static void __exit mdss_edp_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_edp_driver);
}
module_exit(mdss_edp_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("eDP controller driver");

