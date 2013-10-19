/*
 * driver/misc/tsu6721.c - FSA9480 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c/tsu6721.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pmic8058.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
//#include <linux/sii9234.h>

/* Use Only TSU6721 */
#define TSU6721_REG_DEV_T3				0x15
#define INT_MASK2						0xA0

/* DEVICE ID */
#define TSU6721_DEV_ID					0x0A
#define TSU6721_DEV_ID_REV				0x12

/* FSA9480 I2C registers */
#define TSU6721_REG_DEVID				0x01
#define TSU6721_REG_CTRL				0x02
#define TSU6721_REG_INT1				0x03
#define TSU6721_REG_INT2				0x04
#define TSU6721_REG_INT1_MASK			0x05
#define TSU6721_REG_INT2_MASK			0x06
#define TSU6721_REG_ADC					0x07
#define TSU6721_REG_TIMING1				0x08
#define TSU6721_REG_TIMING2				0x09
#define TSU6721_REG_DEV_T1				0x0a
#define TSU6721_REG_DEV_T2				0x0b
#define TSU6721_REG_BTN1				0x0c
#define TSU6721_REG_BTN2				0x0d
#define TSU6721_REG_CK					0x0e
#define TSU6721_REG_CK_INT1				0x0f
#define TSU6721_REG_CK_INT2				0x10
#define TSU6721_REG_CK_INTMASK1			0x11
#define TSU6721_REG_CK_INTMASK2			0x12
#define TSU6721_REG_MANSW1				0x13
#define TSU6721_REG_MANSW2				0x14
#define TSU6721_REG_MANUAL_OVERRIDES1	0x1B
#define TSU6721_REG_RESERVED_1D			0x1D
#define TSU6721_REG_RESERVED_20			0x20


/* Control */
#define CON_SWITCH_OPEN		(1 << 4)
#define CON_RAW_DATA		(1 << 3)
#define CON_MANUAL_SW		(1 << 2)
#define CON_WAIT			(1 << 1)
#define CON_INT_MASK		(1 << 0)
#define CON_MASK		(CON_SWITCH_OPEN | CON_RAW_DATA | \
				CON_MANUAL_SW | CON_WAIT)

/* Device Type 1 */
#define DEV_USB_OTG			(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG			(1 << 5)
#define DEV_CAR_KIT			(1 << 4)
#define DEV_UART			(1 << 3)
#define DEV_USB				(1 << 2)
#define DEV_AUDIO_2			(1 << 1)
#define DEV_AUDIO_1			(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB_CHG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_CAR_KIT)

/* Device Type 2 */
#define DEV_AUDIO_DOCK		(1 << 8)
#define DEV_SMARTDOCK		(1 << 7)
#define DEV_AV			(1 << 6)
#define DEV_TTY				(1 << 5)
#define DEV_PPD				(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	(DEV_JIG_UART_OFF)
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF)
#define DEV_T2_JIG_ALL_MASK	(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

/* Device Type 3 */
#define DEV_MHL				(1 << 0)
#define DEV_VBUS_DEBOUNCE		(1 << 1)
#define DEV_NON_STANDARD		(1 << 2)
#define DEV_AV_VBUS			(1 << 4)
#define DEV_APPLE_CHARGER		(1 << 5)
#define DEV_U200_CHARGER		(1 << 6)

#define DEV_T3_CHARGER_MASK	(DEV_NON_STANDARD | DEV_APPLE_CHARGER | \
				DEV_U200_CHARGER)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 */
#define SW_VAUDIO		((4 << 5) | (4 << 2) | (1 << 1) | (1 << 0))
#define SW_UART			((3 << 5) | (3 << 2))
#define SW_AUDIO		((2 << 5) | (2 << 2) | (1 << 1) | (1 << 0))
#define SW_AUDIO_TSU	((2 << 5) | (2 << 2) | (1 << 0))
#define SW_DHOST		((1 << 5) | (1 << 2) | (1 << 1) | (1 << 0))
#define SW_DHOST_TSU	((1 << 5) | (1 << 2) | (1 << 0))
#define SW_AUTO			((0 << 5) | (0 << 2))
#define SW_USB_OPEN		(1 << 0)
#define SW_ALL_OPEN		(0)

/* Interrupt 1 */
#define INT_DETACH			(1 << 1)
#define INT_ATTACH			(1 << 0)
#define INT_OVP_EN			(1 << 5)
#define INT_OXP_DISABLE		(1 << 7)


#define	ADC_GND					0x00
#define	ADC_MHL					0x01
#define	ADC_DOCK_PREV_KEY		0x04
#define	ADC_DOCK_NEXT_KEY		0x07
#define	ADC_DOCK_VOL_DN			0x0a
#define	ADC_DOCK_VOL_UP			0x0b
#define	ADC_DOCK_PLAY_PAUSE_KEY 0x0d
#define	ADC_CEA936ATYPE1_CHG	0x17
#define	ADC_JIG_USB_OFF			0x18
#define	ADC_JIG_USB_ON			0x19
#define	ADC_DESKDOCK			0x1a
#define	ADC_CEA936ATYPE2_CHG	0x1b
#define	ADC_JIG_UART_OFF		0x1c
#define	ADC_JIG_UART_ON			0x1d
#define	ADC_CARDOCK				0x1d
#define	ADC_OPEN				0x1f

