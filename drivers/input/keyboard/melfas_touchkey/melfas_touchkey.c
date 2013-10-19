/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <asm/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#include <linux/gpio.h>

//#include <mach/gpio.h>
//#include <mach/irqs.h>
//#include <mach/msm8930-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#ifdef CONFIG_CPU_FREQ
#include <mach/cpufreq.h>
#endif

#include "melfas_touchkey.h"
//#include <linux/i2c/melfas_touchkey.h>
#include <linux/of_gpio.h>

#include <linux/regulator/lp8720.h>

/*
Melfas touchkey register
*/
#define KEYCODE_REG 0x00
#define FIRMWARE_VERSION 0x02
#define TOUCHKEY_MODULE_VERSION 0x03
#define TOUCHKEY_ADDRESS	0x20

#define UPDOWN_EVENT_BIT 0x08
#define KEYCODE_BIT 0x07
#define ESD_STATE_BIT 0x10
#define LED_BRIGHT_BIT 0x14
#define LED_BRIGHT 0x0C /*0~14*/
#define I2C_M_WR 0 /* for i2c */

#define DEVICE_NAME "sec_touchkey"	//"melfas_touchkey"
#define INT_PEND_BASE	0xE0200A54

#define MCS5080_CHIP		0x03
#define MCS5080_last_ver	0x59 // 0x09  /*M550 v07*/

#ifdef CONFIG_MACH_MONTBLANC
#define CONFIG_TWO_TOUCH
#endif

// if you want to see log, set this definition to NULL or KERN_WARNING
#define TCHKEY_KERN_DEBUG      KERN_DEBUG
#define _3_TOUCH_SDA_28V 	95
#define _3_TOUCH_SCL_28V	96
#define GPIO_TKEY_INT 564 //pm8941 gpio 29
#define _3_GPIO_TOUCH_INT	GPIO_TKEY_INT //temp
#define IRQ_TOUCH_INT gpio_to_irq(GPIO_TKEY_INT)

#define TOUCHKEY_KEYCODE_MENU 	KEY_MENU
#define TOUCHKEY_KEYCODE_HOME 	KEY_HOMEPAGE
#define TOUCHKEY_KEYCODE_BACK 	KEY_BACK
#define TOUCHKEY_KEYCODE_SEARCH 	KEY_END
#ifdef CONFIG_TWO_TOUCH
#define TOUCHKEY_KEYCODE_WAKE 	KEY_TKEY_WAKEUP
static u8 two_touch_wakeupmode = 0;
static u8 two_touch_seted = 0;
static u8 two_touch_speed = 30;
extern int Is_folder_state(void);	//  0 : open, 1: close 
#endif
#define FLIP_CLOSE 0
#define FLIP_OPEN 1

#define USE_OPEN_CLOSE

#ifdef CONFIG_TWO_TOUCH
static int touchkey_keycode[5] = {0,KEY_MENU , KEY_HOMEPAGE, KEY_BACK, KEY_TKEY_WAKEUP};
#else
static int touchkey_keycode[5] = {0,KEY_MENU , KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH};
#endif
static u8 activation_onoff = 1;	// 0:deactivate   1:activate
static u8 is_suspending = 0;
static u8 user_press_on = 0;
static u8 touchkey_dead = 0;
static u8 menu_sensitivity = 0;
static u8 back_sensitivity = 0;
static u8 home_sensitivity = 0;

static int touchkey_enable = 0;
static int led_onoff = 0;
static int ssuepend = -1;
static u8 version_info[3];
static void	__iomem	*gpio_pend_mask_mem;
static int Flip_status=-1;

struct i2c_touchkey_driver {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct work;
	struct early_suspend	early_suspend;
};
struct i2c_touchkey_driver *touchkey_driver = NULL;
struct workqueue_struct *touchkey_wq;
extern struct class *sec_class;
struct device *sec_touchkey;
struct mutex melfas_tsk_lock;

static const struct i2c_device_id melfas_touchkey_id[] = {
	{DEVICE_NAME, 0},
	{}
};

extern int get_lcd_attached(void);

MODULE_DEVICE_TABLE(i2c, melfas_touchkey_id);

extern void get_touchkey_data(u8 *data, u8 length);
static void init_hw(void);
void melfas_i2c_power_onoff(struct melfas_touchkey_platform_data *pdata, int onoff);
int touchkey_pmic_control(int onoff);
static int i2c_touchkey_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void melfas_touchkey_switch_early_suspend(int FILP_STATE,int firmup_onoff);
static void melfas_touchkey_switch_early_resume(int FILP_STATE,int firmup_onoff);


#ifdef USE_OPEN_CLOSE
static int melfas_touchkey_input_open(struct input_dev *dev);
static void melfas_touchkey_input_close(struct input_dev *dev);
#endif

#ifdef CONFIG_OF
static int melfas_parse_dt(struct device *dev,
			struct melfas_touchkey_platform_data *pdata)
{
	struct device_node *np = dev->of_node;


	/* regulator info */
	//pdata->i2c_pull_up = of_property_read_bool(np, "melfas,i2c-pull-up");
	//pdata->vdd_led = of_get_named_gpio(np, "vdd_led-gpio", 0);

	/* reset, irq gpio info */
	pdata->gpio_scl = of_get_named_gpio_flags(np, "melfas,scl-gpio", 0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "melfas,sda-gpio", 0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "melfas,irq-gpio", 0, &pdata->irq_gpio_flags);
	return 0;
}
#else
static int melfas_parse_dt(struct device *dev,
			struct melfas_touchkey_platform_data *pdata)
{
	return -ENODEV;
}
#endif



