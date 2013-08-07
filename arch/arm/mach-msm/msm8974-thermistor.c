/*
 * msm8974-thermistor.c - thermistor of H-F Project
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SangYoung Son <hello.son@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mach/msm8974-thermistor.h>
#include <mach/sec_thermistor.h>


#ifdef CONFIG_SEC_THERMISTOR
/*Below adc table is same as batt_temp_adc table*/
/* mismatch 8974 */
#if defined(CONFIG_MACH_KS01SKT) || defined(CONFIG_MACH_KS01KTT) || defined(CONFIG_MACH_KS01LGT) || defined(CONFIG_MACH_JACTIVESKT)
static struct sec_therm_adc_table temper_table_ap[] = {
	{27229,		700},
	{27271,		690},
	{27309,		680},
	{27435,		670},
	{27534,		660},
	{27620,		650},
	{27761,		640},
	{27834,		630},
	{27886,		620},
	{27970,		610},
	{28106,		600},
	{28200,		590},
	{28252,		580},
	{28339,		570},
	{28534,		560},
	{28640,		550},
	{28794,		540},
	{28884,		530},
	{28926,		520},
	{29091,		510},
	{29269,		500},
	{29445,		490},
	{29620,		480},
	{29615,		470},
	{29805,		460},
	{30015,		450},
	{30227,		440},
	{30392,		430},
	{30567,		420},
	{30731,		410},
	{30825,		400},
	{31060,		390},
	{31224,		380},
	{31406,		370},
	{31595,		360},
	{31764,		350},
	{31990,		340},
	{32111,		330},
	{32342,		320},
	{32562,		310},
	{32705,		300},
	{33697,		250},
	{34696,		200},
	{35682,		150},
	{36634,		100},
	{37721, 	 50},
	{38541,		  0},
	{39415,		-50},
	{40155,		-100},
	{40730,		-150},
	{41455,		-200},
	{41772,		-250},
	{42149,		-300},
};
#elif defined(CONFIG_MACH_HLTESKT) || defined(CONFIG_MACH_HLTEKTT) || \
	defined(CONFIG_MACH_HLTELGT)