int uart_connecting;
EXPORT_SYMBOL(uart_connecting);
int detached_status;
EXPORT_SYMBOL(detached_status);
extern void of_sii8240_muic_mhl_notify(int event);

struct tsu6721_usbsw {
	struct i2c_client		*client;
	struct tsu6721_platform_data	*pdata;
	int				dev1;
	int				dev2;
	int				dev3;
	int				mansw;
	int				dock_attached;
	int				dev_id;

	struct input_dev	*input;
	int			previous_key;

	struct delayed_work	init_work;
	struct mutex		mutex;
	int				adc;
};

enum {
	DOCK_KEY_NONE			= 0,
	DOCK_KEY_VOL_UP_PRESSED,
	DOCK_KEY_VOL_UP_RELEASED,
	DOCK_KEY_VOL_DOWN_PRESSED,
	DOCK_KEY_VOL_DOWN_RELEASED,
	DOCK_KEY_PREV_PRESSED,
	DOCK_KEY_PREV_RELEASED,
	DOCK_KEY_PLAY_PAUSE_PRESSED,
	DOCK_KEY_PLAY_PAUSE_RELEASED,
	DOCK_KEY_NEXT_PRESSED,
	DOCK_KEY_NEXT_RELEASED,

};

static struct tsu6721_usbsw *local_usbsw;
#define CONFIG_VIDEO_MHL_V2 1
#if defined(CONFIG_VIDEO_MHL_V2)
#define MHL_DEVICE		2
static int isDeskdockconnected;
#endif

static int is_ti_muic(void)
{
	return ((local_usbsw->dev_id == TSU6721_DEV_ID ||
		local_usbsw->dev_id == TSU6721_DEV_ID_REV) ? 1 : 0);
}


void TSU6721_CheckAndHookAudioDock(int value)
{
	struct i2c_client *client = local_usbsw->client;
	struct tsu6721_platform_data *pdata = local_usbsw->pdata;
	int ret = 0;
	if (pdata->mhl_sel)
		pdata->mhl_sel(value);
	if (value) {
		pr_info("TSU6721_CheckAndHookAudioDock ON\n");
		pdata->callback(CABLE_TYPE_CARDOCK, TSU6721_ATTACHED_DESK_DOCK);

		if (is_ti_muic())
			ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_MANSW1, SW_AUDIO_TSU);
		else
			ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_MANSW1, SW_AUDIO);

		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n",
						__func__, ret);

		ret = i2c_smbus_read_byte_data(client,
						TSU6721_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n",
						__func__, ret);

		ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_CTRL,
				ret & ~CON_MANUAL_SW & ~CON_RAW_DATA);
		if (ret < 0)
			dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
	} else {
		dev_info(&client->dev, "TSU6721_CheckAndHookAudioDock Off\n");
		pdata->callback(CABLE_TYPE_CARDOCK, TSU6721_DETACHED_DOCK);

		ret = i2c_smbus_read_byte_data(client,
					TSU6721_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_CTRL,
				ret | CON_MANUAL_SW | CON_RAW_DATA);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);
	}
}

static void tsu6721_reg_init(struct tsu6721_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	unsigned int ctrl = CON_MASK;
	int ret;

	pr_info("tsu6721_reg_init is called\n");

	usbsw->dev_id = i2c_smbus_read_byte_data(client, TSU6721_REG_DEVID);
	local_usbsw->dev_id = usbsw->dev_id;
	if (usbsw->dev_id < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->dev_id);

	dev_info(&client->dev, " tsu6721_reg_init dev ID: 0x%x\n",
			usbsw->dev_id);

	/* mask interrupts (unmask attach/detach only) */
	ret = i2c_smbus_write_byte_data(client, TSU6721_REG_INT1_MASK, 0xfc);

	if (is_ti_muic()) {
		ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_INT2_MASK, INT_MASK2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	} else {
		ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_INT2_MASK, 0x18);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* mask all car kit interrupts */
	ret = i2c_smbus_write_word_data(client,
					TSU6721_REG_CK_INTMASK1, 0x07ff);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* ADC Detect Time: 500ms */
	ret = i2c_smbus_write_byte_data(client, TSU6721_REG_TIMING1, 0x0);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	usbsw->mansw = i2c_smbus_read_byte_data(client, TSU6721_REG_MANSW1);
	if (usbsw->mansw < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->mansw);

	if (usbsw->mansw)
		ctrl &= ~CON_MANUAL_SW;	/* Manual Switching Mode */
	else
		ctrl &= ~(CON_INT_MASK);

	ret = i2c_smbus_write_byte_data(client, TSU6721_REG_CTRL, ctrl);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	if (!is_ti_muic()) {
		/* apply Battery Charging Spec. 1.1 @TA/USB detect */
		ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_RESERVED_20, 0x04);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}
}

static ssize_t tsu6721_show_control(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, TSU6721_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return snprintf(buf, 13, "CONTROL: %02x\n", value);
}

