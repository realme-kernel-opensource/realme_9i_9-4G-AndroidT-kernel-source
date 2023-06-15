// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/soc/qcom/fsa4480-i2c.h>



#ifdef OPLUS_ARCH_EXTENDS
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_TCPC_CLASS
#include "tcpm.h"
#endif /* CONFIG_TCPC_CLASS */
#endif /* OPLUS_ARCH_EXTENDS */

#ifdef OPLUS_ARCH_EXTENDS
 enum switch_vendor {
      FSA4480 = 0,
      HL5280,
      DIO4480
  };
#endif /* OPLUS_ARCH_EXTENDS */

#define FSA4480_I2C_NAME	"fsa4480-driver"

#ifdef OPLUS_ARCH_EXTENDS
#define HL5280_DEVICE_REG_VALUE 0x49
#define DIO4480_DEVICE_REG_VALUE 0xF1
#define FSA4480_DEVICE_ID       0x00
#endif /* OPLUS_ARCH_EXTENDS */
#define FSA4480_SWITCH_SETTINGS 0x04
#define FSA4480_SWITCH_CONTROL  0x05
#define FSA4480_SWITCH_STATUS1  0x07
#define FSA4480_SLOW_L          0x08
#define FSA4480_SLOW_R          0x09
#define FSA4480_SLOW_MIC        0x0A
#define FSA4480_SLOW_SENSE      0x0B
#define FSA4480_SLOW_GND        0x0C
#define FSA4480_DELAY_L_R       0x0D
#define FSA4480_DELAY_L_MIC     0x0E
#define FSA4480_DELAY_L_SENSE   0x0F
#define FSA4480_DELAY_L_AGND    0x10
#ifdef OPLUS_ARCH_EXTENDS
#define FSA4480_FUN_EN          0x12
#define FSA4480_JACK_STATUS     0x17
#endif /* OPLUS_ARCH_EXTENDS */
#define FSA4480_RESET           0x1E

#ifdef OPLUS_BUG_STABILITY
/*
 * 0x1~0xff == 100us~25500us
 */
#define DEFAULT_SWITCH_DELAY		0x12
#endif /* OPLUS_BUG_STABILITY */

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_notifier;
	struct mutex notification_lock;

#ifdef OPLUS_ARCH_EXTENDS
	unsigned int hs_det_pin;
	int hs_det_level;
	enum switch_vendor vendor;
#ifdef CONFIG_TCPC_CLASS
	struct tcpc_device *tcpc_dev;
	bool plug_state;
	bool ext_cc_logic;
	uint8_t new_state;
	uint8_t old_state;
#endif /* CONFIG_TCPC_CLASS */
#endif /* OPLUS_ARCH_EXTENDS */
};

struct fsa4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FSA4480_RESET,
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
#ifdef OPLUS_BUG_STABILITY
	{FSA4480_SWITCH_CONTROL, 0x18},
#endif /* OPLUS_BUG_STABILITY */
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
#ifdef OPLUS_BUG_STABILITY
	{FSA4480_DELAY_L_MIC, DEFAULT_SWITCH_DELAY},
#else
	{FSA4480_DELAY_L_MIC, 0x00},
#endif /* OPLUS_BUG_STABILITY */
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x09},
	{FSA4480_SWITCH_SETTINGS, 0x98},
};

