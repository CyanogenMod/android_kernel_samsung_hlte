/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/irqs.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>
#include <linux/qpnp/pin.h>
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)
#include <linux/qpnp/qpnp-adc.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_charging_common.h>
#ifdef CONFIG_SAMSUNG_BATTERY_DISALLOW_DEEP_SLEEP
#include "lpm_resources.h"
#endif

#define SEC_BATTERY_PMIC_NAME ""

#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
	defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_JACTIVESKT)
#define GPIO_FUELGAUGE_I2C_SDA		23
#define GPIO_FUELGAUGE_I2C_SCL		24
#define GPIO_FUEL_INT				42

#elif defined(CONFIG_MACH_MONTBLANC)
#define GPIO_FUELGAUGE_I2C_SDA		23
#define GPIO_FUELGAUGE_I2C_SCL		24
#define GPIO_FUEL_INT				42

#else
#define GPIO_FUELGAUGE_I2C_SDA		87
#define GPIO_FUELGAUGE_I2C_SCL		88
#define GPIO_FUEL_INT				26

#endif

#ifndef CONFIG_OF
#define MSM_FUELGAUGE_I2C_BUS_ID	19
#endif

#define SHORT_BATTERY_STANDARD		100

static sec_charging_current_t charging_current_table[] = {
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
	defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_JACTIVESKT) || defined(CONFIG_MACH_JS01LTEDCM)
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{1900,	1600,	200,	40*60},
	{460,	460,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1700,	1600,	200,	40*60},
	{0,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1900,	1600,	200,	40*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
#elif defined(CONFIG_MACH_MELIUSCASKT) || defined(CONFIG_MACH_MELIUSCAKTT) || \
	defined(CONFIG_MACH_MELIUSCALGT)
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{1900,	2100,	200,	40*60},
	{460,	460,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1700,	2100,	200,	40*60},
	{0,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1900,	2100,	200,	40*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
#elif defined(CONFIG_MACH_MONTBLANC)
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{1300,	1600,	200,	40*60},
	{460,	460,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1100,	1600,	200,	40*60},
	{0,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1300,	1600,	200,	40*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
#elif defined(CONFIG_MACH_FLTESKT)
	{1700,	2100,	200,	40*60},
	{460,	0,	0,	0},
	{460,	460,	200,	40*60},
	{1700,	2100,	200,	40*60},
	{460,	460,	200,	40*60},
	{900,	1200,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1200,	200,	40*60},
	{460,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1700,	2100,	200,	40*60},
	{460,	0,	0,	0},
	{460,	0,	0,	0},
#else
	{1800,	2100,	200,	40*60},
	{460,	0,	0,	0},
	{460,	460,	200,	40*60},
	{1800,	2100,	200,	40*60},
	{460,	460,	200,	40*60},
	{900,	1200,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1200,	200,	40*60},
	{460,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1800,	2100,	200,	40*60},
	{460,	0,	0,	0},
	{460,	0,	0,	0},
#endif
};

static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static bool sec_bat_adc_ap_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ap_exit(void) {return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)
	int rc = -1, data =  -1;
	struct qpnp_vadc_result results;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
		rc = qpnp_vadc_read(LR_MUX5_PU2_AMUX_THM2, &results);
		if (rc) {
			pr_err("%s: Unable to read batt temperature rc=%d\n",
				__func__, rc);
			return 0;
		}
		data = results.adc_code;
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK:
		break;
	default:
		break;
	}

	return data;
#else
	return 33000;
#endif
}

static bool sec_bat_adc_ic_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel) {return 0; }

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

extern int poweroff_charging;
static bool sec_bat_is_lpm(void) {return (bool)poweroff_charging; }

int extended_cable_type;
extern int current_cable_type;
extern unsigned int system_rev;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	if (POWER_SUPPLY_TYPE_BATTERY < current_cable_type) {
		value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval =
			POWER_SUPPLY_TYPE_WIRELESS<<ONLINE_TYPE_MAIN_SHIFT;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	}

#if defined(CONFIG_SEC_LOCALE_KOR)
#if defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
		defined(CONFIG_MACH_HLTELGT)
	if(system_rev < 3) {
		sec_battery_pdata.temp_check_type =
			SEC_BATTERY_TEMP_CHECK_NONE;
	}