static ssize_t tsu6721_show_device_type(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return snprintf(buf, 11, "DEVICE_TYPE: %02x\n", value);
}

static ssize_t tsu6721_show_manualsw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, TSU6721_REG_MANSW1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if (value == SW_VAUDIO)
		return snprintf(buf, 7, "VAUDIO\n");
	else if (value == SW_UART)
		return snprintf(buf, 5, "UART\n");
	else if (value == SW_AUDIO)
		return snprintf(buf, 6, "AUDIO\n");
	else if (value == SW_DHOST)
		return snprintf(buf, 6, "DHOST\n");
	else if (value == SW_AUTO)
		return snprintf(buf, 5, "AUTO\n");
	else
		return snprintf(buf, 4, "%x", value);
}

static ssize_t tsu6721_set_manualsw(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value, ret;
	unsigned int path = 0;

	value = i2c_smbus_read_byte_data(client, TSU6721_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return 0;

	if (!strncmp(buf, "VAUDIO", 6)) {
		path = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "UART", 4)) {
		path = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		path = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else {
		dev_err(dev, "Wrong command\n");
		return 0;
	}

	usbsw->mansw = path;

	ret = i2c_smbus_write_byte_data(client, TSU6721_REG_MANSW1, path);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(client, TSU6721_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return count;
}
static ssize_t tsu6721_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	unsigned char device_type1, device_type2;

	device_type1 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T1);
	if (device_type1 < 0) {
		dev_err(&client->dev, "%s: err %d ", __func__, device_type1);
		return (ssize_t)device_type1;
	}
	device_type2 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T2);
	if (device_type2 < 0) {
		dev_err(&client->dev, "%s: err %d ", __func__, device_type2);
		return (ssize_t)device_type2;
	}

	if (device_type1 & DEV_T1_USB_MASK || device_type2 & DEV_T2_USB_MASK)
		return snprintf(buf, 22, "USB_STATE_CONFIGURED\n");

	return snprintf(buf, 25, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t tsu6721_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int adc;

	adc = i2c_smbus_read_byte_data(client, TSU6721_REG_ADC);
	if (adc < 0) {
		dev_err(&client->dev,
			"%s: err at read adc %d\n", __func__, adc);
		return snprintf(buf, 9, "UNKNOWN\n");
	}

	return snprintf(buf, 4, "%x\n", adc);
}

static ssize_t tsu6721_reset(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct tsu6721_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int ret;

	if (!strncmp(buf, "1", 1)) {
		dev_info(&client->dev,
			"tsu6721 reset after delay 1000 msec.\n");
		mdelay(1000);
		ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANUAL_OVERRIDES1, 0x01);
		if (ret < 0)
				dev_err(&client->dev,
					"cannot soft reset, err %d\n", ret);

	dev_info(&client->dev, "tsu6721_reset_control done!\n");
	} else {
		dev_info(&client->dev,
			"tsu6721_reset_control, but not reset_value!\n");
	}

	tsu6721_reg_init(usbsw);

	return count;
}


static DEVICE_ATTR(control, S_IRUGO, tsu6721_show_control, NULL);
static DEVICE_ATTR(device_type, S_IRUGO, tsu6721_show_device_type, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWUSR,
		tsu6721_show_manualsw, tsu6721_set_manualsw);
static DEVICE_ATTR(usb_state, S_IRUGO, tsu6721_show_usb_state, NULL);
static DEVICE_ATTR(adc, S_IRUGO, tsu6721_show_adc, NULL);
static DEVICE_ATTR(reset_switch, S_IWUSR | S_IWGRP, NULL, tsu6721_reset);

static struct attribute *tsu6721_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_device_type.attr,
	&dev_attr_switch.attr,
	NULL
};

static const struct attribute_group tsu6721_group = {
	.attrs = tsu6721_attributes,
};

void tsu6721_otg_detach(void)
{
	unsigned int data = 0;
	int ret;
	struct i2c_client *client = local_usbsw->client;

	if (local_usbsw->dev1 & DEV_USB_OTG) {
		dev_info(&client->dev, "%s: real device\n", __func__);
		data = 0x00;
		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW2, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);
		data = SW_ALL_OPEN;
		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW1, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);

		data = 0x1A;
		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);
	} else
		dev_info(&client->dev, "%s: not real device\n", __func__);
}
EXPORT_SYMBOL(tsu6721_otg_detach);


void tsu6721_manual_switching(int path)
{
	struct i2c_client *client = local_usbsw->client;
	int value, ret;
	unsigned int data = 0;

	value = i2c_smbus_read_byte_data(client, TSU6721_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return;

	if (path == SWITCH_PORT_VAUDIO) {
		data = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_UART) {
		data = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUDIO) {
		data = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB) {
		data = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUTO) {
		data = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB_OPEN) {
		data = SW_USB_OPEN;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_ALL_OPEN) {
		data = SW_ALL_OPEN;
		value &= ~CON_MANUAL_SW;
	} else {
		pr_info("%s: wrong path (%d)\n", __func__, path);
		return;
	}

	local_usbsw->mansw = data;

	/* path for FTM sleep */
	if (path ==  SWITCH_PORT_ALL_OPEN) {
		ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANUAL_OVERRIDES1, 0x0a);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW2, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	} else {
		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}

}
EXPORT_SYMBOL(tsu6721_manual_switching);

