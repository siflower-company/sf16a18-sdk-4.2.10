/*
 *	A18 not use IR device on IP6103, and this driver is not full testing,
 *	add this driver just for future to use.
 *
 */
 
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mfd/sfax8.h>


#ifdef CONFIG_SF_IRC_MODE9012
#define MODE		0x0
#elif defined(CONFIG_SF_IRC_MODE8NEC)
#define MODE		0x1
#elif defined(CONFIG_SF_IRC_MODERC5)
#define MODE		0x2
#else
#define MODE		0x3
#endif

#define IRC_IRQ_ENABLE_MASK				(0x1 << 2)
#define IRC_ENABLE_MASK					(0x1 << 3)
#define IRQ_EN							(0x1 << 2)
#define IRC_EN							(0x1 << 3)
#define IRC_MODE_MASK					0x3
#define IRC_IRQ_PENDING_MASK			(0x1 << 2)
#define CLEAR_IRQ_PENDING				(0x1 << 2)
#define MFP_INT_MASK					0x3
#define MFP_INT_FUNC					0x1


struct sfax8_misc {
	struct input_dev *idev;
	struct sfax8 *info;
	struct regmap *regmap;
	struct device *dev;
};

static irqreturn_t sfax8_misc_irq_handler(int irq, void *_sfax8_misc)
{
	struct sfax8_misc *sfmisc = _sfax8_misc;
	u32 value = 0;
	int ret;
#if defined(CONFIG_SF_IRC_MODE9012) || defined(CONFIG_SF_IRC_8NEC)
	u32	lvalue;
	regmap_read(sfmisc->regmap, SFAX8_IRC_IKDC0_REG, &lvalue);
	regmap_read(sfmisc->regmap, SFAX8_IRC_IKDC1_REG, &value);
	value = (value << 8) | lvalue;
#else
	regmap_read(sfmisc->regmap, SFAX8_IRC_IKDC0_REG, &value);
#endif
	input_event(sfmisc->idev, EV_MSC, MSC_SCAN, value);
	input_sync(sfmisc->idev);

	ret = regmap_write(sfmisc->regmap, SFAX8_IRC_CONF1_REG, CLEAR_IRQ_PENDING);
	if (ret){
		dev_dbg(sfmisc->dev, "Clear IRC irq error\n");
	}

	/*GPIO clear irq status*/
	ret = sf_pad_irq_clear(60);
	if (ret)
		dev_dbg("Clear GPIO irq status fail\n");

	return IRQ_HANDLED;

}

static int sfax8_misc_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct input_dev *input_dev;
	struct sfax8_misc *sfmisc;
	int ret;
	int irq;

	dev_dbg(&pdev->dev, "IR mode is %d", MODE);

	input_dev = devm_input_allocate_device(&pdev->dev);
	if (!input_dev){
		dev_err(&pdev->dev, "Input allocate device error %d\n", input_dev);
		ret = -ENOMEM;
		goto err;
	}

	sfmisc = devm_kzalloc(&pdev->dev, sizeof(struct sfax8_misc), GFP_KERNEL);
	if (!sfmisc){
		dev_err(&pdev->dev, "Alloc sfmisc error\n");
		ret = -ENOMEM;
		goto err;
	}

	input_dev->name = "sfax8";
	input_dev->phys = "sfax8/input0";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &pdev->dev;
	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	sfmisc->idev = input_dev;
	sfmisc->dev = &pdev->dev;
	sfmisc->info = dev_get_drvdata(pdev->dev.parent);
	sfmisc->regmap = sfmisc->info->regmap;

	dev_set_drvdata(&pdev->dev, sfmisc);
	ret = input_register_device(input_dev);
	if(ret){
		dev_err(&pdev->dev, "Register input device sfax8 error %d\n", ret);
		goto err;
	}

	np = of_get_child_by_name(pdev->dev.parent->of_node, "miscdev");
	pdev->dev.of_node = np;

	irq = platform_get_irq(pdev, 0);
	if (ret){
		dev_err(&pdev->dev, "Get irq num error %d\n", irq);
		goto err;
	}
	
	/*set gpio irq, just for now*/
	ret = sf_pad_irq_config(60, 0x1,0xff);
	if (ret){
		dev_err(&pdev->dev, "Set gpio irq error\n");
		goto err;
	}

	ret = regmap_update_bits(sfmisc->regmap, SFAX8_IRC_CONF0_REG, IRC_MODE_MASK
			| IRC_IRQ_ENABLE_MASK | IRC_ENABLE_MASK, MODE | IRQ_EN | IRC_EN);
	ret = regmap_update_bits(sfmisc->regmap, SFAX8_MFP_IRQ_REG, MFP_INT_MASK, MFP_INT_FUNC);
	if (ret)
		dev_dbg(&pdev->dev, "Set IR mode fail, error %d\n", ret);

	ret =devm_request_irq(&pdev->dev, irq, sfax8_misc_irq_handler, IRQF_SHARED, pdev->name, sfmisc);
	if (ret){
		dev_err(&pdev->dev, "Request irq error %d\n", ret);
		goto err;
	}

	dev_dbg(&pdev->dev, "Sf16ax8 misc device probe done\n");

err:
	return ret;	

}



static struct platform_driver sfax8_misc_driver = {
		.driver			= {
				.name	= "sfax8-misc",
				.owner	= THIS_MODULE,
		},
		.probe			= sfax8_misc_probe,
};

module_platform_driver(sfax8_misc_driver);

MODULE_AUTHOR("Allen Guo <xijun.guo@siflower.com.cn>");
MODULE_DESCRIPTION("SIFLOWER SFAX8 Input Misc Device driver");
MODULE_LICENSE("GPL");
