/*
 *  sec_board-msm8974.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charging_common.h>

#include <linux/qpnp/pin.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>

#define SHORT_BATTERY_STANDARD      100

#if defined(CONFIG_EXTCON)
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#else
extern int current_cable_type;
#endif
extern unsigned int system_rev;

#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
#if defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
		defined(CONFIG_MACH_HLTELGT)
		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x84,
		.temp_cohot = -1025,
		.temp_cocold = -3675,
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
#elif defined(CONFIG_MACH_HLTEEUR)
		.RCOMP0 = 0x62,
		.RCOMP_charging = 0x7C,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
		.RCOMP0 = 0x75,
		.RCOMP_charging = 0x70,
		.temp_cohot = -375,
		.temp_cocold = -3975,
#elif defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
		defined(CONFIG_MACH_KS01LGT)
		.RCOMP0 = 0x70,
		.RCOMP_charging = 0x79,
		.temp_cohot = -850,
		.temp_cocold = -4200,
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
#else
static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
#if defined(CONFIG_MACH_PICASSO)
		.Capacity = 0x3F76, /* N1/N2: 8123mAh */
#elif defined(CONFIG_MACH_MONDRIAN)
		.Capacity = 0x2456, /* Mondrian : 4651mAh */
#else
		.Capacity = 0x4A38, /* V1/V2: 9500mAh */
#endif
		.low_battery_comp_voltage = 3500,
		.low_battery_table = {
			/* range, slope, offset */
			{-5000,	0,	0},	/* dummy for top limit */
			{-1250, 0,	3320},
			{-750, 97,	3451},
			{-100, 96,	3461},
			{0, 0,	3456},
		},
		.temp_adjust_table = {
			/* range, slope, offset */
			{47000, 122,	8950},
			{60000, 200,	51000},
			{100000, 0,	0},	/* dummy for top limit */
		},
		.type_str = "SDI",
	}
};
#endif


#if defined(CONFIG_SEC_K_PROJECT)
#define CAPACITY_MAX			1000
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			0
#elif defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
	defined(CONFIG_MACH_HLTELGT) || defined(CONFIG_MACH_HLTEDCM)
#define CAPACITY_MAX			980
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			-7
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
#define CAPACITY_MAX			990
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			0
#elif defined(CONFIG_SEC_H_PROJECT)	/* from H USA/EUR */
#define CAPACITY_MAX			990
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			-7
#else /* CONFIG_SEC_KS01_PROJECT */
#define CAPACITY_MAX			1000
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			0
#endif


//static struct qpnp_vadc_chip *adc_client;
static enum qpnp_vadc_channels temp_channel;
static struct sec_fuelgauge_info *sec_fuelgauge =  NULL;

#if defined(CONFIG_MACH_KLTE_EUR) || defined(CONFIG_MACH_K3GDUOS_CTC) || defined(CONFIG_MACH_KLTE_CMCC)|| defined(CONFIG_MACH_KLTE_CTC) || defined(CONFIG_MACH_KACTIVELTE_EUR) \
	|| defined(CONFIG_MACH_KACTIVELTE_ATT)