#if defined(CONFIG_VIDEO_MHL_V2)
int dock_det(void)
{
	return isDeskdockconnected;
}
EXPORT_SYMBOL(dock_det);
#endif

int check_jig_state(void)
{
	int ret;
	ret = i2c_smbus_read_byte_data(local_usbsw->client, TSU6721_REG_DEV_T2);
	return (ret & DEV_T2_JIG_ALL_MASK) ? 1 : 0;
}
EXPORT_SYMBOL(check_jig_state);

static int tsu6721_detect_dev(struct tsu6721_usbsw *usbsw)
{
	int ret, adc;
	unsigned int val1, val2, val3;
	struct tsu6721_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
#if defined(CONFIG_VIDEO_MHL_V2)
//	u8 mhl_ret = 0;
#endif

	val1 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T1);
	if (val1 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, val1);
		return val1;
	}

	val2 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T2);
	if (val2 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, val2);
		return val2;
	}

	val3 = i2c_smbus_read_byte_data(client, is_ti_muic() ?
			TSU6721_REG_DEV_T3 : TSU6721_REG_RESERVED_1D);
	if (val3 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, val3);
		return val3;
	}
	adc = i2c_smbus_read_byte_data(client, TSU6721_REG_ADC);

	if (usbsw->dock_attached)
		pdata->callback(CABLE_TYPE_CARDOCK, TSU6721_DETACHED_DOCK);

	if (adc == 0x10)
		val2 = DEV_SMARTDOCK;
	else if (adc == 0x12) {
		val2 = DEV_AUDIO_DOCK;
		dev_err(&client->dev, "adc is audio");
		val1 = 0;
	}
	dev_err(&client->dev,
			"dev1: 0x%x, dev2: 0x%x, dev3: 0x%x, ADC: 0x%x Jig:%s\n",
			val1, val2, val3, adc,
			(check_jig_state() ? "ON" : "OFF"));

	/* Attached */
	if (val1 || val2 || (val3 & ~DEV_VBUS_DEBOUNCE) ||
			((val3 & DEV_VBUS_DEBOUNCE) && (adc != ADC_OPEN))) {
		/* USB */
		if (val1 & DEV_USB || val2 & DEV_T2_USB_MASK) {
			dev_info(&client->dev, "usb connect\n");
			pdata->callback(CABLE_TYPE_USB, TSU6721_ATTACHED);

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, usbsw->mansw);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* USB_CDP */
		} else if (val1 & DEV_USB_CHG) {
			dev_info(&client->dev, "usb_cdp connect\n");
			pdata->callback(CABLE_TYPE_CDP, TSU6721_ATTACHED);

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, usbsw->mansw);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}

		/* UART */
		} else if (val1 & DEV_T1_UART_MASK || val2 & DEV_T2_UART_MASK) {
			uart_connecting = 1;
			dev_info(&client->dev, "uart connect\n");
			i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, 0x1E);
			pdata->callback(CABLE_TYPE_UARTOFF, TSU6721_ATTACHED);

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, SW_UART);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* CHARGER */
		} else if ((val1 & DEV_T1_CHARGER_MASK) ||
				(val3 & DEV_T3_CHARGER_MASK)) {
			dev_info(&client->dev, "charger connect\n");
			pdata->callback(CABLE_TYPE_AC, TSU6721_ATTACHED);
		/* for SAMSUNG OTG */
		} else if (val1 & DEV_USB_OTG) {
			dev_info(&client->dev, "otg connect\n");
			pdata->callback(CABLE_TYPE_OTG, TSU6721_ATTACHED);

			i2c_smbus_write_byte_data(client, TSU6721_REG_MANSW1,
					is_ti_muic() ? 0x25 : 0x27);
			i2c_smbus_write_byte_data(client,
						TSU6721_REG_MANSW2, 0x02);
			msleep(50);
			i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, 0x1a);
		/* JIG */
		} else if (val2 & DEV_T2_JIG_MASK) {
			dev_info(&client->dev, "jig connect\n");
			pdata->callback(CABLE_TYPE_JIG, TSU6721_ATTACHED);
		/* Desk Dock */
		} else if ((val2 & DEV_AV) || (val3 & DEV_MHL) ||
			(val3 & DEV_AV_VBUS)) {
			if ((!is_ti_muic()) && ((adc & 0x1F) == 0x1A)) {
				pr_info("FSA Deskdock Attach\n");
				TSU6721_CheckAndHookAudioDock(1);
#if defined(CONFIG_VIDEO_MHL_V2)
				isDeskdockconnected = 1;
#endif
				i2c_smbus_write_byte_data(client,
					TSU6721_REG_RESERVED_20, 0x08);
			} else if ((is_ti_muic()) && !(val3 & DEV_MHL)) {
				pr_info("TI Deskdock Attach\n");
				TSU6721_CheckAndHookAudioDock(1);
#if defined(CONFIG_VIDEO_MHL_V2)
				isDeskdockconnected = 1;
#endif
			} else {
				pr_info("MHL Attach\n");
				if (!is_ti_muic())
					i2c_smbus_write_byte_data(client,
						TSU6721_REG_RESERVED_20, 0x08);
#if defined(CONFIG_VIDEO_MHL_V2)
				if (pdata->mhl_notify) {
					pr_info("fsa : MHL_attach_notify\n");
					pdata->mhl_notify(TSU6721_ATTACHED);
				}
#else
				TSU6721_CheckAndHookAudioDock(1);
#endif
			}
		/* Car Dock */
		} else if (val2 & DEV_JIG_UART_ON) {
#if 0
			pdata->callback(CABLE_TYPE_CARDOCK,
				TSU6721_ATTACHED_CAR_DOCK);
			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, SW_AUDIO);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
					TSU6721_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
