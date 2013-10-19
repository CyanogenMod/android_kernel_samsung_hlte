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
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#include "mdss.h"
#include "mdss_fb.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"

#ifdef CONFIG_SAMSUNG_CMC624
#include <linux/i2c/samsung_cmc624.h>
#endif

static unsigned char *mdss_dsi_base;
int contsplash_lkstat;
unsigned int gv_manufacture_id;

int get_lcd_attached(void);

#if defined(CONFIG_DUAL_LCD)
int dsi_clk_on = 0;
extern int IsSwitching;
#endif

#if defined (CONFIG_FB_MSM_MDSS_DSI_DBG)
void xlog(const char *name, u32 data0, u32 data1, u32 data2, u32 data3, u32 data4);
#endif
static int __init mdss_check_contsplash_arg(char *status)
{
	contsplash_lkstat = (int)(*status -'0');
	pr_info("%s : %d", __func__, contsplash_lkstat);
	return 1;
}
__setup("cont_splash=", mdss_check_contsplash_arg);

static int mdss_dsi_regulator_init(struct platform_device *pdev)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_drv_cm_data *dsi_drv = NULL;

	if (!pdev) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	dsi_drv = &(ctrl_pdata->shared_pdata);
	if (ctrl_pdata->power_data.num_vreg > 0) {
		ret = msm_dss_config_vreg(&pdev->dev,
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 1);
	} else {
#if defined(CONFIG_REGULATOR_LP8720)
		if (get_lcd_attached()) {
			dsi_drv->vdd_vreg = devm_regulator_get(&pdev->dev, "lp8720_vdd");
			if (IS_ERR(dsi_drv->vdd_vreg)) {
				pr_err("%s: could not get lp8720_vdd vreg, rc=%ld\n",
					__func__, PTR_ERR(dsi_drv->vdd_vreg));
				return PTR_ERR(dsi_drv->vdd_vreg);
			}

			ret = regulator_set_voltage(dsi_drv->vdd_vreg, 3000000,
					3000000);
			if (ret) {
				pr_err("%s: set voltage failed on lp8720_vdd vreg, rc=%d\n",
					__func__, ret);
				return ret;
			}			

			dsi_drv->vdd_io_lp8720_vreg = devm_regulator_get(&pdev->dev, "lp8720_vddio");
			if (IS_ERR(dsi_drv->vdd_io_lp8720_vreg)) {
				pr_err("%s: could not get lp8720_vddio reg, rc=%ld\n",
					__func__, PTR_ERR(dsi_drv->vdd_io_lp8720_vreg));
				return PTR_ERR(dsi_drv->vdd_io_lp8720_vreg);
			}

			ret = regulator_set_voltage(dsi_drv->vdd_io_lp8720_vreg, 1800000,
					1800000);
			if (ret) {
				pr_err("%s: set voltage failed on lp8720_vddio vreg, rc=%d\n",
					__func__, ret);
				return ret;
			}
		}
#else
		dsi_drv->vdd_vreg = devm_regulator_get(&pdev->dev, "vdd");
		if (IS_ERR(dsi_drv->vdd_vreg)) {
			pr_err("%s: could not get vdda vreg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdd_vreg));
			return PTR_ERR(dsi_drv->vdd_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdd_vreg, 3000000,
				3000000);
		if (ret) {
			pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}
#endif

		dsi_drv->vdd_io_vreg = devm_regulator_get(&pdev->dev, "vddio");
		if (IS_ERR(dsi_drv->vdd_io_vreg)) {
			pr_err("%s: could not get vddio reg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdd_io_vreg));
			return PTR_ERR(dsi_drv->vdd_io_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdd_io_vreg, 1800000,
				1800000);
		if (ret) {
			pr_err("%s: set voltage failed on vddio vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}

		dsi_drv->vdda_vreg = devm_regulator_get(&pdev->dev, "vdda");
		if (IS_ERR(dsi_drv->vdda_vreg)) {
			pr_err("%s: could not get vdda vreg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->vdda_vreg));
			return PTR_ERR(dsi_drv->vdda_vreg);
		}

		ret = regulator_set_voltage(dsi_drv->vdda_vreg, 1200000,
				1200000);
		if (ret) {
			pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
				__func__, ret);
			return ret;
		}
#if defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
		dsi_drv->iovdd_vreg = devm_regulator_get(&pdev->dev, "iovdd");
		if (IS_ERR(dsi_drv->iovdd_vreg)) {
			pr_err("%s: could not get iovddreg, rc=%ld\n",
				__func__, PTR_ERR(dsi_drv->iovdd_vreg));
			return PTR_ERR(dsi_drv->iovdd_vreg);
		}
#endif
	}

	return 0;
}

static int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s: enable=%d\n", __func__, enable);

        #ifdef CONFIG_SAMSUNG_CMC624
	ret = gpio_direction_output((ctrl_pdata->vddi_en), 1);
	if (ret) {
		pr_err("[LCD]%s: unable to set_direction for vddi_en [%d]\n", __func__, ctrl_pdata->vddi_en);
	}

	ret = gpio_direction_output((ctrl_pdata->lcd_en), 1);
	if (ret) {
		pr_err("[LCD]%s: unable to set_direction for lcd_en [%d]\n", __func__, ctrl_pdata->lcd_en);
	}
        #endif

	if (enable) {
                #ifdef CONFIG_SAMSUNG_CMC624
                pr_info("[LCD] %s: VDDI_EN : gpio_set_value : HIGH !!! \n", __func__);
                gpio_set_value((ctrl_pdata->vddi_en), 1);
                msleep(10);

                pr_info("[LCD] %s: LCD_EN : gpio_set_value : HIGH !!! \n", __func__);
                gpio_set_value((ctrl_pdata->lcd_en), 1);
                #endif

#if defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_CMD_FULL_HD_PT_PANEL) \
	|| defined(CONFIG_FB_MSM_MIPI_SAMSUNG_YOUM_CMD_FULL_HD_PT_PANEL)\
	|| defined(CONFIG_MIPI_LCD_S6E3FA0_FORCE_VIDEO_MODE)\
	|| defined(CONFIG_MACH_JS01LTEDCM)
		pr_debug("[LCD] %s: LCD_EN : gpio_set_value : HIGH !!! \n", __func__);
		gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
#endif

		if (ctrl_pdata->power_data.num_vreg > 0) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 1);
			if (ret) {
				pr_err("%s:Failed to enable regulators.rc=%d\n",
					__func__, ret);
				return ret;
			}

			/*
			 * A small delay is needed here after enabling
			 * all regulators and before issuing panel reset
			 */
			msleep(20);
		} else {
#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
#endif
		
			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdd_vreg set opt mode failed.\n",
					 __func__);
				return ret;
			}

#if defined(CONFIG_REGULATOR_LP8720)
			}
			
			if (get_lcd_attached()) {
				ret = regulator_set_optimum_mode(
					(ctrl_pdata->shared_pdata).vdd_io_lp8720_vreg, 100000);
				if (ret < 0) {
					pr_err("%s: vdd_io_lp8720_vreg set opt mode failed.\n",
						__func__);
					return ret;
				}
			}
#endif

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_io_vreg, 100000);
#if defined(CONFIG_FB_MSM_MDSS_DSI_DBG)
			xlog(__func__, enable, 0, 0, 0, 0x66666);
#endif
			if (ret < 0) {
				pr_err("%s: vdd_io_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}

			ret = regulator_set_optimum_mode
			  ((ctrl_pdata->shared_pdata).vdda_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdda_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdd_io_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
			msleep(20);

#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
				ret = regulator_enable(
					(ctrl_pdata->shared_pdata).vdd_io_lp8720_vreg);
				if (ret) {
					pr_err("%s: Failed to enable regulator.\n",
						__func__);
					return ret;
				}
				msleep(20);
			}
#endif

#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
			gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_en_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
			gpio_set_value((ctrl_pdata->lcd_en_gpio), 1);
			pr_info("%s lcd_en on: %d\n", __func__, gpio_get_value(ctrl_pdata->lcd_en_gpio));
			msleep(20);
			gpio_set_value((ctrl_pdata->bl_en_gpio), 1);
			pr_info("%s bl_en on: %d\n", __func__, gpio_get_value(ctrl_pdata->bl_en_gpio));
			msleep(20);
#endif
#if defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
			ret = regulator_set_optimum_mode
			  ((ctrl_pdata->shared_pdata).iovdd_vreg, 100000);
			if (ret < 0) {
				pr_err("%s: vdda_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}
			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).iovdd_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
			msleep(10);
			gpio_set_value((ctrl_pdata->lcd_en_p_gpio), 1);
			pr_info("%s lcd_en_p on: %d\n", __func__, gpio_get_value(ctrl_pdata->lcd_en_p_gpio));
			mdelay(10);
#endif

#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
#endif	

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdd_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
			msleep(20);

#if defined(CONFIG_REGULATOR_LP8720)
			}
#endif

			ret = regulator_enable(
				(ctrl_pdata->shared_pdata).vdda_vreg);
			if (ret) {
				pr_err("%s: Failed to enable regulator.\n",
					__func__);
				return ret;
			}
		}
                #if 0
		if (pdata->panel_info.panel_power_on == 0)
			(ctrl_pdata->panel_data).panel_reset_fn(pdata, 1);
                #endif
	} else {
                #ifdef CONFIG_SAMSUNG_CMC624
                {
                        samsung_cmc624_on(0);
                        cmc_power(0);
                }

                pr_info("[LCD] %s: LCD_EN : gpio_set_value : LOW !!! \n", __func__);
                gpio_set_value((ctrl_pdata->lcd_en), 0);
                msleep(10);

                pr_info("[LCD] %s: VDDI_EN : gpio_set_value : LOW !!! \n", __func__);
                gpio_set_value((ctrl_pdata->vddi_en), 0);
                #endif

		(ctrl_pdata->panel_data).panel_reset_fn(pdata, 0);

		if (ctrl_pdata->power_data.num_vreg > 0) {
			ret = msm_dss_enable_vreg(
				ctrl_pdata->power_data.vreg_config,
				ctrl_pdata->power_data.num_vreg, 0);
			if (ret) {
				pr_err("%s: Failed to disable regs.rc=%d\n",
					__func__, ret);
				return ret;
			}
		} else {
#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
#endif

			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdd_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

#if defined(CONFIG_REGULATOR_LP8720)
			}
#endif

			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdda_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
				ret = regulator_disable(
					(ctrl_pdata->shared_pdata).vdd_io_lp8720_vreg);
				if (ret) {
					pr_err("%s: Failed to disable regulator.\n",
						__func__);
					return ret;
				}
			}
#endif

#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
			gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_en_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
			gpio_set_value((ctrl_pdata->lcd_en_gpio), 0);
			pr_info("%s lcd_en off: %d\n", __func__, gpio_get_value(ctrl_pdata->lcd_en_gpio));
			gpio_set_value((ctrl_pdata->bl_en_gpio), 0);
			pr_info("%s led_en off: %d\n", __func__, gpio_get_value(ctrl_pdata->bl_en_gpio));
#endif
#if defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
			gpio_set_value((ctrl_pdata->lcd_en_p_gpio), 0);
			pr_info("%s lcd_en_p off: %d\n", __func__, gpio_get_value(ctrl_pdata->lcd_en_p_gpio));
			mdelay(1);
			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).iovdd_vreg);
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).iovdd_vreg, 100);
			if (ret < 0) {
				pr_err("%s: iovdd_vreg set opt mode failed.\n",
					 __func__);
				return ret;
			}
