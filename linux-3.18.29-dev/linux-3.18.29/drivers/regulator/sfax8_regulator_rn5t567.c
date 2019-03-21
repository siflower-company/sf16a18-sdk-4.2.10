/*
 * Voltage regulator support for AMS SFAX8 PMIC
 *
 * Copyright (C) 2013 ams
 *
 * Author: Florian Lobmaier <florian.lobmaier@ams.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/sfax8.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/sfax8.h>
#include <linux/slab.h>

//#define ENABLE_DCDC1

/* Regulator IDs */
enum sfax8_regulators_id {
	SFAX8_REGULATOR_DCDC1,
	SFAX8_REGULATOR_DCDC2,
	SFAX8_REGULATOR_DCDC3,
	SFAX8_REGULATOR_DCDC4,
	SFAX8_REGULATOR_LDO1,
	SFAX8_REGULATOR_LDO2,
	SFAX8_REGULATOR_LDO3,
	SFAX8_REGULATOR_LDO4,
	SFAX8_REGULATOR_LDO5,
	SFAX8_REGULATOR_ID_MAX,
};

static int rn5t567_enable_clk_32k(struct device *dev)
{
	struct sfax8 *p = dev_get_drvdata(dev->parent);
	unsigned char buf = 0;
	int ret;
	ret = regmap_bulk_read(p->regmap, SFAX8_RN5T567_OUT32KEN, &buf, 1);
	if(ret){
		dev_err(dev, "%s Can not get 32k clock config value.\n", __func__);
		return ret;
	}
	buf |= 0x2;
	if(regmap_bulk_write(p->regmap, SFAX8_RN5T567_OUT32KEN, &buf, 1)) {
		dev_err(dev, "Setup 32K clock failed.\n");
		return -EAGAIN;
	}

	return 0;
}

static int rn5t567_disable_clk_32k(struct device *dev)
{
	struct sfax8 *p = dev_get_drvdata(dev->parent);
	unsigned char buf = 0;
	int ret;
	ret = regmap_bulk_read(p->regmap, SFAX8_RN5T567_OUT32KEN, &buf, 1);
	if(ret){
		dev_err(dev, "%s Can not get 32k clock config value.\n", __func__);
		return ret;
	}
	buf &= 0x1C;
	if(regmap_bulk_write(p->regmap, SFAX8_RN5T567_OUT32KEN, &buf, 1)) {
		dev_err(dev, "Disable 32K clock failed.\n");
		return -EAGAIN;
	}

	return 0;
}
static const struct sfax8_register_mapping sfax8_reg_lookup[] = {
	{
		.regulator_id = SFAX8_REGULATOR_DCDC1,
		.name = "sfax8-dcdc1",
		.vsel_reg = SFAX8_RN5T567_DC1DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_7_0,
		.enable_reg = SFAX8_RN5T567_DC1CTL,
		.enable_mask = ONE_BIT_SHIFT(0),
		.n_voltages = SFAX8_RN5T567_DCDC_VOLTAGES,
		.sf_pmu_current = 3000000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_DCDC2,
		.name = "sfax8-dcdc2",
		.vsel_reg = SFAX8_RN5T567_DC2DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_7_0,
		.enable_reg = SFAX8_RN5T567_DC2CTL,
		.enable_mask = ONE_BIT_SHIFT(0),
		.n_voltages = SFAX8_RN5T567_DCDC_VOLTAGES,
		.sf_pmu_current = 3000000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_DCDC3,
		.name = "sfax8-dcdc3-core",
		.vsel_reg = SFAX8_RN5T567_DC3DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_7_0,
		.enable_reg = SFAX8_RN5T567_DC3CTL,
		.enable_mask = ONE_BIT_SHIFT(0),
		.n_voltages = SFAX8_RN5T567_DCDC_VOLTAGES,
		.sf_pmu_current = 2000000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_DCDC4,
		.name = "sfax8-dcdc4",
		.vsel_reg = SFAX8_RN5T567_DC4DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_7_0,
		.enable_reg = SFAX8_RN5T567_DC4CTL,
		.enable_mask = ONE_BIT_SHIFT(0),
		.n_voltages = SFAX8_RN5T567_DCDC3_VOLTAGES,
		.sf_pmu_current = 2000000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_LDO1,
		.name = "sfax8-ldo1",
		.vsel_reg = SFAX8_RN5T567_LDO1DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_6_1,
		.enable_reg = SFAX8_RN5T567_LDOEN1,
		.enable_mask = ONE_BIT_SHIFT(0),
		.n_voltages = SFAX8_RN5T567_LDO_VOLTAGES,
		.sf_pmu_current = 300000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_LDO2,
		.name = "sfax8-ldo2",
		.vsel_reg = SFAX8_RN5T567_LDO2DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_6_1,
		.enable_reg = SFAX8_RN5T567_LDOEN1,
		.enable_mask = ONE_BIT_SHIFT(1),
		.n_voltages = SFAX8_RN5T567_LDO_VOLTAGES,
		.sf_pmu_current = 300000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_LDO3,
		.name = "sfax8-ldo3",
		.vsel_reg = SFAX8_RN5T567_LDO3DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_6_1,
		.enable_reg = SFAX8_RN5T567_LDOEN1,
		.enable_mask = ONE_BIT_SHIFT(2),
		.n_voltages = SFAX8_RN5T567_LDO3_VOLTAGES,
		.sf_pmu_current = 300000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_LDO4,
		.name = "sfax8-ldo4",
		.vsel_reg = SFAX8_RN5T567_LDO4DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_6_1,
		.enable_reg = SFAX8_RN5T567_LDOEN1,
		.enable_mask = ONE_BIT_SHIFT(3),
		.n_voltages = SFAX8_RN5T567_LDO_VOLTAGES,
		.sf_pmu_current = 200000,
	},
	{
		.regulator_id = SFAX8_REGULATOR_LDO5,
		.name = "sfax8-ldo5",
		.vsel_reg = SFAX8_RN5T567_LDO5DAC,
		.vsel_mask = SFAX8_RN5T567_VREG_MASK_6_1,
		.enable_reg = SFAX8_RN5T567_LDOEN1,
		.enable_mask = ONE_BIT_SHIFT(4),
		.n_voltages = SFAX8_RN5T567_LDO_VOLTAGES,
		.sf_pmu_current = 200000,
	},
};

