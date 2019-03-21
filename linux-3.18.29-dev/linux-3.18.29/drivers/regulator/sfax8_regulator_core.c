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
static struct platform_device *rg_pdev = NULL;
extern int sfax8_ip6103_regulator_probe(struct platform_device *pdev);
extern int sfax8_rn5t567_regulator_probe(struct platform_device *pdev);

int (*pmu_probe[])(struct platform_device *) =
{
	sfax8_ip6103_regulator_probe,
	sfax8_rn5t567_regulator_probe,
};
int disable_clk_32k(void)
{
	struct clk_32k_ops *ops;
	if (!rg_pdev)
		return -EINVAL;
	ops = dev_get_drvdata(&rg_pdev->dev);
	if(!ops)
		return -EINVAL;
	if(ops->disable)
		return ops->disable(&rg_pdev->dev);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(disable_clk_32k);

int enable_clk_32k(void)
{
	struct clk_32k_ops *ops;
	if (!rg_pdev)
		return -EINVAL;
	ops = dev_get_drvdata(&rg_pdev->dev);
	if(!ops)
		return -EINVAL;
	if(ops->enable)
		return ops->enable(&rg_pdev->dev);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(enable_clk_32k);

static int sfax8_regulator_probe(struct platform_device *pdev)
{
	struct sfax8 *sfax8 = dev_get_drvdata(pdev->dev.parent);
	int ret;
	rg_pdev = pdev;
	ret = pmu_probe[sfax8->type](pdev);
	enable_clk_32k();
	return ret;
}

static const struct of_device_id of_sfax8_regulator_match[] = {
	{ .compatible = "siflower, sfax8-regulator", },
	{},
};
MODULE_DEVICE_TABLE(of, of_sfax8_regulator_match);

static struct platform_driver sfax8_regulator_driver = {
	.driver = {
		.name = "sfax8-regulator",
		.owner = THIS_MODULE,
		.of_match_table = of_sfax8_regulator_match,
	},
	.probe = sfax8_regulator_probe,
};

module_platform_driver(sfax8_regulator_driver);


MODULE_ALIAS("platform:sfax8-regulator");
MODULE_DESCRIPTION("SFAX8 regulator driver");
MODULE_AUTHOR("Xijun Guo <xijun.guo@siflower.com.cn>");
MODULE_LICENSE("GPL");