#endif
			ret = regulator_disable(
				(ctrl_pdata->shared_pdata).vdd_io_vreg);
#if defined(CONFIG_FB_MSM_MDSS_DSI_DBG)
			xlog(__func__, enable, 0, 0, 0, 0x55555);
#endif
			if (ret) {
				pr_err("%s: Failed to disable regulator.\n",
					__func__);
				return ret;
			}

#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
#endif

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdd_vreg set opt mode failed.\n",
					 __func__);
				return ret;
			}

#if defined(CONFIG_REGULATOR_LP8720)
			}
#endif

#if defined(CONFIG_REGULATOR_LP8720)
			if (get_lcd_attached()) {
				ret = regulator_set_optimum_mode(
					(ctrl_pdata->shared_pdata).vdd_io_lp8720_vreg, 100);
				if (ret < 0) {
					pr_err("%s: vdd_io_vreg set opt mode failed.\n",
						__func__);
					return ret;
				}
			}
#endif

			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdd_io_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdd_io_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}
			ret = regulator_set_optimum_mode(
				(ctrl_pdata->shared_pdata).vdda_vreg, 100);
			if (ret < 0) {
				pr_err("%s: vdda_vreg set opt mode failed.\n",
					__func__);
				return ret;
			}
		}
	}
	return 0;
}