#ifdef CONFIG_OF
static struct of_device_id melfas_match_table[] = {
	{ .compatible = "melfas,melfas-tkey",},    
	{ },
};
#else
#define melfas_match_table	NULL
#endif



struct i2c_driver touchkey_i2c_driver =
{
	.id_table = melfas_touchkey_id,
	.probe = i2c_touchkey_probe,
	.driver = {
	.name = DEVICE_NAME,
#ifdef CONFIG_OF
		.of_match_table = melfas_match_table,
#endif		   	
	},

};


static int i2c_touchkey_read(u8 reg, u8 *val, unsigned int len)
{
	int	err=0;
	int	retry = 3;
	struct i2c_msg	msg[1];

	if((touchkey_driver == NULL)||touchkey_dead)
	{
		return -ENODEV;
	}

	while(retry--)
	{
		msg->addr	= touchkey_driver->client->addr;
		msg->flags = I2C_M_RD;
		msg->len   = len;
		msg->buf   = val;
		err = i2c_transfer(touchkey_driver->client->adapter, msg, 1);

		if (err >= 0)
		{
			return 0;
		}
		printk(KERN_ERR "%s %d i2c transfer error\n", __func__, __LINE__);/* add by inter.park */
		mdelay(10);
	}

	return err;

}

static int i2c_touchkey_write(u8 *val, unsigned int len)
{
	int err;
	struct i2c_msg msg[1];

	if((touchkey_driver == NULL)||is_suspending||touchkey_dead)
	{
		return -ENODEV;
	}

	msg->addr = touchkey_driver->client->addr;
	msg->flags = I2C_M_WR;
	msg->len = len;
	msg->buf = val;

	err = i2c_transfer(touchkey_driver->client->adapter, msg, 1);

	if (err >= 0) return 0;

	printk(KERN_ERR "%s %d i2c transfer error\n", __func__, __LINE__);

	return err;
}

static unsigned int touch_state_val;
//extern unsigned int touch_state_val;
extern void TSP_forced_release(void);
void  touchkey_work_func(struct work_struct * p)
{
	u8 data[14];
	int keycode;

	if (Flip_status == FLIP_CLOSE || (!gpio_get_value(_3_GPIO_TOUCH_INT) && !touchkey_dead)) {
		disable_irq_nosync(touchkey_driver->client->irq);
			i2c_touchkey_read(KEYCODE_REG, data, 14);
			keycode = touchkey_keycode[data[0] & KEYCODE_BIT];
			printk(KERN_ERR "[TKEY] %sdata[0] = %d\n",__func__,data[0] & KEYCODE_BIT);
		if(activation_onoff){
			if(data[0] & UPDOWN_EVENT_BIT) // key released
			{
				user_press_on = 0;
				input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] &  KEYCODE_BIT], 0);
				input_sync(touchkey_driver->input_dev);
					printk(KERN_ERR "[TKEY] [R] keycode : %d\n",touchkey_keycode[data[0] &  KEYCODE_BIT]);
			}
			else // key pressed
			{
				if(touch_state_val == 1)
				{
					printk(KERN_ERR "touchkey pressed but don't send event because touch is pressed. \n");
				}
				else
				{
					#ifdef CONFIG_TWO_TOUCH
					if(keycode == TOUCHKEY_KEYCODE_WAKE){
						input_report_key(touchkey_driver->input_dev, keycode, 0);
						input_sync(touchkey_driver->input_dev);
						printk(KERN_ERR "[TKEY] [P] test keycode : %d\n",keycode);
						msleep(5);
						input_report_key(touchkey_driver->input_dev, keycode, 1);
						input_sync(touchkey_driver->input_dev);
						printk(KERN_ERR "[TKEY] [R] test keycode : %d\n",keycode);

					}else
					#endif
					{
					if(keycode == TOUCHKEY_KEYCODE_BACK)
					{
						user_press_on = 3;
						back_sensitivity = data[5];
					}
					else if(keycode == TOUCHKEY_KEYCODE_MENU)
					{
						user_press_on = 1;
						menu_sensitivity = data[3];
					}
					else if(keycode == TOUCHKEY_KEYCODE_HOME)
					{
						user_press_on = 2;
						home_sensitivity = data[4];
					}
					input_report_key(touchkey_driver->input_dev, keycode,1);
					input_sync(touchkey_driver->input_dev);
						printk(KERN_ERR "[TKEY] [P] keycode : %d\n",keycode);
				}
			}
		}
		}
		enable_irq(touchkey_driver->client->irq);
	}else
		printk(KERN_ERR "[TKEY] not enabled Flip=%d,INT=%d,tkey_dead=%d \n",\
			Flip_status,!gpio_get_value(_3_GPIO_TOUCH_INT),!touchkey_dead);
	return ;
}

static irqreturn_t touchkey_interrupt(int irq, void *dummy)
{
	queue_work(touchkey_wq, &touchkey_driver->work);
	return IRQ_HANDLED;
}