#endif
#endif
}

static bool sec_bat_check_jig_status(void)
{
	return (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF);
}
static bool sec_bat_switch_to_check(void) {return true; }
static bool sec_bat_switch_to_normal(void) {return true; }

static int sec_bat_check_cable_callback(void)
{
	return current_cable_type;
}

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_MHL_500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 400;
				charge_current = 400;
				break;
			case ONLINE_POWER_TYPE_MHL_900:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 700;
				charge_current = 700;
				break;
			case ONLINE_POWER_TYPE_MHL_1500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 1300;
				charge_current = 1300;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			default:
				cable_type = cable_main;
				break;
			}
			break;
		case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 1000;
				charge_current = 1000;
				break;
		case ONLINE_SUB_TYPE_SMART_NOTG:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
		default:
			cable_type = cable_main;
			charge_current_max = 0;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_OTG:
		cable_type = current_cable_type;
		break;
	default:
		cable_type = cable_main;
		break;
	}

	if (charge_current_max == 0) {
		charge_current_max =
			charging_current_table[cable_type].input_current_limit;
		charge_current =
			charging_current_table[cable_type].
			fast_charging_current;
	}
	value.intval = charge_current_max;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
	value.intval = charge_current;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
	return cable_type;
}

#ifdef CONFIG_SAMSUNG_BATTERY_DISALLOW_DEEP_SLEEP
static void sec_bat_disallow_deep_sleep_fn(bool disallow)
{
	static bool disallow_deep_sleep = false;

	if(disallow && !disallow_deep_sleep) {
		msm_lpm_disable_pxo_low_power();
		disallow_deep_sleep = true;
		pr_info("%s:%d\n", __func__, disallow_deep_sleep);
	} else if(!disallow && disallow_deep_sleep) {
		msm_lpm_enable_pxo_low_power();
		disallow_deep_sleep = false;
		pr_info("%s:%d\n", __func__, disallow_deep_sleep);
	}
}
#endif

static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	struct regulator *ldo11;
	current_cable_type = cable_type;

	if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
	{
		pr_info("%s set ldo off\n", __func__);
		ldo11 = regulator_get(NULL, "8941_l11");
		if(ldo11 > 0)
		{
			regulator_disable(ldo11);
		}
	}
	else
	{
		pr_info("%s set ldo on\n", __func__);
		ldo11 = regulator_get(NULL, "8941_l11");
		if(ldo11 > 0)
		{
			regulator_enable(ldo11);
		}
	}

#ifdef CONFIG_SAMSUNG_BATTERY_DISALLOW_DEEP_SLEEP
	if (charging_current_table[cable_type].fast_charging_current != 0) {
		sec_bat_disallow_deep_sleep_fn(true);
	} else {
		sec_bat_disallow_deep_sleep_fn(false);
	}
#endif
	return true;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void)
{
	struct power_supply *psy;
	union power_supply_propval value;

	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
				__func__, "sec-charger");
		value.intval = 1;
	} else {
		if (sec_battery_pdata.bat_irq_gpio > 0) {
			value.intval = !gpio_get_value(sec_battery_pdata.bat_irq_gpio);
			if (value.intval == 0) {
				pr_info("%s:  Battery status(%d)\n",
					__func__, value.intval);
				return value.intval;
			}
#if defined(CONFIG_MACH_HLTEATT) || defined(CONFIG_MACH_HLTESPR) || \
			defined(CONFIG_MACH_HLTEVZW) || defined(CONFIG_MACH_HLTETMO) || \
			defined(CONFIG_MACH_HLTEUSC)
			{
				int data, ret;
				struct qpnp_vadc_result result;
				struct qpnp_pin_cfg adc_param = {
					.mode = 4,
					.ain_route = 3,
					.src_sel = 0,
					.master_en =1,
				};
				struct qpnp_pin_cfg int_param = {
					.mode = 0,
					.vin_sel = 2,
					.src_sel = 0,
					.master_en =1,
				};
				ret = qpnp_pin_config(sec_battery_pdata.bat_irq_gpio, &adc_param);
				if (ret < 0)
					pr_info("%s: qpnp config error: %d\n",
							__func__, ret);
				/* check the adc from vf pin */
				qpnp_vadc_read(P_MUX8_1_3, &result);
				data = ((int)result.physical) / 1000;
				if(data < SHORT_BATTERY_STANDARD) {
					pr_info("%s: Short Battery(%dmV) is connected.\n",
							__func__, data);
					value.intval = 0;
				}
				ret = qpnp_pin_config(sec_battery_pdata.bat_irq_gpio, &int_param);
				if (ret < 0)
					pr_info("%s: qpnp config error int: %d\n",
							__func__, ret);
			}
#endif
		} else {
			int ret;
			ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &(value));
			if (ret < 0) {
				pr_err("%s: Fail to sec-charger get_property (%d=>%d)\n",
						__func__, POWER_SUPPLY_PROP_PRESENT, ret);
				value.intval = 1;
			}
		}
	}
	return value.intval;
}
static bool sec_bat_check_result_callback(void) {return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }
static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