static void mdss_dsi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

static int mdss_dsi_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp)
{
	int i, rc = 0;
	int dt_vreg_total = 0;
	u32 *val_array = NULL;
	struct device_node *of_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	of_node = dev->of_node;

	mp->num_vreg = 0;
	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total < 0) {
		pr_debug("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		rc = 0;
		goto error;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, dt_vreg_total);
	}

	if (dt_vreg_total > 0) {
		mp->num_vreg = dt_vreg_total;
		mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
			dt_vreg_total, GFP_KERNEL);
		if (!mp->vreg_config) {
			pr_err("%s: can't alloc vreg mem\n", __func__);
			goto error;
		}
	} else {
		pr_debug("%s: no vreg\n", __func__);
		return 0;
	}

	val_array = devm_kzalloc(dev, sizeof(u32) * dt_vreg_total, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s: can't allocate vreg scratch mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < dt_vreg_total; i++) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string_index(of_node, "qcom,supply-names",
			i, &st);
		if (rc) {
			pr_err("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);

		/* vreg-min-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-min-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = val_array[i];

		/* vreg-max-voltage */
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-max-voltage-level", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = val_array[i];

		/* vreg-peak-current*/
		memset(val_array, 0, sizeof(u32) * dt_vreg_total);
		rc = of_property_read_u32_array(of_node,
			"qcom,supply-peak-current", val_array,
			dt_vreg_total);
		if (rc) {
			pr_err("%s: error reading peak current. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].peak_current = val_array[i];

		pr_debug("%s: %s min=%d, max=%d, pc=%d\n", __func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].peak_current);
	}

	devm_kfree(dev, val_array);

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;

	if (val_array)
		devm_kfree(dev, val_array);
	return rc;
}

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
extern struct mutex power_state_chagne;
static struct mdss_panel_data *pdata_for_esd;
#endif

static int mdss_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already off.\n", __func__, __LINE__);
		return -EPERM;
	}

	pdata->panel_info.panel_power_on = 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
	ret = gpio_tlmm_config(GPIO_CFG(
				ctrl_pdata->disp_te_gpio, 0,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN,
				GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);

	if (ret) {
		pr_err("%s: unable to config tlmm = %d\n",
			__func__, ctrl_pdata->disp_te_gpio);
		gpio_free(ctrl_pdata->disp_te_gpio);
		return -ENODEV;
	}
#endif

//	if (pdata->panel_info.type == MIPI_CMD_PANEL)
//		mdss_dsi_clk_ctrl(ctrl_pdata, 1);

	/* disable DSI controller */
	mdss_dsi_controller_cfg(0, pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, 0);

	ret = mdss_dsi_enable_bus_clocks(ctrl_pdata);
	if (ret) {
		pr_err("%s: failed to enable bus clocks. rc=%d\n", __func__,
			ret);
		mdss_dsi_panel_power_on(pdata, 0);
		return ret;
	}
	mdss_dsi_disable_bus_clocks(ctrl_pdata);

	/* disable DSI phy */
	mdss_dsi_phy_enable(ctrl_pdata->ctrl_base, 0);
	ret = mdss_dsi_panel_power_on(pdata, 0);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		return ret;
	}

	pr_debug("%s-:\n", __func__);

	return ret;
}

int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	u32 clk_rate;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data, dst_bpp;
	u32 dummy_xres, dummy_yres;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already on.\n", __func__, __LINE__);
		return 0;
	}
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	pdata_for_esd = pdata;
#endif

#if defined(CONFIG_DUAL_LCD)
	dsi_clk_on = 1;
#endif

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n",
				__func__, ctrl_pdata, ctrl_pdata->ndx);

	pinfo = &pdata->panel_info;
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
	ret = gpio_tlmm_config(GPIO_CFG(
			ctrl_pdata->disp_te_gpio, 1,
			GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN,
			GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);

	if (ret) {
		pr_err("%s: unable to config tlmm = %d\n",
			__func__, ctrl_pdata->disp_te_gpio);
		gpio_free(ctrl_pdata->disp_te_gpio);
		return -ENODEV;
	}
#endif
	ret = mdss_dsi_panel_power_on(pdata, 1);
	if (ret) {
		pr_err("%s: Panel power on failed\n", __func__);
		return ret;
	}

	pdata->panel_info.panel_power_on = 1;

#if !(defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL) || \
	defined(CONFIG_MACH_JS01LTEDCM))
	if (get_lcd_attached() == 0) {
		pr_err("%s : lcd is not attached..\n",__func__);
	
		mdss_dsi_panel_power_on(pdata, 0);
		pdata->panel_info.panel_power_on = 0;

		return -ENODEV;
	}
