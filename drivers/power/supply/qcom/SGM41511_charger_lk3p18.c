/*
* SGM41511 battery charging driver
*
* Copyright (C) 2020 SG MICRO by Stuart Su
*
* This driver is for Linux kernel 3.18
*
* This package is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/iio/consumer.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/pmic-voter.h>
#include <linux/string.h>


#define MIN_PARALLEL_ICL_UA		250000
#define SUSPEND_CURRENT_UA		2000

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/alarmtimer.h>

#if 1
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#else
#undef pr_info
#define pr_info pr_debug
#endif

/* Register 00h */
#define SGM41511_REG_00      	    0x00
#define REG00_ENHIZ_MASK		    0x80
#define REG00_ENHIZ_SHIFT		    7
#define	REG00_HIZ_ENABLE			1
#define	REG00_HIZ_DISABLE			0
#define	REG00_STAT_CTRL_MASK		0x60
#define REG00_STAT_CTRL_SHIFT		5
#define	REG00_STAT_CTRL_STAT		0
#define	REG00_STAT_CTRL_ICHG		1
#define	REG00_STAT_CTRL_IINDPM		2
#define	REG00_STAT_CTRL_DISABLE		3

#define REG00_IINLIM_MASK		    0x1F
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_LSB			100
#define	REG00_IINLIM_BASE			100

/* Register 01h */ 
#define SGM41511_REG_01		    	0x01
#define REG01_PFM_DIS_MASK	      	0x80
#define	REG01_PFM_DIS_SHIFT			7
#define	REG01_PFM_ENABLE			0
#define	REG01_PFM_DISABLE			1

#define REG01_WDT_RESET_MASK		0x40
#define REG01_WDT_RESET_SHIFT		6
#define REG01_WDT_RESET				1

#define	REG01_OTG_CONFIG_MASK		0x20
#define	REG01_OTG_CONFIG_SHIFT		5
#define	REG01_OTG_ENABLE			1
#define	REG01_OTG_DISABLE			0

#define REG01_CHG_CONFIG_MASK     	0x10
#define REG01_CHG_CONFIG_SHIFT    	4
#define REG01_CHG_DISABLE        	0
#define REG01_CHG_ENABLE         	1

#define REG01_SYS_MINV_MASK       	0x0E
#define REG01_SYS_MINV_SHIFT      	1

#define	REG01_MIN_VBAT_SEL_MASK		0x01
#define	REG01_MIN_VBAT_SEL_SHIFT	0
#define	REG01_MIN_VBAT_2P8V			0
#define	REG01_MIN_VBAT_2P5V			1

/* Register 0x02*/
#define SGM41511_REG_02             0x02
#define	REG02_BOOST_LIM_MASK		0x80
#define	REG02_BOOST_LIM_SHIFT		7
#define	REG02_BOOST_LIM_0P5A		0
#define	REG02_BOOST_LIM_1P2A		1

#define	REG02_Q1_FULLON_MASK		0x40
#define	REG02_Q1_FULLON_SHIFT		6
#define	REG02_Q1_FULLON_ENABLE		1
#define	REG02_Q1_FULLON_DISABLE		0

#define REG02_ICHG_MASK           	0x3F
#define REG02_ICHG_SHIFT          	0
#define REG02_ICHG_BASE           	0
#define REG02_ICHG_LSB            	60

/* Register 0x03*/ 
#define SGM41511_REG_03              0x03
#define REG03_IPRECHG_MASK        	0xF0
#define REG03_IPRECHG_SHIFT       	4
#define REG03_IPRECHG_BASE        	60
#define REG03_IPRECHG_LSB         	60

#define REG03_ITERM_MASK          	0x0F
#define REG03_ITERM_SHIFT         	0
#define REG03_ITERM_BASE          	60
#define REG03_ITERM_LSB           	60

/* Register 0x04*/ 
#define SGM41511_REG_04              0x04
#define REG04_VREG_MASK           	0xF8
#define REG04_VREG_SHIFT          	3
#define REG04_VREG_BASE           	3856
#define REG04_VREG_LSB            	32

#define	REG04_TOPOFF_TIMER_MASK		0x06
#define	REG04_TOPOFF_TIMER_SHIFT	1
#define	REG04_TOPOFF_TIMER_DISABLE	0
#define	REG04_TOPOFF_TIMER_15M		1
#define	REG04_TOPOFF_TIMER_30M		2
#define	REG04_TOPOFF_TIMER_45M		3

#define REG04_VRECHG_MASK         	0x01
#define REG04_VRECHG_SHIFT        	0
#define REG04_VRECHG_100MV        	0
#define REG04_VRECHG_200MV        	1

/* Register 0x05*/ 
#define SGM41511_REG_05             0x05
#define REG05_EN_TERM_MASK        	0x80
#define REG05_EN_TERM_SHIFT       	7
#define REG05_TERM_ENABLE         	1
#define REG05_TERM_DISABLE        	0

#define REG05_WDT_MASK            	0x30
#define REG05_WDT_SHIFT           	4
#define REG05_WDT_DISABLE         	0
#define REG05_WDT_40S             	1 
#define REG05_WDT_80S             	2
#define REG05_WDT_160S            	3
#define REG05_WDT_BASE            	0
#define REG05_WDT_LSB             	40

#define REG05_EN_TIMER_MASK       	0x08
#define REG05_EN_TIMER_SHIFT      	3
#define REG05_CHG_TIMER_ENABLE    	1
#define REG05_CHG_TIMER_DISABLE   	0

#define REG05_CHG_TIMER_MASK      	0x04
#define REG05_CHG_TIMER_SHIFT     	2
#define REG05_CHG_TIMER_5HOURS    	0
#define REG05_CHG_TIMER_10HOURS   	1

#define	REG05_TREG_MASK				0x02
#define	REG05_TREG_SHIFT			1
#define	REG05_TREG_90C				0
#define	REG05_TREG_110C				1

#define REG05_JEITA_ISET_MASK     	0x01
#define REG05_JEITA_ISET_SHIFT    	0
#define REG05_JEITA_ISET_50PCT    	0
#define REG05_JEITA_ISET_20PCT    	1

/* Register 0x06*/
#define SGM41511_REG_06             0x06
#define	REG06_OVP_MASK				0xC0
#define	REG06_OVP_SHIFT				0x6
#define	REG06_OVP_5P5V				0
#define	REG06_OVP_6P2V				1
#define	REG06_OVP_10P5V				2
#define	REG06_OVP_14P3V				3

#define	REG06_BOOSTV_MASK			0x30
#define	REG06_BOOSTV_SHIFT			4
#define	REG06_BOOSTV_4P85V			0
#define	REG06_BOOSTV_5V				1
#define	REG06_BOOSTV_5P15V			2
#define	REG06_BOOSTV_5P3V			3

#define	REG06_VINDPM_MASK			0x0F
#define	REG06_VINDPM_SHIFT			0
#define	REG06_VINDPM_BASE			3900
#define	REG06_VINDPM_LSB			100

/* Register 0x07*/
#define SGM41511_REG_07             0x07
#define REG07_FORCE_DPDM_MASK     	0x80
#define REG07_FORCE_DPDM_SHIFT    	7
#define REG07_FORCE_DPDM          	1

#define REG07_TMR2X_EN_MASK       	0x40
#define REG07_TMR2X_EN_SHIFT      	6
#define REG07_TMR2X_ENABLE        	1
#define REG07_TMR2X_DISABLE       	0

