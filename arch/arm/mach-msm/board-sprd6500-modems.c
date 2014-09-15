/* linux/arch/arm/mach-xxxx/board-superior-cmcc-modems.c
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <linux/platform_data/modem.h>

#include <mach/gpiomux.h>


#define GPIO_GSM_PHONE_ON	127
#define GPIO_PDA_ACTIVE		118
#define GPIO_PHONE_ACTIVE	107
#define GPIO_CP_DUMP_INT	119
#define GPIO_AP_CP_INT1		0
#define GPIO_AP_CP_INT2		0

#define GPIO_UART_SEL		135
#define GPIO_SIM_SEL		123
#define ESC_SIM_DETECT_IRQ	123

#if defined(CONFIG_GSM_MODEM_GG_DUOS)
/* gsm target platform data */
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x00,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[2] = {
		.name = "umts_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[3] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[4] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[5] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[6] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[7] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[8] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	/*
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	*/
};

static struct modem_data gsm_modem_data = {
	.name = "sprd6500",

	.modem_type = SPRD_SC6500,
	.link_types = LINKTYPE(LINKDEV_SPI),
	.modem_net = TDSCDMA_NETWORK,
	.use_handover = false,
	.ipc_version = SIPC_VER_42,

	.num_iodevs = ARRAY_SIZE(gsm_io_devices),
	.iodevs = gsm_io_devices,
};