void samsung_switching_tkey(int flip)
{
	if (touchkey_driver == NULL)
		return;

	printk(KERN_ERR "[TKEY] switching_tkey, Flip_status(0 OPEN/1 CLOSE) : %d, flip : %d,%d,%d \n", Flip_status, flip,touchkey_enable,is_suspending);
	
	if (Flip_status != flip)
	{
		Flip_status=flip;
		if (flip == FLIP_CLOSE) {
			if(touchkey_enable != 1 && is_suspending != 1)
			melfas_touchkey_switch_early_resume(flip,0);
		}
		else {
			if(touchkey_enable != 0 && is_suspending != 1)
			melfas_touchkey_switch_early_suspend(flip,0);
			}
	}
}
EXPORT_SYMBOL(samsung_switching_tkey);

static void melfas_touchkey_switch_early_suspend(int FILP_STATE,int firmup_onoff){

	unsigned char data;
	data = 0x02;
	touchkey_enable = 0;
	printk(KERN_DEBUG "[TKEY] switch_early_suspend +++\n");
	mutex_lock(&melfas_tsk_lock);
	if (touchkey_driver == NULL)
		goto end;
	
	if(user_press_on == 1)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_MENU, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release MENU\n");
	}
	else if(user_press_on == 2)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_HOME, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release HOME\n");
	}
	else if(user_press_on == 3)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_BACK, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release BACK\n");
	}
	
	user_press_on = 0;

	i2c_touchkey_write(&data, 1);	//Key LED force off
	if(!firmup_onoff)
	disable_irq(touchkey_driver->client->irq);
	
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	
	touchkey_pmic_control(0);
	msleep(30);

	ssuepend = 1;
#ifdef USE_IRQ_FREE
	free_irq(touchkey_driver->client->irq, NULL);
#endif

end:
	mutex_unlock(&melfas_tsk_lock);

	printk(KERN_DEBUG "[TKEY] switch_early_suspend ---\n");
}

static void melfas_touchkey_switch_early_resume(int FILP_STATE,int firmup_onoff){
	unsigned char data1 = 0x01;
	u8 led_bright[2]= {LED_BRIGHT_BIT,LED_BRIGHT};
	printk(KERN_DEBUG "[TKEY] switch_early_resume +++\n");
	mutex_lock(&melfas_tsk_lock);
	
	if (((Flip_status == FLIP_OPEN) && (!firmup_onoff)) || touchkey_driver == NULL) {
		printk(KERN_DEBUG "[TKEY] FLIP_OPEN ---\n");
		goto end;
		}
	touchkey_pmic_control(1);	
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	msleep(100);

#ifdef USE_IRQ_FREE
	msleep(50);

	printk("%s, %d\n",__func__, __LINE__);
	err = request_threaded_irq(touchkey_driver->client->irq, NULL, touchkey_interrupt,
			IRQF_DISABLED | IRQF_TRIGGER_LOW | IRQF_ONESHOT, "touchkey_int", NULL);
	if (err) {
		printk(KERN_ERR "%s Can't allocate irq .. %d\n", __FUNCTION__, err);
	}
#endif
	if(!firmup_onoff)
	enable_irq(touchkey_driver->client->irq);
	touchkey_enable = 1;
	ssuepend =0;
	if (led_onoff){
		i2c_touchkey_write(led_bright, 2);
		i2c_touchkey_write(&data1, 1);
	}

end:
	mutex_unlock(&melfas_tsk_lock);
	printk(KERN_DEBUG "[TKEY] switch_early_resume---\n");
}

#ifdef USE_OPEN_CLOSE
static void melfas_touchkey_input_close(struct input_dev *dev)
{
	struct melfas_touchkey_platform_data *pdata = input_get_drvdata(dev);

	unsigned char data = 0x02;
	printk(KERN_ERR"[TKEY] %s : early_suspend +++\n",__func__);
	mutex_lock(&melfas_tsk_lock);
	if(user_press_on == 1)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_MENU, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release MENU\n");
	}
	else if(user_press_on == 2)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_HOME, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release HOME\n");
	}
	else if(user_press_on == 3)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_BACK, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release BACK\n");
	}
	user_press_on = 0;

	if ((touchkey_enable == 0 && is_suspending == 1) || ssuepend == 1) {
	printk(KERN_ERR"[TKEY] already suspend %d,%d,%d---\n",touchkey_enable,is_suspending,ssuepend);
		if( ssuepend == 1)
			melfas_i2c_power_onoff(pdata,0);
	is_suspending = 1;
	goto end;
	}

#ifdef CONFIG_TWO_TOUCH

	printk(KERN_ERR "[TKEY] wakeup condition %d, %d \n", two_touch_wakeupmode, Is_folder_state());
	//disable_irq(touchkey_driver->client->irq);
	if((two_touch_wakeupmode == 1)&&(Is_folder_state())){
		unsigned char wakeup_mode = 0x15;
		unsigned char speed_control[2] = {0x17,40};
		unsigned char read_data;
		// if sysfs 1
		// if flip close

		speed_control[1] = two_touch_speed;
		i2c_touchkey_write(&wakeup_mode, 1);

		msleep(5);
		i2c_touchkey_read(0, &read_data, 1);
		msleep(1);
		if(read_data != 1)
			printk(KERN_ERR "[TKEY] %s, wakeup mode fail  %x, \n", __func__, read_data);
		
		i2c_touchkey_write(speed_control, 2);
		printk(KERN_ERR "[TKEY] %s, Waekup mode set, on, speed %d \n", __func__, speed_control[1]);
		msleep(10);
		enable_irq_wake(touchkey_driver->client->irq);
		two_touch_seted = 1;

		i2c_touchkey_write(&data, 1);	/*Key LED force off*/
		touchkey_enable = 0;
		is_suspending = 1;
		ssuepend = 0;
		led_onoff =0;
	
	}else{

	touchkey_enable = 0;
	is_suspending = 1;
	ssuepend = 0;
	led_onoff =0;

	if(touchkey_dead)
	{
		printk(KERN_ERR "touchkey died after ESD");
		goto end;
	}
	i2c_touchkey_write(&data, 1);	/*Key LED force off*/
	disable_irq(touchkey_driver->client->irq);

		gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		melfas_i2c_power_onoff(pdata,0);
	touchkey_pmic_control(0);
	msleep(30);
	}