#define REG07_BATFET_DIS_MASK     	0x20
#define REG07_BATFET_DIS_SHIFT    	5
#define REG07_BATFET_OFF          	1
#define REG07_BATFET_ON          	0

#define REG07_JEITA_VSET_MASK     	0x10
#define REG07_JEITA_VSET_SHIFT    	4
#define REG07_JEITA_VSET_4100     	0
#define REG07_JEITA_VSET_VREG     	1

#define	REG07_BATFET_DLY_MASK		0x08
#define	REG07_BATFET_DLY_SHIFT		3
#define	REG07_BATFET_DLY_0S			0
#define	REG07_BATFET_DLY_10S		1

#define	REG07_BATFET_RST_EN_MASK	0x04
#define	REG07_BATFET_RST_EN_SHIFT	2
#define	REG07_BATFET_RST_DISABLE	0
#define	REG07_BATFET_RST_ENABLE		1

#define	REG07_VDPM_BAT_TRACK_MASK	 0x03
#define	REG07_VDPM_BAT_TRACK_SHIFT 	 0
#define	REG07_VDPM_BAT_TRACK_DISABLE 0
#define	REG07_VDPM_BAT_TRACK_200MV	 1
#define	REG07_VDPM_BAT_TRACK_250MV	 2
#define	REG07_VDPM_BAT_TRACK_300MV	 3

/* Register 0x08*/
#define SGM41511_REG_08           0x08
#define REG08_VBUS_STAT_MASK      0xE0
#define REG08_VBUS_STAT_SHIFT     5
#define REG08_VBUS_TYPE_NONE	  0
#define REG08_VBUS_TYPE_USB       1
#define REG08_VBUS_TYPE_ADAPTER   3
#define REG08_VBUS_TYPE_OTG       7

#define REG08_CHRG_STAT_MASK      0x18
#define REG08_CHRG_STAT_SHIFT     3
#define REG08_CHRG_STAT_IDLE      0
#define REG08_CHRG_STAT_PRECHG    1
#define REG08_CHRG_STAT_FASTCHG   2
#define REG08_CHRG_STAT_CHGDONE   3

#define REG08_PG_STAT_MASK        0x04
#define REG08_PG_STAT_SHIFT       2
#define REG08_POWER_GOOD          1

#define REG08_THERM_STAT_MASK     0x02
#define REG08_THERM_STAT_SHIFT    1

#define REG08_VSYS_STAT_MASK      0x01
#define REG08_VSYS_STAT_SHIFT     0
#define REG08_IN_VSYS_STAT        1

/* Register 0x09*/
#define SGM41511_REG_09           0x09
#define REG09_FAULT_WDT_MASK      0x80
#define REG09_FAULT_WDT_SHIFT     7
#define REG09_FAULT_WDT           1

#define REG09_FAULT_BOOST_MASK    0x40 
#define REG09_FAULT_BOOST_SHIFT   6 

#define REG09_FAULT_CHRG_MASK     0x30
#define REG09_FAULT_CHRG_SHIFT    4
#define REG09_FAULT_CHRG_NORMAL   0
#define REG09_FAULT_CHRG_INPUT    1
#define REG09_FAULT_CHRG_THERMAL  2
#define REG09_FAULT_CHRG_TIMER    3

#define REG09_FAULT_BAT_MASK      0x08
#define REG09_FAULT_BAT_SHIFT     3
#define	REG09_FAULT_BAT_OVP		  1

#define REG09_FAULT_NTC_MASK      0x07
#define REG09_FAULT_NTC_SHIFT     0
#define	REG09_FAULT_NTC_NORMAL	  0
#define REG09_FAULT_NTC_WARM      2
#define REG09_FAULT_NTC_COOL      3
#define REG09_FAULT_NTC_COLD      5
#define REG09_FAULT_NTC_HOT       6

/* Register 0x0A */
#define SGM41511_REG_0A             0x0A
#define	REG0A_VBUS_GD_MASK			0x80
#define	REG0A_VBUS_GD_SHIFT			7
#define	REG0A_VBUS_GD				1

#define	REG0A_VINDPM_STAT_MASK		0x40
#define	REG0A_VINDPM_STAT_SHIFT		6
#define	REG0A_VINDPM_ACTIVE			1

#define	REG0A_IINDPM_STAT_MASK		0x20
#define	REG0A_IINDPM_STAT_SHIFT		5
#define	REG0A_IINDPM_ACTIVE			1

#define	REG0A_TOPOFF_ACTIVE_MASK	0x08
#define	REG0A_TOPOFF_ACTIVE_SHIFT	3
#define	REG0A_TOPOFF_ACTIVE			1

#define	REG0A_ACOV_STAT_MASK		0x04
#define	REG0A_ACOV_STAT_SHIFT		2
#define	REG0A_ACOV_ACTIVE			1

#define	REG0A_VINDPM_INT_MASK		0x02
#define	REG0A_VINDPM_INT_SHIFT		1
#define	REG0A_VINDPM_INT_ENABLE		0
#define	REG0A_VINDPM_INT_DISABLE	1

#define	REG0A_IINDPM_INT_MASK		0x01
#define	REG0A_IINDPM_INT_SHIFT		0
#define	REG0A_IINDPM_INT_ENABLE		0
#define	REG0A_IINDPM_INT_DISABLE	1

#define	REG0A_INT_MASK_MASK			0x03
#define	REG0A_INT_MASK_SHIFT		0

/* Register 0x0B */

#define	SGM41511_REG_0B				0x0B
#define	REG0B_REG_RESET_MASK		0x80
#define	REG0B_REG_RESET_SHIFT		7
#define	REG0B_REG_RESET				1

#define REG0B_PN_MASK             	0x78
#define REG0B_PN_SHIFT            	3

#define REG0B_SGMPART_MASK             	0x04
#define REG0B_SGMPART_SHIFT            	2

#define REG0B_DEV_REV_MASK        	0x03
#define REG0B_DEV_REV_SHIFT       	0

#define DIE_LOW_RANGE_BASE_DEGC			34
#define DIE_LOW_RANGE_DELTA			16
#define DIE_LOW_RANGE_MAX_DEGC			97
#define DIE_LOW_RANGE_SHIFT			4
static int BatteryTestStatus_enable = 0;

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300	= 5300,
};

struct sgm_chg_param {
	const char	*name;
	u16		reg;
	u16		mask;
	u16		lshift;
	int		min_u;
	int		max_u;
	int		step_u;
};

struct sgm_params {
	struct sgm_chg_param	fcc;
	struct sgm_chg_param	ov;
	struct sgm_chg_param	usb_icl;
};

static struct sgm_params v1_params = {
	.fcc		= {
		.name	= "fast charge current",
		.reg	= SGM41511_REG_02,
		.mask	= REG02_ICHG_MASK,
		.lshift	= REG02_ICHG_SHIFT,
		.min_u	= 0,
		.max_u	= 3000000,
		.step_u	= 60000,
	},
	.ov		= {
		.name	= "battery over voltage",
		.reg	= SGM41511_REG_04,
		.mask	= REG04_VREG_MASK,
		.lshift	= REG04_VREG_SHIFT,
		.min_u	= 3856000,
		.max_u	= 4624000,
		.step_u	= 32000,
	},
	.usb_icl	= {
		.name   = "usb input current limit",
		.reg    = SGM41511_REG_00,
		.mask	= REG00_IINLIM_MASK,
		.lshift	= REG00_IINLIM_SHIFT,
		.min_u  = 100000,
		.max_u  = 3200000,
		.step_u = 100000,
	},
};
enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6200 = 6200,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14300 = 14300,
};