#else
/* gsm target platform data */
static struct modem_io_t gsm_io_devices[] = {
	[0] = {
		.name = "gsm_boot0",
		.id = 0x1,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[1] = {
		.name = "gsm_ipc0",
		.id = 0x01,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[2] = {
		.name = "gsm_rfs0",
		.id = 0x28,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[3] = {
		.name = "gsm_multi_pdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[4] = {
		.name = "gsm_rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[5] = {
		.name = "gsm_rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[6] = {
		.name = "gsm_rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[7] = {
		.name = "gsm_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	/*
	[8] = {
		.name = "gsm_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[9] = {
		.name = "gsm_ramdump0",
		.id = 0x1,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	[10] = {
		.name = "gsm_loopback0",
		.id = 0x3F,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.links = LINKTYPE(LINKDEV_SPI),
	},
	*/
};

static struct modem_data gsm_modem_data = {
	.name = "sprd6500",

	.modem_type = SPRD_SC6500,
	.link_types = LINKTYPE(LINKDEV_SPI),
	.modem_net = TDSCDMA_NETWORK,
	.use_handover = false,
	.ipc_version = SIPC_VER_40,

	.num_iodevs = ARRAY_SIZE(gsm_io_devices),
	.iodevs = gsm_io_devices,
};

#endif


#if 0 //def CONFIG_OF

void sprd6500_modem_cfg_gpio(struct platform_device *pdev)
{
	static const char *modem_name;
	int gpio_cp_on, gpio_pda_active;
	int gpio_ap_cp_int2, gpio_uart_sel;
	int gpio_phone_active, gpio_cp_dump_int;

	pr_err("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	pr_err("%s:%d, debug info name=%s", __func__, __LINE__, pdev->name);

	if (!pdev->dev.of_node)	{
		pr_err("%s:%d, node not-found\n", __func__, __LINE__);
		return -ENODEV;
	}

	modem_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!modem_name)
		pr_err("%s:%d, sprd6500 modem name not specified\n",
						__func__, __LINE__);
	else
		pr_err("%s: sprd6500 modem Name = %s\n", __func__, modem_name);


	gpio_cp_on = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_cp_on", 0);

	pr_err("gpio_cp_on : %d \n", gpio_cp_on);

	if (!gpio_is_valid(gpio_cp_on)) {
		pr_err("gpio_cp_on not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_cp_on, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_set_value(gpio_cp_on, 0);
	}

	gpio_pda_active = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_pda_active", 0);

	pr_err("gpio_pda_active : %d \n", gpio_pda_active);

	if (!gpio_is_valid(gpio_pda_active)) {
		pr_err("gpio_pda_active not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_pda_active, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_set_value(gpio_pda_active, 0);
	}

	gpio_ap_cp_int2 = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_ap_cp_int2", 0);

	pr_err("gpio_ap_cp_int2 : %d \n", gpio_ap_cp_int2);

	if (!gpio_is_valid(gpio_ap_cp_int2)) {
		pr_err("gpio_ap_cp_int2 not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_ap_cp_int2, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_set_value(gpio_ap_cp_int2, 0);
	}

	gpio_uart_sel = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_uart_sel", 0);

	pr_err("gpio_uart_sel : %d \n", gpio_uart_sel);

	if (!gpio_is_valid(gpio_uart_sel)) {
		pr_err("gpio_uart_sel not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_uart_sel, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
		gpio_set_value(gpio_uart_sel, 0);
	}

	gpio_phone_active = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_phone_active", 0);

	pr_err("gpio_phone_active : %d \n", gpio_phone_active);

	if (!gpio_is_valid(gpio_phone_active)) {
		pr_err("gpio_phone_active not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_phone_active, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
	}

	gpio_cp_dump_int = of_get_named_gpio(pdev->dev.of_node,
						 "samsung,gpio_cp_dump_int", 0);

	pr_err("gpio_cp_dump_int : %d \n", gpio_cp_dump_int);

	if (!gpio_is_valid(gpio_cp_dump_int)) {
		pr_err("gpio_cp_dump_int not specified\n");
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_cp_dump_int, GPIOMUX_FUNC_GPIO,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
			GPIO_CFG_ENABLE);
	}

	gsm_modem_data.gpio_cp_on = gpio_cp_on;
	gsm_modem_data.gpio_pda_active = gpio_pda_active;
	gsm_modem_data.gpio_phone_active = gpio_phone_active;
	gsm_modem_data.gpio_cp_dump_int =gpio_cp_dump_int;
	gsm_modem_data.gpio_ap_cp_int2 = gpio_ap_cp_int2;
	gsm_modem_data.gpio_uart_sel = gpio_uart_sel;

#if defined(CONFIG_SIM_DETECT)
	gsm_modem_data.gpio_sim_sel= gpio_uart_sel;
#endif

	pr_info("sprd6500_modem_cfg_gpio done\n");
}

#else

void sprd6500_modem_cfg_gpio(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_GSM_PHONE_ON, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_GSM_PHONE_ON, 0);

	gpio_tlmm_config(GPIO_CFG(GPIO_PDA_ACTIVE, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_PDA_ACTIVE, 0);

	gpio_tlmm_config(GPIO_CFG(GPIO_AP_CP_INT2, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_AP_CP_INT2, 0);

	gpio_tlmm_config(GPIO_CFG(GPIO_UART_SEL, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_UART_SEL, 0);

	gpio_tlmm_config(GPIO_CFG(GPIO_PHONE_ACTIVE, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(GPIO_CP_DUMP_INT, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(GPIO_AP_CP_INT1, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);

	gsm_modem_data.gpio_cp_on = GPIO_GSM_PHONE_ON;
	gsm_modem_data.gpio_pda_active = GPIO_PDA_ACTIVE;
	gsm_modem_data.gpio_phone_active = GPIO_PHONE_ACTIVE;
	gsm_modem_data.gpio_cp_dump_int =GPIO_CP_DUMP_INT;
	gsm_modem_data.gpio_ap_cp_int1 = GPIO_AP_CP_INT1;
	gsm_modem_data.gpio_ap_cp_int2 = GPIO_AP_CP_INT2;
	gsm_modem_data.gpio_uart_sel = GPIO_UART_SEL;
	gsm_modem_data.gpio_sim_sel= GPIO_SIM_SEL;

	pr_info("sprd6500_modem_cfg_gpio done\n");
	pr_info("uart_sel : [%d]\n", gpio_get_value(GPIO_UART_SEL));
}

#endif

static struct resource gsm_modem_res[] = {
	[0] = {
		.name = "cp_active_irq",
		.start = 0,
		.end = 0,
		.flags = IORESOURCE_IRQ,
	},
#if defined(CONFIG_SIM_DETECT)
	[1] = {
		.name = "sim_irq",
		.start = ESC_SIM_DETECT_IRQ,
		.end = ESC_SIM_DETECT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
#endif
};

/* if use more than one modem device, then set id num */
static struct platform_device gsm_modem = {
	.name = "mif_sipc4",
	.id = -1,
	.num_resources = ARRAY_SIZE(gsm_modem_res),
	.resource = gsm_modem_res,
	.dev = {
		.platform_data = &gsm_modem_data,
	},
};

static int __init init_modem(void)
{
	sprd6500_modem_cfg_gpio();

	gsm_modem_res[0].start = gpio_to_irq(GPIO_PHONE_ACTIVE);
	gsm_modem_res[0].end = gpio_to_irq(GPIO_PHONE_ACTIVE);

	platform_device_register(&gsm_modem);

	mif_info("board init_sprd6500_modem done\n");
	return 0;
}
late_initcall(init_modem);
