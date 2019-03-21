#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

//enable 32k clock
#include <linux/mfd/sfax8.h>
extern int ip6103_pwr_mgmt_probe(struct platform_device *pdev);
extern int rn5t567_pwr_mgmt_probe(struct platform_device *pdev);

int (*power_probe[])(struct platform_device *) = {
	ip6103_pwr_mgmt_probe,
	rn5t567_pwr_mgmt_probe,
};

static int pwr_mgmt_probe(struct platform_device *pdev)
{
	struct sfax8 *sfax8 = dev_get_drvdata(pdev->dev.parent);
	return power_probe[sfax8->type](pdev);
}

static const struct of_device_id of_pwr_mgmt_match[] = {
	{ .compatible = "siflower, power-management", },
	{},
};

static struct platform_driver pwr_mgmt_driver = {
	.driver = {
		.name = "power-management",
		.owner = THIS_MODULE,
		.of_match_table = of_pwr_mgmt_match,
	},
	.probe = pwr_mgmt_probe,
};

module_platform_driver(pwr_mgmt_driver);

MODULE_DESCRIPTION("Power Management Driver");
MODULE_AUTHOR("Allen Guo <xijun.guo@siflower.com.cn>");
MODULE_LICENSE("GPL");