enum SGM41511_part_no {
	SGM41511 = 0x02,
};

enum SGM41511_charge_state {
	CHARGE_STATE_IDLE = REG08_CHRG_STAT_IDLE,
	CHARGE_STATE_PRECHG = REG08_CHRG_STAT_PRECHG,
	CHARGE_STATE_FASTCHG = REG08_CHRG_STAT_FASTCHG,
	CHARGE_STATE_CHGDONE = REG08_CHRG_STAT_CHGDONE,
};

struct smb_dt_props {
	bool	disable_ctm;
	int	pl_mode;
	int	pl_batfet_mode;
	bool	hw_die_temp_mitigation;
	u32	die_temp_threshold;
};
struct SGM41511 {
	struct device		*dev;
	char			*name;
	struct regmap		*regmap;
	int			max_fcc;

	struct smb_dt_props	dt;
	struct sgm_params	param;

	struct mutex		write_lock;
	struct mutex		suspend_lock;

	struct power_supply	*parallel_psy;
	struct pmic_revid_data	*pmic_rev_id;
	int			d_health;
	int			c_health;
	int			c_charger_temp_max;
	int			die_temp_deciDegC;
	int			suspended_usb_icl;
	int			charge_type;
	int			vbatt_uv;
	int			fcc_ua;
	bool			exit_die_temp;
	struct delayed_work	SGM41511_dump_work;
	bool			disabled;
	bool			suspended;
	bool			charging_enabled;
	bool			pin_status;

	struct votable		*irq_disable_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*chg_disable_votable;

/////////////////////////////////////////

	struct i2c_client *client;
	enum SGM41511_part_no part_no;
	int revision;
	int status;
	struct mutex data_lock;
	struct mutex charging_disable_lock;
	bool in_hiz;
	bool dis_safety;
	bool vindpm_triggered;
	bool iindpm_triggered;
	bool in_therm_regulation;
	bool in_vsys_regulation;
	bool power_good;
	bool vbus_good;
	bool topoff_active;
	bool acov_triggered;
	int charge_state;
	int fault_status;
	int skip_writes;
	int skip_reads;
	struct dentry *debug_root;
};

#define IS_USBIN(mode)				\
	((mode == POWER_SUPPLY_PL_USBIN_USBIN) \
	 || (mode == POWER_SUPPLY_PL_USBIN_USBIN_EXT))

#define PARALLEL_ENABLE_VOTER			"PARALLEL_ENABLE_VOTER"
#define MAIN_CHARGER_DIAG		"MAIN_CHARGER_DIAG"


#ifdef OPLUS_FEATURE_CHG_BASIC
static int factory_test_lock = 0;
#endif

static int SGM41511_update_bits(struct SGM41511 *SGM, u8 reg, u8 mask, u8 data);
static int SGM41511_set_parallel_charging(struct SGM41511 *SGM, bool disabled);

static int __SGM41511_read_reg(struct SGM41511* SGM, u8 reg, u8 *data)
{
	s32 rc;

	rc = i2c_smbus_read_byte_data(SGM->client, reg);
	if (rc < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return rc;
	}

	*data = (u8)rc;

	return 0;
}

static int __SGM41511_write_reg(struct SGM41511* SGM, int reg, u8 val)
{
	s32 rc;

	rc = i2c_smbus_write_byte_data(SGM->client, reg, val);
	if (rc < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",val, reg, rc);
		return rc;
	}

	return 0;
}

static int SGM41511_read_byte(struct SGM41511 *SGM, u8 *data, u8 reg)
{
	int rc;

	if (SGM->skip_reads) {
		*data = 0;
		return 0;
	}
	rc = __SGM41511_read_reg(SGM, reg, data);

	return rc;
}

static int SGM41511_write_byte(struct SGM41511 *SGM, u8 reg, u8 data)
{
	int rc;

	if (SGM->skip_writes) {
		return 0;
	}

	mutex_lock(&SGM->write_lock);
	rc = __SGM41511_write_reg(SGM, reg, data);
	mutex_unlock(&SGM->write_lock);

	if (rc) {
		pr_err("Failed: reg=%02X, rc=%d\n", reg, rc);
	}

	return rc;
}

static int smb1355_set_charge_param(struct SGM41511 *SGM,
			struct sgm_chg_param *param, int val_u)
{
	int rc;
	u8 val_raw;

	if (val_u > param->max_u || val_u < param->min_u) {
		pr_err("%s: %d is out of range [%d, %d]\n",
			param->name, val_u, param->min_u, param->max_u);
		return -EINVAL;
	}
#ifdef OPLUS_FEATURE_CHG_BASIC
	else
	{
		pr_info("%s: %d is in range [%d, %d]\n",
		param->name, val_u, param->min_u, param->max_u);
	}
#endif
	val_raw = ((val_u - param->min_u) / param->step_u) << param->lshift;

	rc = SGM41511_update_bits(SGM, param->reg, param->mask, val_raw);
	if (rc < 0) {
		pr_err("%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	return rc;
}

static int smb1355_get_charge_param(struct SGM41511 *SGM,
				struct sgm_chg_param *param, int *val_u)
{
	int rc;
	u8 val_raw;

	rc = SGM41511_read_byte(SGM, &val_raw, param->reg);
	if (rc < 0) {
		pr_err("%s: Couldn't read from 0x%04x rc=%d\n",
			param->name, param->reg, rc);
		return rc;
	}

	*val_u = ((val_raw & param->mask) >> param->lshift) * param->step_u + param->min_u;

	return rc;
}

int SGM41511_get_fcc(struct SGM41511 *SGM, union power_supply_propval *val)
{
	int rc = 0;

	mutex_lock(&SGM->suspend_lock);
	if (SGM->suspended) {
		val->intval = SGM->fcc_ua;
		goto done;
	}

	rc = smb1355_get_charge_param(SGM, &SGM->param.fcc, &val->intval);
	if (rc < 0)
		pr_err("failed to read fcc rc=%d\n", rc);
	else
		SGM->fcc_ua = val->intval;
done:
	mutex_unlock(&SGM->suspend_lock);
	return rc;
}

static int SGM41511_update_bits(struct SGM41511 *SGM, u8 reg, u8 mask, u8 data)
{
	int rc;
	u8 tmp;

	if (SGM->skip_reads || SGM->skip_writes)
		return 0;

	mutex_lock(&SGM->write_lock);
	rc = __SGM41511_read_reg(SGM, reg, &tmp);
	if (rc) {
		pr_err("Failed: reg=%02X, rc=%d\n", reg, rc);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	rc = __SGM41511_write_reg(SGM, reg, tmp);
	if (rc) {
		pr_err("Failed: reg=%02X, rc=%d\n", reg, rc);
	}

out:
	mutex_unlock(&SGM->write_lock);

	return rc;
}

static int SGM41511_disable_otg(struct SGM41511 *SGM)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_01, REG01_OTG_CONFIG_MASK, val);
}

static int SGM41511_enable_charger(struct SGM41511 *SGM)
{
	int rc;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	rc = SGM41511_update_bits(SGM, SGM41511_REG_01, REG01_CHG_CONFIG_MASK, val);

	return rc;
}

static int SGM41511_disable_charger(struct SGM41511 *SGM)
{
	int rc;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	rc = SGM41511_update_bits(SGM, SGM41511_REG_01, REG01_CHG_CONFIG_MASK, val);

	return rc;
}

int SGM41511_set_fcc(struct SGM41511 *SGM, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE)/REG02_ICHG_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_02, REG02_ICHG_MASK, ichg << REG02_ICHG_SHIFT);
}