static int sfax8_pmu_get_current_limit(struct regulator_dev *rdev)
{
	int id = rdev_get_id(rdev);
	return sfax8_reg_lookup[id].sf_pmu_current;
}

static int sfax8_pmu_set_current_limit(struct regulator_dev *rdev,
		int min_uA, int max_uA)
{
	struct sfax8_regulators *sfax8_regs = rdev_get_drvdata(rdev);
	dev_err(sfax8_regs->dev,"Current is fixed\n");
	return 0;
}

/*static int sfax8_regulator_enable_disable(struct regulator_dev *rdev)
{
	struct sfax8_regulators *sfax8_regs = rdev_get_drvdata(rdev);
	dev_dbg(sfax8_regs->dev,"svcc can not support enable or disable\n");
	return 0;
}*/

static struct regulator_ops sfax8_ops = {
	.is_enabled = regulator_is_enabled_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_current_limit = sfax8_pmu_get_current_limit,
	.set_current_limit = sfax8_pmu_set_current_limit,
};

/*static struct regulator_ops sfax8_svcc_ops = {
	.is_enabled = regulator_is_enabled_regmap,
	.enable = sfax8_regulator_enable_disable,
	.disable = sfax8_regulator_enable_disable,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_current_limit = sfax8_pmu_get_current_limit,
	.set_current_limit = sfax8_pmu_set_current_limit,
};*/

static struct of_regulator_match sfax8_regulator_matches[] = {
	{ .name = "dcdc1", },
	{ .name = "dcdc2", },
	{ .name = "dcdc3", },
	{ .name = "dcdc4", },
	{ .name = "ldo1", },
	{ .name = "ldo2", },
	{ .name = "ldo3", },
	{ .name = "ldo4", },
	{ .name = "ldo5", },
};

static int sfax8_get_regulator_dt_data(struct platform_device *pdev,
		struct sfax8_regulators *sfax8_regs)
{
	struct device_node *np;
	//struct sfax8_regulator_config_data *reg_config;
	int ret;