#if defined(CONFIG_MACH_H3GDUOS_CTC)
static const sec_bat_adc_table_data_t temp_table[] = {
	{27240,	700},
	{27662,	650},
	{28202,	600},
	{28777,	550},
	{29444,	500},
	{30193,	450},
	{31040,	400},
	{31953,	350},
	{32942,	300},
	{33991,	250},
	{35070,	200},
	{36168,	150},
	{37238,	100},
	{38234,	50},
	{38414,	40},
	{38640,	30},
	{38866,	20},
	{39092,	10},
	{39139,	0},
	{39342,	-10},
	{39545,	-30},
	{39748,	-40},
	{39954,	-50},
	{40641,	-100},
	{41204,	-150},
	{41689,	-200},
};
#elif defined(CONFIG_MACH_HLTEUSC)
static const sec_bat_adc_table_data_t temp_table[] = {
	{26962,	700},
	{27363,	650},
	{27850,	600},
	{28407,	550},
	{29084,	500},
	{29857,	450},
	{30684,	400},
	{31576,	350},
	{32674,	300},
	{33694,	250},
	{34848,	200},
	{35963,	150},
	{37118,	100},
	{38166,	50},
	{39104,	0},
	{39932,	-50},
	{40629,	-100},
	{41224,	-150},
	{41704,	-200},
};
#elif defined(CONFIG_MACH_HLTEKDI)
static const sec_bat_adc_table_data_t temp_table[] = {
	{27293,	700},
	{27703,	650},
	{28203,	600},
	{28773,	550},
	{29414,	500},
	{30181,	450},
	{31023,	400},
	{31932,	350},
	{32924,	300},
	{33962,	250},
	{35007,	200},
	{36069,	150},
	{37080,	100},
	{38102,	50},
	{39049,	0},
	{39542,	-50},
	{40564,	-100},
	{41130,	-150},
	{41651,	-200},
};
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
static const sec_bat_adc_table_data_t temp_table[] = {
	{27130,	700},
	{27446,	650},
	{27858,	600},
	{28318,	550},
	{28939,	500},
	{28961,	450},
	{30441,	400},
	{31282,	350},
	{32259,	300},
	{33343,	250},
	{34402,	200},
	{35261,	150},
	{36440,	100},
	{37616,	50},
	{38419,	0},
	{39282,	-50},
	{40180,	-100},
	{41123,	-150}, /* H project */
	{41619,	-200}, /* H project */
};
#elif defined(CONFIG_MACH_MONTBLANC)
static const sec_bat_adc_table_data_t temp_table[] = {
	{26250,	800},	
	{26526,	750},		
	{26914,	700},
	{27310,	650},
	{27822,	600},
	{28244,	550},
	{28981,	500},
	{29906,	450},
	{30541,	400},
	{31318,	350},
	{32411,	300},
	{33485,	250},
	{34683,	200},
	{35822,	150},
	{37068,	100},
	{38147,	50},
	{39123,	0},
	{39968,	-50},
	{40590,	-100},
	{41215,	-150},
	{41724,	-200},
};
#else
static const sec_bat_adc_table_data_t temp_table[] = {
	{27281,	700},
	{27669,	650},
	{28178,	600},
	{28724,	550},
	{29342,	500},
	{30101,	450},
	{30912,	400},
	{31807,	350},
	{32823,	300},
	{33858,	250},
	{34950,	200},
	{36049,	150},
	{37054,	100},
	{38025,	50},
	{38219,	40},
	{38448,	30},
	{38626,	20},
	{38795,	10},
	{38989,	0},
	{39229,	-10},
	{39540,	-30},
	{39687,	-40},
	{39822,	-50},
	{40523,	-100},
	{41123,	-150},
	{41619,	-200},
};
#endif
/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	60 * 60,	/* SLEEP */
};