int SGM41511_set_term_current(struct SGM41511 *SGM, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_03, REG03_ITERM_MASK, iterm << REG03_ITERM_SHIFT);
}

int SGM41511_set_prechg_current(struct SGM41511 *SGM, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_03, REG03_IPRECHG_MASK, iprechg << REG03_IPRECHG_SHIFT);
}

int SGM41511_set_chargevolt(struct SGM41511 *SGM, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	val = (volt - REG04_VREG_BASE)/REG04_VREG_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_04, REG04_VREG_MASK, val << REG04_VREG_SHIFT);
}

int SGM41511_get_prop_voltage_max(struct SGM41511 *SGM, union power_supply_propval *val)
{
	int rc = 0;

	mutex_lock(&SGM->suspend_lock);
	if (SGM->suspended) {
		val->intval = SGM->vbatt_uv;
		goto done;
	}

	rc = smb1355_get_charge_param(SGM, &SGM->param.ov, &val->intval);
	if (rc < 0)
		pr_err("failed to read vbatt rc=%d\n", rc);
	else {
		SGM->vbatt_uv = val->intval;
	}

done:
	mutex_unlock(&SGM->suspend_lock);
	return rc;
}

int SGM41511_set_input_volt_limit(struct SGM41511 *SGM, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_06, REG06_VINDPM_MASK, val << REG06_VINDPM_SHIFT);
}

static int SGM41511_set_icl(struct SGM41511 *SGM, int curr)
{
	int rc = 0;

	if (!IS_USBIN(SGM->dt.pl_mode))
		return 0;

	if ((curr / 1000) < 100) {
		/* disable parallel path (ICL < 100mA) */
		rc = SGM41511_set_parallel_charging(SGM, true);
	} else {
		rc = SGM41511_set_parallel_charging(SGM, false);
		if (rc < 0)
			return rc;

		rc = smb1355_set_charge_param(SGM,
				&SGM->param.usb_icl, curr);
		SGM->suspended_usb_icl = 0;
	}

	return rc;
}

int SGM41511_set_input_current_limit(struct SGM41511 *SGM, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;

	return SGM41511_update_bits(SGM, SGM41511_REG_00, REG00_IINLIM_MASK, val << REG00_IINLIM_SHIFT);
}

int SGM41511_get_input_current_limit(struct SGM41511 *SGM, int* curr)
{
	u8 val;
	int rc;

	rc = SGM41511_read_byte(SGM, &val, SGM41511_REG_00);
	if (rc){
		return rc;
	}
	*curr = REG00_IINLIM_BASE + ((val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT)* REG00_IINLIM_LSB;
	return rc;
}

int SGM41511_set_watchdog_timer(struct SGM41511 *SGM, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return SGM41511_update_bits(SGM, SGM41511_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(SGM41511_set_watchdog_timer);

int SGM41511_disable_watchdog_timer(struct SGM41511 *SGM)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_disable_watchdog_timer);

int SGM41511_reset_watchdog_timer(struct SGM41511 *SGM)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_01, REG01_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_reset_watchdog_timer);

int SGM41511_reset_chip(struct SGM41511 *SGM)
{
	int rc;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	rc = SGM41511_update_bits(SGM, SGM41511_REG_0B, REG0B_REG_RESET_MASK, val);

	return rc;
}
EXPORT_SYMBOL_GPL(SGM41511_reset_chip);

int SGM41511_enter_hiz_mode(struct SGM41511 *SGM)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_00, REG00_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_enter_hiz_mode);

int SGM41511_exit_hiz_mode(struct SGM41511 *SGM)
{
	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_00, REG00_ENHIZ_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_exit_hiz_mode);

static int SGM41511_enable_term(struct SGM41511* SGM, bool enable)
{
	u8 val;
	int rc;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	rc = SGM41511_update_bits(SGM, SGM41511_REG_05, REG05_EN_TERM_MASK, val);

	return rc;
}
EXPORT_SYMBOL_GPL(SGM41511_enable_term);

int SGM41511_set_boost_voltage(struct SGM41511 *SGM, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5V;

	return SGM41511_update_bits(SGM, SGM41511_REG_06, REG06_BOOSTV_MASK, val << REG06_BOOSTV_SHIFT);
}

static int SGM41511_set_acovp_threshold(struct SGM41511 *SGM, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14300)
		val = REG06_OVP_14P3V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6200)
		val = REG06_OVP_6P2V;
	else
		val = REG06_OVP_5P5V;

	return SGM41511_update_bits(SGM, SGM41511_REG_06, REG06_OVP_MASK, val << REG06_OVP_SHIFT);
}

static int SGM41511_set_stat_ctrl(struct SGM41511 *SGM, int ctrl)
{
	u8 val;
	val = ctrl;

	return SGM41511_update_bits(SGM, SGM41511_REG_00, REG00_STAT_CTRL_MASK, val << REG00_STAT_CTRL_SHIFT);
}

static int SGM41511_set_int_mask(struct SGM41511 *SGM, int mask)
{
	u8 val;
	val = mask;

	return SGM41511_update_bits(SGM, SGM41511_REG_0A, REG0A_INT_MASK_MASK, val << REG0A_INT_MASK_SHIFT);
}

static int SGM41511_enable_batfet(struct SGM41511 *SGM)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_07, REG07_BATFET_DIS_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_enable_batfet);

static int SGM41511_disable_batfet(struct SGM41511 *SGM)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_07, REG07_BATFET_DIS_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_disable_batfet);

static int SGM41511_set_batfet_delay(struct SGM41511 *SGM, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_07, REG07_BATFET_DLY_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_set_batfet_delay);

static int SGM41511_set_vdpm_bat_track(struct SGM41511 *SGM)
{
	const u8 val = REG07_VDPM_BAT_TRACK_200MV << REG07_VDPM_BAT_TRACK_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_07, REG07_VDPM_BAT_TRACK_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_set_vdpm_bat_track);

static int SGM41511_enable_safety_timer(struct SGM41511 *SGM)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_05, REG05_EN_TIMER_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_enable_safety_timer);

static int SGM41511_disable_safety_timer(struct SGM41511 *SGM)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return SGM41511_update_bits(SGM, SGM41511_REG_05, REG05_EN_TIMER_MASK, val);
}
EXPORT_SYMBOL_GPL(SGM41511_disable_safety_timer);

static int SGM41511_get_prop_batt_charge_type(struct SGM41511 *SGM,union power_supply_propval *val)
{
	u8 data = 0;
	int rc = 0;

	rc = SGM41511_read_byte(SGM, &data, SGM41511_REG_08);
	if (rc < 0) {
		pr_err("Couldn't read SMB1355_BATTERY_STATUS_3 rc=%d\n", rc);
		return rc;
	}
	data &= REG08_CHRG_STAT_MASK;
	data >>= REG08_CHRG_STAT_SHIFT;
	switch (data) {
		case CHARGE_STATE_FASTCHG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case CHARGE_STATE_PRECHG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case CHARGE_STATE_CHGDONE:
		case CHARGE_STATE_IDLE:
		default:
			pr_err("POWER_SUPPLY_CHARGE_TYPE_NONE\n");
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}
	
	pr_err("psp=%d\n",val->intval);
	return rc;
}