static sec_bat_adc_table_data_t temp_table[] = {
	{26884,	700},
	{27346,	650},
	{27750,	600},
	{28213,	550},
	{28760,	500},
	{29384,	450},
	{30180,	400},
	{31095,	350},
	{32085,	300},
	{33132,	250},
	{34242,	200},
	{35340,	150},
	{36430,	100},
	{37471,	50},
	{38406,	0},
	{39388,	-50},
	{40184,	-100},
	{40852,	-150},
	{41420,	-200},
};
#elif defined (CONFIG_MACH_KLTE_DCM)
static sec_bat_adc_table_data_t temp_table[] = {
	{26884,	700},
	{27346,	650},
	{27750,	600},
	{28213,	550},
	{28760,	500},
	{29384,	450},
	{30180,	400},
	{31095,	350},
	{32085,	300},
	{33132,	250},
	{34242,	200},
	{35340,	150},
	{36430,	100},
	{37471,	50},
	{38406,	0},
	{39388,	-50},
	{40184,	-100},
	{40852,	-150},
	{41420,	-200},
};
#elif defined (CONFIG_MACH_KLTE_SBM)
static sec_bat_adc_table_data_t temp_table[] = {
	{26884,	700},
	{27346,	650},
	{27750,	600},
	{28213,	550},
	{28760,	500},
	{29384,	450},
	{30180,	400},
	{31095,	350},
	{32085,	300},
	{33132,	250},
	{34242,	200},
	{35340,	150},
	{36430,	100},
	{37471,	50},
	{38406,	0},
	{39388,	-50},
	{40184,	-100},
	{40852,	-150},
	{41420,	-200},
};
#elif defined (CONFIG_MACH_KLTE_KDI)
static sec_bat_adc_table_data_t temp_table[] = {
	{26884,	700},
	{27346,	650},
	{27750,	600},
	{28213,	550},
	{28760,	500},
	{29384,	450},
	{30180,	400},
	{31095,	350},
	{32085,	300},
	{33132,	250},
	{34242,	200},
	{35340,	150},
	{36430,	100},
	{37471,	50},
	{38406,	0},
	{39388,	-50},
	{40184,	-100},
	{40852,	-150},
	{41420,	-200},
};
#elif defined(CONFIG_MACH_H3GDUOS_CTC)
static sec_bat_adc_table_data_t temp_table[] = {
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
static sec_bat_adc_table_data_t temp_table[] = {
	{25908, 900},
	{26111, 850},
	{26357, 800},
	{26651, 750},
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
static sec_bat_adc_table_data_t temp_table[] = {
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
#elif defined(CONFIG_SEC_H_PROJECT)
static sec_bat_adc_table_data_t temp_table[] = {
	{25950, 900},
	{26173, 850},
	{26424, 800},
	{26727, 750},
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
#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
static sec_bat_adc_table_data_t temp_table[] = {
	{26960,	700},
	{27435,	650},
	{27885,	600},
	{28463,	550},
	{29087,	500},
	{29988,	450},
	{30883,	400},
	{31831,	350},
	{32868,	300},
	{33667,	250},
	{34657,	200},
	{35871,	150},
	{36980,	100},
	{38018,	50},
	{38965,	0},
	{39722,	-50},
	{40720,	-100},
	{41309,	-150},
	{41772,	-200},
};
#elif defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || \
		defined(CONFIG_MACH_KS01LGT)
static sec_bat_adc_table_data_t temp_table[] = {
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
#else
static sec_bat_adc_table_data_t temp_table[] = {
	{25950, 900},
	{26173, 850},
	{26424, 800},
	{26727, 750},
	{26884,	700},
	{27346,	650},
	{27750,	600},
	{28213,	550},
	{28760,	500},
	{29384,	450},
	{30180,	400},
	{31095,	350},
	{32085,	300},
	{33132,	250},
	{34242,	200},
	{35340,	150},
	{36430,	100},
	{37471,	50},
	{38406,	0},
	{39388,	-50},
	{40184,	-100},
	{40852,	-150},
	{41420,	-200},
};
#endif


#if defined(CONFIG_SEC_K_PROJECT)
#define TEMP_HIGH_THRESHOLD_EVENT	700
#define TEMP_HIGH_RECOVERY_EVENT		420
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	700
#define TEMP_HIGH_RECOVERY_NORMAL	420
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		700
#define TEMP_HIGH_RECOVERY_LPM		420
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0

#elif defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
		defined(CONFIG_MACH_HLTELGT)
#define TEMP_HIGH_THRESHOLD_EVENT	680
#define TEMP_HIGH_RECOVERY_EVENT	440
#define TEMP_LOW_THRESHOLD_EVENT	-45
#define TEMP_LOW_RECOVERY_EVENT	0
#define TEMP_HIGH_THRESHOLD_NORMAL	680
#define TEMP_HIGH_RECOVERY_NORMAL	440
#define TEMP_LOW_THRESHOLD_NORMAL	-45
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		680
#define TEMP_HIGH_RECOVERY_LPM		440
#define TEMP_LOW_THRESHOLD_LPM		-45
#define TEMP_LOW_RECOVERY_LPM		0

#elif defined(CONFIG_MACH_HLTEATT) || defined(CONFIG_MACH_HLTETMO)
#define TEMP_HIGH_THRESHOLD_EVENT	 610
#define TEMP_HIGH_RECOVERY_EVENT	 400
#define TEMP_LOW_THRESHOLD_EVENT	 -45
#define TEMP_LOW_RECOVERY_EVENT	 0
#define TEMP_HIGH_THRESHOLD_NORMAL	 490
#define TEMP_HIGH_RECOVERY_NORMAL	 440
#define TEMP_LOW_THRESHOLD_NORMAL	 -45
#define TEMP_LOW_RECOVERY_NORMAL	 0
#define TEMP_HIGH_THRESHOLD_LPM	 490
#define TEMP_HIGH_RECOVERY_LPM	 440
#define TEMP_LOW_THRESHOLD_LPM	 -45
#define TEMP_LOW_RECOVERY_LPM	 0

#elif defined(CONFIG_MACH_HLTESPR)
#define TEMP_HIGH_THRESHOLD_EVENT	 640
#define TEMP_HIGH_RECOVERY_EVENT	 438
#define TEMP_LOW_THRESHOLD_EVENT	 -37
#define TEMP_LOW_RECOVERY_EVENT	 11
#define TEMP_HIGH_THRESHOLD_NORMAL	 529
#define TEMP_HIGH_RECOVERY_NORMAL	 438
#define TEMP_LOW_THRESHOLD_NORMAL	 -37
#define TEMP_LOW_RECOVERY_NORMAL	 11
#define TEMP_HIGH_THRESHOLD_LPM	 529
#define TEMP_HIGH_RECOVERY_LPM	 459
#define TEMP_LOW_THRESHOLD_LPM	 -23
#define TEMP_LOW_RECOVERY_LPM	 -10

#elif defined(CONFIG_MACH_HLTEVZW)
#define TEMP_HIGH_THRESHOLD_EVENT	 650
#define TEMP_HIGH_RECOVERY_EVENT	 450
#define TEMP_LOW_THRESHOLD_EVENT	 -45
#define TEMP_LOW_RECOVERY_EVENT	 -10
#define TEMP_HIGH_THRESHOLD_NORMAL	 550
#define TEMP_HIGH_RECOVERY_NORMAL	 450
#define TEMP_LOW_THRESHOLD_NORMAL	 -45
#define TEMP_LOW_RECOVERY_NORMAL	 -10
#define TEMP_HIGH_THRESHOLD_LPM	 520
#define TEMP_HIGH_RECOVERY_LPM	 470
#define TEMP_LOW_THRESHOLD_LPM	 -45
#define TEMP_LOW_RECOVERY_LPM	 -15

#elif defined(CONFIG_MACH_HLTEDCM)
#define TEMP_HIGH_THRESHOLD_EVENT	 625
#define TEMP_HIGH_RECOVERY_EVENT	 420
#define TEMP_LOW_THRESHOLD_EVENT	  -45
#define TEMP_LOW_RECOVERY_EVENT	 -5
#define TEMP_HIGH_THRESHOLD_NORMAL	 625
#define TEMP_HIGH_RECOVERY_NORMAL	 420
#define TEMP_LOW_THRESHOLD_NORMAL	 -45
#define TEMP_LOW_RECOVERY_NORMAL	 -5
#define TEMP_HIGH_THRESHOLD_LPM	 625
#define TEMP_HIGH_RECOVERY_LPM	 420
#define TEMP_LOW_THRESHOLD_LPM	 -45
#define TEMP_LOW_RECOVERY_LPM	 -5

#elif defined(CONFIG_MACH_HLTEKDI)
#define TEMP_HIGH_THRESHOLD_EVENT	 660
#define TEMP_HIGH_RECOVERY_EVENT	 420
#define TEMP_LOW_THRESHOLD_EVENT	  -50
#define TEMP_LOW_RECOVERY_EVENT	 0
#define TEMP_HIGH_THRESHOLD_NORMAL	 660
#define TEMP_HIGH_RECOVERY_NORMAL	 420
#define TEMP_LOW_THRESHOLD_NORMAL	 -50
#define TEMP_LOW_RECOVERY_NORMAL	 0
#define TEMP_HIGH_THRESHOLD_LPM	 660
#define TEMP_HIGH_RECOVERY_LPM	 420
#define TEMP_LOW_THRESHOLD_LPM	 -50
#define TEMP_LOW_RECOVERY_LPM	 0

#elif defined(CONFIG_MACH_HLTEUSC)
#define TEMP_HIGH_THRESHOLD_EVENT	 650
#define TEMP_HIGH_RECOVERY_EVENT	 440
#define TEMP_LOW_THRESHOLD_EVENT	 -45
#define TEMP_LOW_RECOVERY_EVENT	 0
#define TEMP_HIGH_THRESHOLD_NORMAL	 640
#define TEMP_HIGH_RECOVERY_NORMAL	 450
#define TEMP_LOW_THRESHOLD_NORMAL	 -45
#define TEMP_LOW_RECOVERY_NORMAL	 0
#define TEMP_HIGH_THRESHOLD_LPM	 650
#define TEMP_HIGH_RECOVERY_LPM	 440
#define TEMP_LOW_THRESHOLD_LPM	 -45
#define TEMP_LOW_RECOVERY_LPM	 0
/* H Project*/
#elif defined(CONFIG_SEC_H_PROJECT)
#define TEMP_HIGH_THRESHOLD_EVENT	650
#define TEMP_HIGH_RECOVERY_EVENT	440
#define TEMP_LOW_THRESHOLD_EVENT	-45
#define TEMP_LOW_RECOVERY_EVENT	0
#define TEMP_HIGH_THRESHOLD_NORMAL	650
#define TEMP_HIGH_RECOVERY_NORMAL	440
#define TEMP_LOW_THRESHOLD_NORMAL	-45
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		650
#define TEMP_HIGH_RECOVERY_LPM		440
#define TEMP_LOW_THRESHOLD_LPM		-45
#define TEMP_LOW_RECOVERY_LPM		0

#elif defined(CONFIG_MACH_FLTEEUR) || defined(CONFIG_MACH_FLTESKT)
#define TEMP_HIGH_THRESHOLD_EVENT	700
#define TEMP_HIGH_RECOVERY_EVENT	460
#define TEMP_LOW_THRESHOLD_EVENT	-50
#define TEMP_LOW_RECOVERY_EVENT	0
#define TEMP_HIGH_THRESHOLD_NORMAL	700
#define TEMP_HIGH_RECOVERY_NORMAL	460
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		700
#define TEMP_HIGH_RECOVERY_LPM		460
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0
#elif defined(CONFIG_SEC_KS01_PROJECT)
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01LGT)
#define TEMP_HIGH_THRESHOLD_EVENT	670
#define TEMP_HIGH_RECOVERY_EVENT	420
#define TEMP_LOW_THRESHOLD_EVENT	-45
#define TEMP_LOW_RECOVERY_EVENT	0
#define TEMP_HIGH_THRESHOLD_NORMAL	670
#define TEMP_HIGH_RECOVERY_NORMAL	420
#define TEMP_LOW_THRESHOLD_NORMAL	-45
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		670
#define TEMP_HIGH_RECOVERY_LPM		420
#define TEMP_LOW_THRESHOLD_LPM		-45
#define TEMP_LOW_RECOVERY_LPM		0
#else /* CONFIG_MACH_KS01KTT */
#define TEMP_HIGH_THRESHOLD_EVENT	670
#define TEMP_HIGH_RECOVERY_EVENT	440
#define TEMP_LOW_THRESHOLD_EVENT	-45
#define TEMP_LOW_RECOVERY_EVENT	0
#define TEMP_HIGH_THRESHOLD_NORMAL	670
#define TEMP_HIGH_RECOVERY_NORMAL	440
#define TEMP_LOW_THRESHOLD_NORMAL	-45
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		670
#define TEMP_HIGH_RECOVERY_LPM		440
#define TEMP_LOW_THRESHOLD_LPM		-45
#define TEMP_LOW_RECOVERY_LPM		0
#endif
#elif defined(CONFIG_MACH_MONDRIAN) || defined(CONFIG_SEC_KACTIVE_PROJECT)
#define TEMP_HIGH_THRESHOLD_EVENT	650
#define TEMP_HIGH_RECOVERY_EVENT		440
#define TEMP_LOW_THRESHOLD_EVENT		-45
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	650
#define TEMP_HIGH_RECOVERY_NORMAL	440
#define TEMP_LOW_THRESHOLD_NORMAL	-45
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		650
#define TEMP_HIGH_RECOVERY_LPM		440
#define TEMP_LOW_THRESHOLD_LPM		-45
#define TEMP_LOW_RECOVERY_LPM		0
#elif defined(CONFIG_MACH_VIENNAVZW)
#define TEMP_HIGH_THRESHOLD_EVENT	590
#define TEMP_HIGH_RECOVERY_EVENT		470
#define TEMP_LOW_THRESHOLD_EVENT		-15
#define TEMP_LOW_RECOVERY_EVENT		20
#define TEMP_HIGH_THRESHOLD_NORMAL	520
#define TEMP_HIGH_RECOVERY_NORMAL	470
#define TEMP_LOW_THRESHOLD_NORMAL	-15
#define TEMP_LOW_RECOVERY_NORMAL	20
#define TEMP_HIGH_THRESHOLD_LPM		500
#define TEMP_HIGH_RECOVERY_LPM		460
#define TEMP_LOW_THRESHOLD_LPM		-40
#define TEMP_LOW_RECOVERY_LPM		5
#elif defined(CONFIG_MACH_VIENNAEUR) || defined(CONFIG_MACH_V2)
#define TEMP_HIGH_THRESHOLD_EVENT	590
#define TEMP_HIGH_RECOVERY_EVENT		410
#define TEMP_LOW_THRESHOLD_EVENT		-40
#define TEMP_LOW_RECOVERY_EVENT		10
#define TEMP_HIGH_THRESHOLD_NORMAL	590
#define TEMP_HIGH_RECOVERY_NORMAL	410
#define TEMP_LOW_THRESHOLD_NORMAL	-40
#define TEMP_LOW_RECOVERY_NORMAL	10
#define TEMP_HIGH_THRESHOLD_LPM		590
#define TEMP_HIGH_RECOVERY_LPM		410
#define TEMP_LOW_THRESHOLD_LPM		-40
#define TEMP_LOW_RECOVERY_LPM		10
#elif defined(CONFIG_MACH_PICASSO)
#define TEMP_HIGH_THRESHOLD_EVENT	560
#define TEMP_HIGH_RECOVERY_EVENT		460
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	560
#define TEMP_HIGH_RECOVERY_NORMAL	460
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		560
#define TEMP_HIGH_RECOVERY_LPM		460
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0
#else
#define TEMP_HIGH_THRESHOLD_EVENT	700
#define TEMP_HIGH_RECOVERY_EVENT		420
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	700
#define TEMP_HIGH_RECOVERY_NORMAL	420
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		700
#define TEMP_HIGH_RECOVERY_LPM		420
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0
#endif


void sec_bat_check_batt_id(struct sec_battery_info *battery)
{
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)
#if defined(CONFIG_FUELGAUGE_MAX17050)
	int rc, data =  -1;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(NULL, LR_MUX5_PU2_AMUX_THM2, &results);
	if (rc) {
		pr_err("%s: Unable to read batt id rc=%d\n",
				__func__, rc);
		return;
	}
	data = results.adc_code;

	pr_info("%s: batt_id_adc = (%d)\n", __func__, data);

	/* SDI: 28500, ATL: 31000 */
	if (data > 31000) {
		battery->pdata->vendor = "ATL ATL";
#if defined(CONFIG_MACH_PICASSO)
		samsung_battery_data[0].Capacity = 0x4074; /* Picasso */
#elif defined(CONFIG_MACH_MONDRIAN)
		samsung_battery_data[0].Capacity = 0x2614; /* Mondrian : 4874mAh */
#else
		samsung_battery_data[0].Capacity = 0x4958; /* Vienna */
#endif
		samsung_battery_data[0].type_str = "ATL";
	}

	pr_err("%s: batt_type(%s), batt_id(%d), cap(0x%x), type(%s)\n",
			__func__, battery->pdata->vendor, data,
			samsung_battery_data[0].Capacity, samsung_battery_data[0].type_str);
#endif
#endif
}

static void sec_bat_adc_ap_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
#if 0
	struct power_supply *psy_fuelgauge;
	struct sec_fuelgauge_info *fuelgauge;

	psy_fuelgauge = get_power_supply_by_name(battery->pdata->fuelgauge_name);
	if (!psy_fuelgauge) {
		pr_err("%s : can't get sec-fuelgauge\n", __func__);
	} else {
		fuelgauge = container_of(psy_fuelgauge, struct sec_fuelgauge_info, psy_fg);

		adc_client = qpnp_get_vadc(&fuelgauge->client->dev, "sec-fuelgauge");

		if (IS_ERR(adc_client)) {
			int rc;
			rc = PTR_ERR(adc_client);
			if (rc != -EPROBE_DEFER)
				pr_err("%s: Fail to get vadc %d\n", __func__, rc);
		}
	}
#endif

#if defined(CONFIG_ARCH_MSM8974PRO)
	temp_channel = LR_MUX5_PU1_AMUX_THM2;
#elif defined(CONFIG_SEC_H_PROJECT) || defined(CONFIG_SEC_KS01_PROJECT)
	temp_channel = LR_MUX5_PU2_AMUX_THM2;
#else
	temp_channel = LR_MUX4_PU2_AMUX_THM1;
#endif

#if defined(CONFIG_FUELGAUGE_MAX17050)
	/* battery id checking*/
	sec_bat_check_batt_id(battery);
#endif
}

static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	struct qpnp_vadc_result results;
	int rc = -1;
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP :
		rc = qpnp_vadc_read(NULL, temp_channel, &results);
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
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK :
		qpnp_vadc_read(NULL, P_MUX8_1_3, &results);
		data = ((int)results.physical) / 1000;
		break;
	default :
		break;
	}

	pr_debug("%s: data(%d)\n", __func__, data);

	return data;
}