#endif
			usbsw->dock_attached = TSU6721_ATTACHED;


		/* SmartDock */
		} else if (val2 & DEV_SMARTDOCK) {
			usbsw->adc = adc;
			dev_info(&client->dev, "smart dock connect\n");

			usbsw->mansw = SW_DHOST;
			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, SW_DHOST);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_read_byte_data(client,
					TSU6721_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_SMART_DOCK,
				TSU6721_ATTACHED);
#if defined(CONFIG_VIDEO_MHL_V2)
			if (pdata->mhl_notify) {
				pr_info("fsa : smart dock - MHL_attach_notify\n");
				pdata->mhl_notify(TSU6721_ATTACHED);
			}
#endif
		/* Audio Dock */
		} else if (val2 & DEV_AUDIO_DOCK) {
			usbsw->adc = adc;
			dev_info(&client->dev, "audio dock connect\n");

			usbsw->mansw = SW_DHOST;

			if (is_ti_muic())
				ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, SW_DHOST_TSU);
			else
				ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_MANSW1, SW_DHOST);

			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
				TSU6721_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
				TSU6721_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_AUDIO_DOCK,
				TSU6721_ATTACHED);
		} else if (val3 & DEV_VBUS_DEBOUNCE) {
			dev_info(&client->dev,
					"Incompatible Charger connect\n");
			pdata->callback(CABLE_TYPE_INCOMPATIBLE,
					TSU6721_ATTACHED);
		}
	/* Detached */
	} else {
		/* USB */
		if (usbsw->dev1 & DEV_USB ||
				usbsw->dev2 & DEV_T2_USB_MASK) {
			pdata->callback(CABLE_TYPE_USB, TSU6721_DETACHED);
		} else if (usbsw->dev1 & DEV_USB_CHG) {
			pdata->callback(CABLE_TYPE_CDP, TSU6721_DETACHED);

		/* UART */
		} else if (usbsw->dev1 & DEV_T1_UART_MASK ||
				usbsw->dev2 & DEV_T2_UART_MASK) {
			pdata->callback(CABLE_TYPE_UARTOFF, TSU6721_DETACHED);
			uart_connecting = 0;
			dev_info(&client->dev, "[TSU6721] uart disconnect\n");

		/* CHARGER */
		} else if ((usbsw->dev1 & DEV_T1_CHARGER_MASK) ||
				(usbsw->dev3 & DEV_T3_CHARGER_MASK)) {
			pdata->callback(CABLE_TYPE_AC, TSU6721_DETACHED);
		/* for SAMSUNG OTG */
		} else if (usbsw->dev1 & DEV_USB_OTG) {
			i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL, 0x1E);
		/* JIG */
		} else if (usbsw->dev2 & DEV_T2_JIG_MASK) {
			pdata->callback(CABLE_TYPE_JIG, TSU6721_DETACHED);
		/* Desk Dock */
		} else if ((usbsw->dev2 & DEV_AV) || (usbsw->dev3 & DEV_MHL) ||
				(usbsw->dev3 & DEV_AV_VBUS)) {

			pr_info("Deskdock/MHL Detach\n");

			if (!is_ti_muic())
				i2c_smbus_write_byte_data(client,
					TSU6721_REG_RESERVED_20, 0x04);
#if defined(CONFIG_VIDEO_MHL_V2)
			if (isDeskdockconnected)
				TSU6721_CheckAndHookAudioDock(0);

			if (pdata->mhl_notify)
				pdata->mhl_notify(TSU6721_DETACHED);
			detached_status = 1;

			isDeskdockconnected = 0;
#else
			TSU6721_CheckAndHookAudioDock(0);
#endif
		/* Car Dock */
		} else if (usbsw->dev2 & DEV_JIG_UART_ON) {
			pdata->callback(CABLE_TYPE_CARDOCK,
				TSU6721_DETACHED_DOCK);
			ret = i2c_smbus_read_byte_data(client,
					TSU6721_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_CTRL,
					ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
			usbsw->dock_attached = TSU6721_DETACHED;
		} else if (usbsw->adc == 0x10) {
			dev_info(&client->dev, "smart dock disconnect\n");
			ret = i2c_smbus_read_byte_data(client,
						TSU6721_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					TSU6721_REG_CTRL,
					ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_SMART_DOCK,
				TSU6721_DETACHED);
			usbsw->adc = 0;
#if defined(CONFIG_VIDEO_MHL_V2)
			if (pdata->mhl_notify)
				pdata->mhl_notify(TSU6721_DETACHED);
#endif
		} else if (usbsw->adc == 0x12) {
			dev_info(&client->dev, "audio dock disconnect\n");
			ret = i2c_smbus_read_byte_data(client,
				    TSU6721_REG_CTRL);
			    if (ret < 0)
					dev_err(&client->dev,
					    "%s: err %d\n", __func__, ret);
				ret = i2c_smbus_write_byte_data(client,
						TSU6721_REG_CTRL,
						ret | CON_MANUAL_SW);
			    if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_AUDIO_DOCK,
					TSU6721_DETACHED);
			usbsw->adc = 0;
		} else if (usbsw->dev3 & DEV_VBUS_DEBOUNCE) {
			dev_info(&client->dev,
					"Incompatible Charger disconnect\n");
			pdata->callback(CABLE_TYPE_INCOMPATIBLE,
					TSU6721_DETACHED);
		}
	}
	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
	usbsw->dev3 = val3;

	return adc;
}