#endif

	ret = mdss_dsi_enable_bus_clocks(ctrl_pdata);
	if (ret) {
		pr_err("%s: failed to enable bus clocks. rc=%d\n", __func__,
			ret);
		mdss_dsi_panel_power_on(pdata, 0);
		return ret;
	}

	mdss_dsi_phy_sw_reset((ctrl_pdata->ctrl_base));
	mdss_dsi_phy_init(pdata);
	mdss_dsi_disable_bus_clocks(ctrl_pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, 1);

	clk_rate = pdata->panel_info.clk_rate;
	clk_rate = min(clk_rate, pdata->panel_info.clk_max);

	dst_bpp = pdata->panel_info.fbc.enabled ?
		(pdata->panel_info.fbc.target_bpp) : (pinfo->bpp);

	hbp = mult_frac(pdata->panel_info.lcdc.h_back_porch, dst_bpp,
			pdata->panel_info.bpp);
	hfp = mult_frac(pdata->panel_info.lcdc.h_front_porch, dst_bpp,
			pdata->panel_info.bpp);
	vbp = mult_frac(pdata->panel_info.lcdc.v_back_porch, dst_bpp,
			pdata->panel_info.bpp);
	vfp = mult_frac(pdata->panel_info.lcdc.v_front_porch, dst_bpp,
			pdata->panel_info.bpp);
	hspw = mult_frac(pdata->panel_info.lcdc.h_pulse_width, dst_bpp,
			pdata->panel_info.bpp);
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = mult_frac(pdata->panel_info.xres, dst_bpp,
			pdata->panel_info.bpp);
	height = pdata->panel_info.yres;

	mipi  = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = pdata->panel_info.lcdc.xres_pad;
		dummy_yres = pdata->panel_info.lcdc.yres_pad;

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x24,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x28,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
			(vspw + vbp + height + dummy_yres +
				vfp - 1) << 16 | (hspw + hbp +
				width + dummy_xres + hfp - 1));

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x30, (hspw << 16));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x34, 0);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x38, (vspw << 16));

	} else {		/* command mode */
		if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB666)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
			bpp = 2;
		else
			bpp = 3;	/* Default format set to RGB888 */

		ystride = width * bpp + 1;

		/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
		data = (ystride << 16) | (mipi->vc << 8) | DTYPE_DCS_LWRITE;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, data);

		/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
		data = height << 16 | width;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, data);
	}

	mdss_dsi_sw_reset(pdata);
	mdss_dsi_host_init(mipi, pdata);
	// LP11
	{
		u32 tmp;
		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp &= ~(1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}
	// LP11

	(ctrl_pdata->panel_data).panel_reset_fn(pdata, 1);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, 0);

	pr_debug("%s-:\n", __func__);
	return 0;
}

static int mdss_dsi_unblank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

#if !defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT)) {
		ret = ctrl_pdata->on(pdata);
		if (ret) {
			pr_err("%s: unable to initialize the panel\n",
							__func__);
			return ret;
		}
		ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
	}
#endif

	mdss_dsi_op_mode_config(mipi->mode, pdata);

#if defined(CONFIG_DUAL_LCD)
	dsi_clk_on = 2;
#endif

	pr_debug("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_blank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

#if defined(CONFIG_DUAL_LCD)
	printk("mdss_dsi_off : IsSwitching[%d]\n", IsSwitching);
	dsi_clk_on = 0;

	if(IsSwitching) {
		int retry=3;
		while(retry>0 && IsSwitching) {
			msleep(300);
			retry--;
		}
	}
#endif

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		ret = ctrl_pdata->off(pdata);
		if (ret) {
			pr_err("%s: Panel OFF failed\n", __func__);
			return ret;
		}
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
	}
	pr_debug("%s-:End\n", __func__);
	return ret;
}
int mdss_dsi_cont_splash_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_info("%s:%d DSI on for continuous splash.\n", __func__, __LINE__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mipi = &pdata->panel_info.mipi;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%p ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	WARN((ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT),
		"Incorrect Ctrl state=0x%x\n", ctrl_pdata->ctrl_state);

#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
	// need to control power to complete panel off (JDI recommendation)
	ret = mdss_dsi_panel_power_on(pdata, 0);
	msleep(20);
	ret = mdss_dsi_panel_power_on(pdata, 1);
#endif
	mdss_dsi_sw_reset(pdata);
	mdss_dsi_host_init(mipi, pdata);

#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
	// LP11
	{
		u32 tmp;
		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp &= ~(1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}
	// LP11
	(ctrl_pdata->panel_data).panel_reset_fn(pdata, 1);
#endif

	if (1) /* (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE)*/{
		mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);
		ret = mdss_dsi_unblank(pdata);
		if (ret) {
			pr_err("%s: unblank failed\n", __func__);
			return ret;
		}
	}

	pr_debug("%s-:End\n", __func__);
	return ret;
}

static int mdss_dsi_dfps_config(struct mdss_panel_data *pdata, char *new_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	u32 dsi_ctrl;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if ((*new_fps !=
	     ctrl_pdata->panel_data.panel_info.mipi.frame_rate)) {

		rc = mdss_dsi_clk_div_config(&ctrl_pdata->panel_data.panel_info,
					     *new_fps);
		if (rc) {
			pr_err("%s: unable to initialize the clk dividers\n",
							__func__);
			return rc;
		}

		ctrl_pdata->pclk_rate =
		  ctrl_pdata->panel_data.panel_info.mipi.dsi_pclk_rate;
		ctrl_pdata->byte_clk_rate =
		  ctrl_pdata->panel_data.panel_info.clk_rate / 8;

		dsi_ctrl = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0004);
		dsi_ctrl &= ~0x2;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
		mdss_dsi_sw_reset(pdata);
		mdss_dsi_host_init(&pdata->panel_info.mipi, pdata);
		mdss_dsi_controller_cfg(true, pdata);
#if 0
		mdss_dsi_clk_ctrl(ctrl_pdata, 0);
		mdss_dsi_unprepare_clocks(ctrl_pdata);
		mdss_dsi_prepare_clocks(ctrl_pdata);
		mdss_dsi_clk_ctrl(ctrl_pdata, 1);
#endif
		wmb();
		dsi_ctrl |= 0x2;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, dsi_ctrl);
	} else
		pr_debug("%s: Panel is already at this FPS\n", __func__);


	return rc;
}