static void sec_bat_adc_ap_exit(void)
{
}

static void sec_bat_adc_none_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
}

static int sec_bat_adc_none_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
}

static void sec_bat_adc_ic_init(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
}

static int sec_bat_adc_ic_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] ADC = %d\n", __func__, adc);
	return adc;
}

static void adc_init_type(struct platform_device *pdev,
			struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	pr_debug("[%s]adc = %d\n", __func__, adc);

	return adc;
}

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}

bool sec_bat_check_jig_status(void)
{
#if defined(CONFIG_SEC_H_PROJECT) || defined(CONFIG_SEC_F_PROJECT) || \
	defined(CONFIG_SEC_KS01_PROJECT) || defined(CONFIG_MACH_MONDRIAN)
	extern bool is_jig_attached;	// from sec-switch
	return is_jig_attached;
#elif defined(CONFIG_SEC_K_PROJECT)
	return false;
#else
	if (!sec_fuelgauge) {
		pr_err("%s: sec_fuelgauge is empty\n", __func__);
		return false;
	}

	if (sec_fuelgauge->pdata->jig_irq >= 0) {
		if (gpio_get_value_cansleep(sec_fuelgauge->pdata->jig_irq))
			return true;
		else
			return false;
	} else {
		pr_err("%s: jig_irq is invalid\n", __func__);
		return false;
	}
#endif
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
bool sec_bat_check_callback(struct sec_battery_info *battery)
{
	struct power_supply *psy;
	union power_supply_propval value;

	pr_info("%s:  battery->pdata->bat_irq_gpio(%d)\n",
			__func__, battery->pdata->bat_irq_gpio);
	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
				__func__, "sec-charger");
		value.intval = 1;
	} else {
		if (battery->pdata->bat_irq_gpio > 0) {
			value.intval = !gpio_get_value(battery->pdata->bat_irq_gpio);
				pr_info("%s:  Battery status(%d)\n",
						__func__, value.intval);
			if (value.intval == 0) {
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
					ret = qpnp_pin_config(battery->pdata->bat_irq_gpio, &adc_param);
					if (ret < 0)
						pr_info("%s: qpnp config error: %d\n",
								__func__, ret);
					/* check the adc from vf pin */
					qpnp_vadc_read(NULL, P_MUX8_1_3, &result);
					data = ((int)result.physical) / 1000;
					pr_info("%s: (%dmV) is connected.\n",
							__func__, data);
					if(data < SHORT_BATTERY_STANDARD) {
						pr_info("%s: Short Battery(%dmV) is connected.\n",
								__func__, data);
						value.intval = 0;
					}
					ret = qpnp_pin_config(battery->pdata->bat_irq_gpio, &int_param);
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
bool sec_bat_check_cable_result_callback(
		int cable_type)
{
#if defined(CONFIG_SEC_H_PROJECT)
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
#elif defined(CONFIG_SEC_K_PROJECT)
	struct regulator *max77826_ldo6;
	current_cable_type = cable_type;

	if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
	{
		pr_info("%s set ldo off\n", __func__);
		max77826_ldo6 = regulator_get(NULL, "max77826_ldo6");
		if(max77826_ldo6 > 0) {
			regulator_disable(max77826_ldo6);
		}
	}
	else
	{
		pr_info("%s set ldo on\n", __func__);
		max77826_ldo6 = regulator_get(NULL, "max77826_ldo6");
		if(max77826_ldo6 > 0) {
			regulator_enable(max77826_ldo6);
		}
	}
#endif
	return true;
}

int sec_bat_check_cable_callback(struct sec_battery_info *battery)
{
	union power_supply_propval value;
	msleep(750);

	if (battery->pdata->ta_irq_gpio == 0) {
		pr_err("%s: ta_int_gpio is 0 or not assigned yet(cable_type(%d))\n",
			__func__, current_cable_type);
	} else {
		if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY &&
			!gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS IN\n", __func__);

			value.intval = POWER_SUPPLY_TYPE_UARTOFF;
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;

			return POWER_SUPPLY_TYPE_UARTOFF;
		}

		if ((battery->cable_type == POWER_SUPPLY_TYPE_UARTOFF ||
			battery->cable_type == POWER_SUPPLY_TYPE_CARDOCK) &&
			gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS OUT\n", __func__);

			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

			return POWER_SUPPLY_TYPE_BATTERY;
		}
	}

	return current_cable_type;
}

void board_battery_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	if ((!battery->pdata->temp_adc_table) &&
		(battery->pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC)) {
		pr_info("%s : assign temp adc table\n", __func__);

		battery->pdata->temp_adc_table = temp_table;
		battery->pdata->temp_amb_adc_table = temp_table;

		battery->pdata->temp_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
		battery->pdata->temp_amb_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
	}

	battery->pdata->event_check = true;
	battery->pdata->temp_high_threshold_event = TEMP_HIGH_THRESHOLD_EVENT;
	battery->pdata->temp_high_recovery_event = TEMP_HIGH_RECOVERY_EVENT;
	battery->pdata->temp_low_threshold_event = TEMP_LOW_THRESHOLD_EVENT;
	battery->pdata->temp_low_recovery_event = TEMP_LOW_RECOVERY_EVENT;
	battery->pdata->temp_high_threshold_normal = TEMP_HIGH_THRESHOLD_NORMAL;
	battery->pdata->temp_high_recovery_normal = TEMP_HIGH_RECOVERY_NORMAL;
	battery->pdata->temp_low_threshold_normal = TEMP_LOW_THRESHOLD_NORMAL;
	battery->pdata->temp_low_recovery_normal = TEMP_LOW_RECOVERY_NORMAL;
	battery->pdata->temp_high_threshold_lpm = TEMP_HIGH_THRESHOLD_LPM;
	battery->pdata->temp_high_recovery_lpm = TEMP_HIGH_RECOVERY_LPM;
	battery->pdata->temp_low_threshold_lpm = TEMP_LOW_THRESHOLD_LPM;
	battery->pdata->temp_low_recovery_lpm = TEMP_LOW_RECOVERY_LPM;

	adc_init_type(pdev, battery);
}

#if defined(CONFIG_FUELGAUGE_MAX77823)
void board_fuelgauge_init(struct max77823_fuelgauge_data *fuelgauge)
{
	sec_fuelgauge = 0;
	if (!fuelgauge->pdata->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
			fuelgauge->pdata->battery_data = (void *)samsung_battery_data;
	}
}
#else
void board_fuelgauge_init(struct sec_fuelgauge_info *fuelgauge)
{
	sec_fuelgauge = fuelgauge;

	if (!fuelgauge->pdata->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
			fuelgauge->pdata->battery_data = (void *)samsung_battery_data;
	}

	fuelgauge->pdata->capacity_max = CAPACITY_MAX;
	fuelgauge->pdata->capacity_max_margin = CAPACITY_MAX_MARGIN;
	fuelgauge->pdata->capacity_min = CAPACITY_MIN;

#if defined(CONFIG_FUELGAUGE_MAX17048)
	pr_info("%s: RCOMP0: 0x%x, RCOMP_charging: 0x%x, "
		"temp_cohot: %d, temp_cocold: %d, "
		"is_using_model_data: %d, type_str: %s, "
		"capacity_max: %d, capacity_max_margin: %d, "
		"capacity_min: %d, \n", __func__ ,
		get_battery_data(fuelgauge).RCOMP0,
		get_battery_data(fuelgauge).RCOMP_charging,
		get_battery_data(fuelgauge).temp_cohot,
		get_battery_data(fuelgauge).temp_cocold,
		get_battery_data(fuelgauge).is_using_model_data,
		get_battery_data(fuelgauge).type_str,
		fuelgauge->pdata->capacity_max,
		fuelgauge->pdata->capacity_max_margin,
		fuelgauge->pdata->capacity_min
		);
#endif
}
#endif

void cable_initial_check(struct sec_battery_info *battery)
{
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, current_cable_type);
	if (POWER_SUPPLY_TYPE_BATTERY != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval = 1;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	}
}