static int tsu6721_check_dev(struct tsu6721_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int device_type1, device_type2, device_type3, device_type = 0;

	device_type1 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T1);
	if (device_type1 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type1);
		return 0;
	}
	device_type2 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T2);
	if (device_type2 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type2);
		return 0;
	}
	device_type3 = i2c_smbus_read_byte_data(client, TSU6721_REG_DEV_T3);
	if (device_type3 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type3);
		return 0;
	}
	device_type = device_type1+(device_type2<<8)+(device_type3<<16);

	return device_type;
}

static int tsu6721_handle_dock_vol_key(struct tsu6721_usbsw *info, int adc)
{
	struct input_dev *input = info->input;
	int pre_key = info->previous_key;
	unsigned int code = 0;
	int state = 0;

	if (adc == ADC_OPEN) {
		switch (pre_key) {
		case DOCK_KEY_VOL_UP_PRESSED:
			code = KEY_VOLUMEUP;
			state = 0;
			info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
			break;
		case DOCK_KEY_VOL_DOWN_PRESSED:
			code = KEY_VOLUMEDOWN;
			state = 0;
			info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
			break;
		case DOCK_KEY_PREV_PRESSED:
			code = KEY_PREVIOUSSONG;
			state = 0;
			info->previous_key = DOCK_KEY_PREV_RELEASED;
			break;
		case DOCK_KEY_PLAY_PAUSE_PRESSED:
			code = KEY_PLAYPAUSE;
			state = 0;
			info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
			break;
		case DOCK_KEY_NEXT_PRESSED:
			code = KEY_NEXTSONG;
			state = 0;
			info->previous_key = DOCK_KEY_NEXT_RELEASED;
			break;
		default:
			return 0;
		}
		input_event(input, EV_KEY, code, state);
		input_sync(input);
		return 0;
	}

	if (pre_key == DOCK_KEY_NONE) {
		if (adc != ADC_DOCK_VOL_UP && adc != ADC_DOCK_VOL_DN
			&& adc != ADC_DOCK_PREV_KEY && adc != ADC_DOCK_NEXT_KEY
			&& adc != ADC_DOCK_PLAY_PAUSE_KEY)
			return 0;
	}


	switch (adc) {
	case ADC_DOCK_VOL_UP:
		code = KEY_VOLUMEUP;
		state = 1;
		info->previous_key = DOCK_KEY_VOL_UP_PRESSED;
		break;
	case ADC_DOCK_VOL_DN:
		code = KEY_VOLUMEDOWN;
		state = 1;
		info->previous_key = DOCK_KEY_VOL_DOWN_PRESSED;
		break;
	case ADC_DOCK_PREV_KEY-1 ... ADC_DOCK_PREV_KEY+1:
		code = KEY_PREVIOUSSONG;
		state = 1;
		info->previous_key = DOCK_KEY_PREV_PRESSED;
		break;
	case ADC_DOCK_PLAY_PAUSE_KEY-1 ... ADC_DOCK_PLAY_PAUSE_KEY+1:
		code = KEY_PLAYPAUSE;
		state = 1;
		info->previous_key = DOCK_KEY_PLAY_PAUSE_PRESSED;
		break;
	case ADC_DOCK_NEXT_KEY-1 ... ADC_DOCK_NEXT_KEY+1:
		code = KEY_NEXTSONG;
		state = 1;
		info->previous_key = DOCK_KEY_NEXT_PRESSED;
		break;
	case ADC_DESKDOCK:
		if (pre_key == DOCK_KEY_VOL_UP_PRESSED) {
			code = KEY_VOLUMEUP;
			state = 0;
			info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
		} else if (pre_key == DOCK_KEY_VOL_DOWN_PRESSED) {
			code = KEY_VOLUMEDOWN;
			state = 0;
			info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
		} else if (pre_key == DOCK_KEY_PREV_PRESSED) {
			code = KEY_PREVIOUSSONG;
			state = 0;
			info->previous_key = DOCK_KEY_PREV_RELEASED;
		} else if (pre_key == DOCK_KEY_PLAY_PAUSE_PRESSED) {
			code = KEY_PLAYPAUSE;
			state = 0;
			info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
		} else if (pre_key == DOCK_KEY_NEXT_PRESSED) {
			code = KEY_NEXTSONG;
			state = 0;
			info->previous_key = DOCK_KEY_NEXT_RELEASED;
		} else {
			return 0;
		}
		break;
	default:
		break;
		return 0;
	}

	input_event(input, EV_KEY, code, state);
	input_sync(input);

	return 1;
}