static int SGM41511_get_prop_charge_type(struct SGM41511 *SGM,union power_supply_propval *val)
{
	int rc =0;
	mutex_lock(&SGM->suspend_lock);
	if (SGM->suspended) {
		val->intval = SGM->charge_type;
		goto done;
	}
	rc = SGM41511_get_prop_batt_charge_type(SGM, val);
	if (rc < 0)
		pr_err("failed to read batt_charge_type %d\n", rc);
	else
		SGM->charge_type = val->intval;

done:
	mutex_unlock(&SGM->suspend_lock);
	return rc;
}

static bool SGM41511_get_prop_charge_status(struct SGM41511 *SGM)
{
	int rc;
	u8 status;

	rc = SGM41511_read_byte(SGM, &status, SGM41511_REG_08);
	if (rc) {
		return false;
	}

	mutex_lock(&SGM->data_lock);
	SGM->charge_state = (status & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;
	mutex_unlock(&SGM->data_lock);

	switch(SGM->charge_state) {
		case CHARGE_STATE_FASTCHG:
		case CHARGE_STATE_PRECHG:
			return true;
		case CHARGE_STATE_CHGDONE:
		case CHARGE_STATE_IDLE:
		default:
			return false;
	}

}

static enum power_supply_property SGM41511_parallel_props[] = {
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_CHARGER_TEMP,
	POWER_SUPPLY_PROP_CHARGER_TEMP_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_PARALLEL_MODE,
	POWER_SUPPLY_PROP_CONNECTOR_HEALTH,
	POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE,
	POWER_SUPPLY_PROP_PARALLEL_FCC_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED,
	POWER_SUPPLY_PROP_MIN_ICL,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_DIE_HEALTH,
#ifdef OPLUS_FEATURE_CHG_BASIC
	POWER_SUPPLY_PROP_SMB1355_TEST,
#endif
};
static int SGM41511_charger_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct SGM41511 *SGM = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			rc = SGM41511_get_prop_charge_type(SGM, val);
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_PIN_ENABLED:
			val->intval = SGM41511_get_prop_charge_status(SGM);
			break;
		case POWER_SUPPLY_PROP_CHARGER_TEMP:
			val->intval = -22;
			break;
		case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
			val->intval = -22;
			break;
		case POWER_SUPPLY_PROP_INPUT_SUSPEND:
			val->intval = SGM->disabled;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			rc = SGM41511_get_prop_voltage_max(SGM, val);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
			rc = SGM41511_get_fcc(SGM, val);
			break;
		case POWER_SUPPLY_PROP_MODEL_NAME:
			val->strval = "smb1355";
			pr_err("SGM POWER_SUPPLY_PROP_MODEL_NAME:%d\n", val->intval);
			break;
		case POWER_SUPPLY_PROP_PARALLEL_MODE:
			val->intval = POWER_SUPPLY_PL_USBIN_USBIN;
			break;
		case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		case POWER_SUPPLY_PROP_DIE_HEALTH:
			val->intval = POWER_SUPPLY_HEALTH_COOL;
			break;
		case POWER_SUPPLY_PROP_PARALLEL_BATFET_MODE:
			val->intval = POWER_SUPPLY_PL_NON_STACKED_BATFET;
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMITED:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			if (IS_USBIN(SGM->dt.pl_mode)) {
			/* Report cached ICL until its configured correctly */
				if (SGM->suspended_usb_icl)
					val->intval = SGM->suspended_usb_icl;
				else
					rc = SGM41511_get_input_current_limit(SGM, &val->intval);
			} else {
				val->intval = 0;
			}
			break;
		case POWER_SUPPLY_PROP_MIN_ICL:
			val->intval = MIN_PARALLEL_ICL_UA;
			break;
		case POWER_SUPPLY_PROP_PARALLEL_FCC_MAX:
			val->intval = 6000000;
			break;
		case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		/* Not in ship mode as long as device is active */
			val->intval = 0;
			break;
#ifdef OPLUS_FEATURE_CHG_BASIC
		case POWER_SUPPLY_PROP_SMB1355_TEST:
		val->intval = factory_test_lock;
		break;
#endif
		default:
			pr_err("parallel psy get prop %d not supported\n",
				psp);
			return -EINVAL;
		}

		if (rc < 0) {
			pr_err("Couldn't get prop %d rc = %d\n", psp, rc);
			return -ENODATA;
		}

		return rc;
}
static int SGM41511_set_parallel_charging(struct SGM41511 *SGM, bool disable)
{
	int rc;

	if (SGM->disabled == disable)
		return 0;

	if (IS_USBIN(SGM->dt.pl_mode)) {
		/*
		 * Initialize ICL configuration to minimum value while
		 * depending upon the set icl configuration method to properly
		 * configure the ICL value. At the same time, cache the value
		 * of ICL to be reported as 2mA.
		 */
		SGM->suspended_usb_icl = SUSPEND_CURRENT_UA;
		smb1355_set_charge_param(SGM,
				&SGM->param.usb_icl, MIN_PARALLEL_ICL_UA);
	}
	if (disable) {
		//cancel_delayed_work_sync(&SGM->SGM41511_dump_work);
		rc = SGM41511_disable_charger(SGM);
	} else {
		//schedule_delayed_work(&SGM->SGM41511_dump_work, 0);
		rc = SGM41511_enable_charger(SGM);
		
	}
	if (rc < 0) {
		pr_err("Couldn't configure charge enable source rc=%d\n", rc);
		disable = true;
	}
	/* Only enable temperature measurement for s/w based mitigation */
	if (!SGM->dt.hw_die_temp_mitigation) {
		if (disable) {
			SGM->exit_die_temp = true;
			//cancel_delayed_work_sync(&SGM->SGM41511_dump_work);
		} else {
			/* start the work to measure temperature */
			SGM->exit_die_temp = false;
			//schedule_delayed_work(&SGM->SGM41511_dump_work, 0);
		}
	}

	SGM->disabled = disable;
	SGM->charging_enabled= !disable;

	return 0;
}