	np = of_get_child_by_name(pdev->dev.parent->of_node, "rn5t567-regulators");
	if (!np) {
		dev_err(&pdev->dev, "Device is not having regulators node\n");
		return -ENODEV;
	}
	pdev->dev.of_node = np;

	ret = of_regulator_match(&pdev->dev, np, sfax8_regulator_matches,
			ARRAY_SIZE(sfax8_regulator_matches));
	of_node_put(np);
	if (ret <= 0) {
		dev_err(&pdev->dev, "Parsing of regulator node failed: %d\n",
			ret);
		return ret;
	}
	sfax8_regs->regulator_num = ret;
	sfax8_regs->desc = devm_kzalloc(&pdev->dev, ret * sizeof(struct regulator_desc),
			GFP_KERNEL);
	if (!sfax8_regs->desc){
		dev_err(&pdev->dev, "can not malloc memory for sfax8 regulator desc\n");
		return -ENOMEM;
	}

	sfax8_regs->rdevs = devm_kzalloc(&pdev->dev, ret * sizeof(struct regulator_dev *),
			GFP_KERNEL);

	if (!sfax8_regs->rdevs){
		dev_err(&pdev->dev, "can not malloc memory for sfax8 regulator rdev\n");
		ret = -ENOMEM;
		goto err;
	}

	sfax8_regs->of_match = sfax8_regulator_matches;
	/*
	for (id = 0; id < ARRAY_SIZE(sfax8_regulator_matches); ++id) {
		reg_config = &sfax8_regs->reg_config_data[id];
		reg_config->reg_init = sfax8_regulator_matches[id].init_data;
	}
	*/
	return 0;
err:
	devm_kfree(&pdev->dev, sfax8_regs->desc);
	return ret;
}

