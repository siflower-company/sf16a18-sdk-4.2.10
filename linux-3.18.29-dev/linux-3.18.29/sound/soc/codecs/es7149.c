/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define DAI_NAME	"codec_es7149"

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE |  \
			SNDRV_PCM_FMTBIT_S32_LE )

struct es7149_gpios{
	int speaker_gpio;
	int boot_gpio;
	int reset_gpio;
};

static struct snd_soc_codec_driver soc_codec_es7149 = {
	//this is a dummy driver
};

static struct snd_soc_dai_driver es7149_dai = {
	.name		= "dai_es7149",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture 	= {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
};

static int es7149_gpio_init(struct platform_device *pdev)
{
	struct es7149_gpios *gpios;
	struct device *dev =  &pdev->dev;
	int ret;
	gpios = devm_kzalloc(dev, sizeof(struct es7149_gpios), GFP_KERNEL);
	if (!gpios){
		dev_err(dev, "alloc memory failed!\n");
		ret = -ENOMEM;
		goto err;
	}

	if (!dev->of_node){
		dev_err(dev, "not find device node!\n");
		ret = -EINVAL;
		goto free;
	}
	gpios->speaker_gpio = of_get_named_gpio(dev->of_node, "speaker-gpio", 0);
	if (!gpio_is_valid(gpios->speaker_gpio)){
		ret = -EINVAL;
		goto free;
	}
	gpios->boot_gpio = of_get_named_gpio(dev->of_node, "boot-gpio", 0);
	if (!gpio_is_valid(gpios->boot_gpio)){
		ret = -EINVAL;
		goto free;
	}
	gpios->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpio", 0);
	if (!gpio_is_valid(gpios->reset_gpio)){
		ret = -EINVAL;
		goto free;
	}

	ret = gpio_request(gpios->speaker_gpio, "speaker-gpio");
	if (ret) {
		dev_err(dev, "request speaker gpio failed!\n");
		goto free;
	}
	ret = gpio_direction_output(gpios->speaker_gpio, 1);
	if (ret)
		goto free_speaker_gpio;

	ret = gpio_request(gpios->reset_gpio, "reset-gpio");
	if (ret) {
		dev_err(dev, "request reset gpio failed!\n");
		goto free_speaker_gpio;
	}
	ret = gpio_direction_output(gpios->reset_gpio, 1);
	if (ret)
		goto free_reset_gpio;

	ret = gpio_request(gpios->boot_gpio, "boot-gpio");
	if (ret) {
		dev_err(dev, "request boot gpio failed!\n");
		goto free_reset_gpio;
	}
	ret = gpio_direction_output(gpios->boot_gpio, 1);
	if (ret)
		goto free_boot_gpio;


	dev_set_drvdata(dev, gpios);
	return 0;

free_boot_gpio:
	gpio_free(gpios->boot_gpio);
free_reset_gpio:
	gpio_free(gpios->reset_gpio);
free_speaker_gpio:
	gpio_free(gpios->speaker_gpio);
free:
	devm_kfree(dev, gpios);
err:
	return ret;
}

static int es7149_probe(struct platform_device *pdev)
{
	int ret;

	ret = es7149_gpio_init(pdev);
	if (ret){
		dev_err(&pdev->dev, "init gpio failed!\n");
		goto err;
	}

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_es7149,
			&es7149_dai, 1);
	if (ret){
		dev_err(&pdev->dev, "es7149 probe failed!\n");
		goto err;
	}
	return 0;
err:
	return ret;
}

static int es7149_remove(struct platform_device *pdev)
{
	struct es7149_gpios *gpios;
	snd_soc_unregister_codec(&pdev->dev);
	gpios = dev_get_drvdata(&pdev->dev);
	gpio_free(gpios->speaker_gpio);
	gpio_free(gpios->boot_gpio);
	gpio_free(gpios->reset_gpio);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id codec_es7149_ids[] = {
	{ .compatible = "es7149", },
	{ }
};
MODULE_DEVICE_TABLE(of, codec_es7149_ids);
#endif

static struct platform_driver es7149_driver = {
	.probe		= es7149_probe,
	.remove		= es7149_remove,
	.driver		= {
		.name	= DAI_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(codec_es7149_ids),
	},
};

module_platform_driver(es7149_driver);
MODULE_LICENSE("GPL");