static int SGM41511_charger_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct SGM41511 *SGM = power_supply_get_drvdata(psy);
	int rc = 0;

	mutex_lock(&SGM->suspend_lock);
	if (SGM->suspended) {
		pr_err("parallel power supply set prop %d\n", prop);
		goto done;
	}

	switch (prop) {
		case POWER_SUPPLY_PROP_INPUT_SUSPEND:
			if (factory_test_lock == 0) {
				rc = SGM41511_set_parallel_charging(SGM, (bool)val->intval);
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
#ifdef OPLUS_FEATURE_CHG_BASIC
			if (factory_test_lock == 0)
			SGM41511_set_icl(SGM,val->intval);
#else
			SGM41511_set_icl(SGM,val->intval);
#endif
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			rc = smb1355_set_charge_param(SGM, &SGM->param.ov,
						val->intval);
			break;
		case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
#ifdef OPLUS_FEATURE_CHG_BASIC
			if (factory_test_lock == 0) {
				rc = smb1355_set_charge_param(SGM, &SGM->param.fcc,
							val->intval);
			}
#else
				rc = smb1355_set_charge_param(SGM, &SGM->param.fcc,
									val->intval);
#endif
			break;
		case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
			SGM->c_health = val->intval;
			power_supply_changed(SGM->parallel_psy);
			break;
		case POWER_SUPPLY_PROP_CHARGER_TEMP_MAX:
			SGM->c_charger_temp_max = val->intval;
		case POWER_SUPPLY_PROP_SET_SHIP_MODE:
			if (!val->intval)
				break;
			SGM41511_enable_batfet(SGM);
			break;
#ifdef OPLUS_FEATURE_CHG_BASIC
		case POWER_SUPPLY_PROP_SMB1355_TEST:
			if (!SGM->chg_disable_votable) {
				SGM->chg_disable_votable = find_votable("CHG_DISABLE");
			}
			if (val->intval == 1 && factory_test_lock != 1) {
				factory_test_lock = 1;
				vote(SGM->chg_disable_votable, MAIN_CHARGER_DIAG, true, 0);//disable main charge
				rc = smb1355_set_charge_param(SGM, &SGM->param.fcc, 1000000);
				rc = smb1355_set_charge_param(SGM, &SGM->param.usb_icl, 1000000);
				SGM41511_enable_charger(SGM);
			} else if (val->intval == 0) {
				factory_test_lock = 0;
				rc = smb1355_set_charge_param(SGM, &SGM->param.fcc, 0);
				rc = smb1355_set_charge_param(SGM, &SGM->param.usb_icl, 0);
				vote(SGM->chg_disable_votable, MAIN_CHARGER_DIAG, false, 0);//enable main charge
			}
			pr_err("factory_test_lock = %d,intval=%d\n", factory_test_lock, val->intval);
			break;
#endif
		default:
			pr_debug("parallel power supply set prop %d not supported\n",
					prop);
			rc = -EINVAL;
		}
done:
	mutex_unlock(&SGM->suspend_lock);
	return rc;

}

static int SGM41511_charger_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
		switch (prop) {
		case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
			return 1;
#ifdef OPLUS_FEATURE_CHG_BASIC
		case POWER_SUPPLY_PROP_SMB1355_TEST:
			return 1;
#endif
		default:
			break;
		}

		return 0;
}

static struct power_supply_desc parallel_psy_desc = {
	.name			= "parallel",
	.type			= POWER_SUPPLY_TYPE_PARALLEL,
	.properties 	= SGM41511_parallel_props,
	.num_properties 	= ARRAY_SIZE(SGM41511_parallel_props),
	.get_property		= SGM41511_charger_get_property,
	.set_property		= SGM41511_charger_set_property,
	.property_is_writeable	= SGM41511_charger_is_writeable,
};
static int SGM41511_psy_register(struct SGM41511 *SGM)
{
	struct power_supply_config parallel_cfg = {};

	parallel_cfg.drv_data = SGM;
	parallel_cfg.of_node = SGM->dev->of_node;

	/* change to SGM41511's property list */
	parallel_psy_desc.properties = SGM41511_parallel_props;
	parallel_psy_desc.num_properties = ARRAY_SIZE(SGM41511_parallel_props);

	SGM->parallel_psy = devm_power_supply_register(SGM->dev,
						   &parallel_psy_desc,
						   &parallel_cfg);
	if (IS_ERR(SGM->parallel_psy)) {
		pr_err("Couldn't register parallel power supply\n");
		return PTR_ERR(SGM->parallel_psy);
	}

	return 0;
}

static void SGM41511_psy_unregister(struct SGM41511 *SGM)
{
	power_supply_unregister(SGM->parallel_psy);
}
#define MFG_ID_SGM41511			0x1
#define MFG_ID_BQ25601			0x0
#define SGM41511_MAX_PARALLEL_FCC_UA	3000000
#define BQ25601_MAX_PARALLEL_FCC_UA	6000000
static int SGM41511_detect_version(struct SGM41511 *SGM)
{
	int rc;
	u8 val;

	rc = SGM41511_read_byte(SGM, &val, SGM41511_REG_0B);
	if(rc == 0){
		val = !!(val & REG0B_SGMPART_MASK);
	}
	switch (val) {
	case MFG_ID_SGM41511:
		SGM->name = "SGM41511";
		SGM->max_fcc = SGM41511_MAX_PARALLEL_FCC_UA;
		break;
	case MFG_ID_BQ25601:
		SGM->name = "BQ25601";
		SGM->max_fcc = BQ25601_MAX_PARALLEL_FCC_UA;
		break;
	default:
		pr_err("Invalid value of REVID val=%d", val);
		return -EINVAL;
	}

	return rc;
}

static int SGM41511_init_device(struct SGM41511 *SGM)
{
	int rc;

	SGM41511_set_vdpm_bat_track(SGM);
	if (rc)
		pr_err("Failed to set prechg current, rc = %d\n",rc);

	rc = SGM41511_set_int_mask(SGM, REG0A_IINDPM_INT_MASK | REG0A_VINDPM_INT_MASK);
	if (rc)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	return 0;
}

static int SGM41511_detect_device(struct SGM41511* SGM)
{
	int rc;
	u8 data;

	rc = SGM41511_read_byte(SGM, &data, SGM41511_REG_0B);
	if(rc == 0){
		SGM->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		SGM->revision = (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return rc;
}

static int sgm41511_debug_mask = 1;
module_param_named(
	sgm41511_debug_mask, sgm41511_debug_mask, int, 0600
);

#define DIE_TEMP_MEAS_PERIOD_MS		10000
static void SGM41511_dump_reg(struct SGM41511* SGM)
{
	u8 status;
	u8 addr;
	int rc;
	u8 val;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		if (addr == 0x09) {
			pr_err("SGM Reg[09] = 0x%02X\n", SGM->fault_status);
			continue;
		}

		rc = SGM41511_read_byte(SGM, &val, addr);
		if (!rc) {
			if (sgm41511_debug_mask)
				pr_err("SGM Reg[%02X] = 0x%02X\n", addr, val);
		} else
			pr_err("SGM Reg red err\n");
	}

	rc = SGM41511_read_byte(SGM, &status, SGM41511_REG_0A);
	if (rc) {
		pr_err("failed to read reg0a\n");
		return;
	}

	mutex_lock(&SGM->data_lock);
	SGM->vbus_good = !!(status & REG0A_VBUS_GD_MASK);
	SGM->vindpm_triggered = !!(status & REG0A_VINDPM_STAT_MASK);
	SGM->iindpm_triggered = !!(status & REG0A_IINDPM_STAT_MASK);
	SGM->topoff_active = !!(status & REG0A_TOPOFF_ACTIVE_MASK);
	SGM->acov_triggered = !!(status & REG0A_ACOV_STAT_MASK);
	mutex_unlock(&SGM->data_lock);
	if (sgm41511_debug_mask) {
		if (!SGM->power_good)
			pr_info("Power Poor\n");
		if (!SGM->vbus_good)
			pr_err("Vbus voltage not good!\n");
		if (SGM->vindpm_triggered)
			pr_err("VINDPM triggered\n");
		if (SGM->iindpm_triggered)
			pr_err("IINDPM triggered\n");
		if (SGM->acov_triggered)
		pr_err("ACOV triggered\n");
	
		if (SGM->fault_status & REG09_FAULT_WDT_MASK)
			pr_err("Watchdog timer expired!\n");
		if (SGM->fault_status & REG09_FAULT_BOOST_MASK)
			pr_err("Boost fault occurred!\n");

		status = (SGM->fault_status & REG09_FAULT_CHRG_MASK) >> REG09_FAULT_CHRG_SHIFT;
		if (status == REG09_FAULT_CHRG_INPUT)
			pr_err("input fault!\n");
		else if (status == REG09_FAULT_CHRG_THERMAL)
			pr_err("charge thermal shutdown fault!\n");
		else if (status == REG09_FAULT_CHRG_TIMER)
			pr_err("charge timer expired fault!\n");

		if (SGM->fault_status & REG09_FAULT_BAT_MASK)
			pr_err("battery ovp fault!\n");

		status = (SGM->fault_status & REG09_FAULT_NTC_MASK) >> REG09_FAULT_NTC_SHIFT;

		if (status == REG09_FAULT_NTC_WARM)
			pr_debug("JEITA ACTIVE: WARM\n");
		else if (status == REG09_FAULT_NTC_COOL)
			pr_debug("JEITA ACTIVE: COOL\n");
		else if (status == REG09_FAULT_NTC_COLD)
			pr_debug("JEITA ACTIVE: COLD\n");
		else if (status == REG09_FAULT_NTC_HOT)
			pr_debug("JEITA ACTIVE: HOT!\n");
	}
}
static void SGM41511_dump_work(struct work_struct *work)
{
	struct SGM41511 *SGM = container_of(work, struct SGM41511,
							SGM41511_dump_work.work);
	SGM41511_dump_reg(SGM);
	schedule_delayed_work(&SGM->SGM41511_dump_work,
			msecs_to_jiffies(DIE_TEMP_MEAS_PERIOD_MS));
}

static ssize_t SGM41511_show_registers(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int rc ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "SGM41511 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		rc = SGM41511_read_byte(SGM, &val, addr);
		if (rc == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t SGM41511_store_registers(struct device *dev,	struct device_attribute *attr, const char *buf, size_t count)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	int rc;
	unsigned int reg;
	unsigned int val;

	rc = sscanf(buf, "%x %x", &reg, &val);
		if (rc == 2 && reg < 0x0B) {
		SGM41511_write_byte(SGM, (unsigned char)reg, (unsigned char)val);
	}

	return count;
}

static ssize_t SGM41511_battery_test_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BatteryTestStatus_enable);
}

