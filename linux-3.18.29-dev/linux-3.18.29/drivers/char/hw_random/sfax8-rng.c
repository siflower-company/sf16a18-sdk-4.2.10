/*
 * sfax8-rng.c - Random Number Generator driver for the sfax8 socs
 *
 * Copyright (C) 2017 Siflower Communication Technology Co.
 * Chang Li <chang.li@siflower.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <sf16a18.h>

#define SFAX8_RNG_CLK_EN			0x17CC
#define SFAX8_RNG_SOFT_CLK_EN		0x4C04
#define SFAX8_RNG_CONTROL_OFFSET		0x0
#define SFAX8_RNG_OUT1_OFFSET		0x8
#define SFAX8_SEED_SETUP			BIT(1)
#define SFAX8_RNG_START			BIT(0)
#define SFAX8_AUTOSUSPEND_DELAY	100

struct sfax8_rng {
	struct device *dev;
	struct hwrng rng;
	struct regmap *map;
	void __iomem *mem;
	struct clk *clk;
	struct clk *bus_clk;
};

static u32 sfax8_rng_readl(struct sfax8_rng *rng, u32 offset)
{
	return	__raw_readl(rng->mem + offset);
}

static void sfax8_rng_writel(struct sfax8_rng *rng, u32 val, u32 offset)
{
	__raw_writel(val, rng->mem + offset);
}

static int sfax8_init(struct hwrng *rng)
{
	struct sfax8_rng *sfax8_rng = container_of(rng,
						struct sfax8_rng, rng);
	sfax8_rng_writel(sfax8_rng, SFAX8_SEED_SETUP,
				SFAX8_RNG_CONTROL_OFFSET);	
	return 0;

}

static int sfax8_read(struct hwrng *rng, void *buf,
					size_t max, bool wait)
{
	struct sfax8_rng *sfax8_rng = container_of(rng,
						struct sfax8_rng, rng);
	u32 *data = buf;


	sfax8_rng_writel(sfax8_rng, SFAX8_RNG_START, SFAX8_RNG_CONTROL_OFFSET);
	*data = sfax8_rng_readl(sfax8_rng, SFAX8_RNG_OUT1_OFFSET);

	return 4;
}

static int sfax8_rng_probe(struct platform_device *pdev)
{
	struct sfax8_rng *sfax8_rng;
	struct resource *res;
	struct device_node *np;
	int err;
	np = pdev->dev.of_node;
	sfax8_rng = devm_kzalloc(&pdev->dev, sizeof(struct sfax8_rng),
					GFP_KERNEL);
	if (!sfax8_rng)
		return -ENOMEM;

	sfax8_rng->dev = &pdev->dev;
	sfax8_rng->rng.name = "sfax8";
	sfax8_rng->rng.init =	sfax8_init;
	sfax8_rng->rng.read = sfax8_read;
	
	sfax8_rng->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sfax8_rng->clk)) {
		dev_err(&pdev->dev, "clock intialization failed.\n");
		err = PTR_ERR(sfax8_rng->clk);
		return err;
	}
	sfax8_rng->bus_clk = of_clk_get((&pdev->dev)->of_node, 1);;
	if (IS_ERR(sfax8_rng->bus_clk)) {
		dev_err(&pdev->dev, "clock intialization failed.\n");
		err = PTR_ERR(sfax8_rng->bus_clk);
		return err;
	}

	clk_prepare_enable(sfax8_rng->clk);
	clk_prepare_enable(sfax8_rng->bus_clk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sfax8_rng->mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sfax8_rng->mem))
		return PTR_ERR(sfax8_rng->mem);


	if(release_reset(SF_CRYPTO_SOFT_RESET))
		return -EINVAL;
	platform_set_drvdata(pdev, sfax8_rng);
		
	return hwrng_register(&sfax8_rng->rng);
}

static int sfax8_rng_remove(struct platform_device *pdev)
{
	struct sfax8_rng *sfax8_rng = platform_get_drvdata(pdev);

	hwrng_unregister(&sfax8_rng->rng);
	hold_reset(SF_CRYPTO_SOFT_RESET);
	return 0;
}

static const struct of_device_id sfax8_rng_of_match[] = {
	{ .compatible = "siflower,sfax8-rng", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_rng_of_match);

static struct platform_driver sfax8_rng_driver = {
	.driver		= {
		.name	= "sfax8-rng",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sfax8_rng_of_match),
	},
	.probe		= sfax8_rng_probe,
	.remove		= sfax8_rng_remove,
};

module_platform_driver(sfax8_rng_driver);

MODULE_DESCRIPTION("SFAX8 H/W Random Number Generator driver");
MODULE_AUTHOR("CHANG LI <chang.li@siflower.com.cn>");
MODULE_LICENSE("GPL");