/* for MAX17048 */
static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
	defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_JACTIVESKT)
		.RCOMP0 = 0x70,
		.RCOMP_charging = 0x79,
		.temp_cohot = -850,
		.temp_cocold = -4200,
#elif defined(CONFIG_MACH_MELIUSCASKT) || defined(CONFIG_MACH_MELIUSCAKTT) || \
		defined(CONFIG_MACH_MELIUSCALGT)
		.RCOMP0 = 0x85,
		.RCOMP_charging = 0x85,
		.temp_cohot = -925,
		.temp_cocold = -8175,
#elif defined(CONFIG_MACH_MONTBLANC)
		.RCOMP0 = 0x50,
		.RCOMP_charging = 0x65,
		.temp_cohot = -425,
		.temp_cocold = -4200,
#elif defined(CONFIG_MACH_HLTEDCM)
		.RCOMP0 = 0x7A,
		.RCOMP_charging = 0x84,
		.temp_cohot = -1025,
		.temp_cocold = -3675,
#elif defined(CONFIG_MACH_HLTEKDI)
		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x84,
		.temp_cohot = -1025,
		.temp_cocold = -3675,
#elif defined(CONFIG_MACH_JS01LTEDCM)
		.RCOMP0 = 0x5D,
		.RCOMP_charging = 0x7F,
		.temp_cohot = -700,
		.temp_cocold = -4875,
#elif defined(CONFIG_MACH_HLTEVZW) || defined(CONFIG_MACH_HLTESPR)
		.RCOMP0 = 0x70,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#elif defined(CONFIG_MACH_HLTEATT)
		.RCOMP0 = 0x75,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#elif defined(CONFIG_MACH_HLTEUSC)
		.RCOMP0 = 0x75,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#elif defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
		defined(CONFIG_MACH_HLTELGT)
		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x84,
		.temp_cohot = -1025,
		.temp_cocold = -3675,
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
		.RCOMP0 = 0x68,
		.RCOMP_charging = 0x68,
		.temp_cohot = -1375,
		.temp_cocold = -3975,
#elif defined(CONFIG_MACH_H3GDUOS_CU)
		.RCOMP0 = 0x75,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#elif defined(CONFIG_MACH_HLTEEUR)
		.RCOMP0 = 0x62,
		.RCOMP_charging = 0x7C,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#else
		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#endif
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};

#ifndef CONFIG_OF
/* temp, same with board-8974-sec.c */
#define IF_PMIC_IRQ_BASE	353 /* temp val */
#endif

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
		},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_AP,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)samsung_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 0,
#ifndef CONFIG_OF
	.bat_irq = IF_PMIC_IRQ_BASE + MAX77803_CHG_IRQ_BATP_I,
#else
	.bat_irq = 0, // from dts
#endif
	.bat_irq_gpio = 0,
	.bat_irq_attr = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
#if defined(CONFIG_MACH_HLTEVZW)
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_PSY |
		SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE,
#else
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_PSY,
#endif
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL |
		SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	.event_check = true,
	.event_waiting_time = 600,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_INT,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 1440,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_JACTIVESKT) || defined(CONFIG_MACH_JS01LTEDCM)
	.temp_high_threshold_event = 670,
	.temp_high_recovery_event = 420,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 670,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 670,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_KS01KTT)
	.temp_high_threshold_event = 670,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 670,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 670,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_MONTBLANC)
	.temp_high_threshold_event = 800,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -300,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 800,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -300,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 800,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -300,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
	defined(CONFIG_MACH_HLTELGT)
	.temp_high_threshold_event = 680,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 680,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 680,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_HLTEATT) || defined(CONFIG_MACH_HLTETMO)
	.temp_high_threshold_event = 610,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 490,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 490,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_HLTESPR)
	.temp_high_threshold_event = 640,
	.temp_high_recovery_event = 438,
	.temp_low_threshold_event = -37,
	.temp_low_recovery_event = 11,

	.temp_high_threshold_normal = 529,
	.temp_high_recovery_normal = 438,
	.temp_low_threshold_normal = -37,
	.temp_low_recovery_normal = 11,

	.temp_high_threshold_lpm = 529,
	.temp_high_recovery_lpm = 459,
	.temp_low_threshold_lpm = -23,
	.temp_low_recovery_lpm = -10,