static ssize_t SGM41511_battery_test_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		retval = -EINVAL;
	else
		BatteryTestStatus_enable = input;

	pr_err("BatteryTestStatus_enable = %d\n", BatteryTestStatus_enable);

	return retval;
}

static ssize_t SGM41511_show_hiz(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", SGM->in_hiz);
}

static ssize_t SGM41511_store_hiz(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	int rc;
	unsigned int val;

	rc = sscanf(buf, "%d", &val);
	if (rc == 1) {
		if (val)
			rc = SGM41511_enter_hiz_mode(SGM);
		else
			rc = SGM41511_exit_hiz_mode(SGM);
	}
	if (!rc)
		SGM->in_hiz = !!val;

	return rc;
}

static ssize_t SGM41511_show_dis_safety(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", SGM->dis_safety);
}

static ssize_t SGM41511_store_dis_safety(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct SGM41511 *SGM = dev_get_drvdata(dev);
	int rc;
	unsigned int val;

	rc = sscanf(buf, "%d", &val);
	if (rc == 1) {
		if (val)
			rc = SGM41511_disable_safety_timer(SGM);
		else
			rc = SGM41511_enable_safety_timer(SGM);
	}
	if (!rc)
		SGM->dis_safety = !!val;

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, SGM41511_show_registers, SGM41511_store_registers);
static DEVICE_ATTR(BatteryTestStatus, S_IRUGO | S_IWUSR, SGM41511_battery_test_status_show, SGM41511_battery_test_status_store);
static DEVICE_ATTR(hiz, S_IRUGO | S_IWUSR, SGM41511_show_hiz, SGM41511_store_hiz);
static DEVICE_ATTR(dissafety, S_IRUGO | S_IWUSR, SGM41511_show_dis_safety, SGM41511_store_dis_safety);

static struct attribute *SGM41511_attributes[] = {
	&dev_attr_registers.attr,
	&dev_attr_BatteryTestStatus.attr,
	&dev_attr_hiz.attr,
	&dev_attr_dissafety.attr,
	NULL,
};

static const struct attribute_group SGM41511_attr_group = {
	.attrs = SGM41511_attributes,
};