#ifdef OPLUS_ARCH_EXTENDS
int fsa4480_get_chip_vendor(struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;


	return fsa_priv->vendor;
}
EXPORT_SYMBOL(fsa4480_get_chip_vendor);
#endif

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	u32 prev_control, prev_enable;

	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, &prev_control);
	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &prev_enable);

	if (prev_control == switch_control && prev_enable == switch_enable) {
		dev_dbg(fsa_priv->dev, "%s: settings unchanged\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x80);

#ifdef OPLUS_ARCH_EXTENDS
	if(fsa_priv->vendor == DIO4480) {
		regmap_write(fsa_priv->regmap, FSA4480_RESET, 0x01);//reset DIO4480
		usleep_range(1000, 1005);
	}
#endif

	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
	/* FSA4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
#ifdef OPLUS_BUG_STABILITY
	usleep_range(DEFAULT_SWITCH_DELAY*100, DEFAULT_SWITCH_DELAY*100+50);
#endif /* OPLUS_BUG_STABILITY */
}

#ifdef OPLUS_ARCH_EXTENDS
#ifdef CONFIG_TCPC_CLASS
static int fsa4480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	int ret;
	union power_supply_propval mode;
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, psy_nb);
	struct device *dev;
	struct tcp_notify *noti = ptr;

	if (IS_ERR_OR_NULL(fsa_priv))
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if (fsa_priv->ext_cc_logic) {
		dev_info(dev, "%s: typeC event: %d plug_state: %d\n", __func__, evt, fsa_priv->plug_state);
		switch (evt) {
		case TCP_NOTIFY_TYPEC_STATE:
			dev_info(dev, "%s: old_state: %d, new_state: %d\n",
				__func__, noti->typec_state.old_state, noti->typec_state.new_state);
			fsa_priv->new_state = noti->typec_state.new_state;
			fsa_priv->old_state = noti->typec_state.old_state;
			if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
				noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
				/* AUDIO plug in */
				dev_info(dev, "%s: audio plug in\n", __func__);
				fsa_priv->plug_state = true;
				pm_stay_awake(fsa_priv->dev);
				//cancel_work(&fsa_priv->usbc_analog_work);
				schedule_work(&fsa_priv->usbc_analog_work);
			} else if (fsa_priv->plug_state == true
				&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
				/* AUDIO plug out */
				dev_info(dev, "%s: audio plug out\n", __func__);
				fsa_priv->plug_state = false;
				pm_stay_awake(fsa_priv->dev);
				//cancel_work(&fsa_priv->usbc_analog_work);
				schedule_work(&fsa_priv->usbc_analog_work);
			}

			break;
		default:
			break;
		};

		return NOTIFY_OK;
	} else {
		if ((struct power_supply *)ptr != fsa_priv->usb_psy ||
					evt != PSY_EVENT_PROP_CHANGED)
			return 0;

		ret = power_supply_get_property(fsa_priv->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
		if (ret) {
			dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
				__func__, ret);
			return ret;
		}

		dev_err(dev, "%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
			__func__, mode.intval, fsa_priv->usbc_mode.counter,
			POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);

		switch (mode.intval) {
		case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		case POWER_SUPPLY_TYPEC_NONE:
			if (atomic_read(&(fsa_priv->usbc_mode)) == mode.intval)
				break; /* filter notifications received before */
			atomic_set(&(fsa_priv->usbc_mode), mode.intval);

			dev_dbg(dev, "%s: queueing usbc_analog_work\n",
				__func__);
			pm_stay_awake(fsa_priv->dev);
			queue_work(system_freezable_wq, &fsa_priv->usbc_analog_work);
			break;
		default:
			break;
		}
		return ret;
	}
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	if (fsa_priv->ext_cc_logic) {
		dev_info(dev, "%s: plug_state %d\n", __func__, fsa_priv->plug_state);
		if (fsa_priv->plug_state) {
			/* activate switches */
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);
			/* notify call chain on event */
			blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
			POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER, NULL);
			if (gpio_is_valid(fsa_priv->hs_det_pin)) {
				dev_info(dev, "%s: set hs_det_pin to low.\n", __func__);
				gpio_direction_output(fsa_priv->hs_det_pin, 0);
			}
		} else {
			if (gpio_is_valid(fsa_priv->hs_det_pin)) {
				dev_info(dev, "%s: set hs_det_pin to high.\n", __func__);
				gpio_direction_output(fsa_priv->hs_det_pin, 1);
			}

			/* notify call chain on event */
			blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
					POWER_SUPPLY_TYPEC_NONE, NULL);
			/* deactivate switches */
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		}
	} else {
		/* get latest mode again within locked context */
		rc = power_supply_get_property(fsa_priv->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
		if (rc) {
			dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
				__func__, rc);
			goto done;
		}

		dev_err(dev, "%s: setting GPIOs active = %d\n",
			__func__, mode.intval != POWER_SUPPLY_TYPEC_NONE);

		switch (mode.intval) {
		/* add all modes FSA should notify for in here */
		case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
			/* activate switches */
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

			/* notify call chain on event */
			blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
			mode.intval, NULL);
			break;
		case POWER_SUPPLY_TYPEC_NONE:
			/* notify call chain on event */
			blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
					POWER_SUPPLY_TYPEC_NONE, NULL);

			/* deactivate switches */
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
			break;
		default:
			/* ignore other usb connection modes */
			break;
		}
	}