static struct sec_therm_adc_table temper_table_ap[] = {
	{26729,	730},
	{26784,	720},
	{26836,	710},
	{26910,	700},
	{26976,	690},
	{27048,	680},
	{27124,	670},
	{27227,	660},
	{27332,	650},
	{27413,	640},
	{27522,	630},
	{27588,	620},
	{27670,	610},
	{27835,	600},
	{27934,	590},
	{28036,	580},
	{28125,	570},
	{28231,	560},
	{28348,	550},
	{28463,	540},
	{28589,	530},
	{28703,	520},
	{28830,	510},
	{28958,	500},
	{29089,	490},
	{29212,	480},
	{29354,	470},
	{29499,	460},
	{29648,	450},
	{29805,	440},
	{29950,	430},
	{30109,	420},
	{30259,	410},
	{30441,	400},
	{30600,	390},
	{30757,	380},
	{30926,	370},
	{31077,	360},
	{31289,	350},
	{31457,	340},
	{31663,	330},
	{31872,	320},
	{32056,	310},
	{32292,	300},
	{32472,	290},
	{32701,	280},
	{32915,	270},
	{33061,	260},
	{33285,	250},
	{33468,	240},
	{33675,	230},
	{33882,	220},
	{34092,	210},
	{34291,	200},
	{34536,	190},
	{34725,	180},
	{34953,	170},
	{35165,	160},
	{35348,	150},
	{35567,	140},
	{35744,	130},
	{35997,	120},
	{36202,	110},
	{36431,	100},
	{36649,	90},
	{36818,	80},
	{37066,	70},
	{37222,	60},
	{37459,	50},
	{37608,	40},
	{37841,	30},
	{37987,	20},
	{38205,	10},
	{38416,	0},
	{38604,	-10},
	{38788,	-20},
	{38959,	-30},
	{39121,	-40},
	{39280,	-50},
	{39431,	-60},
	{39591,	-70},
	{39748,	-80},
	{39895,	-90},
	{40043,	-100},
	{40172,	-110},
	{40307,	-120},
	{40449,	-130},
	{40565,	-140},
	{40712,	-150},
	{40788,	-160},
	{40932,	-170},
	{41010,	-180},
	{41135,	-190},
	{41225,	-200},
};
#else
static struct sec_therm_adc_table temper_table_ap[] = {
	{25939,	 900},
	{25967,	 890},
	{25994,	 880},
	{26035,	 870},
	{26073,	 860},
	{26120,	 850},
	{26164,	 840},
	{26207,	 830},
	{26253,	 820},
	{26301,	 810},
	{26355,	 800},
	{26432,	 790},
	{26497,	 780},
	{26560,	 770},
	{26609,	 760},
	{26677,	 750},
	{26752,	 740},
	{26830,	 730},
	{26941,	 720},
	{27047,	 710},
	{27167,	 700},
	{27271,	 690},
	{27355,	 680},
	{27438,	 670},
	{27522,	 660},
	{27605,	 650},
	{27721,	 640},
	{27836,	 630},
	{27952,	 620},
	{28067,	 610},
	{28182,	 600},
	{28296,	 590},
	{28409,	 580},
	{28522,	 570},
	{28635,	 560},
	{28748,	 550},
	{28852,	 540},
	{28955,	 530},
	{29058,	 520},
	{29161,	 510},
	{29286,	 500},
	{29410,	 490},
	{29555,	 480},
	{29700,	 470},
	{29845,	 460},
	{29990,	 450},
	{30188,	 440},
	{30386,	 430},
	{30584,	 420},
	{30782,	 410},
	{30981,	 400},
	{31164,	 390},
	{31347,	 380},
	{31530,	 370},
	{31713,	 360},
	{31896,	 350},
	{32081,	 340},
	{32266,	 330},
	{32450,	 320},
	{32635,	 310},
	{32820,	 300},
	{33047,	 290},
	{33274,	 280},
	{33502,	 270},
	{33729,	 260},
	{33956,	 250},
	{34172,	 240},
	{34388,	 230},
	{34605,	 220},
	{34821,	 210},
	{35037,	 200},
	{35246,	 190},
	{35455,	 180},
	{35664,	 170},
	{35873,	 160},
	{36083,	 150},
	{36302,	 140},
	{36522,	 130},
	{36741,	 120},
	{36961,	 110},
	{37180,	 100},
	{37398,	  90},
	{37615,	  80},
	{37833,	  70},
	{38050,	  60},
	{38267,	  50},
	{38443,	  40},
	{38620,	  30},
	{38796,	  20},
	{38972,	  10},
	{39148,	   0},
	{39302,	 -10},
	{39455,	 -20},
	{39609,	 -30},
	{39762,	 -40},
	{39916,	 -50},
	{40050,  -60},
	{40184,	 -70},
	{40318,	 -80},
	{40452,	 -90},
	{40586,	 -100},
	{40713,	 -110},
	{40841,	 -120},
	{40968,	 -130},
	{41095,	 -140},
	{41222,	 -150},
	{41292,	 -160},
	{41363,	 -170},
	{41433,	 -180},
	{41503,	 -190},
	{41573,	 -200},
};
#endif