static irqreturn_t tsu6721_irq_thread(int irq, void *data)
{
	struct tsu6721_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr, intr2, detect;

	/* TSU6721 : Read interrupt -> Read Device
	 TSU6721 : Read Device -> Read interrupt */
	if (is_ti_muic())
		msleep(50);
	pr_info("tsu6721_irq_thread is called\n");
	/* device detection */
	mutex_lock(&usbsw->mutex);
	detect = tsu6721_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);
	pr_info("%s: detect dev_adc: %x\n", __func__, detect);

	/* read and clear interrupt status bits */
	intr = i2c_smbus_read_byte_data(client, TSU6721_REG_INT1);
	dev_info(&client->dev, "%s: intr : 0x%x",
					__func__, intr & 0xff);

	intr2 = i2c_smbus_read_byte_data(client, TSU6721_REG_INT2);
	dev_info(&client->dev, "%s: intr2 : 0x%x\n",
					__func__, intr2 & 0xff);

	if (intr & INT_OVP_EN)
		usbsw->pdata->oxp_callback(ENABLE);
	else if (intr & INT_OXP_DISABLE)
		usbsw->pdata->oxp_callback(DISABLE);

	if (intr < 0 || intr2 < 0) {
		msleep(100);
		dev_err(&client->dev, "%s: err %d, %d\n",
				__func__, intr, intr2);
		intr = i2c_smbus_read_byte_data(client, TSU6721_REG_INT1);
		if (intr < 0)
			dev_err(&client->dev,
				"%s: err at read %d\n", __func__, intr);
		intr2 = i2c_smbus_read_byte_data(client, TSU6721_REG_INT2);
		if (intr2 < 0)
			dev_err(&client->dev,
				"%s: err at read %d\n", __func__, intr2);
		tsu6721_reg_init(usbsw);
		return IRQ_HANDLED;
	} else if (intr == 0 && intr2 == 0) {
		/* interrupt was fired, but no status bits were set,
		so device was reset. In this case, the registers were
		reset to defaults so they need to be reinitialised. */
		tsu6721_reg_init(usbsw);
	}
	/* ADC_value(key pressed) changed at AV_Dock.*/
	if (intr2) {
		if (intr2 & 0x4) { /* for adc change */
			tsu6721_handle_dock_vol_key(usbsw, detect);
			dev_info(&client->dev,
					"intr2: 0x%x, adc_val: %x\n",
							intr2, detect);
		} else if (intr2 & 0x2) { /* for smart dock */
			i2c_smbus_read_byte_data(client, TSU6721_REG_INT1);
			i2c_smbus_read_byte_data(client, TSU6721_REG_INT2);
		} else if (intr2 & 0x1) { /* for av change (desk dock, hdmi) */
			dev_info(&client->dev,
				"%s enter Av charing\n", __func__);
			tsu6721_detect_dev(usbsw);
		} else {
			dev_info(&client->dev,
				"%s intr2 but, nothing happend, intr2: 0x%x\n",
				__func__, intr2);
		}
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

static int tsu6721_irq_init(struct tsu6721_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
			tsu6721_irq_thread, IRQF_TRIGGER_FALLING,
			"tsu6721 micro USB", usbsw);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,
				"failed to enable wakeup src %d\n", ret);
	}

	return 0;
}

static void tsu6721_init_detect(struct work_struct *work)
{
	struct tsu6721_usbsw *usbsw = container_of(work,
			struct tsu6721_usbsw, init_work.work);
	int ret = 0;

	dev_info(&usbsw->client->dev, "%s\n", __func__);

	mutex_lock(&usbsw->mutex);
	tsu6721_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	ret = tsu6721_irq_init(usbsw);
	if (ret)
		dev_info(&usbsw->client->dev,
				"failed to enable  irq init %s\n", __func__);
}

#ifdef CONFIG_OF
static int musb_parse_dt(struct device *dev, struct tsu6721_platform_data *pdata)
{
#if 0
	// pdata->irq_gpio = of_get_named_gpio(pdev->of_node,"fs9485,irq-gpio",0);
	struct device_node *np = dev->of_node;

	/* regulator info */
	pdata->i2c_pull_up = of_property_read_bool(np, "tsu6721,i2c-pull-up");

	/* reset, irq gpio info */
	pdata->gpio_scl = of_get_named_gpio_flags(np, "tsu6721,scl-gpio",
				0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "tsu6721,sda-gpio",
				0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "tsu6721,irq-gpio",
		0, &pdata->irq_gpio_flags);
#endif   
	return 0;
}
#else
static int musb_parse_dt(struct device *pdev, struct tsu6721_platform_data *pdata)
{
	   return -ENODEV;
}
#endif