#elif defined(CONFIG_MACH_HLTEVZW)
	.temp_high_threshold_event = 650,
	.temp_high_recovery_event = 450,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = -10,

	.temp_high_threshold_normal = 550,
	.temp_high_recovery_normal = 450,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = -10,

	.temp_high_threshold_lpm = 520,
	.temp_high_recovery_lpm = 470,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = -15,
#elif defined(CONFIG_MACH_HLTEDCM)
	.temp_high_threshold_event = 625,
	.temp_high_recovery_event = 420,
	.temp_low_threshold_event =  -45,
	.temp_low_recovery_event = -5,
	.temp_high_threshold_normal = 625,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = -5,
	.temp_high_threshold_lpm = 625,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = -5,
#elif defined(CONFIG_MACH_HLTEKDI)
	.temp_high_threshold_event = 660,
	.temp_high_recovery_event = 420,
	.temp_low_threshold_event =  -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 660,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 660,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_HLTEUSC)
	.temp_high_threshold_event = 650,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 640,
	.temp_high_recovery_normal = 450,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 650,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_FLTEEUR)
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 400,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 400,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_FLTESKT)
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 400,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 400,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_H3GDUOS_CTC)
	.temp_high_threshold_event = 590,
	.temp_high_recovery_event = 420,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 590,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 590,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_HLTEEUR) || defined(CONFIG_MACH_H3GDUOS_CU)
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 400,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 400,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#else
	.temp_high_threshold_event = 650,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 650,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 650,
	.temp_high_recovery_lpm = 440,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#endif

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4300,

	.recharge_check_count = 1,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 98,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
	defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_MONTBLANC) || defined(CONFIG_MACH_JACTIVESKT)
	.fg_irq = GPIO_FUEL_INT,
#else
	.fg_irq = 0,
#endif
	.fg_irq_attr =
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.fuel_alert_soc = 2,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,
		/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC, */
#if defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
	defined(CONFIG_MACH_HLTELGT) || defined(CONFIG_MACH_HLTEDCM) 
	.capacity_max = 980,
	.capacity_min = -7,
#elif defined(CONFIG_MACH_HLTEKDI)
	.capacity_max = 980,
	.capacity_min = -7,
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
	.capacity_max = 990,
	.capacity_min = 0,
#else
	.capacity_max = 990,
	.capacity_min = -7,
#endif
	.capacity_max_margin = 50,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = 0,
	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

#ifndef CONFIG_OF
static struct i2c_gpio_platform_data gpio_i2c_data_fgchg = {
	.sda_pin = GPIO_FUELGAUGE_I2C_SDA,
	.scl_pin = GPIO_FUELGAUGE_I2C_SCL,
};

struct platform_device sec_device_fgchg = {
	.name = "i2c-gpio",
	.id = MSM_FUELGAUGE_I2C_BUS_ID,
	.dev.platform_data = &gpio_i2c_data_fgchg,
};

static struct i2c_board_info sec_brdinfo_fgchg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};
#endif

static struct platform_device *samsung_battery_devices[] __initdata = {
#ifndef CONFIG_OF
	&sec_device_fgchg,
#endif
	&sec_device_battery,
};

void __init samsung_init_battery(void)
{
	/* FUEL_SDA/SCL setting */
	platform_add_devices(
		samsung_battery_devices,
		ARRAY_SIZE(samsung_battery_devices));

#ifndef CONFIG_OF
	i2c_register_board_info(
		MSM_FUELGAUGE_I2C_BUS_ID,
		sec_brdinfo_fgchg,
		ARRAY_SIZE(sec_brdinfo_fgchg));
#endif
}

#endif