static int mdss_dsi_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s+:event=%d\n", __func__, event);

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = mdss_dsi_on(pdata);
		if (ctrl_pdata->dsi_on_state == DSI_LP_MODE)
			rc = mdss_dsi_unblank(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		ctrl_pdata->ctrl_state |= CTRL_STATE_MDP_ACTIVE;
		ctrl_pdata->mdp_tg_on = 1;
		if (ctrl_pdata->dsi_on_state == DSI_HS_MODE)
			rc = mdss_dsi_unblank(pdata);
		break;
	case MDSS_EVENT_BLANK:
		if (ctrl_pdata->dsi_off_state == DSI_HS_MODE)
			rc = mdss_dsi_blank(pdata);
		break;
	case MDSS_EVENT_PANEL_OFF:
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		ctrl_pdata->mdp_tg_on = 0;
		if (ctrl_pdata->dsi_off_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata);
		rc = mdss_dsi_off(pdata);
		break;
	case MDSS_EVENT_FB_REGISTERED:
		if (ctrl_pdata->registered) {
			pr_debug("%s:event=%d, calling panel registered callback \n",
				 __func__, event);
			rc = ctrl_pdata->registered(pdata);

			/*
			 *  Okay, since framebuffer is registered, display the kernel logo if needed
			*/
#if !defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
			if ((!ctrl_pdata->panel_data.panel_info.cont_splash_enabled)
					&& (ctrl_pdata->panel_data.panel_info.early_lcd_on))
				load_samsung_boot_logo(); 
	
#endif
			}
		break;
	case MDSS_EVENT_CONT_SPLASH_FINISH:
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		ctrl_pdata->mdp_tg_on = 0;
		if (1){ //(ctrl_pdata->on_cmds.link_state == DSI_LP_MODE) {
			rc = mdss_dsi_cont_splash_on(pdata);
		} else {
			pr_debug("%s:event=%d, Dsi On not called: ctrl_state: %d\n",
				 __func__, event,
				 ctrl_pdata->dsi_on_state);
			rc = -EINVAL;
		}
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		mdss_dsi_clk_req(ctrl_pdata, (int)arg);
		break;
	case MDSS_EVENT_DSI_CMDLIST_KOFF:
		mdss_dsi_cmdlist_commit(ctrl_pdata, 1);
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		if (ctrl_pdata->dsi_on_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			rc = mdss_dsi_blank(pdata);
		}
		break;
	case MDSS_EVENT_FIRST_FRAME_UPDATE:
		pr_debug("MDSS_FIRST_FRAME_UPDATE\n");
		ctrl_pdata->mdp_tg_on = 1;
	/*Event is send only if cont_splash feature is enabled */
		if (ctrl_pdata->dsi_off_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
			rc = mdss_dsi_blank(pdata);
		}
		break;
	case MDSS_EVENT_PANEL_UPDATE_FPS:
		if (arg != NULL) {
			rc = mdss_dsi_dfps_config(pdata, arg);
			pr_debug("%s:update fps to = %d\n",
				__func__, *(char *)arg);
		}
		break;
	default:
		if(ctrl_pdata->vendor_event_handler)
			rc = ctrl_pdata->vendor_event_handler(event);
		else
			pr_debug("%s: unhandled event=%d\n", __func__, event);

		break;
	}
	pr_debug("%s-:event=%d, rc=%d\n", __func__, event, rc);
	return rc;
}

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
void esd_recovery()
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata_for_esd == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
	}

	ctrl_pdata = container_of(pdata_for_esd, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s+: ctrl=%p ndx=%d\n",
				__func__, ctrl_pdata, ctrl_pdata->ndx);

	if (pdata_for_esd) {
		if (pdata_for_esd->panel_info.panel_power_on) {
			mutex_lock(&power_state_chagne);
			pr_info("%s In ESD_RECOVERY",__func__);

			ctrl_pdata->panel_data.event_handler(pdata_for_esd, MDSS_EVENT_PANEL_OFF, NULL);

			mdelay(20);
			ctrl_pdata->panel_data.event_handler(pdata_for_esd, MDSS_EVENT_UNBLANK, NULL);

#if defined(CONFIG_MDNIE_LITE_TUNING)
			//is_negative_on();
#endif
			mutex_unlock(&power_state_chagne);
		}
	}
}
#endif
static int __devinit mdss_dsi_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;
	u32 index;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdev->dev.of_node) {
		struct resource *mdss_dsi_mres;
		const char *ctrl_name;

		ctrl_pdata = platform_get_drvdata(pdev);
		if (!ctrl_pdata) {
			ctrl_pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct mdss_dsi_ctrl_pdata), GFP_KERNEL);
			if (!ctrl_pdata) {
				pr_err("%s: FAILED: cannot alloc dsi ctrl\n",
					__func__);
				rc = -ENOMEM;
				goto error_no_mem;
			}
			platform_set_drvdata(pdev, ctrl_pdata);
		}

		ctrl_name = of_get_property(pdev->dev.of_node, "label", NULL);
		if (!ctrl_name)
			pr_info("%s:%d, DSI Ctrl name not specified\n",
						__func__, __LINE__);
		else
			pr_info("%s: DSI Ctrl name = %s\n",
				__func__, ctrl_name);

		rc = of_property_read_u32(pdev->dev.of_node,
					  "cell-index", &index);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Cell-index not specified, rc=%d\n",
							__func__, rc);
			goto error_no_mem;
		}

		if (index == 0)
			pdev->id = 1;
		else
			pdev->id = 2;

		mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!mdss_dsi_mres) {
			pr_err("%s:%d unable to get the MDSS resources",
				       __func__, __LINE__);
			rc = -ENOMEM;
			goto error_no_mem;
		}
		if (mdss_dsi_mres) {
			mdss_dsi_base = ioremap(mdss_dsi_mres->start,
				resource_size(mdss_dsi_mres));
			if (!mdss_dsi_base) {
				pr_err("%s:%d unable to remap dsi resources",
					       __func__, __LINE__);
				rc = -ENOMEM;
				goto error_no_mem;
			}
		}

		rc = of_platform_populate(pdev->dev.of_node,
					NULL, NULL, &pdev->dev);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: failed to add child nodes, rc=%d\n",
							__func__, rc);
			goto error_ioremap;
		}

		/* Parse the regulator information */
		rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data);
		if (rc) {
			pr_err("%s: failed to get vreg data from dt. rc=%d\n",
				__func__, rc);
			goto error_ioremap;
		}

		pr_debug("%s: Dsi Ctrl->%d initialized\n", __func__, index);
	}

	return 0;

	mdss_dsi_put_dt_vreg_data(&pdev->dev, &ctrl_pdata->power_data);