static int tsu6721_doc_init(void)
{
	   return 0;
}

static int __devinit tsu6721_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tsu6721_usbsw *usbsw;
	int ret = 0;
	struct device *switch_dev;
	int error;
	struct tsu6721_platform_data *pdata;

	printk("%s\n",__func__);
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct tsu6721_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
					return -ENOMEM;
		}
		error = musb_parse_dt(&client->dev, pdata);
		if (error)
	    	return error;
		pdata->callback = muic_callback;
		pdata->dock_init = tsu6721_doc_init;
		pdata->mhl_notify = of_sii8240_muic_mhl_notify;
	} else 
		pdata = client->dev.platform_data;
	
	if (!pdata)
		return -EINVAL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	usbsw = kzalloc(sizeof(struct tsu6721_usbsw), GFP_KERNEL);
	if (!usbsw ) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		kfree(usbsw);
		return -ENOMEM;
	}

	usbsw->client = client;
	
	if (client->dev.of_node)
		usbsw->pdata = pdata;
	else
		usbsw->pdata = client->dev.platform_data;

	if (!usbsw->pdata)
		goto fail1;
	
	i2c_set_clientdata(client, usbsw);

	mutex_init(&usbsw->mutex);

	local_usbsw = usbsw;

	if (usbsw->pdata->cfg_gpio)
		usbsw->pdata->cfg_gpio();

	tsu6721_reg_init(usbsw);

	ret = sysfs_create_group(&client->dev.kobj, &tsu6721_group);
	if (ret) {
		dev_err(&client->dev,
				"failed to create tsu6721 attribute group\n");
		goto fail2;
	}

	/* make sysfs node /sys/class/sec/switch/usb_state */
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_dev)) {
		pr_err("[TSU6721] Failed to create device (switch_dev)!\n");
		ret = PTR_ERR(switch_dev);
		goto fail2;
	}

	ret = device_create_file(switch_dev, &dev_attr_usb_state);
	if (ret < 0) {
		pr_err("[TSU6721] Failed to create file (usb_state)!\n");
		goto err_create_file_state;
	}

	ret = device_create_file(switch_dev, &dev_attr_adc);
	if (ret < 0) {
		pr_err("[TSU6721] Failed to create file (adc)!\n");
		goto err_create_file_adc;
	}

	ret = device_create_file(switch_dev, &dev_attr_reset_switch);
	if (ret < 0) {
		pr_err("[TSU6721] Failed to create file (reset_switch)!\n");
		goto err_create_file_reset_switch;
	}

	dev_set_drvdata(switch_dev, usbsw);
	/* tsu6721 dock init*/
	if (usbsw->pdata->dock_init)
		usbsw->pdata->dock_init();

	/* tsu6721 reset */
	if (usbsw->pdata->reset_cb)
		usbsw->pdata->reset_cb();

	/* set tsu6721 init flag. */
	if (usbsw->pdata->set_init_flag)
		usbsw->pdata->set_init_flag();

	/* initial cable detection */
	INIT_DELAYED_WORK(&usbsw->init_work, tsu6721_init_detect);
	schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(2700));

	return 0;

err_create_file_reset_switch:
	device_remove_file(switch_dev, &dev_attr_reset_switch);
err_create_file_adc:
	device_remove_file(switch_dev, &dev_attr_adc);
err_create_file_state:
	device_remove_file(switch_dev, &dev_attr_usb_state);
fail2:
	if (client->irq)
		free_irq(client->irq, usbsw);
fail1:
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit tsu6721_remove(struct i2c_client *client)
{
	struct tsu6721_usbsw *usbsw = i2c_get_clientdata(client);
	cancel_delayed_work(&usbsw->init_work);
	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, usbsw);
	}
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);

	sysfs_remove_group(&client->dev.kobj, &tsu6721_group);
	kfree(usbsw);
	return 0;
}

static int tsu6721_resume(struct i2c_client *client)
{
	struct tsu6721_usbsw *usbsw = i2c_get_clientdata(client);

/* add for tsu6721_irq_thread i2c error during wakeup */
	tsu6721_check_dev(usbsw);

	i2c_smbus_read_byte_data(client, TSU6721_REG_INT1);
	i2c_smbus_read_byte_data(client, TSU6721_REG_INT2);

	/* device detection */
	mutex_lock(&usbsw->mutex);
	tsu6721_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	return 0;
}


static const struct i2c_device_id tsu6721_id[] = {
	{"tsu6721", 0},
	{}
};
static struct of_device_id muic_match_table[] = {
	{ .compatible = "tsu6721,muic",},
	{ },
};
MODULE_DEVICE_TABLE(i2c, tsu6721_id);

static struct i2c_driver tsu6721_i2c_driver = {
	.driver = {
		.name = "tsu6721",
		.owner = THIS_MODULE,
		.of_match_table = muic_match_table,
	},
	.probe = tsu6721_probe,
	.remove = __devexit_p(tsu6721_remove),
	.resume = tsu6721_resume,
	.id_table = tsu6721_id,
};

module_i2c_driver(tsu6721_i2c_driver);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("TSU6721 USB Switch driver");
MODULE_LICENSE("GPL");