static int show_registers(struct seq_file *m, void *data)
{
	struct SGM41511 *SGM = m->private;
	u8 addr;
	int rc;
	u8 val;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		rc = SGM41511_read_byte(SGM, &val, addr);
		if (!rc)
			seq_printf(m, "Reg[%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}

static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct SGM41511 *SGM = inode->i_private;

	return single_open(file, show_registers, SGM);
}

static const struct file_operations reg_debugfs_ops = {
	.owner = THIS_MODULE,
	.open = reg_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_debugfs_entry(struct SGM41511 *SGM)
{
	SGM->debug_root = debugfs_create_dir("SGM41511", NULL);
	if (!SGM->debug_root)
		pr_err("Failed to create debug dir\n");

	if (SGM->debug_root) {

		debugfs_create_file("registers", S_IFREG | S_IRUGO,
			SGM->debug_root, SGM, &reg_debugfs_ops);

		debugfs_create_x32("fault_status", S_IFREG | S_IRUGO,
			SGM->debug_root, &(SGM->fault_status));

		debugfs_create_x32("charge_state", S_IFREG | S_IRUGO,
			SGM->debug_root, &(SGM->charge_state));

		debugfs_create_x32("skip_reads",
			S_IFREG | S_IWUSR | S_IRUGO,
			SGM->debug_root,
			&(SGM->skip_reads));
		debugfs_create_x32("skip_writes",
			S_IFREG | S_IWUSR | S_IRUGO,
			SGM->debug_root,
			&(SGM->skip_writes));
	}
}

#define DEFAULT_DIE_TEMP_LOW_THRESHOLD		90
static int SGM41511_parse_dt(struct SGM41511 *SGM)
{
		struct device_node *node = SGM->dev->of_node;
		int rc = 0;

		if (!node) {
			pr_err("device tree node missing\n");
			return -EINVAL;
		}

		SGM->dt.disable_ctm =
			of_property_read_bool(node, "qcom,disable-ctm");

		/*
		 * If parallel-mode property is not present default
		 * parallel configuration is USBMID-USBMID.
		 */
		rc = of_property_read_u32(node,
			"qcom,parallel-mode", &SGM->dt.pl_mode);
		if (rc < 0)
			SGM->dt.pl_mode = POWER_SUPPLY_PL_USBIN_USBIN;

		/*
		 * If stacked-batfet property is not present default
		 * configuration is NON-STACKED-BATFET.
		 */
		SGM->dt.pl_batfet_mode = POWER_SUPPLY_PL_NON_STACKED_BATFET;
		if (of_property_read_bool(node, "qcom,stacked-batfet"))
			SGM->dt.pl_batfet_mode = POWER_SUPPLY_PL_STACKED_BATFET;

		SGM->dt.hw_die_temp_mitigation = of_property_read_bool(node,
						"qcom,hw-die-temp-mitigation");

		SGM->dt.die_temp_threshold = DEFAULT_DIE_TEMP_LOW_THRESHOLD;
		of_property_read_u32(node, "qcom,die-temp-threshold-degc",
					&SGM->dt.die_temp_threshold);
		if (SGM->dt.die_temp_threshold > DIE_LOW_RANGE_MAX_DEGC)
			SGM->dt.die_temp_threshold = DIE_LOW_RANGE_MAX_DEGC;

		return 0;
}

static int SGM41511_init_hw(struct SGM41511 *SGM)
{
	int rc;

	SGM41511_set_stat_ctrl(SGM,0x0);
	SGM41511_disable_otg(SGM);
	rc = SGM41511_set_acovp_threshold(SGM,VAC_OVP_10500);
	if (rc)
		pr_err("Failed to set acovp threshold, rc = %d\n",rc);
	rc = SGM41511_disable_watchdog_timer(SGM);
	if (rc)
		pr_err("Failed to set watchdog_timer, rc = %d\n",rc);
	rc = SGM41511_disable_safety_timer(SGM);
	if (rc)
		pr_err("Failed to set safety_timer, rc = %d\n",rc);
	/* initialize FCC to 0 */
	rc = smb1355_set_charge_param(SGM, &SGM->param.fcc,
							0);
	if (rc)
		pr_err("Failed to set fcc, rc = %d\n",rc);
	rc = SGM41511_set_parallel_charging(SGM, true);
	if (rc) {
		pr_err("Couldn't disable parallel path rc=%d\n", rc);
		return rc;
	}
	return 0;
}
static struct of_device_id SGM41511_charger_match_table[];
static int SGM41511_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct SGM41511 *SGM;
	const struct of_device_id *id2;
	int rc;

	SGM = devm_kzalloc(&client->dev, sizeof(struct SGM41511), GFP_KERNEL);
	if (!SGM) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	SGM->dev = &client->dev;
	SGM->client = client;
	i2c_set_clientdata(client, SGM);


	SGM->param = v1_params;
	SGM->c_health = -EINVAL;
	SGM->d_health = -EINVAL;
	SGM->c_charger_temp_max = -EINVAL;
	mutex_init(&SGM->write_lock);
	mutex_init(&SGM->suspend_lock);
	mutex_init(&SGM->data_lock);
	mutex_init(&SGM->charging_disable_lock);

	SGM->disabled = false;
	SGM->die_temp_deciDegC = -EINVAL;

	rc = SGM41511_detect_device(SGM);
	if(rc) {
		pr_err("No SGM41511 device found!\n");
		return -ENODEV;
	}

	id2 = of_match_device(of_match_ptr(SGM41511_charger_match_table), SGM->dev);
	if (!id2) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}
	pr_err("find a matching device %d\n",id2);

	rc = SGM41511_detect_version(SGM);
	if (rc < 0) {
		pr_err("Couldn't detect SGM41511/BQ25601 SGM type rc=%d\n", rc);
		return -EINVAL;
	}
	rc = SGM41511_parse_dt(SGM);

	if (rc < 0) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	rc = SGM41511_init_hw(SGM);
	if (rc < 0) {
		pr_err("Couldn't initialize hardware rc=%d\n", rc);
		goto cleanup;
	}

	rc = SGM41511_init_device(SGM);//it's important
	if (rc) {
		pr_err("Failed to init device\n");
		goto cleanup;
	}
	rc = SGM41511_psy_register(SGM);
	if (rc) {
		pr_err("Failed to SGM41511_psy_register\n");
		goto err_1;
	}
	if (1) {
		INIT_DELAYED_WORK(&SGM->SGM41511_dump_work, SGM41511_dump_work);
		schedule_delayed_work(&SGM->SGM41511_dump_work, 0);
	}
	create_debugfs_entry(SGM);

	rc = sysfs_create_group(&SGM->dev->kobj, &SGM41511_attr_group);
	if (rc) {
		dev_err(SGM->dev, "failed to register sysfs. err: %d\n", rc);
	}
	pr_err("SGM41511 probe successfully, Part Num:%d, Revision:%d\n!", SGM->part_no, SGM->revision);

	return 0;
err_1:
	SGM41511_psy_unregister(SGM);
cleanup:
	i2c_set_clientdata(client, NULL);
	return rc;
}

static int SGM41511_suspend(struct device *dev)
{
	return 0;
}

static int SGM41511_suspend_noirq(struct device *dev)
{
	return 0;
}

static bool is_voter_available(struct SGM41511 *SGM)
{
	if (!SGM->fcc_votable) {
		SGM->fcc_votable = find_votable("FCC");
		if (!SGM->fcc_votable) {
			pr_debug("Couldn't find FCC votable\n");
			return false;
		}
	}

	if (!SGM->fv_votable) {
		SGM->fv_votable = find_votable("FV");
		if (!SGM->fv_votable) {
			pr_debug("Couldn't find FV votable\n");
			return false;
		}
	}
	return true;
}

static int SGM41511_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SGM41511 *SGM = i2c_get_clientdata(client);

	mutex_lock(&SGM->suspend_lock);
	SGM->suspended = false;
	mutex_unlock(&SGM->suspend_lock);

	/*
	 * During suspend i2c failures are fixed by reporting cached
	 * SGM state, to report correct values we need to invoke
	 * callbacks for the fcc and fv votables. To avoid excessive
	 * invokes to callbacks invoke only when SGM41511 is enabled.
	 */
	if (is_voter_available(SGM) && SGM->charging_enabled) {
		rerun_election(SGM->fcc_votable);
		rerun_election(SGM->fv_votable);
	}

	power_supply_changed(SGM->parallel_psy);

	return 0;
}

static int SGM41511_charger_remove(struct i2c_client *client)
{
	struct SGM41511 *SGM = i2c_get_clientdata(client);

	SGM41511_psy_unregister(SGM);

	mutex_destroy(&SGM->charging_disable_lock);
	mutex_destroy(&SGM->data_lock);
	mutex_destroy(&SGM->write_lock);

	debugfs_remove_recursive(SGM->debug_root);
	sysfs_remove_group(&SGM->dev->kobj, &SGM41511_attr_group);

	return 0;
}

static void SGM41511_charger_shutdown(struct i2c_client *client)
{
	struct SGM41511 *SGM = i2c_get_clientdata(client);
	int rc;

	/* disable parallel charging path */
	SGM41511_set_parallel_charging(SGM, true);

	if (rc < 0)
		pr_err("Couldn't disable parallel path rc=%d\n", rc);
}

static struct of_device_id SGM41511_charger_match_table[] = {
	{.compatible = "SG,SGM41511-charger",},
	{},
};
MODULE_DEVICE_TABLE(of,SGM41511_charger_match_table);

static const struct i2c_device_id SGM41511_charger_id[] = {
	{ "SGM41511-charger", SGM41511 },
	{},
};
MODULE_DEVICE_TABLE(i2c, SGM41511_charger_id);

static const struct dev_pm_ops SGM41511_pm_ops = {
	.resume = SGM41511_resume,
	.suspend_noirq = SGM41511_suspend_noirq,
	.suspend = SGM41511_suspend,
};

static struct i2c_driver SGM41511_charger_driver = {
	.driver = {
		.name = "SGM41511-charger",
		.owner = THIS_MODULE,
		.of_match_table = SGM41511_charger_match_table,
		.pm = &SGM41511_pm_ops,
	},
	.id_table = SGM41511_charger_id,
	.probe = SGM41511_charger_probe,
	.remove = SGM41511_charger_remove,
	.shutdown = SGM41511_charger_shutdown,
};

module_i2c_driver(SGM41511_charger_driver);

MODULE_DESCRIPTION("SGM SGM41511 Charger Driver");
MODULE_LICENSE("GPL v2");