#if defined(CONFIG_MACH_HLTEDCM) || defined(CONFIG_MACH_HLTEKDI)
static struct sec_therm_adc_table temper_table_flash[] = {
	{26135,  850},
        {26159,  840},
        {26199,  830},
        {26241,  820},
        {26293,  810},
        {26351,  800},
        {26391,  790},
        {26439,  780},
        {26483,  770},
        {26536,  760},
        {26599,  750},
        {26639,  740},
        {26729,  730},
        {26786,  720},
        {26846,  710},
        {26915,  700},
        {26978,  690},
        {27048,  680},
        {27134,  670},
        {27225,  660},
        {27301,  650},
        {27412,  640},
        {27467,  630},
        {27591,  620},
        {27679,  610},
        {27779,  600},
        {27879,  590},
        {27982,  580},
        {28087,  570},
        {28209,  560},
        {28345,  550},
        {28461,  540},
        {28574,  530},
        {28702,  520},
        {28830,  510},
        {28951,  500},
        {29098,  490},
        {29243,  480},
        {29384,  470},
        {29542,  460},
        {29680,  450},
        {29832,  440},
        {29996,  430},
        {30153,  420},
        {30312,  410},
        {30477,  400},
        {30630,  390},
        {30799,  380},
        {30961,  370},
        {31104,  360},
        {31273,  350},
        {31455,  340},
        {31643,  330},
        {31828,  320},
        {32024,  310},
        {32230,  300},
        {32425,  290},
        {32638,  280},
        {32836,  270},
        {33088,  260},
        {33302,  250},
        {33523,  240},
        {33732,  230},
        {33947,  220},
        {34178,  210},
        {34397,  200},
        {34627,  190},
        {34855,  180},
        {35087,  170},
        {35282,  160},
        {35500,  150},
        {35715,  140},
        {35924,  130},
        {36129,  120},
        {36351,  110},
        {36553,  100},
        {36750,  90},
        {36954,  80},
        {37121,  70},
        {37304,  60},
        {37501,  50},
        {37703,  40},
        {37895,  30},
        {38090,  20},
        {38228,  10},
        {38326,  0},
        {38411,  -10},
        {38508,  -20},
        {38683,  -30},
        {38894,  -40},
        {39082,  -50},
        {39242,  -60},
        {39393,  -70},
        {39540,  -80},
        {39706,  -90},
        {39868,  -100},
        {40012,  -110},
        {40167,  -120},
        {40311,  -130},
        {40437,  -140},
        {40565,  -150},
        {40693,  -160},
        {40821,  -170},
        {40941,  -180},
        {41052,  -190},
        {41154,  -200},
};
#endif

static int get_msm8974_siop_level(int temp)
{
	static int prev_temp = 400;
	static int prev_level;
	int level = -1;


#if defined(CONFIG_MACH_HLTEDCM) || defined(CONFIG_MACH_HLTEKDI) || defined(CONFIG_MACH_JS01LTEDCM)
	 /* This is only for JPN JF-DCM model, currently the SIOP is not using this function.
		 However, the JPN vendor(DoCoMo) wants to implement the Camera APP shutdown 
		 functionality to avoid over-heat damage. For this, only JPN model gives thermistor value
		 from the driver layer to platform layer. In this routine, the "notify_change_of_temperature()"
		 function gives thermistor value and also SIOP value together. This SIOP value is invalid information
		 and could give an effect to SIOP APP. That is why this enforcing return code is added.
	 */
	 return -1;
#endif

	if (temp > prev_temp) {
		if (temp >= 540)
			level = 4;
		else if (temp >= 530)
			level = 3;
		else if (temp >= 480)
			level = 2;
		else if (temp >= 440)
			level = 1;
		else
			level = 0;
	} else {
		if (temp < 410)
			level = 0;
		else if (temp < 440)
			level = 1;
		else if (temp < 480)
			level = 2;
		else if (temp < 530)
			level = 3;
		else
			level = 4;

		if (level > prev_level)
			level = prev_level;
	}

	prev_temp = temp;

	prev_level = level;

	return level;
}


static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
	.polling_interval = 30 * 1000, /* msecs */
	.get_siop_level = get_msm8974_siop_level,
#if defined(CONFIG_MACH_HLTEDCM) || defined(CONFIG_MACH_HLTEKDI) || defined(CONFIG_MACH_JS01LTEDCM)
	#ifndef CONFIG_MACH_JS01LTEDCM
	.adc_flash_arr_size = ARRAY_SIZE(temper_table_flash),
	.adc_table_flash = temper_table_flash,
	#endif
	.no_polling 	= 0,
#else
	.no_polling     = 1,
#endif	
};


struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};

#endif
