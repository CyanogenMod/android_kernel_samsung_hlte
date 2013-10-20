/*
 * Copyright (C) 2013 Samsung Electronics Co. Ltd.
 *  Hyuk Kang <hyuk78.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/host_notify.h>

#ifdef CONFIG_CHARGER_BQ24260
#include <linux/gpio.h>
#define GPIO_USB_VBUS_MSM 50
#endif

struct dwc3_sec {
	struct notifier_block nb;
	struct dwc3_msm *dwcm;
};
static struct dwc3_sec sec_noti;

#ifdef CONFIG_CHARGER_BQ24260
int bq24260_otg_control(int enable)
{
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	int current_cable_type;

	pr_info("%s: enable(%d)\n", __func__, enable);

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -1;
	}

	if (enable)
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
	else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

	value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	}
	return ret;
}

struct booster_data sec_booster = {
	.name = "bq24260",
	.boost = bq24260_otg_control,
};

static irqreturn_t msm_usb_vbus_msm_irq(int irq, void *data)
{
	struct dwc3_sec *snoti = &sec_noti;
	struct dwc3_msm *dwcm;
	int enable = gpio_get_value(GPIO_USB_VBUS_MSM);
	pr_info("%s usb_vbus_msm=%d\n", __func__, enable);
	dwcm = snoti->dwcm;
	if (!dwcm) {
		pr_err("%s: dwc3_otg (dwcm) is null\n", __func__);
		return NOTIFY_BAD;
	}
	if (dwcm->ext_xceiv.id == DWC3_ID_GROUND && enable == 0) {
		pr_info("%s over current\n", __func__);
		sec_otg_notify(HNOTIFY_OVERCURRENT);
		return IRQ_HANDLED;
	}
	sec_otg_notify(enable ?
		HNOTIFY_OTG_POWER_ON : HNOTIFY_OTG_POWER_OFF);
	return IRQ_HANDLED;
}

static int usb_vbus_msm_init(struct dwc3_msm *dwcm, struct usb_phy *phy)
{
	int ret;

	sec_otg_register_booster(&sec_booster);

	ret = gpio_tlmm_config(GPIO_CFG(GPIO_USB_VBUS_MSM, 0, GPIO_CFG_INPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	if (unlikely(ret)) {
	    pr_err("%s GPIO_USB_VBUS_MSM gpio_tlmm_config failed. ret=%d\n", __func__, ret);
	    return ret;
	}

	pr_info("%s usb_vbus_msm=%d\n", __func__, gpio_get_value(GPIO_USB_VBUS_MSM));
	ret = request_threaded_irq(gpio_to_irq(GPIO_USB_VBUS_MSM),
						NULL, msm_usb_vbus_msm_irq,
						IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING,
						"usb_vbus_msm", dwcm);
	if (ret)
		pr_err("%s request irq failed for usb_vbus_msm\n", __func__);
	else
		pr_err("%s request irq succeed for usb_vbus_msm\n", __func__);
	return ret;
}
#endif

static int sec_otg_ext_notify(struct dwc3_msm *mdwc, int enable)
{
	mdwc->ext_xceiv.id = enable ? DWC3_ID_GROUND : DWC3_ID_FLOAT;

	if (atomic_read(&mdwc->in_lpm)) {
		dev_info(mdwc->dev, "%s: calling resume_work\n", __func__);
		dwc3_resume_work(&mdwc->resume_work.work);
	} else {
		dev_info(mdwc->dev, "%s: notifying xceiv event\n", __func__);
		if (mdwc->otg_xceiv)
			mdwc->ext_xceiv.notify_ext_events(mdwc->otg_xceiv->otg,
							DWC3_EVENT_XCEIV_STATE);
	}
	return 0;
}

int sec_handle_event(int enable)
{
	struct dwc3_sec *snoti = &sec_noti;
	struct dwc3_msm *dwcm;

	pr_info("%s: event %d\n", __func__, enable);

	if (!snoti) {
		pr_err("%s: dwc3_otg (snoti) is null\n", __func__);
		return NOTIFY_BAD;
	}

	dwcm = snoti->dwcm;
	if (!dwcm) {
		pr_err("%s: dwc3_otg (dwcm) is null\n", __func__);
		return NOTIFY_BAD;
	}

	if (enable) {
		pr_info("ID clear\n");
		sec_otg_ext_notify(dwcm, 1);
	} else {
		pr_info("ID set\n");
		sec_otg_ext_notify(dwcm, 0);
	}

	return 0;
}
EXPORT_SYMBOL(sec_handle_event);

static int sec_otg_notifications(struct notifier_block *nb,
				   unsigned long event, void *unused)
{
	struct dwc3_sec *snoti = container_of(nb, struct dwc3_sec, nb);
	struct dwc3_msm *dwcm;

	pr_info("%s: event %lu\n", __func__, event);

	if (!snoti) {
		pr_err("%s: dwc3_otg (snoti) is null\n", __func__);
		return NOTIFY_BAD;
	}

	dwcm = snoti->dwcm;
	if (!dwcm) {
		pr_err("%s: dwc3_otg (dwcm) is null\n", __func__);
		return NOTIFY_BAD;
	}

	switch (event) {
	case HNOTIFY_NONE: break;
	case HNOTIFY_VBUS: break;
	case HNOTIFY_ID:
		pr_info("ID clear\n");
		sec_otg_ext_notify(dwcm, 1);
		break;
	case HNOTIFY_CHARGER: break;
	case HNOTIFY_ENUMERATED: break;
	case HNOTIFY_ID_PULL:
		pr_info("ID set\n");
		sec_otg_ext_notify(dwcm, 0);
		break;
	case HNOTIFY_OVERCURRENT: break;
	case HNOTIFY_OTG_POWER_ON: break;
	case HNOTIFY_OTG_POWER_OFF: break;
	case HNOTIFY_SMARTDOCK_ON: 
		pr_info("ID clear\n");
		sec_otg_ext_notify(dwcm, 1);
		break;
	case HNOTIFY_SMARTDOCK_OFF: 
		pr_info("ID set\n");
		sec_otg_ext_notify(dwcm, 0);
		break;
	case HNOTIFY_AUDIODOCK_ON: break;
	case HNOTIFY_AUDIODOCK_OFF: break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int sec_otg_init(struct dwc3_msm *dwcm, struct usb_phy *phy)
{
	int ret = 0;

	pr_info("%s: register notifier\n", __func__);
	sec_noti.nb.notifier_call = sec_otg_notifications;
	sec_noti.dwcm = dwcm;

#ifdef CONFIG_CHARGER_BQ24260
	usb_vbus_msm_init(dwcm, phy);
#endif

#if 0
	ret = usb_register_notifier(phy, &sec_noti.nb);
	if (ret) {
		pr_err("%s: usb_register_notifier failed\n", __func__);
		return ret;
	}
#endif
	return ret;
}