int sfax8_rn5t567_regulator_probe(struct platform_device *pdev)
{
	struct sfax8 *sfax8 = dev_get_drvdata(pdev->dev.parent);
	struct sfax8_regulators *sfax8_regs;
	//struct sfax8_regulator_config_data *reg_config;
	struct regulator_dev *rdev;
	struct regulator_config config = { };
	struct regulator_ops *ops;
	struct clk_32k_ops *clk_ops;
	int id, match_id;
	int ret, registed_num;

	dev_dbg(&pdev->dev, "start probe sfax8 regulator\n");

	sfax8_regs = devm_kzalloc(&pdev->dev, sizeof(*sfax8_regs),
				GFP_KERNEL);
	if (!sfax8_regs)
		return -ENOMEM;

	clk_ops = devm_kzalloc(&pdev->dev, sizeof(struct clk_32k_ops),
				GFP_KERNEL);
	if (!clk_ops){
		ret = -ENOMEM;
		goto err;
	}
	clk_ops->enable = rn5t567_enable_clk_32k;
	clk_ops->disable= rn5t567_disable_clk_32k;

	sfax8_regs->dev = &pdev->dev;
	sfax8_regs->sfax8 = sfax8;
	clk_ops->sf_rg = sfax8_regs;
	platform_set_drvdata(pdev, clk_ops);

	ret = sfax8_get_regulator_dt_data(pdev, sfax8_regs);
	if (ret < 0)
		goto err2;

	config.dev = &pdev->dev;
	config.driver_data = sfax8_regs;
	config.regmap = sfax8->regmap;

	for (id = 0, match_id = 0; id < sfax8_regs->regulator_num; ) {
		if (!sfax8_regs->of_match[match_id].of_node){
			match_id++;
			if (match_id > SFAX8_REGULATOR_ID_MAX){
				dev_err(&pdev->dev, "some error occureed!\n");
				registed_num = id - 1;
				goto err5;
			}
			continue;
		}
		//reg_config = &sfax8_regs->reg_config_data[id];

		sfax8_regs->desc[id].name = sfax8_reg_lookup[match_id].name;
		sfax8_regs->desc[id].supply_name = sfax8_reg_lookup[match_id].sname;
		sfax8_regs->desc[id].id = sfax8_reg_lookup[match_id].regulator_id;
		sfax8_regs->desc[id].n_voltages =
					sfax8_reg_lookup[match_id].n_voltages;
		sfax8_regs->desc[id].type = REGULATOR_VOLTAGE;
		sfax8_regs->desc[id].owner = THIS_MODULE;
		sfax8_regs->desc[id].vsel_reg = sfax8_reg_lookup[match_id].vsel_reg;
		sfax8_regs->desc[id].vsel_mask =
					sfax8_reg_lookup[match_id].vsel_mask;
/*		if (id != SFAX8_REGULATOR_SVCC){
			sfax8_regs->desc[id].enable_reg =
					sfax8_reg_lookup[match_id].enable_reg;
			sfax8_regs->desc[id].enable_mask =
					sfax8_reg_lookup[match_id].enable_mask;
			ops = &sfax8_ops;
		}else{
			ops = &sfax8_svcc_ops;
		}*/
		sfax8_regs->desc[id].enable_reg =
				sfax8_reg_lookup[match_id].enable_reg;
		sfax8_regs->desc[id].enable_mask =
				sfax8_reg_lookup[match_id].enable_mask;
		ops = &sfax8_ops;
		switch (id) {
		case SFAX8_REGULATOR_DCDC1:
		case SFAX8_REGULATOR_DCDC2:
		case SFAX8_REGULATOR_DCDC3:
		case SFAX8_REGULATOR_DCDC4:
			sfax8_regs->desc[id].min_uV = 600000;
			sfax8_regs->desc[id].uV_step = 12500;
			sfax8_regs->desc[id].linear_min_sel = 0;
			sfax8_regs->desc[id].enable_time = 500;
			break;
		case SFAX8_REGULATOR_LDO1:
		case SFAX8_REGULATOR_LDO2:
		case SFAX8_REGULATOR_LDO4:
		case SFAX8_REGULATOR_LDO5:
			sfax8_regs->desc[id].min_uV = 900000;
			sfax8_regs->desc[id].uV_step = 50000;
			sfax8_regs->desc[id].linear_min_sel = 0;
			sfax8_regs->desc[id].enable_time = 500;
			break;
		case SFAX8_REGULATOR_LDO3:
			sfax8_regs->desc[id].min_uV = 600000;
			sfax8_regs->desc[id].uV_step = 50000;
			sfax8_regs->desc[id].linear_min_sel = 0;
			sfax8_regs->desc[id].enable_time = 500;
			break;
		default:
			break;
		}

		sfax8_regs->desc[id].ops = ops;
		config.init_data = sfax8_regs->of_match[match_id].init_data;
		config.of_node = sfax8_regs->of_match[match_id].of_node;

		dev_dbg(&pdev->dev, "register regulator num %d\n", id);
		/*
		printk("regulator debug info : regulator name %s; id : %d; match id : %d\n",
				sfax8_regs->of_match[match_id].name, id, match_id);
		*/

		rdev = devm_regulator_register(&pdev->dev,
					&sfax8_regs->desc[id], &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "regulator %d register failed %d\n",
				id, ret);
			goto err3;
		}

		sfax8_regs->rdevs[id] = rdev;
		ret = regulator_enable_regmap(rdev);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Regulator %d enable failed: %d\n",
				id, ret);
			goto err4;
		}
		id++;
		match_id++;
/*
			ret = sfax8_extreg_init(sfax8_regs, id,
					reg_config->ext_control);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"SFAX8 ext control failed: %d", ret);
				return ret;
			}
		}*/
	}
	return 0;
err5:
	if (registed_num > 0)
		regulator_disable_regmap(sfax8_regs->rdevs[registed_num]);
err4:
	for (id = 0; id < registed_num; id++){
		regulator_disable_regmap(sfax8_regs->rdevs[id]);
	}
err3:
	for (id = 0; id <= registed_num; id++){
		devm_regulator_unregister(&pdev->dev, sfax8_regs->rdevs[id]);
	}
	devm_kfree(&pdev->dev, sfax8_regs->rdevs);
	devm_kfree(&pdev->dev, sfax8_regs->desc);
err2:
	devm_kfree(&pdev->dev, clk_ops);
err:
	devm_kfree(&pdev->dev, sfax8_regs);
	return ret;
}
EXPORT_SYMBOL(sfax8_rn5t567_regulator_probe);