#else	
	touchkey_enable = 0;
	is_suspending = 1;
	ssuepend = 0;
	led_onoff =0;

	if(touchkey_dead)
	{
		printk(KERN_ERR "touchkey died after ESD");
		goto end;
	}
	i2c_touchkey_write(&data, 1);	/*Key LED force off*/
	disable_irq(touchkey_driver->client->irq);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	melfas_i2c_power_onoff(pdata,0);
	touchkey_pmic_control(0);
	msleep(30);
#endif
end:
	mutex_unlock(&melfas_tsk_lock);
	printk(KERN_ERR"[TKEY] %s : early_suspend ---\n",__func__);
}

static int melfas_touchkey_input_open(struct input_dev *dev)
{
	struct melfas_touchkey_platform_data *pdata = input_get_drvdata(dev);

	unsigned char data1 = 0x01;
	u8 led_bright[2]= {LED_BRIGHT_BIT,LED_BRIGHT};
	printk(KERN_ERR"[TKEY] %s : early_resume +++\n",__func__);
	mutex_lock(&melfas_tsk_lock);

	if((touchkey_enable == 1 && is_suspending == 0) || Flip_status == 1) {
	 printk(KERN_ERR"[TKEY] already resume or FLIP open %d,%d,%d---\n",touchkey_enable,is_suspending,Flip_status);
		if( Flip_status == 1){
		melfas_i2c_power_onoff(pdata,1);
#ifdef CONFIG_TWO_TOUCH
		if(two_touch_seted == 1){
			printk(KERN_ERR "[TKEY] %s, Waekup mode set, off ", __func__);
			disable_irq_wake(touchkey_driver->client->irq);
			two_touch_seted = 0;
			disable_irq(touchkey_driver->client->irq);
			melfas_i2c_power_onoff(pdata,0);
			touchkey_pmic_control(0);
			msleep(10);
		}
#endif		
	}
	 is_suspending = 0;
	 ssuepend = 1;
	 goto end;
	}

	touchkey_enable = 1;
	is_suspending = 0;
	ssuepend = 0;

	if(touchkey_dead)
	{
		printk(KERN_ERR "touchkey died after ESD");
		goto end;
	}

#ifdef CONFIG_TWO_TOUCH
	if(two_touch_seted == 1){
		#if 0  // send cmd
		unsigned char normal_mode = 0x16;

		i2c_touchkey_write(&normal_mode, 1);
		printk(KERN_ERR "[TKEY] %s, Waekup mode set, off ", __func__);
		disable_irq_wake(touchkey_driver->client->irq);
		two_touch_seted = 0;
		//enable_irq(touchkey_driver->client->irq);
		#else	// power on/off
		
		printk(KERN_ERR "[TKEY] %s, Waekup mode set, off \n", __func__);
		disable_irq_wake(touchkey_driver->client->irq);
		two_touch_seted = 0;

		disable_irq(touchkey_driver->client->irq);
		melfas_i2c_power_onoff(pdata,0);
		touchkey_pmic_control(0);
		msleep(10);

		melfas_i2c_power_onoff(pdata,1);
		touchkey_pmic_control(1);
		msleep(100);
		enable_irq(touchkey_driver->client->irq);
	
		#endif
		
	}else{
	melfas_i2c_power_onoff(pdata,1);
	touchkey_pmic_control(1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	msleep(100);

	enable_irq(touchkey_driver->client->irq);
	}
#else	
	melfas_i2c_power_onoff(pdata,1);
	touchkey_pmic_control(1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	msleep(100);

	enable_irq(touchkey_driver->client->irq);
#endif

	if (led_onoff){
		i2c_touchkey_write(led_bright, 2);
		i2c_touchkey_write(&data1, 1);
	}
end:
	mutex_unlock(&melfas_tsk_lock);
	printk(KERN_ERR"[TKEY] %s : early_resume ---\n",__func__);

	return 0;
}
#endif	// End of CONFIG_HAS_EARLYSUSPEND

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_touchkey_early_suspend(struct early_suspend *h)
{
	unsigned char data = 0x02;
    printk(KERN_ERR"[TKEY] early_suspend +++\n");
	mutex_lock(&melfas_tsk_lock);
	if(user_press_on == 1)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_MENU, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release MENU\n");
	}
	else if(user_press_on == 2)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_HOME, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release HOME\n");
	}
	else if(user_press_on == 3)	{
		input_report_key(touchkey_driver->input_dev, TOUCHKEY_KEYCODE_BACK, 0);
		input_sync(touchkey_driver->input_dev);
		printk("[TKEY] force release BACK\n");
	}
	user_press_on = 0;

	if ((touchkey_enable == 0 && is_suspending == 1) || ssuepend == 1) {
	printk(KERN_ERR"[TKEY] already suspend %d,%d,%d---\n",touchkey_enable,is_suspending,ssuepend);
	is_suspending = 1;
	goto end;
	}

	touchkey_enable = 0;
	is_suspending = 1;
	ssuepend = 0;
	led_onoff =0;
	

	if(touchkey_dead)
	{
		printk(KERN_ERR "touchkey died after ESD");
		goto end;
	}
	i2c_touchkey_write(&data, 1);	/*Key LED force off*/
	disable_irq(touchkey_driver->client->irq);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SDA_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(_3_TOUCH_SCL_28V, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	touchkey_pmic_control(0);
	msleep(30);

end:
	mutex_unlock(&melfas_tsk_lock);
    printk(KERN_ERR"[TKEY] early_suspend ---\n");
}

static void melfas_touchkey_early_resume(struct early_suspend *h)
{
	unsigned char data1 = 0x01;
	u8 led_bright[2]= {LED_BRIGHT_BIT,LED_BRIGHT};
    printk(KERN_ERR"[TKEY] early_resume +++\n");
	mutex_lock(&melfas_tsk_lock);

	if((touchkey_enable == 1 && is_suspending == 0) || Flip_status == 1) {
	 printk(KERN_ERR"[TKEY] already resume or FLIP open %d,%d,%d---\n",touchkey_enable,is_suspending,Flip_status);
	 is_suspending = 0;
	 ssuepend = 1;
	 goto end;
	}

	touchkey_enable = 1;
	is_suspending = 0;
	ssuepend = 0;
	
	if(touchkey_dead)
	{
		printk(KERN_ERR "touchkey died after ESD");
		goto end;
	}
	touchkey_pmic_control(1);
	gpio_tlmm_config(GPIO_CFG(pdata->gpio_scl, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(pdata->gpio_sda, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	msleep(100);
	
	enable_irq(touchkey_driver->client->irq);
	if (led_onoff){
		i2c_touchkey_write(led_bright, 2);
		i2c_touchkey_write(&data1, 1);
	}
end:
	mutex_unlock(&melfas_tsk_lock);
   printk(KERN_ERR"[TKEY] early_resume ---\n");
}
#endif	// End of CONFIG_HAS_EARLYSUSPEND


static void melfas_config_gpio_i2c(struct melfas_touchkey_platform_data *pdata, int onoff)
{
	if (onoff) {
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_scl, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_sda, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	} else {
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_scl, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_sda, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	}
}

static void melfas_request_gpio(struct melfas_touchkey_platform_data *pdata)
{
	int ret;

	ret = gpio_request(pdata->gpio_scl, "touchkey_scl");
	if (ret) {
		printk(KERN_ERR "%s: unable to request touchkey_scl [%d]\n",
				__func__, pdata->gpio_scl);
		//return;
	}

	//gpio_tlmm_config(GPIO_CFG(pdata->gpio_scl, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(pdata->gpio_scl, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	ret = gpio_request(pdata->gpio_sda, "touchkey_sda");
	if (ret) {
		printk(KERN_ERR "%s: unable to request touchkey_sda [%d]\n",
				__func__, pdata->gpio_sda);
		//return;
	}

	//gpio_tlmm_config(GPIO_CFG(pdata->gpio_sda, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(pdata->gpio_sda, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	ret = gpio_request(pdata->gpio_int, "touchkey_irq");
	if (ret) {
		printk(KERN_ERR "%s: unable to request touchkey_irq [%d]\n",
				__func__, pdata->gpio_int);
		//return;
	}

}

extern int mcsdl_download_binary_data(u8 chip_ver);

static int i2c_touchkey_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct input_dev *input_dev;
	struct melfas_touchkey_platform_data *pdata;
	int err = 0;
	u8 updated = 0;
	
printk(KERN_ERR "[TKEY] %s\n",__func__);
	printk("melfas touchkey probe called!\n");

	touchkey_driver = kzalloc(sizeof(struct i2c_touchkey_driver), GFP_KERNEL);
	if (touchkey_driver == NULL)
	{
		dev_err(dev, "failed to create our state\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct melfas_touchkey_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_info(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = melfas_parse_dt(&client->dev, pdata);
		if (err)
			return err;
	}else
		pdata = client->dev.platform_data;


///touchkey_init
	init_hw();

	pdata->client = client;
	melfas_i2c_power_onoff(pdata,1);

	msleep(100);

	get_touchkey_data(version_info, 3);

	printk(TCHKEY_KERN_DEBUG "%s F/W version: 0x%x, Module version:0x%x\n",__FUNCTION__, version_info[1], version_info[2]);

#if 1
//-------------------   Auto Firmware Update Routine Start   -------------------//

	if (version_info[1] == 0xff) {
		mdelay(350);
		mcsdl_download_binary_data(MCS5080_CHIP);
		updated = 1;
	}
	else
	{
		if ((version_info[1]==0x10) ||(version_info[1] < MCS5080_last_ver)||(version_info[1] == 0xFF))
		{
			printk(KERN_ERR"Touchkey IC F/W update start!!");
			mdelay(350);
			mcsdl_download_binary_data(MCS5080_CHIP);
			mdelay(100);
			updated = 1;
		} else
			printk(KERN_ERR"Touchkey IC F/W is last, can't update!");
		if (updated)
		{
			get_touchkey_data(version_info, 3);
			printk(TCHKEY_KERN_DEBUG "Updated F/W version: 0x%x, Module version:0x%x\n", version_info[1], version_info[2]);
		}
	}

//-------------------   Auto Firmware Update Routine End   -------------------//
#endif
///touchkey_init

	melfas_request_gpio(pdata);

	irq_set_irq_type(gpio_to_irq(pdata->gpio_int), IRQF_TRIGGER_FALLING);

	melfas_config_gpio_i2c(pdata, 0);

	get_touchkey_data(version_info, 3);

	printk(TCHKEY_KERN_DEBUG "%s F/W version: 0x%x, Module version:0x%x\n",__FUNCTION__, version_info[1], version_info[2]);



	touchkey_driver->client = client;

	touchkey_driver->client->irq = gpio_to_irq(pdata->gpio_int);

	strlcpy(touchkey_driver->client->name, DEVICE_NAME, I2C_NAME_SIZE);

	input_dev = input_allocate_device();

	if (!input_dev)
		return -ENOMEM;

	touchkey_driver->input_dev = input_dev;

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "sec_touchkey/input2";
	input_dev->id.bustype = BUS_HOST;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(touchkey_keycode[1], input_dev->keybit);
	set_bit(touchkey_keycode[2], input_dev->keybit);
	set_bit(touchkey_keycode[3], input_dev->keybit);
	set_bit(touchkey_keycode[4], input_dev->keybit);

	mutex_init(&melfas_tsk_lock);

	input_set_drvdata(input_dev, pdata);

	err = input_register_device(input_dev);
	if (err)
	{
		input_free_device(input_dev);
		return err;
	}

	i2c_set_clientdata(client, pdata);

	gpio_pend_mask_mem = ioremap(INT_PEND_BASE, 0x10);
	touchkey_wq = create_singlethread_workqueue("sec_touchkey_wq");
	if (!touchkey_wq)
		return -ENOMEM;

	INIT_WORK(&touchkey_driver->work, touchkey_work_func);

#ifdef USE_OPEN_CLOSE
	input_dev->open = melfas_touchkey_input_open;
	input_dev->close = melfas_touchkey_input_close;
#endif	

#ifdef CONFIG_HAS_EARLYSUSPEND
	touchkey_driver->early_suspend.suspend = melfas_touchkey_early_suspend;
	touchkey_driver->early_suspend.resume = melfas_touchkey_early_resume;
	register_early_suspend(&touchkey_driver->early_suspend);
#endif				/* CONFIG_HAS_EARLYSUSPEND */
	touchkey_enable = 1;
	if (request_irq(touchkey_driver->client->irq, touchkey_interrupt, IRQF_DISABLED, DEVICE_NAME, touchkey_driver))
	{
		printk(KERN_ERR "%s Can't allocate irq ..\n", __FUNCTION__);
		return -EBUSY;
	}
	return 0;
}



static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}


///
//struct mxt_data *data = i2c_get_clientdata(client);

void melfas_i2c_power_onoff(struct melfas_touchkey_platform_data *pdata, int onoff)
{
	int rc = 0;

	dev_info(&pdata->client->dev, "%s: power %s\n",
			__func__, onoff ? "on" : "off");

	if (!pdata->vcc_en) {
		//if (info->pdata->i2c_pull_up) {
			pdata->vcc_en = regulator_get(&pdata->client->dev,
				"vcc_en");
			if (IS_ERR(pdata->vcc_en)) {
				rc = PTR_ERR(pdata->vcc_en);
				dev_info(&pdata->client->dev,
					"Regulator(vcc_en) get failed rc=%d\n", rc);
				goto error_get_vtg_i2c;
			}
			if (regulator_count_voltages(pdata->vcc_en) > 0) {
				rc = regulator_set_voltage(pdata->vcc_en,
				1800000, 1800000);
				if (rc) {
					dev_info(&pdata->client->dev,
					"regulator(vcc_en) set_vtg failed rc=%d\n",
					rc);
					goto error_set_vtg_i2c;
				}
			}
		//}
	}
	if (onoff) {
		//if (info->pdata->i2c_pull_up) {
			rc = reg_set_optimum_mode_check(pdata->vcc_en,
						10000);
			if (rc < 0) {
				dev_info(&pdata->client->dev,
				"Regulator vcc_en set_opt failed rc=%d\n",
				rc);
				goto error_reg_opt_i2c;
			}

			rc = regulator_enable(pdata->vcc_en);
			if (rc) {
				dev_info(&pdata->client->dev,
				"Regulator vcc_en enable failed rc=%d\n",
				rc);
				goto error_reg_en_vcc_en;
			}
		//}
	} else {
		//if (pdata->pdata->i2c_pull_up) {
			reg_set_optimum_mode_check(pdata->vcc_en, 0);
			regulator_disable(pdata->vcc_en);
		//}
	}
	msleep(50);


	return;

error_reg_en_vcc_en:
	//if (pdata->pdata->i2c_pull_up) {
		reg_set_optimum_mode_check(pdata->vcc_en, 0);
	//}
error_reg_opt_i2c:
error_set_vtg_i2c:
	regulator_put(pdata->vcc_en);
error_get_vtg_i2c:
	return;
}


static struct regulator *sub_ldo2=NULL;

int touchkey_pmic_control(int onoff)
{
	int ret;
printk(KERN_ERR "[TKEY] %s : onoff = %d (%d)\n",__func__,onoff,__LINE__);
/*
	struct melfas_touchkey_platform_data *pdata;
	pdata = kzalloc(sizeof(struct melfas_touchkey_platform_data), GFP_KERNEL);

	if(onoff){
		printk(KERN_ERR "[TKEY] %s : %d\n",__func__,__LINE__);
		melfas_power_onoff(pdata,1);
	}
	else{
		printk(KERN_ERR "[TKEY] %s : %d\n",__func__,__LINE__);
		melfas_power_onoff(pdata,0);
	}

*/

if(!sub_ldo2){
	sub_ldo2 = regulator_get(NULL, "lp8720_ldo2");
	if (IS_ERR(sub_ldo2)) {
		pr_err("lp8720 : could not get sub_ldo2, rc = %ld\n", PTR_ERR(sub_ldo2));
		sub_ldo2 = NULL;
	}
	if(sub_ldo2 != NULL)
	{
		ret = regulator_set_voltage(sub_ldo2, 3300000, 3300000);
		if (ret) 
			pr_err("set_voltage sub_ldo2 failed, rc=%d\n", ret);
	}
}

	if(onoff){
		ret = regulator_enable(sub_ldo2); /*2.8V*/
		if (ret) 
			pr_err("enable sub_ldo2 failed, rc=%d\n", ret);
	}
	else{
		ret = regulator_disable(sub_ldo2);
		if (ret) 
			pr_err("set_voltage sub_ldo failed, rc=%d\n", ret);
	}

	gpio_tlmm_config(GPIO_CFG(561, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); 	
	gpio_set_value(561, 1);
	
	//static struct regulator *reg_l29;
	//static struct regulator *reg_l30;
//	struct melfas_touchkey_platform_data *pdata;	

/*
	struct regulator *vcc_en=NULL;

	printk(KERN_ERR "%s: power %s\n", __func__, onoff ? "on" : "off");
	
	if (!vcc_en) {
		vcc_en = regulator_get(NULL, "vcc_en");
		if (IS_ERR(vcc_en)) {
			pr_err("%s: could not get 8917_l29, rc = %ld\n",
				__func__, PTR_ERR(vcc_en));
			return 1;
		}
		ret = regulator_set_voltage(vcc_en, 1800000, 1800000);
		if (ret) {
			pr_err("%s: unable to set l29 voltage to 1.8V\n",
				__func__);
			return ret;
		}
	}

	if (onoff) {
		ret = regulator_enable(vcc_en);
		if (ret) {
			pr_err("%s: enable l29 failed, rc=%d\n",
				__func__, ret);
			return ret;
		}

	} else {
		if (regulator_is_enabled(vcc_en))
			ret = regulator_disable(vcc_en);
		else
			printk(KERN_ERR
				"%s: rugulator L29(1.8V) is disabled\n",
					__func__);
		if (ret) {
			pr_err("%s: disable l29 failed, rc=%d\n",
				__func__, ret);
			return ret;
		}
	}
*/	
	return 0;	

}

static void init_hw(void)
{
printk(KERN_ERR "[TKEY] %s\n",__func__);
	touchkey_pmic_control(1);

//	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
//			GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);

//	irq_set_irq_type(IRQ_TOUCH_INT, IRQF_TRIGGER_FALLING);
}


int touchkey_update_open (struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t touchkey_update_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

ssize_t touchkey_update_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	return count;
}

int touchkey_update_release (struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t touchkey_activation_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	printk(TCHKEY_KERN_DEBUG "called %s\n", __func__);
	sscanf(buf, "%hhu", &activation_onoff);
	printk(TCHKEY_KERN_DEBUG "deactivation test = %d\n", activation_onoff);
	return size;
}

static ssize_t touchkey_version_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk(TCHKEY_KERN_DEBUG "called %s\n", __func__);
	return sprintf(buf, "0x%02x\n", version_info[1]);
}

static ssize_t touchkey_recommend_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	u8 recommended_ver;
	printk(TCHKEY_KERN_DEBUG "called %s\n", __func__);
	recommended_ver = MCS5080_last_ver;

	return sprintf(buf, "0x%02x\n", recommended_ver);
}

static ssize_t touchkey_firmup_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	if(Flip_status){
		printk(KERN_ERR "[TKEY] flip opened\n");
		melfas_touchkey_switch_early_resume(Flip_status,1);
	}else{
		disable_irq(touchkey_driver->client->irq);
	}
	printk(TCHKEY_KERN_DEBUG "Touchkey firm-up start!\n");
	get_touchkey_data(version_info, 3);
	printk(TCHKEY_KERN_DEBUG "F/W version: 0x%x, Module version:0x%x\n", version_info[1], version_info[2]);
	if ((version_info[1] < MCS5080_last_ver) || (version_info[1] == 0xff)){
		mdelay(350);
		mcsdl_download_binary_data(MCS5080_CHIP);
		mdelay(100);
		get_touchkey_data(version_info, 3);
		printk(TCHKEY_KERN_DEBUG "Updated F/W version: 0x%x, Module version:0x%x\n", version_info[1], version_info[2]);
	}
	else
		printk(KERN_ERR "Touchkey IC module is new, can't update!");
	if(Flip_status){
		printk(KERN_ERR "[TKEY] flip opened\n");
		melfas_touchkey_switch_early_suspend(Flip_status,1);
	}else{
		enable_irq(touchkey_driver->client->irq);
	}
	return size;
}

static ssize_t touchkey_init_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk(TCHKEY_KERN_DEBUG"called %s \n", __func__);
	return sprintf(buf, "%d\n", touchkey_dead);
}