done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}
#else /* CONFIG_TCPC_CLASS */
static int fsa4480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	int ret;
	union power_supply_propval mode;
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, psy_nb);
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if ((struct power_supply *)ptr != fsa_priv->usb_psy ||
				evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(fsa_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (ret) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, ret);
		return ret;
	}

	dev_dbg(dev, "%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, fsa_priv->usbc_mode.counter,
		POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);

	switch (mode.intval) {
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
	case POWER_SUPPLY_TYPEC_NONE:
		if (atomic_read(&(fsa_priv->usbc_mode)) == mode.intval)
			break; /* filter notifications received before */
		atomic_set(&(fsa_priv->usbc_mode), mode.intval);

		dev_dbg(dev, "%s: queueing usbc_analog_work\n",
			__func__);
		pm_stay_awake(fsa_priv->dev);
		queue_work(system_freezable_wq, &fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}
	return ret;
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode again within locked context */
	rc = power_supply_get_property(fsa_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (rc) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}
	dev_dbg(dev, "%s: setting GPIOs active = %d\n",
		__func__, mode.intval != POWER_SUPPLY_TYPEC_NONE);

	switch (mode.intval) {
	/* add all modes FSA should notify for in here */
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		/* activate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
		mode.intval, NULL);
		break;
	case POWER_SUPPLY_TYPEC_NONE:
		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
				POWER_SUPPLY_TYPEC_NONE, NULL);

		/* deactivate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}

done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}
#endif /* CONFIG_TCPC_CLASS */
#endif /* OPLUS_ARCH_EXTENDS */
/*
 * fsa4480_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on success, or error code
 */
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&fsa_priv->fsa4480_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(fsa_priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);
	rc = fsa4480_usbc_analog_setup_switches(fsa_priv);

	#ifndef OPLUS_BUG_STABILITY
	regmap_update_bits(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0x07,
			   fsa_priv->switch_control);
	#endif /* OPLUS_BUG_STABILITY */
	return rc;
}
EXPORT_SYMBOL(fsa4480_reg_notifier);

/*
 * fsa4480_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on pass, or error code
 */
int fsa4480_unreg_notifier(struct notifier_block *nb,
			     struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;
	struct device *dev;
	union power_supply_propval mode;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode within locked context */
#ifdef CONFIG_TCPC_CLASS
	if (fsa_priv->ext_cc_logic) {
		if (fsa_priv->plug_state) {
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		}
	} else {
		rc = power_supply_get_property(fsa_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
		if (rc) {
			dev_dbg(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
				__func__, rc);
			goto done;
		}
		/* Do not reset switch settings for usb digital hs */
		if (mode.intval == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	}
#else
	rc = power_supply_get_property(fsa_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (rc) {
		dev_dbg(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}
	/* Do not reset switch settings for usb digital hs */
	if (mode.intval == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
#endif /* CONFIG_TCPC_CLASS */
	rc = blocking_notifier_chain_unregister
					(&fsa_priv->fsa4480_notifier, nb);
done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}
EXPORT_SYMBOL(fsa4480_unreg_notifier);

static int fsa4480_validate_display_port_settings(struct fsa4480_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * fsa4480_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to fsa4480 device
 * @event - fsa_function enum
 *
 * Returns int on whether the switch happened or not
 */
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

#ifdef OPLUS_ARCH_EXTENDS
	pr_info("%s - switch event: %d\n", __func__, event);
#endif /* OPLUS_ARCH_EXTENDS */

	switch (event) {
	case FSA_MIC_GND_SWAP:
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		break;
	#ifdef OPLUS_ARCH_EXTENDS
	case FSA_CONNECT_LR:
		usleep_range(50, 55);
		regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x9F);
		pr_info("%s - panzhao connect LR  \n", __func__);
		break;
	#endif /* OPLUS_ARCH_EXTENDS */
	case FSA_USBC_ORIENTATION_CC1:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_ORIENTATION_CC2:
		fsa4480_usbc_update_settings(fsa_priv, 0x78, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(fsa4480_switch_event);

static void fsa4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct fsa4480_priv *fsa_priv =
		container_of(work, struct fsa4480_priv, usbc_analog_work);

	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	fsa4480_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
}

static void fsa4480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

static int fsa4480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct fsa4480_priv *fsa_priv;
	int rc = 0;
	#ifdef OPLUS_ARCH_EXTENDS
	unsigned int reg_value = 0;
	#endif /* OPLUS_ARCH_EXTENDS */
	#ifdef OPLUS_ARCH_EXTENDS
	pr_err("%s enter fsa4480_probe\n", __func__);
	#endif /* OPLUS_ARCH_EXTENDS */
	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	memset(fsa_priv, 0, sizeof(struct fsa4480_priv));
	fsa_priv->dev = &i2c->dev;

#ifdef OPLUS_ARCH_EXTENDS
	fsa_priv->hs_det_pin = of_get_named_gpio(i2c->dev.of_node, "hs-det-gpio", 0);
	if (!gpio_is_valid(fsa_priv->hs_det_pin)) {
		rc = gpio_request(fsa_priv->hs_det_pin, "hs_det_gpio");
		if (rc) {
			pr_err("%s: hs_det_pin request failed, rc:%d\n", __func__, rc);
		} else {
			gpio_direction_output(fsa_priv->hs_det_pin, 1);
		}
	} else {
		gpio_direction_output(fsa_priv->hs_det_pin, 1);
	}
#ifdef CONFIG_TCPC_CLASS
	if (of_find_property(i2c->dev.of_node, "ext_cc_logic", NULL)) {
		fsa_priv->ext_cc_logic = of_property_read_bool(i2c->dev.of_node, "ext_cc_logic");
		if (fsa_priv->ext_cc_logic) {
			fsa_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
			if (!fsa_priv->tcpc_dev) {
				pr_err("%s get tcpc device type_c_port0 fail\n", __func__);
				goto err_data;
			}

			fsa_priv->psy_nb.notifier_call = fsa4480_usbc_event_changed;
			fsa_priv->psy_nb.priority = 0;
			rc = register_tcp_dev_notifier(fsa_priv->tcpc_dev, &fsa_priv->psy_nb, TCP_NOTIFY_TYPE_ALL);
			if (rc < 0) {
				pr_err("%s: register tcpc notifer fail\n", __func__);
				goto err_data;
			}
		}
	} else {
		fsa_priv->usb_psy = power_supply_get_by_name("usb");
		if (!fsa_priv->usb_psy) {
			rc = -EPROBE_DEFER;
			dev_err(fsa_priv->dev,
				"%s: could not get USB psy info: %d\n",
				__func__, rc);
			goto err_data;
		}

		fsa_priv->psy_nb.notifier_call = fsa4480_usbc_event_changed;
		fsa_priv->psy_nb.priority = 0;
		rc = power_supply_reg_notifier(&fsa_priv->psy_nb);
		if (rc) {
			dev_err(fsa_priv->dev, "%s: power supply reg failed: %d\n",
				__func__, rc);
			goto err_supply;
		}
	}
#else
	fsa_priv->usb_psy = power_supply_get_by_name("usb");
	if (!fsa_priv->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_err(fsa_priv->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
		goto err_data;
	}
#endif /* CONFIG_TCPC_CLASS */
#endif /* OPLUS_ARCH_EXTENDS */
	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_supply;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_supply;
	}

#ifdef OPLUS_ARCH_EXTENDS
	regmap_write(fsa_priv->regmap, FSA4480_RESET, 0x01);//reset DIO4480
	usleep_range(1*1000, 1*1005);
	regmap_read(fsa_priv->regmap, FSA4480_DEVICE_ID, &reg_value);
	dev_info(fsa_priv->dev, "%s: device id reg value: 0x%x\n", __func__, reg_value);
	if (reg_value == HL5280_DEVICE_REG_VALUE) {
		dev_info(fsa_priv->dev, "%s: switch chip is HL5280\n", __func__);
		fsa_priv->vendor = HL5280;
	} else if (reg_value == DIO4480_DEVICE_REG_VALUE) {
		dev_info(fsa_priv->dev, "%s: switch chip is DIO4480\n", __func__);
		fsa_priv->vendor = DIO4480;
	} else {
		dev_info(fsa_priv->dev, "%s: switch chip is FSA4480\n", __func__);
		fsa_priv->vendor = FSA4480;
	}
#ifndef CONFIG_TCPC_CLASS
	fsa_priv->psy_nb.notifier_call = fsa4480_usbc_event_changed;
	fsa_priv->psy_nb.priority = 0;
	rc = power_supply_reg_notifier(&fsa_priv->psy_nb);
	if (rc) {
		dev_err(fsa_priv->dev, "%s: power supply reg failed: %d\n",
			__func__, rc);
		goto err_supply;
	}
#endif /* CONFIG_TCPC_CLASS */
	if (fsa_priv->vendor != DIO4480) {
		fsa4480_update_reg_defaults(fsa_priv->regmap);
	} else {
		regmap_write(fsa_priv->regmap, FSA4480_RESET, 0x01);//reset DIO4480
		usleep_range(1*1000, 1*1005);
	}
#endif /* OPLUS_ARCH_EXTENDS */

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

	fsa_priv->fsa4480_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((fsa_priv->fsa4480_notifier).rwsem);
	fsa_priv->fsa4480_notifier.head = NULL;

	return 0;

err_supply:
	power_supply_put(fsa_priv->usb_psy);
err_data:
#ifdef OPLUS_ARCH_EXTENDS
	if (gpio_is_valid(fsa_priv->hs_det_pin)) {
		gpio_free(fsa_priv->hs_det_pin);
	}
#endif /* OPLUS_ARCH_EXTENDS */
	devm_kfree(&i2c->dev, fsa_priv);
	return rc;
}

static int fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	/* deregister from PMI */
#ifdef CONFIG_TCPC_CLASS
	if (fsa_priv->ext_cc_logic) {
		fsa4480_unreg_notifier(&fsa_priv->psy_nb, i2c->dev.of_node);
	} else {
		power_supply_unreg_notifier(&fsa_priv->psy_nb);
		power_supply_put(fsa_priv->usb_psy);
	}
#else
	power_supply_unreg_notifier(&fsa_priv->psy_nb);
	power_supply_put(fsa_priv->usb_psy);
#endif /* CONFIG_TCPC_CLASS */
	mutex_destroy(&fsa_priv->notification_lock);
#ifdef OPLUS_ARCH_EXTENDS
	if (gpio_is_valid(fsa_priv->hs_det_pin)) {
		gpio_free(fsa_priv->hs_det_pin);
	}

	devm_kfree(&i2c->dev, fsa_priv);
#endif /* OPLUS_ARCH_EXTENDS */
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

#ifdef OPLUS_ARCH_EXTENDS
//sync the code of reset the registers of typec switch when shutdown
static void fsa4480_shutdown(struct i2c_client *i2c) {
	struct fsa4480_priv *fsa_priv =
		(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv) {
		return;
	}

	pr_info("%s: recover all register while shutdown\n", __func__);

	if (fsa_priv->vendor == DIO4480) {
		regmap_write(fsa_priv->regmap, FSA4480_RESET, 0x01);//reset DIO4480
		return;
	}

	fsa4480_update_reg_defaults(fsa_priv->regmap);

	return;
}
#endif /* OPLUS_ARCH_EXTENDS */

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{
		.compatible = "qcom,fsa4480-i2c",
	},
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
#ifdef OPLUS_ARCH_EXTENDS
//sync the code of reset the registers of typec switch when shutdown
	.shutdown = fsa4480_shutdown,
#endif /* OPLUS_ARCH_EXTENDS */
};

static int __init fsa4480_init(void)
{
	int rc;

	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
late_initcall(fsa4480_init);

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL v2");