error_ioremap:
	iounmap(mdss_dsi_base);
error_no_mem:
	devm_kfree(&pdev->dev, ctrl_pdata);

	return rc;
}

static int __devexit mdss_dsi_ctrl_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	if (msm_dss_config_vreg(&pdev->dev,
			ctrl_pdata->power_data.vreg_config,
			ctrl_pdata->power_data.num_vreg, 1) < 0)
		pr_err("%s: failed to de-init vregs\n", __func__);
	mdss_dsi_put_dt_vreg_data(&pdev->dev, &ctrl_pdata->power_data);
	mfd = platform_get_drvdata(pdev);
	iounmap(mdss_dsi_base);
	return 0;
}

struct device dsi_dev;

int mdss_dsi_retrieve_ctrl_resources(struct platform_device *pdev, int mode,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 index;
	struct resource *mdss_dsi_mres;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Cell-index not specified, rc=%d\n",
						__func__, rc);
		return rc;
	}

	if (index == 0) {
		if (mode != DISPLAY_1) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else if (index == 1) {
		if (mode != DISPLAY_2) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else {
		pr_err("%s:%d Unknown Ctrl mapped to panel",
			       __func__, __LINE__);
		return -EPERM;
	}

	mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mdss_dsi_mres) {
		pr_err("%s:%d unable to get the DSI ctrl resources",
			       __func__, __LINE__);
		return -ENOMEM;
	}

	ctrl->ctrl_base = ioremap(mdss_dsi_mres->start,
		resource_size(mdss_dsi_mres));
	if (!(ctrl->ctrl_base)) {
		pr_err("%s:%d unable to remap dsi resources",
			       __func__, __LINE__);
		return -ENOMEM;
	}

	ctrl->reg_size = resource_size(mdss_dsi_mres);

	pr_info("%s: dsi base=%x size=%x\n",
		__func__, (int)ctrl->ctrl_base, ctrl->reg_size);

	return 0;
}

#ifdef DEBUG_LDI_STATUS
int read_ldi_status(void);
#endif
void mdss_dsi_dump_power_clk(struct mdss_panel_data *pdata, int flag) {
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL)
	u8 rc, te_count = 0;
	u8 te_max = 250;
#endif
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL)
	pr_info(" ============ waiting for TE ============\n");
	for (te_count = 0 ; te_count < te_max ; te_count++)
	{
		rc = gpio_get_value(ctrl_pdata->disp_te_gpio);
		if(rc != 0)
		{
			pr_info("%s: gpio_get_value(ctrl_pdata->disp_te_gpio) =%d\n",
				__func__, rc);
			break;
		}
		
		udelay(80);
	}		
#endif

	pr_info(" ============ dump power & clk start ============\n");
	if ((ctrl_pdata->shared_pdata).vdd_vreg)
		pr_info("vdd_vreg : %d\n", regulator_is_enabled((ctrl_pdata->shared_pdata).vdd_vreg));
	if ((ctrl_pdata->shared_pdata).vdd_io_vreg)
		pr_info("vdd_io_vreg : %d\n", regulator_is_enabled((ctrl_pdata->shared_pdata).vdd_io_vreg));
	if ((ctrl_pdata->shared_pdata).vdda_vreg)
		pr_info("vdda_vreg : %d\n", regulator_is_enabled((ctrl_pdata->shared_pdata).vdda_vreg));
	clock_debug_print_clock2(ctrl_pdata->pixel_clk);
	clock_debug_print_clock2(ctrl_pdata->byte_clk);
	clock_debug_print_clock2(ctrl_pdata->esc_clk);
	pr_info("%s: ctrl ndx=%d clk_cnt=%d\n",
			__func__, ctrl_pdata->ndx, ctrl_pdata->clk_cnt);
	pr_info(" ============ dump power & clk end ============\n");
	pr_info(" === check manufacture ID cf) EVT0 0xXXXX0X / EVT1 0xXXXX2X ===\n");
	pr_info(" Current LDI manufacture ID = 0x%x	\n", gv_manufacture_id);	
#if !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_FULL_HD_PT_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_SAMSUNG_OCTA_VIDEO_WVGA_S6E88A0_PT_PANEL)
	if(te_count == te_max)
	{
		pr_info("LDI doesn't generate TE/ manufacture ID = 0x%x", gv_manufacture_id);
#ifdef DEBUG_LDI_STATUS	
		if(flag)
		{
			if(read_ldi_status())
			pr_err("%s : Can not read LDI status\n",__func__);
		}
#endif		
		panic("LDI doesn't generate TE/ manufacture ID = 0x%x", gv_manufacture_id);
	}

#endif
}

int dsi_panel_device_register(struct platform_device *pdev,
			      struct mdss_panel_common_pdata *panel_data)
{
	struct mipi_panel_info *mipi;
	int rc;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct device_node *dsi_ctrl_np = NULL;
	struct platform_device *ctrl_pdev = NULL;
	bool broadcast;
#if 0
	bool cont_splash_enabled = false;
#endif