static ssize_t touchkey_menu_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk(TCHKEY_KERN_DEBUG "called %s \n", __func__);
	return sprintf(buf, "%d\n", menu_sensitivity);
}
static ssize_t touchkey_home_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk(TCHKEY_KERN_DEBUG "called %s \n", __func__);
	return sprintf(buf, "%d\n", home_sensitivity);
}

static ssize_t touchkey_back_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk(TCHKEY_KERN_DEBUG "called %s \n", __func__);
	return sprintf(buf, "%d\n", back_sensitivity);
}

static ssize_t touch_led_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u8 data;
	u8 led_bright[2]= {LED_BRIGHT_BIT,LED_BRIGHT};
	//data = 1 on, 0 off
	sscanf(buf, "%hhu", &data);
	led_onoff = (data == 0) ? 0 : 1;
	data = (data == 0) ? 2 : 1;

	if (!touchkey_enable)
		return -1;
	i2c_touchkey_write(led_bright, 2);
	i2c_touchkey_write(&data, 1);	// LED on(data=1) or off(data=2)
	return size;
}

static ssize_t touchkey_enable_disable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	// this function is called when platform shutdown thread begins
	printk(TCHKEY_KERN_DEBUG "called %s %c \n",__func__, *buf);
	if(*buf == '0')
	{
		is_suspending = 1;
	    disable_irq(touchkey_driver->client->irq);
	}
	else
	{
	    printk(KERN_ERR "%s: unknown command %c \n",__func__, *buf);
	}

        return size;
}

