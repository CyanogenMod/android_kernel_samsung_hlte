
#include <linux/input/matrix_keypad.h>

#define R_BASE		536 //600
#define C_BASE		544//608

static unsigned int mpq_row_gpios[] = {R_BASE, R_BASE+3, R_BASE+5, R_BASE+6, R_BASE+7};			// kbc, input
static unsigned int mpq_col_gpios[] = {C_BASE, C_BASE+1, C_BASE+2, C_BASE+4, C_BASE+5};			// output
//unsigned int mpq_row_gpios[] = {C_BASE, C_BASE+1, C_BASE+2, C_BASE+4, C_BASE+5};		// kbc, output
//unsigned int mpq_col_gpios[] = {R_BASE, R_BASE+3, R_BASE+5, R_BASE+6, R_BASE+7};

static const unsigned int folder_keymap[] = {
/* KEY(row, col, keycode) */
	/* row = scan, col - sense */
#if 0
	KEY(0, 0, KEY_SEND),
	KEY(0, 1, KEY_BACKSPACE), 
	KEY(0, 2, KEY_CAMERA), 
	KEY(0, 3, KEY_CAMERA), 
	KEY(0, 4, KEY_ENTER),

	KEY(1, 0, KEY_1),
	KEY(1, 1, KEY_2),
	KEY(1, 2, KEY_3),
	KEY(1, 3, KEY_LEFT), 
	KEY(1, 4, KEY_DOWN),

	KEY(2, 0, KEY_4),
	KEY(2, 1, KEY_5), 
	KEY(2, 2, KEY_6), 
	KEY(2, 3, KEY_BACK), 
	KEY(2, 4, KEY_RIGHT),

	KEY(3, 0, KEY_7),	
	KEY(3, 1, KEY_8),
	KEY(3, 2, KEY_9),
	KEY(3, 3, KEY_UP),	

	KEY(4, 0, KEY_NUMERIC_STAR), 
	KEY(4, 1, KEY_0), 
	KEY(4, 2, KEY_NUMERIC_POUND), 
	KEY(4, 3, KEY_MENU), 



#else
	KEY(0, 0, KEY_SEND),
	KEY(0, 1, KEY_1), 
	KEY(0, 2, KEY_4), 
	KEY(0, 3, KEY_7), 
	KEY(0, 4, KEY_NUMERIC_STAR),

	KEY(1, 0, KEY_BACKSPACE),
	KEY(1, 1, KEY_2),
	KEY(1, 2, KEY_5),
	KEY(1, 3, KEY_8), 
	KEY(1, 4, KEY_0),

	KEY(2, 0, KEY_HOMEPAGE),
	KEY(2, 1, KEY_3), 
	KEY(2, 2, KEY_6), 
	KEY(2, 3, KEY_9), 
	KEY(2, 4, KEY_NUMERIC_POUND),
	
	KEY(3, 0, KEY_NET_SEL),	
	KEY(3, 1, KEY_LEFT), //KEY_RIGHT),
	KEY(3, 2, KEY_BACK),
	KEY(3, 3, KEY_UP),	
	KEY(3, 4, KEY_MENU), 
	
	KEY(4, 0, KEY_ENTER), 
	KEY(4, 1, KEY_DOWN), 
	KEY(4, 2, KEY_RIGHT), //KEY_LEFT), 
#endif	
};

static struct matrix_keymap_data folder_keymap_data = {
	.keymap		= folder_keymap,
	.keymap_size	= ARRAY_SIZE(folder_keymap),
};

static struct matrix_keypad_platform_data folder_keypad_data = {
	.keymap_data		= &folder_keymap_data,
	.row_gpios		= mpq_row_gpios,
	.col_gpios		= mpq_col_gpios,
	.num_row_gpios		= 5,
	.num_col_gpios		= 5,
	.col_scan_delay_us	= 5000, //32000,
	.debounce_ms		= 10, //20,
	.wakeup			= 1,
	.active_low		= 1,
	.no_autorepeat		= 1,
};

static struct platform_device folder_keypad_device = {
	.name           = "montblanc_3x4_keypad", //"matrix-keypad", // "ks02_3x4_keypad",  //"matrix-keypad"
	.id             = -1,
	.dev            = {
		.platform_data  = &folder_keypad_data,
	},
};



#if defined(CONFIG_INPUT_FLIP)
static void flip_init_hw(void)
{
	int ret;


	ret = gpio_request(44, "HALL_SW");
	if (ret)
		pr_err("failed to request gpio(HALL_SW)\n");
	gpio_tlmm_config(GPIO_CFG(44, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	/* s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP); */
}

struct sec_flip_pdata {
	int wakeup;
};

static struct sec_flip_pdata sec_flip_dev_data = {
	.wakeup			= 1,
};

static struct platform_device sec_flip_device = {
	.name = "sec_flip",
	.id = -1,
	.dev = {
		.platform_data = &sec_flip_dev_data,
	}
};
#endif