	mipi  = &panel_data->panel_info.mipi;

	panel_data->panel_info.type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	rc = mdss_dsi_clk_div_config(&panel_data->panel_info, mipi->frame_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n", __func__);
		return rc;
	}

	dsi_ctrl_np = of_parse_phandle(pdev->dev.of_node,
				       "qcom,dsi-ctrl-phandle", 0);
	if (!dsi_ctrl_np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	ctrl_pdev = of_find_device_by_node(dsi_ctrl_np);
	ctrl_pdata = platform_get_drvdata(ctrl_pdev);
	if (!ctrl_pdata) {
		pr_err("%s: no dsi ctrl driver data\n", __func__);
		return -EINVAL;
	}

	rc = mdss_dsi_regulator_init(ctrl_pdev);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	broadcast = of_property_read_bool(pdev->dev.of_node,
					  "qcom,mdss-pan-broadcast-mode");
	if (broadcast)
		ctrl_pdata->shared_pdata.broadcast_enable = 1;

	ctrl_pdata->disp_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						     "qcom,enable-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->disp_en_gpio, "disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_en_gpio);
			return -ENODEV;
		}
	}

	ctrl_pdata->disp_te_gpio = of_get_named_gpio(pdev->dev.of_node,
						     "qcom,te-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio)) {
		pr_err("%s:%d, Disp_te gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->disp_te_gpio, "disp_te");
		if (rc) {
			pr_err("request TE gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
		rc = gpio_tlmm_config(GPIO_CFG(
				ctrl_pdata->disp_te_gpio, 1,
				GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN,
				GPIO_CFG_2MA),
				GPIO_CFG_ENABLE);

		if (rc) {
			pr_err("%s: unable to config tlmm = %d\n",
				__func__, ctrl_pdata->disp_te_gpio);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
		
		rc = gpio_direction_input(ctrl_pdata->disp_te_gpio);
		if (rc) {
			pr_err("set_direction for disp_en gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
		pr_debug("%s: te_gpio=%d\n", __func__,
					ctrl_pdata->disp_te_gpio);
	}


	ctrl_pdata->rst_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,rst-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
				rc);
			gpio_free(ctrl_pdata->rst_gpio);
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_free(ctrl_pdata->disp_en_gpio);
			return -ENODEV;
		}
	}

 #ifdef CONFIG_SAMSUNG_CMC624
	ctrl_pdata->boost_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,boost-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->boost_gpio)) {
		pr_err("%s:%d, booster gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->boost_gpio, "boost_gpio");
		if (rc) {
			pr_err("request booster gpio failed, rc=%d\n",rc);
			gpio_free(ctrl_pdata->boost_gpio);
			return -ENODEV;
		}
	}
	ctrl_pdata->vddi_en = of_get_named_gpio(pdev->dev.of_node, "qcom,vddi_en", 0);
	if (!gpio_is_valid(ctrl_pdata->vddi_en)) {
		pr_err("%s:%d, vddi_en gpio not specified\n", __func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->vddi_en, "vddi_en");
		if (rc) {
			pr_err("request vddi_en gpio failed, rc=%d\n",rc);
			gpio_free(ctrl_pdata->vddi_en);
			return -ENODEV;
		}
	}

	ctrl_pdata->lcd_en = of_get_named_gpio(pdev->dev.of_node, "qcom,lcd_en", 0);
	if (!gpio_is_valid(ctrl_pdata->lcd_en)) {
		pr_err("%s:%d, lcd_en gpio not specified\n", __func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->lcd_en, "lcd_en");
		if (rc) {
			pr_err("request vddi_en gpio failed, rc=%d\n",rc);
			gpio_free(ctrl_pdata->lcd_en);
			return -ENODEV;
		}
	}
#endif

#if defined(CONFIG_DUAL_LCD)
	ctrl_pdata->lcd_sel_gpio = of_get_named_gpio(pdev->dev.of_node, "samsung,lcd-sel-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->lcd_sel_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n", __func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->lcd_sel_gpio, "lcd_sel");
		if (rc) {
			pr_err("request lcd_sel gpio failed, rc=%d\n", rc);
			gpio_free(ctrl_pdata->lcd_sel_gpio);
			return -ENODEV;
		}
	}
#endif

#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
	ctrl_pdata->lcd_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,lcd-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->lcd_en_gpio)) {
		pr_err("%s:%d, lcd_en_gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->lcd_en_gpio, "lcd_en_gpio");
		if (rc) {
			pr_err("request lcd_en_gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->lcd_en_gpio);
			return -ENODEV;
		}
		rc = gpio_tlmm_config(GPIO_CFG(ctrl_pdata->lcd_en_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

		if (rc) {
			pr_err("%s: unable to config tlmm = %d\n",
				__func__, ctrl_pdata->lcd_en_gpio);
			gpio_free(ctrl_pdata->lcd_en_gpio);
			return -ENODEV;
		}

		rc = gpio_direction_output(ctrl_pdata->lcd_en_gpio,1);
		if (rc) {
			pr_err("set_direction for lcd_en_gpio gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->lcd_en_gpio);
			return -ENODEV;
		}
		pr_info("%s: lcd_en_gpio=%d\n", __func__,
					ctrl_pdata->lcd_en_gpio);
	}

	ctrl_pdata->bl_en_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,bl-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->bl_en_gpio)) {
		pr_err("%s:%d, bl_en gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->bl_en_gpio, "bl_en_gpio");
		if (rc) {
			pr_err("request bl_en gpio failed, rc=%d\n",
				rc);
			gpio_free(ctrl_pdata->bl_en_gpio);
			return -ENODEV;
		}
	}
#endif
#if defined(CONFIG_FB_MSM_MIPI_TFT_VIDEO_FULL_HD_PT_PANEL)
	ctrl_pdata->lcd_en_p_gpio = of_get_named_gpio(pdev->dev.of_node,
						 "qcom,lcd-en-p-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->lcd_en_p_gpio)) {
		pr_err("%s:%d, lcd_en_p gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->lcd_en_p_gpio, "lcd_en_p_gpio");
		if (rc) {
			pr_err("request lcd_en_p gpio failed, rc=%d\n",
				rc);
			gpio_free(ctrl_pdata->lcd_en_p_gpio);
			return -ENODEV;
		}
	}
#endif

	if (mdss_dsi_clk_init(ctrl_pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize Dsi ctrl clks\n", __func__);
		return -EPERM;
	}

	if (mdss_dsi_retrieve_ctrl_resources(ctrl_pdev,
					     panel_data->panel_info.pdest,
					     ctrl_pdata)) {
		pr_err("%s: unable to get Dsi controller res\n", __func__);
		return -EPERM;
	}

	ctrl_pdata->panel_data.event_handler = mdss_dsi_event_handler;

	ctrl_pdata->panel_data.panel_reset_fn = panel_data->panel_reset;

//	ctrl_pdata->on_cmd_buf = panel_data->on_cmd_buf;
//	ctrl_pdata->on_cmd_len = panel_data->on_cmd_len;
	ctrl_pdata->dsi_on_state = panel_data->dsi_on_state;
//	ctrl_pdata->off_cmd_buf = panel_data->off_cmd_buf;
//	ctrl_pdata->off_cmd_len = panel_data->off_cmd_len;
	ctrl_pdata->dsi_off_state = panel_data->dsi_off_state;

	memcpy(&((ctrl_pdata->panel_data).panel_info),
				&(panel_data->panel_info),
				       sizeof(struct mdss_panel_info));

	ctrl_pdata->panel_data.set_backlight = panel_data->bl_fnc;
	ctrl_pdata->bklt_ctrl = panel_data->panel_info.bklt_ctrl;
	ctrl_pdata->pwm_pmic_gpio = panel_data->panel_info.pwm_pmic_gpio;
	ctrl_pdata->pwm_period = panel_data->panel_info.pwm_period;
	ctrl_pdata->pwm_lpg_chan = panel_data->panel_info.pwm_lpg_chan;
	ctrl_pdata->bklt_max = panel_data->panel_info.bl_max;
	ctrl_pdata->panel_data.panel_reset_fn = panel_data->panel_reset;
	ctrl_pdata->vendor_event_handler = panel_data->event_handler;

	if (ctrl_pdata->bklt_ctrl == BL_PWM)
		mdss_dsi_panel_pwm_cfg(ctrl_pdata);

	mdss_dsi_ctrl_init(ctrl_pdata);
	/*
	 * register in mdp driver
	 */

	ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = panel_data->panel_info.clk_rate / 8;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate);

	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;

	if(!contsplash_lkstat)
	{
		pr_info("%s:%d Continuous splash flag not found.\n",
			__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
		ctrl_pdata->panel_data.panel_info.panel_power_on = 0;
	}
	else
	{
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);
		
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
		ctrl_pdata->panel_data.panel_info.panel_power_on = 1;
		rc = mdss_dsi_panel_power_on(&(ctrl_pdata->panel_data), 1);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}
		mdss_dsi_clk_ctrl(ctrl_pdata, 1);
		ctrl_pdata->ctrl_state |=
			(CTRL_STATE_MDP_ACTIVE);

	}
	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		dev_err(&pdev->dev, "unable to register MIPI DSI panel\n");
		if (ctrl_pdata->rst_gpio)
			gpio_free(ctrl_pdata->rst_gpio);
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_free(ctrl_pdata->disp_en_gpio);
#ifdef CONFIG_FB_MSM_MIPI_JDI_TFT_VIDEO_FULL_HD_PT_PANEL
		if (gpio_is_valid(ctrl_pdata->lcd_en_gpio))
			gpio_free(ctrl_pdata->lcd_en_gpio);
		if (gpio_is_valid(ctrl_pdata->bl_en_gpio))
			gpio_free(ctrl_pdata->bl_en_gpio);
#endif
		return rc;
	}

	pr_debug("%s: mdss_register panel success\n", __func__);
	ctrl_pdata->on = panel_data->on;
	ctrl_pdata->off = panel_data->off;
	/*
	 *  Added to call the driver to initialise internal data and to display
	 *  kernel logo. In ES5, the frame buffer registration might be done later,
	 *  hence logo display call also can be made once FB is registered
	 */
	ctrl_pdata->registered = panel_data->panel_registered;
	ctrl_pdata->dimming_init = panel_data->dimming_init;
	ctrl_pdata->panel_blank = panel_data->blank;

	if (panel_data->panel_info.pdest == DISPLAY_1) {
		mdss_debug_register_base("dsi0",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 0;
	} else {
		mdss_debug_register_base("dsi1",
			ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);
		ctrl_pdata->ndx = 1;
	}

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

static const struct of_device_id mdss_dsi_ctrl_dt_match[] = {
	{.compatible = "qcom,mdss-dsi-ctrl"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_ctrl_dt_match);

static struct platform_driver mdss_dsi_ctrl_driver = {
	.probe = mdss_dsi_ctrl_probe,
	.remove = __devexit_p(mdss_dsi_ctrl_remove),
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dsi_ctrl",
		.of_match_table = mdss_dsi_ctrl_dt_match,
	},
};

static int mdss_dsi_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_ctrl_driver);
}

static int __init mdss_dsi_driver_init(void)
{
	int ret;

	ret = mdss_dsi_register_driver();
	if (ret) {
		pr_err("mdss_dsi_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_driver_init);

static void __exit mdss_dsi_driver_cleanup(void)
{
	iounmap(mdss_dsi_base);
	platform_driver_unregister(&mdss_dsi_ctrl_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
MODULE_AUTHOR("Chandan Uddaraju <chandanu@codeaurora.org>");