#ifdef CONFIG_TWO_TOUCH
static ssize_t touchkey_wakeup_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk(TCHKEY_KERN_DEBUG "called %s %c \n",__func__, *buf);

	if(*buf == '1'){
		two_touch_wakeupmode = 1;
	}else{
		two_touch_wakeupmode = 0;
	}
    return size;
}

static ssize_t touchkey_wakeup_mode_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int show;

	show = snprintf(buf, sizeof(buf), "%d\n", two_touch_wakeupmode);
	return show;
}

static ssize_t touchkey_wakeup_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk(TCHKEY_KERN_DEBUG "called %s %c \n",__func__, *buf);

	if(*buf == '3'){
		two_touch_speed = 30;
	}else if(*buf == '4'){
		two_touch_speed = 40;
	}else if(*buf == '5'){
		two_touch_speed = 50;
	}else{
		two_touch_speed = 30;
		printk(KERN_ERR "%s: unknown command %c \n",__func__, *buf);
	}
    return size;
}


static DEVICE_ATTR(two_touch_wakeup_mode, 0664, touchkey_wakeup_mode_show, touchkey_wakeup_mode);
static DEVICE_ATTR(two_touch_speed_set, 0664, NULL, touchkey_wakeup_speed);

#endif
static DEVICE_ATTR(touchkey_activation, 0664, NULL, touchkey_activation_store);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO, touchkey_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO, touchkey_recommend_show, NULL);
static DEVICE_ATTR(recommended_version, S_IRUGO, touchkey_recommend_show, NULL);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_firmup_store);
static DEVICE_ATTR(touchkey_init, S_IRUGO, touchkey_init_show, NULL);
static DEVICE_ATTR(touchkey_menu, S_IRUGO, touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_show, NULL);
static DEVICE_ATTR(touchkey_home, S_IRUGO, touchkey_home_show, NULL);
static DEVICE_ATTR(brightness, 0664, NULL, touch_led_control);
static DEVICE_ATTR(enable_disable, 0664, NULL, touchkey_enable_disable);

static int __init touchkey_init(void)
{

	int ret = 0;

	printk(KERN_ERR "[TKEY] %s\n",__func__);

	if (get_lcd_attached() == 0)
	{ 
		printk(KERN_ERR"[TKEY] touchkey_init skip !! get_lcd_attached(0)");
		return -ENODEV;
	}
	
	sec_touchkey=device_create(sec_class, NULL, 0, NULL, "sec_touchkey");

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_activation) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_activation.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_version_panel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_version_panel.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_version_phone) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_version_phone.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_recommended_version) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_recommended_version.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_update) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_update.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_init) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_init.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_menu) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_menu.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_back) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_back.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_touchkey_home) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_home.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_brightness) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_brightness.attr.name);

	if (device_create_file(sec_touchkey, &dev_attr_enable_disable) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_enable_disable.attr.name);

#ifdef CONFIG_TWO_TOUCH
	if (device_create_file(sec_touchkey, &dev_attr_two_touch_wakeup_mode) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_two_touch_wakeup_mode.attr.name);
	
	if (device_create_file(sec_touchkey, &dev_attr_two_touch_speed_set) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_two_touch_speed_set.attr.name);
#endif

	ret = i2c_add_driver(&touchkey_i2c_driver);

	if(ret||(touchkey_driver==NULL))
	{
		touchkey_dead = 1;
		printk("ret = %d, touch_driver= %p:", ret, touchkey_driver);
		printk(KERN_ERR
		       "melfas touch keypad registration failed, module not inserted.ret= %d\n",
		       ret);
	}

	printk(KERN_ERR "[TKEY] %s : %d ret=%d\n",__func__,__LINE__,ret);
	return ret;
}

static void __exit touchkey_exit(void)
{
printk(KERN_ERR "[TKEY] %s\n",__func__);
	i2c_del_driver(&touchkey_i2c_driver);
	if (touchkey_wq)
		destroy_workqueue(touchkey_wq);
	mutex_destroy(&melfas_tsk_lock);
	gpio_free(_3_GPIO_TOUCH_INT);
}

module_init(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("melfas touch keypad");
